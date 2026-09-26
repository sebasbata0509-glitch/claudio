// CPU benchmark: real-time load of the full engine at several sample rates and
// block sizes. Load = processing time / audio duration, on one core.
//
//   nutsweller_bench [seconds]

#include "dsp/Engine.h"
#include "dsp/Presets.h"
#include "support/TestSignals.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace nsw;

namespace
{
struct Scenario
{
    const char* name;
    Params params;
    std::vector<MidiEvent> notes;
};

double measure (const Scenario& sc, double fs, int block, double seconds)
{
    const auto phrase = test::vocalPhrase (fs);
    const auto total = (size_t) (seconds * fs);
    std::vector<float> inL (total), inR (total), outL ((size_t) block), outR ((size_t) block);
    for (size_t i = 0; i < total; ++i)
    {
        inL[i] = phrase.samples[i % phrase.samples.size()];
        inR[i] = inL[i] * 0.9f;
    }

    Engine e;
    e.setParams (sc.params);
    e.prepare (fs, block, 2, 2);
    Transport t;
    t.hasTempo = t.isPlaying = t.hasPosition = true;
    t.bpm = 120.0;

    const auto start = std::chrono::steady_clock::now();
    for (size_t pos = 0; pos + (size_t) block <= total; pos += (size_t) block)
    {
        t.ppqAtBlockStart = (double) pos / fs * 2.0;
        e.setTransport (t);
        const float* in[2] = { inL.data() + pos, inR.data() + pos };
        float* out[2] = { outL.data(), outR.data() };
        const bool first = pos == 0;
        e.process (in, out, block, first ? sc.notes.data() : nullptr, first ? (int) sc.notes.size() : 0);
    }
    const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    return elapsed / seconds;
}
} // namespace

int main (int argc, char** argv)
{
    const double seconds = argc > 1 ? std::atof (argv[1]) : 10.0;

    std::vector<Scenario> scenarios;
    scenarios.push_back ({ "Porter Up (default)", factoryPresets()[0].params, {} });

    Params heavy = factoryPresets()[0].params;
    heavy.harmonics = 0.6f;
    heavy.fm = 0.5f;
    heavy.fmRatio = 1.5f;
    heavy.alternator = 0.5f;
    heavy.smear = 0.8f;
    heavy.stereo = 1.0f;
    heavy.mod.seqOn = true;
    heavy.mod.macro = 0.5f;
    heavy.mod.envDest = ModDest::Formant;
    heavy.mod.envTrigger = EnvTrigger::InputLevel;
    scenarios.push_back ({ "Everything on, MIDI off", heavy, {} });

    Params poly = heavy;
    poly.midiMode = MidiMode::Poly;
    std::vector<MidiEvent> chord;
    for (int n : { 60, 64, 67, 71, 74, 77, 81, 84 })
        chord.push_back ({ 0, MidiEvent::Type::NoteOn, n, 0.8f });
    scenarios.push_back ({ "Everything on, POLY 8 voices", poly, chord });

    Params low = factoryPresets()[0].params;
    low.range = DetectionRange::Low;
    scenarios.push_back ({ "Porter Up, LOW range", low, {} });

    std::printf ("NutSweller CPU load (%% of one core, stereo, %.0f s of audio per cell)\n\n", seconds);
    for (const auto& sc : scenarios)
    {
        std::printf ("%s\n  %-9s", sc.name, "rate");
        const int blocks[] = { 32, 64, 128, 256, 512, 1024, 2048 };
        for (int b : blocks)
            std::printf ("%8d", b);
        std::printf ("\n");
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            std::printf ("  %-9.0f", fs);
            for (int b : blocks)
                std::printf ("%7.2f%%", 100.0 * measure (sc, fs, b, seconds));
            std::printf ("\n");
        }
        std::printf ("\n");
    }
    return 0;
}
