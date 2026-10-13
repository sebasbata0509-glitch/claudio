// tone-ayan-cli: exercise the analysis + resynthesis engine without a DAW.
//
//   tone-ayan-cli analyze in.wav [project.json]
//   tone-ayan-cli render  in.wav edits.json out.wav [--engine psola|pv]
//   tone-ayan-cli compare in.wav edits.json
//   tone-ayan-cli demo    out.wav            (writes a slightly out-of-tune synthetic sung phrase)
//
// edits.json is either a full project (as written by `analyze`) or a list of edits, see README.

#include "analysis/Analyzer.h"
#include "common/MathUtil.h"
#include "common/WavFile.h"
#include "engine/EditModel.h"
#include "engine/Renderer.h"
#include "model/Edits.h"
#include "model/Serialize.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <iterator>
#include <random>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace tone;
using nlohmann::json;

namespace
{
const char* kNoteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

std::string noteName (float midi)
{
    const int m = (int) std::lround (midi);
    const int cents = (int) std::lround ((midi - (float) m) * 100.0f);
    char buf[32];
    std::snprintf (buf, sizeof (buf), "%s%d%+d", kNoteNames[((m % 12) + 12) % 12], m / 12 - 1, cents);
    return buf;
}

std::string readFile (const std::string& path)
{
    std::ifstream f (path);
    if (! f)
        throw std::runtime_error ("cannot open " + path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct Session
{
    std::shared_ptr<SourceAudio> source;
    std::shared_ptr<AnalysisResult> analysis;
};

Session load (const std::string& wav)
{
    Session s;
    s.source = std::make_shared<SourceAudio> (readWavMono (wav));
    const auto t0 = std::chrono::steady_clock::now();
    JobControl job;
    int lastPct = -1;
    job.onProgress = [&lastPct] (float p)
    {
        const int pct = (int) (p * 100.0f);
        if (pct / 10 != lastPct / 10)
        {
            lastPct = pct;
            std::fprintf (stderr, "\ranalysing... %3d%%", pct);
        }
    };
    s.analysis = std::make_shared<AnalysisResult> (analyse (s.source->samples.data(), s.source->samples.size(), s.source->sampleRate, {}, job));
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::fprintf (stderr, "\ranalysed %.2f s of audio in %.2f s: %zu notes, %zu pitch marks\n",
                  s.source->lengthSeconds(), secs, s.analysis->notes.size(), s.analysis->marks.size());
    return s;
}

void printNotes (const Project& p)
{
    std::printf ("  #   start     end   dur(s)  note      shift  stretch formant gain\n");
    for (size_t i = 0; i < p.notes.size(); ++i)
    {
        const auto& n = p.notes[i];
        std::printf ("%3zu %7.3f %7.3f  %6.3f  %-8s %+6.2f  %6.2f  %+5.1f %+5.1f\n", i, n.dstStart, n.dstEnd, n.dstDuration(),
                     noteName (n.targetMidi()).c_str(), n.pitchShift, n.stretch(), n.formantShift, n.gainDb);
    }
}

NoteIdSet selectNotes (const Project& p, const json& e)
{
    NoteIdSet ids;
    auto addIndex = [&] (int i)
    {
        if (i < 0 || i >= (int) p.notes.size())
            throw std::runtime_error ("note index out of range: " + std::to_string (i));
        ids.insert (p.notes[(size_t) i].id);
    };
    if (e.contains ("note"))
        addIndex (e["note"].get<int>());
    if (e.contains ("notes"))
    {
        if (e["notes"].is_string() && e["notes"] == "all")
            for (const auto& n : p.notes)
                ids.insert (n.id);
        else
            for (const auto& i : e["notes"])
                addIndex (i.get<int>());
    }
    return ids;
}

/** Applies a project file or an edit list to the freshly analysed project. */
Project applyEdits (const Session& s, const std::string& path, SynthEngine& engine)
{
    const auto text = readFile (path);
    const auto j = json::parse (text);
    if (j.value ("format", std::string()) == "tone-ayan-project")
        return projectFromJson (text);

    Project p = projectFromAnalysis (*s.analysis);
    const auto centreOf = makeCenterFn (s.analysis->track);
    if (j.contains ("scale"))
    {
        p.scale.root = Scale::rootFromName (j["scale"].value ("root", std::string ("C")));
        p.scale.type = Scale::typeFromName (j["scale"].value ("type", std::string ("Chromatic")));
    }
    if (j.value ("engine", std::string ("psola")) == "pv")
        engine = SynthEngine::PhaseVocoder;

    for (const auto& e : j.value ("edits", json::array()))
    {
        const auto ids = selectNotes (p, e);
        if (e.contains ("pitch"))      edits::transpose (p, ids, e["pitch"].get<float>());
        if (e.contains ("setPitch"))
            for (auto id : ids)
                if (auto* n = p.find (id))
                    n->pitchShift = e["setPitch"].get<float>() - n->centerMidi;
        if (e.contains ("center"))     edits::correctPitchCenter (p, ids, e["center"].get<float>());
        if (e.contains ("correctToScale")) edits::correctToScale (p, ids, e["correctToScale"].get<float>());
        if (e.contains ("drift"))      edits::setDrift (p, ids, e["drift"].get<float>());
        if (e.contains ("modulation")) edits::setModulation (p, ids, e["modulation"].get<float>());
        if (e.contains ("formant"))    edits::setFormant (p, ids, e["formant"].get<float>());
        if (e.contains ("gain"))       edits::setGain (p, ids, e["gain"].get<float>());
        if (e.contains ("time"))       edits::moveTime (p, ids, e["time"].get<double>());
        if (e.contains ("stretch"))
            for (auto id : ids)
                if (const auto* n = p.find (id))
                    edits::resizeEdge (p, id, edits::Edge::End, n->dstStart + n->dstDuration() * e["stretch"].get<double>());
        if (e.contains ("split"))
            for (auto id : ids)
                if (const auto* n = p.find (id))
                    edits::split (p, id, n->dstStart + n->dstDuration() * e["split"].get<double>(), centreOf);
        if (e.contains ("merge") && e["merge"].get<bool>())
            edits::merge (p, ids, centreOf);
    }
    return p;
}

std::vector<float> renderWith (const Session& s, const Project& p, SynthEngine engine, double* seconds = nullptr)
{
    Renderer r (s.source, s.analysis);
    r.setEngine (engine);
    std::vector<float> out;
    const auto t0 = std::chrono::steady_clock::now();
    r.render (p, out);
    if (seconds != nullptr)
        *seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    return out;
}

/** Re-analyses a render and reports measured vs target pitch per note. Returns max |error| in cents. */
float report (const std::vector<float>& out, double sr, const Project& p, const AnalysisResult& a)
{
    const auto track = runPYin (out.data(), out.size(), sr);
    const EditModel model (p, a.track);
    float worst = 0.0f;
    std::printf ("  #  target     measured   error(cents)\n");
    for (size_t i = 0; i < p.notes.size(); ++i)
    {
        const auto& n = p.notes[i];
        const double pad = 0.2 * n.dstDuration();
        std::vector<float> measured, expected;
        for (int f = track.frameAt (n.dstStart + pad); f <= track.frameAt (n.dstEnd - pad); ++f)
        {
            if (! track.frames[(size_t) f].voiced)
                continue;
            const double src = model.timeMap().toSrc (track.timeOfFrame (f));
            const int sf = a.track.frameAt (src);
            if (! a.track.frames[(size_t) sf].voiced)
                continue;
            measured.push_back (track.frames[(size_t) f].midi);
            expected.push_back (model.editedMidiAtFrame ((int) i, sf));
        }
        if (measured.empty())
        {
            std::printf ("%3zu  %-9s  (unvoiced)\n", i, noteName (n.targetMidi()).c_str());
            continue;
        }
        const float err = 100.0f * (median (measured) - median (expected));
        worst = std::max (worst, std::abs (err));
        std::printf ("%3zu  %-9s  %-9s  %+7.2f\n", i, noteName (median (expected)).c_str(), noteName (median (measured)).c_str(), err);
    }
    return worst;
}

/** A sung-like phrase: vowel formants, vibrato, drift, legato glides, a breath, slightly flat/sharp notes. */
void writeDemo (const std::string& path)
{
    const double sr = 44100.0;
    struct N { float midi; double dur; float detuneCents; };
    const N melody[] = { { 57, 0.45, -28 }, { 60, 0.45, 18 }, { 64, 0.7, -35 }, { 62, 0.4, 22 },
                         { 60, 0.35, -15 }, { 59, 0.35, 30 }, { 60, 0.9, -20 } };
    std::vector<float> out ((size_t) (0.25 * sr), 0.0f);
    std::mt19937 rng (3);
    std::uniform_real_distribution<float> uni (-1.0f, 1.0f);

    double phase = 0.0, t = 0.0, prevMidi = melody[0].midi + melody[0].detuneCents / 100.0;
    for (size_t k = 0; k < std::size (melody); ++k)
    {
        const auto& note = melody[k];
        const double target = note.midi + note.detuneCents / 100.0;
        if (k == 4)
        {
            // breath / consonant between phrases
            for (int i = 0; i < (int) (0.18 * sr); ++i)
                out.push_back (0.03f * uni (rng) * (float) std::sin (kPi * i / (0.18 * sr)));
            prevMidi = target;
        }
        const auto len = (size_t) (note.dur * sr);
        for (size_t i = 0; i < len; ++i, t += 1.0 / sr)
        {
            const double local = (double) i / sr;
            const double glide = std::min (1.0, local / 0.06);
            const double vibDepth = std::min (1.0, local / 0.25) * 0.35; // semitones, fading in
            const double midi = prevMidi + (target - prevMidi) * smoothStep (glide)
                                + vibDepth * std::sin (2.0 * kPi * 5.6 * t) + 0.15 * std::sin (2.0 * kPi * 0.7 * t);
            const double f0 = 440.0 * std::exp2 ((midi - 69.0) / 12.0);
            double v = 0.0;
            for (int h = 1; h * f0 < 0.45 * sr; ++h)
            {
                const double f = h * f0;
                const double env = 1.0 / (1.0 + std::pow ((f - 650.0) / 120.0, 2.0)) + 0.55 / (1.0 + std::pow ((f - 1150.0) / 130.0, 2.0))
                                   + 0.25 / (1.0 + std::pow ((f - 2600.0) / 180.0, 2.0)) + 0.02;
                v += env * std::sin (h * phase);
            }
            phase = std::fmod (phase + 2.0 * kPi * f0 / sr, 2.0 * kPi * 64.0);
            const double edge = std::min ({ 1.0, local / 0.02, (note.dur - local) / 0.02 });
            const double amp = (k == 3 || k == 6) ? 1.0 : 0.85; // legato phrase dynamics
            out.push_back ((float) (0.22 * amp * std::max (0.25, edge) * v + 0.002 * uni (rng)));
        }
        prevMidi = target;
    }
    out.resize (out.size() + (size_t) (0.3 * sr), 0.0f);
    writeWavMono (path, out.data(), out.size(), sr);
    std::fprintf (stderr, "wrote %s (%.2f s)\n", path.c_str(), (double) out.size() / sr);
}

int usage()
{
    std::fprintf (stderr,
                  "Tone-ayan CLI\n"
                  "  tone-ayan-cli analyze in.wav [project.json]\n"
                  "  tone-ayan-cli render  in.wav edits.json out.wav [--engine psola|pv]\n"
                  "  tone-ayan-cli compare in.wav edits.json\n"
                  "  tone-ayan-cli demo    out.wav\n");
    return 2;
}
} // namespace

int main (int argc, char** argv)
{
    try
    {
        if (argc < 3)
            return usage();
        const std::string cmd = argv[1];

        if (cmd == "demo")
        {
            writeDemo (argv[2]);
            return 0;
        }

        if (cmd == "analyze" || cmd == "analyse")
        {
            const auto s = load (argv[2]);
            const auto p = projectFromAnalysis (*s.analysis);
            printNotes (p);
            if (argc > 3)
            {
                std::ofstream (argv[3]) << projectToJson (p, 2);
                std::fprintf (stderr, "wrote %s\n", argv[3]);
            }
            return 0;
        }

        if (cmd == "render" && argc >= 5)
        {
            const auto s = load (argv[2]);
            auto engine = SynthEngine::Psola;
            const auto p = applyEdits (s, argv[3], engine);
            for (int i = 5; i + 1 < argc; ++i)
                if (std::string (argv[i]) == "--engine")
                    engine = std::string (argv[i + 1]) == "pv" ? SynthEngine::PhaseVocoder : SynthEngine::Psola;
            printNotes (p);
            double secs = 0.0;
            const auto out = renderWith (s, p, engine, &secs);
            writeWavMono (argv[4], out.data(), out.size(), s.source->sampleRate);
            std::fprintf (stderr, "rendered %.2f s with %s in %.2f s -> %s\n", (double) out.size() / s.source->sampleRate,
                          engine == SynthEngine::Psola ? "PSOLA" : "phase vocoder", secs, argv[4]);
            return 0;
        }

        if (cmd == "compare" && argc >= 4)
        {
            const auto s = load (argv[2]);
            auto engine = SynthEngine::Psola;
            const auto p = applyEdits (s, argv[3], engine);
            for (auto e : { SynthEngine::Psola, SynthEngine::PhaseVocoder })
            {
                double secs = 0.0;
                const auto out = renderWith (s, p, e, &secs);
                std::printf ("\n== %s (render %.2f s)\n", e == SynthEngine::Psola ? "TD-PSOLA" : "Phase vocoder", secs);
                const float worst = report (out, s.source->sampleRate, p, *s.analysis);
                std::printf ("worst pitch error: %.2f cents\n", worst);
            }
            return 0;
        }

        return usage();
    }
    catch (const std::exception& e)
    {
        std::fprintf (stderr, "error: %s\n", e.what());
        return 1;
    }
}
