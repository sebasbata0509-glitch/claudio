#include "support/Signals.h"

#include "engine/EditModel.h"
#include "engine/Renderer.h"
#include "model/Edits.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace tone;
using namespace tone::test;

namespace
{
struct Fixture
{
    std::shared_ptr<SourceAudio> source = std::make_shared<SourceAudio>();
    std::shared_ptr<AnalysisResult> analysis;
    Project project;

    explicit Fixture (std::vector<float> x, double sr = 44100.0)
    {
        source->samples = std::move (x);
        source->sampleRate = sr;
        analysis = std::make_shared<AnalysisResult> (analyse (source->samples.data(), source->samples.size(), sr));
        project = projectFromAnalysis (*analysis);
    }

    std::vector<float> render (SynthEngine e = SynthEngine::Psola, RenderStats* stats = nullptr)
    {
        if (renderer == nullptr)
            renderer = std::make_unique<Renderer> (source, analysis);
        renderer->setEngine (e);
        std::vector<float> out;
        renderer->render (project, out, {}, stats);
        return out;
    }

    NoteIdSet allIds() const
    {
        NoteIdSet s;
        for (const auto& n : project.notes)
            s.insert (n.id);
        return s;
    }

    std::unique_ptr<Renderer> renderer;
};

/** Vowel phrase: two sung notes, a noise "consonant", a third note, plus silences. */
std::vector<float> phrase (double sr)
{
    std::vector<float> x ((size_t) (0.1 * sr), 0.0f);
    auto a = vowel (noteSequence ({ 57, 60 }, 0.4), 0.8, sr);
    applyFades (a, sr);
    append (x, a);
    append (x, noise (0.12, sr, 0.04f, 7));
    auto b = vowel ([] (double) { return (double) midiToHz (64); }, 0.5, sr);
    applyFades (b, sr);
    append (x, b);
    append (x, std::vector<float> ((size_t) (0.1 * sr), 0.0f));
    return x;
}

double errorDb (const std::vector<float>& ref, const std::vector<float>& out, size_t a, size_t b)
{
    std::vector<float> diff;
    for (size_t i = a; i < b; ++i)
        diff.push_back (out[i] - ref[i]);
    return rmsDb (diff.data(), diff.size()) - rmsDb (ref.data() + a, b - a);
}
} // namespace

TEST_CASE ("Unedited render reproduces the input", "[render]")
{
    const double sr = 44100.0;
    Fixture f (phrase (sr), sr);
    REQUIRE (f.project.notes.size() == 3);
    for (auto engine : { SynthEngine::Psola, SynthEngine::PhaseVocoder })
    {
        const auto out = f.render (engine);
        REQUIRE (out.size() >= f.source->samples.size());
        const double err = errorDb (f.source->samples, out, 0, f.source->samples.size());
        INFO ("engine " << (int) engine << " reconstruction error " << err << " dB");
        CHECK (err < -30.0);
    }
}

TEST_CASE ("PSOLA pitch shift lands on the target pitch", "[render]")
{
    const double sr = 44100.0;
    const float shift = GENERATE (-12.0f, -5.0f, -1.0f, 0.5f, 2.0f, 7.0f, 12.0f);
    auto x = vowel ([] (double) { return 220.0; }, 1.0, sr);
    applyFades (x, sr);
    Fixture f (x, sr);
    REQUIRE (f.project.notes.size() == 1);
    edits::transpose (f.project, f.allIds(), shift);
    const auto out = f.render();
    const float measured = measuredMidi (out, sr, 0.2, 0.8);
    const float target = f.project.notes[0].targetMidi();
    INFO ("shift " << shift << ": target " << target << " measured " << measured);
    CHECK (std::abs (measured - target) * 100.0f < 5.0f);
}

TEST_CASE ("Phase-vocoder engine shift lands on the target pitch", "[render]")
{
    const double sr = 44100.0;
    const float shift = GENERATE (-7.0f, 3.0f, 12.0f);
    auto x = vowel ([] (double) { return 196.0; }, 1.0, sr);
    applyFades (x, sr);
    Fixture f (x, sr);
    edits::transpose (f.project, f.allIds(), shift);
    const auto out = f.render (SynthEngine::PhaseVocoder);
    const float measured = measuredMidi (out, sr, 0.2, 0.8);
    INFO ("shift " << shift << " measured " << measured << " target " << f.project.notes[0].targetMidi());
    CHECK (std::abs (measured - f.project.notes[0].targetMidi()) * 100.0f < 5.0f);
}

TEST_CASE ("Formants are preserved when shifting pitch", "[render]")
{
    const double sr = 44100.0, f0 = 180.0;
    const float shift = GENERATE (-7.0f, 5.0f, 9.0f);
    const auto engine = GENERATE (SynthEngine::Psola, SynthEngine::PhaseVocoder);
    auto x = vowel ([f0] (double) { return f0; }, 1.2, sr);
    applyFades (x, sr);
    Fixture f (x, sr);
    edits::transpose (f.project, f.allIds(), shift);
    const auto out = f.render (engine);

    const double r = semitonesToRatio (shift), f0Out = f0 * r;
    // Formants kept: harmonics follow the original envelope.
    const double kept = envelopeErrorDb (out, sr, f0Out, 0.2, 1.0, vowelEnvelope);
    // What a plain resample ("chipmunk") would give: the envelope moves with the pitch.
    const double moved = envelopeErrorDb (out, sr, f0Out, 0.2, 1.0, [r] (double fr) { return vowelEnvelope (fr / r); });
    INFO ("engine " << (int) engine << " shift " << shift << ": error vs original envelope " << kept
                    << " dB, vs moved envelope " << moved << " dB");
    CHECK (kept < 3.0);
    CHECK (kept < 0.5 * moved);
}

TEST_CASE ("Formant shift moves the envelope on purpose", "[render]")
{
    const double sr = 44100.0, f0 = 180.0;
    auto x = vowel ([f0] (double) { return f0; }, 1.2, sr);
    applyFades (x, sr);
    Fixture f (x, sr);
    edits::setFormant (f.project, f.allIds(), 4.0f);
    const auto out = f.render();
    const double fr = semitonesToRatio (4.0);
    const double shifted = envelopeErrorDb (out, sr, f0, 0.2, 1.0, [fr] (double q) { return vowelEnvelope (q / fr); });
    const double original = envelopeErrorDb (out, sr, f0, 0.2, 1.0, vowelEnvelope);
    INFO ("error vs shifted envelope " << shifted << " dB, vs original " << original << " dB");
    CHECK (shifted < original);
    CHECK (std::abs (measuredMidi (out, sr, 0.2, 1.0) - hzToMidi ((float) f0)) * 100.0f < 5.0f);
}

TEST_CASE ("Unvoiced parts are preserved untouched", "[render]")
{
    const double sr = 44100.0;
    Fixture f (phrase (sr), sr);
    edits::transpose (f.project, f.allIds(), 4.0f);
    const auto out = f.render();
    // The noise burst sits between 0.9 s and 1.02 s in the source; keep away from fades.
    const auto a = (size_t) (0.93 * sr), b = (size_t) (0.99 * sr);
    const double err = errorDb (f.source->samples, out, a, b);
    INFO ("noise error " << err << " dB");
    CHECK (err < -30.0);
}

TEST_CASE ("Time edits move and stretch notes", "[render]")
{
    const double sr = 44100.0;
    Fixture f (phrase (sr), sr);
    REQUIRE (f.project.notes.size() == 3);
    const auto lastId = f.project.notes[2].id;
    const double origStart = f.project.notes[2].dstStart, origDur = f.project.notes[2].dstDuration();

    edits::resizeEdge (f.project, lastId, edits::Edge::End, origStart + 1.5 * origDur);
    const auto out = f.render();
    CHECK (out.size() >= (size_t) ((origStart + 1.5 * origDur) * sr));

    // The stretched note keeps its pitch and now lasts ~1.5x.
    const float measured = measuredMidi (out, sr, origStart + 0.1, origStart + 1.4 * origDur);
    CHECK (std::abs (measured - 64.0f) * 100.0f < 5.0f);
    const auto track = runPYin (out.data(), out.size(), sr);
    double lastVoiced = 0.0;
    for (size_t i = 0; i < track.frames.size(); ++i)
        if (track.frames[i].voiced)
            lastVoiced = track.timeOfFrame ((int) i);
    INFO ("voiced until " << lastVoiced << " expected ~" << origStart + 1.5 * origDur);
    CHECK (std::abs (lastVoiced - (origStart + 1.5 * origDur)) < 0.04);
}

TEST_CASE ("Modulation and drift controls reshape the pitch curve", "[render]")
{
    const double sr = 44100.0;
    auto x = vowel ([] (double t) { return 220.0 * std::exp2 (0.5 / 12.0 * std::sin (2.0 * kPi * 5.5 * t)); }, 1.5, sr);
    applyFades (x, sr);
    Fixture f (x, sr);
    REQUIRE (f.project.notes.size() == 1);

    auto depth = [sr] (const std::vector<float>& y)
    {
        const auto tr = runPYin (y.data(), y.size(), sr);
        float lo = 1000.0f, hi = -1000.0f;
        for (int i = tr.frameAt (0.3); i <= tr.frameAt (1.2); ++i)
            if (tr.frames[(size_t) i].voiced)
            {
                lo = std::min (lo, tr.frames[(size_t) i].midi);
                hi = std::max (hi, tr.frames[(size_t) i].midi);
            }
        return hi - lo;
    };

    const float original = depth (x);
    edits::setModulation (f.project, f.allIds(), 0.0f);
    const float flattened = depth (f.render());
    edits::setModulation (f.project, f.allIds(), 2.0f);
    const float doubled = depth (f.render());
    INFO ("vibrato depth original " << original << " flattened " << flattened << " doubled " << doubled);
    CHECK (flattened < 0.3f * original);
    CHECK (doubled > 1.6f * original);
}

TEST_CASE ("Render cache only re-renders what changed", "[render]")
{
    const double sr = 44100.0;
    Fixture f (phrase (sr), sr);
    RenderStats s1, s2, s3;
    const auto first = f.render (SynthEngine::Psola, &s1);
    CHECK (s1.unitsRendered == s1.unitsTotal);

    const auto second = f.render (SynthEngine::Psola, &s2);
    CHECK (s2.unitsRendered == 0);
    CHECK (first == second);

    edits::transpose (f.project, { f.project.notes[2].id }, 2.0f);
    f.render (SynthEngine::Psola, &s3);
    INFO ("re-rendered " << s3.unitsRendered << " of " << s3.unitsTotal);
    CHECK (s3.unitsRendered > 0);
    CHECK (s3.unitsRendered < s3.unitsTotal);
}
