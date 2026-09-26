// Offline renderer: run a WAV (or the built-in synthetic sung phrase) through
// the NutSweller engine exactly as the plugin would, and write the result.
//
//   nutsweller_render --in vocal.wav --out out.wav --pitch 12 --formant 4
//   nutsweller_render --demo --out demo.wav --preset "Porter Up"
//   nutsweller_render --write-demo demo_input.wav
//
// The output is latency-compensated (the reported latency is trimmed) so it
// lines up with the input when both are dropped into a DAW.

#include "dsp/Engine.h"
#include "dsp/Presets.h"
#include "dsp/WavFile.h"
#include "support/TestSignals.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace nsw;

namespace
{
void usage()
{
    std::puts (
        "usage: nutsweller_render (--in FILE | --demo [--rate HZ]) --out FILE [options]\n"
        "       nutsweller_render --write-demo FILE [--rate HZ]\n"
        "options:\n"
        "  --preset NAME       start from a factory preset (Porter Up, Pure Octave, Chipmunk, Giant, Glitch Choir, Init)\n"
        "  --pitch ST          -24..24          --octave N      -2..2\n"
        "  --formant ST        -12..12          --harmonics X   -1..1\n"
        "  --alternator PCT    0..100           --fm AMT        0..100     --ratio R  0.25..8\n"
        "  --glide MS          0..500           --smear PCT     0..100\n"
        "  --stereo PCT        0..100           --detune CENTS  0..50\n"
        "  --mix PCT           0..100           --gain DB       -24..12\n"
        "  --range low|high    --snap on|off    --block N (default 512)\n"
        "  --midi-note N       MONO mode, hold MIDI note N for the whole render\n"
        "  --stereo-out        force a stereo output file (default: stereo if input is stereo or stereo/detune used)");
}
} // namespace

int main (int argc, char** argv)
{
    std::string inPath, outPath, demoOut;
    bool demo = false, forceStereo = false;
    double rate = 48000.0;
    int block = 512, midiNote = -1;
    Params p = factoryPresets()[5].params; // Init

    auto num = [] (const char* s) { return (float) std::atof (s); };

    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        const char* v = i + 1 < argc ? argv[i + 1] : "";
        auto take = [&] { ++i; return v; };
        if (a == "--in") inPath = take();
        else if (a == "--out") outPath = take();
        else if (a == "--demo") demo = true;
        else if (a == "--write-demo") demoOut = take();
        else if (a == "--rate") rate = num (take());
        else if (a == "--block") block = std::max (1, (int) num (take()));
        else if (a == "--preset")
        {
            const std::string name = take();
            bool found = false;
            for (const auto& pr : factoryPresets())
                if (name == pr.name)
                {
                    p = pr.params;
                    found = true;
                }
            if (! found)
            {
                std::fprintf (stderr, "unknown preset '%s'\n", name.c_str());
                return 2;
            }
        }
        else if (a == "--pitch") p.pitch = num (take());
        else if (a == "--octave") p.octave = num (take());
        else if (a == "--formant") p.formant = num (take());
        else if (a == "--harmonics") p.harmonics = num (take());
        else if (a == "--alternator") p.alternator = num (take()) / 100.0f;
        else if (a == "--fm") p.fm = num (take()) / 100.0f;
        else if (a == "--ratio") p.fmRatio = num (take());
        else if (a == "--glide") p.glideMs = num (take());
        else if (a == "--smear") p.smear = num (take()) / 100.0f;
        else if (a == "--stereo") p.stereo = num (take()) / 100.0f;
        else if (a == "--detune") p.detuneCents = num (take());
        else if (a == "--mix") p.mix = num (take()) / 100.0f;
        else if (a == "--gain") p.outputDb = num (take());
        else if (a == "--range") p.range = std::strcmp (take(), "low") == 0 ? DetectionRange::Low : DetectionRange::High;
        else if (a == "--snap") p.snap = std::strcmp (take(), "off") != 0;
        else if (a == "--midi-note") midiNote = (int) num (take());
        else if (a == "--stereo-out") forceStereo = true;
        else
        {
            usage();
            return 2;
        }
    }

    if (! demoOut.empty())
    {
        const auto s = test::vocalPhrase (rate);
        AudioFile f;
        f.sampleRate = rate;
        f.channels = { s.samples };
        return writeWav (demoOut, f) ? 0 : 1;
    }

    if ((inPath.empty() && ! demo) || outPath.empty())
    {
        usage();
        return 2;
    }

    AudioFile input;
    if (demo)
    {
        input.sampleRate = rate;
        input.channels = { test::vocalPhrase (rate).samples };
    }
    else if (! readWav (inPath, input))
    {
        std::fprintf (stderr, "could not read %s\n", inPath.c_str());
        return 1;
    }

    const int numIn = std::min (2, input.numChannels());
    const int numOut = (numIn == 2 || forceStereo || p.stereo > 0.0f || p.detuneCents > 0.0f) ? 2 : 1;

    if (midiNote >= 0)
        p.midiMode = MidiMode::Mono;

    Engine engine;
    engine.setParams (p);
    engine.prepare (input.sampleRate, block, numIn, numOut);
    const int latency = engine.getLatencySamples();

    // Pad the end so the latency trim keeps the full length.
    const int total = input.numSamples() + latency;
    std::vector<std::vector<float>> in ((size_t) numIn, std::vector<float> ((size_t) total, 0.0f));
    for (int c = 0; c < numIn; ++c)
        std::copy (input.channels[(size_t) c].begin(), input.channels[(size_t) c].end(), in[(size_t) c].begin());
    std::vector<std::vector<float>> out ((size_t) numOut, std::vector<float> ((size_t) total, 0.0f));

    MidiEvent noteOn { 0, MidiEvent::Type::NoteOn, midiNote, 1.0f };
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        const float* ip[2] = { in[0].data() + pos, in[(size_t) (numIn - 1)].data() + pos };
        float* op[2] = { out[0].data() + pos, out[(size_t) (numOut - 1)].data() + pos };
        const bool sendNote = midiNote >= 0 && pos == 0;
        engine.process (ip, op, n, sendNote ? &noteOn : nullptr, sendNote ? 1 : 0);
    }

    AudioFile result;
    result.sampleRate = input.sampleRate;
    for (auto& ch : out)
        result.channels.emplace_back (ch.begin() + latency, ch.end());

    if (! writeWav (outPath, result))
    {
        std::fprintf (stderr, "could not write %s\n", outPath.c_str());
        return 1;
    }
    std::printf ("rendered %s: %d ch, %.0f Hz, latency %d samples (%.2f ms, trimmed)\n", outPath.c_str(), numOut,
                 input.sampleRate, latency, 1000.0 * latency / input.sampleRate);
    return 0;
}
