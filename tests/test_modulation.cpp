// Macro, step sequencer (free-running and host-synced) and ADSR (MIDI and input-level triggered).

#include "dsp/Engine.h"
#include "support/Check.h"
#include "support/TestSignals.h"

using namespace nsw;
using namespace nsw::test;

namespace
{
constexpr double fs = 48000.0;
constexpr int block = 128;

struct Run
{
    std::vector<float> out;
    std::vector<float> envelope; // per block
    std::vector<int> step;       // per block
    int latency = 0;
};

Run render (const std::vector<float>& in, const Params& p, const std::vector<MidiEvent>& midi = {},
            const Transport* transport = nullptr)
{
    Engine e;
    e.setParams (p);
    e.prepare (fs, block, 1, 1);
    Run r;
    r.out.resize (in.size());
    r.latency = e.getLatencySamples();
    double ppq = transport ? transport->ppqAtBlockStart : 0.0;
    for (size_t pos = 0; pos < in.size(); pos += block)
    {
        const int n = (int) std::min ((size_t) block, in.size() - pos);
        std::vector<MidiEvent> ev;
        for (auto m : midi)
            if (m.sampleOffset >= (int) pos && m.sampleOffset < (int) pos + n)
            {
                m.sampleOffset -= (int) pos;
                ev.push_back (m);
            }
        if (transport)
        {
            auto t = *transport;
            t.ppqAtBlockStart = ppq;
            e.setTransport (t);
            ppq += n / fs * t.bpm / 60.0;
        }
        const float* ip[1] = { in.data() + pos };
        float* op[1] = { r.out.data() + pos };
        e.process (ip, op, n, ev.data(), (int) ev.size());
        r.envelope.push_back (e.display().envelope.load());
        r.step.push_back (e.display().seqStep.load());
    }
    return r;
}

std::vector<float> vowel (double hz, double seconds, double silenceBefore = 0.0)
{
    Signal s;
    s.sampleRate = fs;
    VowelSynth v (fs);
    v.setFormants (VowelSynth::vowelA());
    v.silence (s, silenceBefore);
    v.voiced (s, hz, hz, seconds, 0.4f, 0.0);
    return s.samples;
}

float medianPitch (const Run& r, double from, double to)
{
    PitchDetector d;
    d.prepare (fs);
    std::vector<float> v;
    for (size_t i = 0; i < r.out.size(); ++i)
        if (d.process (r.out[i]) && d.latest().voiced)
        {
            const double t = (double) (d.latest().time - r.latency) / fs;
            if (t >= from && t <= to)
                v.push_back (d.latest().frequency);
        }
    return percentile (v, 0.5f);
}

bool near (float hz, float target, float cents = 10.0f) { return hz > 0 && std::abs (centsError (hz, target)) < cents; }

void testMacro()
{
    const auto in = vowel (200.0, 1.5);
    Params p;
    p.mix = 1.0f;
    p.mod.macro = 1.0f;
    p.mod.macroDepth[(size_t) ModDest::Pitch - 1] = 0.5f; // half of the 24 st span
    const float full = medianPitch (render (in, p), 0.4, 1.3);
    p.mod.macro = 0.5f;
    const float half = medianPitch (render (in, p), 0.4, 1.3);
    check (near (full, 400.0f) && near (half, 200.0f * semitonesToRatio (6.0f)),
           fmt ("macro -> pitch (depth 50%%): macro 100%% gives %.1f Hz (want 400), 50%% gives %.1f Hz (want %.1f)", full,
                half, 200.0f * semitonesToRatio (6.0f)));
}

void testSequencerFree()
{
    const auto in = vowel (200.0, 2.2);
    Params p;
    p.mix = 1.0f;
    p.mod.seqOn = true;
    p.mod.seqRate = SeqRate::Quarter; // 0.5 s per step at 120 bpm
    p.mod.seqLength = 2;
    p.mod.seqDepth = 12.0f;
    p.mod.seqValue = {};
    p.mod.seqValue[1] = 1.0f;
    const auto r = render (in, p);
    const float s0 = medianPitch (r, 0.15, 0.42), s1 = medianPitch (r, 0.62, 0.92), s2 = medianPitch (r, 1.12, 1.42);
    check (near (s0, 200.0f) && near (s1, 400.0f) && near (s2, 200.0f),
           fmt ("sequencer 1/4 @ 120 bpm (free-run): steps read %.1f / %.1f / %.1f Hz (want 200 / 400 / 200)", s0, s1, s2));

    p.mod.seqGlide[1] = 1.0f; // glide across the whole second step
    const auto g = render (in, p);
    const float early = medianPitch (g, 0.55, 0.65), late = medianPitch (g, 0.85, 0.95);
    check (early < late - 40.0f && late > 330.0f,
           fmt ("sequencer step glide: pitch rises through the step (%.1f Hz early -> %.1f Hz late)", early, late));
}

void testSequencerSync()
{
    const auto in = vowel (200.0, 0.5);
    Params p;
    p.mod.seqOn = true;
    p.mod.seqRate = SeqRate::Sixteenth;
    Transport t;
    t.hasTempo = t.isPlaying = t.hasPosition = true;
    t.bpm = 90.0;
    t.ppqAtBlockStart = 2.0; // beat 3 -> sixteenth index 8
    const auto r = render (in, p, {}, &t);
    check (r.step.front() == 8, fmt ("sequencer follows host position: ppq 2.0 at 1/16 -> step %d (want 8)", r.step.front()));
    // 90 bpm, 1/16 = 1/6 s; after 1/3 s (two steps) we should be on step 10.
    const size_t idx = (size_t) (0.34 * fs / block);
    check (r.step[idx] == 10, fmt ("sequencer runs at host tempo: step %d after 0.34 s at 90 bpm (want 10)", r.step[idx]));
}

void testEnvelopeMidi()
{
    const auto in = vowel (200.0, 2.5);
    Params p;
    p.mix = 1.0f;
    p.mod.envDest = ModDest::Pitch;
    p.mod.envDepth = 0.5f; // +12 st at full level
    p.mod.attackMs = 5.0f;
    p.mod.decayMs = 50.0f;
    p.mod.sustain = 1.0f;
    p.mod.releaseMs = 60.0f;
    const auto r = render (in, p,
                           { { (int) (0.5 * fs), MidiEvent::Type::NoteOn, 60, 1.0f },
                             { (int) (1.5 * fs), MidiEvent::Type::NoteOff, 60, 0.0f } });
    const float before = medianPitch (r, 0.15, 0.45), held = medianPitch (r, 0.7, 1.4), after = medianPitch (r, 1.8, 2.3);
    check (near (before, 200.0f) && near (held, 400.0f) && near (after, 200.0f),
           fmt ("ADSR (MIDI) -> pitch +12: %.1f Hz before, %.1f Hz while held, %.1f Hz after release", before, held, after));
}

void testEnvelopeLevel()
{
    const auto in = vowel (200.0, 1.0, 0.6);
    Params p;
    p.mod.envDest = ModDest::Mix;
    p.mod.envTrigger = EnvTrigger::InputLevel;
    p.mod.envThresholdDb = -30.0f;
    p.mod.attackMs = 10.0f;
    const auto r = render (in, p);
    const float quiet = r.envelope[(size_t) (0.5 * fs / block)];
    const float loud = r.envelope[(size_t) (1.2 * fs / block)];
    check (quiet < 0.01f && loud > 0.5f,
           fmt ("ADSR (input level) opens with the voice: %.3f during silence, %.3f while singing", quiet, loud));
}
} // namespace

int main()
{
    testMacro();
    testSequencerFree();
    testSequencerSync();
    testEnvelopeMidi();
    testEnvelopeLevel();
    return finish ("test_modulation");
}
