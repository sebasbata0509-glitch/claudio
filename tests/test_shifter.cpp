// PSOLA shifter / engine tests: latency alignment, pitch accuracy of the
// shifted output, formant movement independent of pitch, unvoiced
// pass-through, block-size independence and robustness.

#include "dsp/Engine.h"
#include "support/Check.h"
#include "support/Spectrum.h"
#include "support/TestSignals.h"

using namespace nsw;
using namespace nsw::test;

namespace
{
using Channels = std::vector<std::vector<float>>;

Channels render (const std::vector<float>& input, double fs, const Params& p, int numOut = 1, int block = 256,
                 const std::vector<MidiEvent>& midi = {})
{
    Engine e;
    e.setParams (p);
    e.prepare (fs, block, 1, numOut);
    Channels out ((size_t) numOut, std::vector<float> (input.size()));
    std::vector<MidiEvent> blockEvents;
    for (size_t pos = 0; pos < input.size(); pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, input.size() - pos);
        blockEvents.clear();
        for (auto ev : midi)
            if (ev.sampleOffset >= (int) pos && ev.sampleOffset < (int) pos + n)
            {
                ev.sampleOffset -= (int) pos;
                blockEvents.push_back (ev);
            }
        const float* in[1] = { input.data() + pos };
        float* o[2] = { out[0].data() + pos, numOut > 1 ? out[1].data() + pos : nullptr };
        e.process (in, o, n, blockEvents.data(), (int) blockEvents.size());
    }
    return out;
}

Params neutral()
{
    Params p;
    p.mix = 1.0f;
    return p;
}

struct PitchScore
{
    float median = 0, p95 = 0, voiced = 0, meanConfidence = 0;
    int frames = 0;
};

/** Detect the pitch of `out` and compare with truth(t - latency) * 2^(semis/12). */
PitchScore scorePitch (const std::vector<float>& out, const Signal& truth, int latency, float semis,
                       DetectionRange range)
{
    PitchDetector det;
    det.prepare (truth.sampleRate);
    det.setRange (range);
    std::vector<float> errs;
    int frames = 0, voiced = 0;
    double conf = 0;
    const int guard = det.getMaxPeriodSamples() * 2;
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (! det.process (out[i]))
            continue;
        const auto& e = det.latest();
        const int64_t tin = e.time - latency;
        if (tin - guard < 0 || tin + guard >= (int64_t) truth.f0.size())
            continue;
        bool allVoiced = true;
        for (int64_t k = tin - guard; k <= tin + guard; k += 16)
            allVoiced &= truth.f0[(size_t) k] > 0.0f;
        if (! allVoiced)
            continue;
        ++frames;
        conf += e.confidence;
        if (! e.voiced)
            continue;
        ++voiced;
        errs.push_back (std::abs (centsError (e.frequency, truth.f0[(size_t) tin] * semitonesToRatio (semis))));
    }
    PitchScore s;
    s.frames = frames;
    s.median = percentile (errs, 0.5f);
    s.p95 = percentile (errs, 0.95f);
    s.voiced = frames ? (float) voiced / (float) frames : 0.0f;
    s.meanConfidence = frames ? (float) (conf / frames) : 0.0f;
    return s;
}

double voicedCentroid (const std::vector<float>& x, const Signal& truth, int latency)
{
    // Average spectrum over the longest voiced stretch (shifted by latency).
    std::vector<double> total;
    size_t i = 0;
    while (i < truth.f0.size())
    {
        if (truth.f0[i] <= 0.0f)
        {
            ++i;
            continue;
        }
        size_t j = i;
        while (j < truth.f0.size() && truth.f0[j] > 0.0f)
            ++j;
        const size_t from = i + (size_t) latency + 2048, to = std::min (j + (size_t) latency, x.size());
        if (to > from + 4096)
        {
            const auto p = ltas (x, from, to);
            if (total.empty())
                total.assign (p.size(), 0.0);
            for (size_t k = 0; k < p.size(); ++k)
                total[k] += p[k];
        }
        i = j;
    }
    return total.empty() ? 0.0 : centroid (total, truth.sampleRate, 700.0, 6000.0);
}

void testLatencyAndDryPath (double fs)
{
    const auto sig = noise (fs, 1.0, 0.3f, 99);
    auto p = neutral();
    Engine probe;
    probe.prepare (fs, 256, 1, 1);
    const int latency = probe.getLatencySamples();

    // Dry only: exact delayed copy.
    p.mix = 0.0f;
    const auto dry = render (sig.samples, fs, p);
    double maxErr = 0;
    for (size_t i = (size_t) latency; i < sig.samples.size(); ++i)
        maxErr = std::max (maxErr, (double) std::abs (dry[0][i] - sig.samples[i - (size_t) latency]));
    check (maxErr < 1.0e-6, fmt ("dry path @ %.0f is an exact %d-sample delay (%.2f ms), max err %.2g", fs, latency,
                                 1000.0 * latency / fs, maxErr));

    // Wet, no shift, unvoiced input: granular identity aligned with the dry path.
    p.mix = 1.0f;
    const auto wet = render (sig.samples, fs, p);
    double num = 0, den = 0;
    for (size_t i = (size_t) latency + 4096; i < sig.samples.size(); ++i)
    {
        const double d = wet[0][i] - sig.samples[i - (size_t) latency];
        num += d * d;
        den += (double) sig.samples[i - (size_t) latency] * sig.samples[i - (size_t) latency];
    }
    const double snr = 10.0 * std::log10 (den / std::max (num, 1e-20));
    check (snr > 40.0, fmt ("wet (0 st, unvoiced) @ %.0f matches the latency-compensated input: SNR %.1f dB", fs, snr));

    // 50% blend of identical-delay paths must not comb filter.
    p.mix = 0.5f;
    const auto blend = render (sig.samples, fs, p);
    double eIn = 0, eOut = 0;
    for (size_t i = (size_t) latency + 4096; i < sig.samples.size(); ++i)
    {
        eIn += (double) sig.samples[i - (size_t) latency] * sig.samples[i - (size_t) latency];
        eOut += (double) blend[0][i] * blend[0][i];
    }
    check (std::abs (10.0 * std::log10 (eOut / eIn)) < 0.2,
           fmt ("50%% dry/wet blend @ %.0f keeps level (%.3f dB): no comb filtering", fs, 10.0 * std::log10 (eOut / eIn)));
}

void testPitchShift (double fs)
{
    const auto phrase = vocalPhrase (fs);
    Engine probe;
    probe.prepare (fs, 256, 1, 1);
    const int latency = probe.getLatencySamples();

    for (float semis : { 12.0f, 7.0f, 3.0f, -5.0f, -12.0f })
    {
        auto p = neutral();
        p.pitch = semis;
        const auto out = render (phrase.samples, fs, p);
        const auto range = semis < 0 ? DetectionRange::Low : DetectionRange::High;
        const auto s = scorePitch (out[0], phrase, latency, semis, range);
        check (s.median < 10.0f && s.p95 < 30.0f && s.voiced > 0.9f && s.meanConfidence > 0.85f,
               fmt ("%+.0f st @ %.0f: output pitch median err %.2f c, p95 %.2f c, voiced %.1f%%, periodicity %.3f", semis,
                    fs, s.median, s.p95, 100.0f * s.voiced, s.meanConfidence));
    }
}

void testFormants (double fs)
{
    // Steady low vowels (dense harmonics) so the spectral envelope is well sampled.
    Signal sig;
    sig.sampleRate = fs;
    VowelSynth v (fs);
    v.setFormants (VowelSynth::vowelA());
    v.voiced (sig, 130.0, 130.0, 1.6, 0.4f, 0.0);
    Engine probe;
    probe.prepare (fs, 256, 1, 1);
    const auto latency = (size_t) probe.getLatencySamples();
    const size_t from = (size_t) (0.3 * fs), to = (size_t) (1.5 * fs);
    const auto envIn = logEnvelope (ltas (sig.samples, from, to, 8192), fs, 300.0, 120.0);

    auto p = neutral();
    p.pitch = 12.0f;
    for (float formant : { 0.0f, 4.0f, 12.0f, -6.0f })
    {
        p.formant = formant;
        const auto out = render (sig.samples, fs, p);
        const auto envOut = logEnvelope (ltas (out[0], from + latency, to + latency, 8192), fs, 300.0, 120.0);
        const double a = envelopeScale (envIn, envOut, fs, 400.0, 4000.0);
        const double expected = semitonesToRatio (formant);
        check (std::abs (std::log2 (a / expected)) < 0.15,
               fmt ("+12 st, formant %+.0f @ %.0f: spectral envelope scaled x%.3f (expected x%.3f)", formant, fs, a,
                    expected));
    }

    // Pitch must not depend on formant.
    const auto phrase = vocalPhrase (fs);
    p.formant = 4.0f;
    const auto out = render (phrase.samples, fs, p);
    const auto s = scorePitch (out[0], phrase, (int) latency, 12.0f, DetectionRange::High);
    check (s.median < 10.0f && s.voiced > 0.9f,
           fmt ("+12 st with formant +4 @ %.0f keeps pitch: median err %.2f c, voiced %.1f%%", fs, s.median,
                100.0f * s.voiced));
}

void testSteadyNotes (double fs)
{
    Signal sig;
    sig.sampleRate = fs;
    VowelSynth v (fs);
    v.setFormants (VowelSynth::vowelA());
    for (double hz : { 150.0, 220.0, 310.0 })
        v.voiced (sig, hz, hz, 0.8, 0.4f, 0.0);
    Engine probe;
    probe.prepare (fs, 256, 1, 1);
    for (float semis : { 12.0f, 7.0f, -12.0f })
    {
        auto p = neutral();
        p.pitch = semis;
        const auto out = render (sig.samples, fs, p);
        const auto s = scorePitch (out[0], sig, probe.getLatencySamples(), semis,
                                   semis < 0 ? DetectionRange::Low : DetectionRange::High);
        check (s.median < 3.0f && s.p95 < 12.0f && s.voiced > 0.95f,
               fmt ("steady notes %+.0f st @ %.0f: median err %.2f c, p95 %.2f c, voiced %.1f%%", semis, fs, s.median,
                    s.p95, 100.0f * s.voiced));
    }
}

void testUnvoicedPassThrough (double fs)
{
    // Breath + "s" only; with formant 0 the fallback is an identity even while pitch is +12.
    Signal s;
    s.sampleRate = fs;
    VowelSynth v (fs);
    v.unvoiced (s, 0.6, 0.2f, false);
    v.unvoiced (s, 0.4, 0.3f, true);
    auto p = neutral();
    p.pitch = 12.0f;
    const auto out = render (s.samples, fs, p);
    Engine probe;
    probe.prepare (fs, 256, 1, 1);
    const auto latency = (size_t) probe.getLatencySamples();
    double num = 0, den = 0;
    for (size_t i = latency + 4096; i < s.samples.size(); ++i)
    {
        const double d = out[0][i] - s.samples[i - latency];
        num += d * d;
        den += (double) s.samples[i - latency] * s.samples[i - latency];
    }
    const double snr = 10.0 * std::log10 (den / std::max (num, 1e-20));
    check (snr > 30.0, fmt ("breaths/fricatives at +12 st @ %.0f pass through cleanly: SNR %.1f dB", fs, snr));
}

void testBlockSizeIndependence (double fs)
{
    const auto phrase = vocalPhrase (fs);
    auto p = neutral();
    p.pitch = 12.0f;
    p.formant = 4.0f;
    p.smear = 0.3f;
    p.stereo = 0.5f;
    p.detuneCents = 10.0f;
    const auto a = render (phrase.samples, fs, p, 2, 32);
    const auto b = render (phrase.samples, fs, p, 2, 2048);
    const auto c = render (phrase.samples, fs, p, 2, 237);
    bool same = a == b && a == c;
    check (same, fmt ("output @ %.0f is identical for block sizes 32 / 237 / 2048", fs));
}

void testRobustness (double fs)
{
    const auto phrase = vocalPhrase (fs);
    Random r (4242);
    Engine e;
    Params p;
    e.prepare (fs, 512, 1, 2);
    std::vector<float> l (512), rr (512);
    bool finite = true;
    float peak = 0;
    for (int rep = 0; rep < 3; ++rep)
        for (size_t pos = 0; pos + 512 <= phrase.samples.size(); pos += 512)
        {
            p.pitch = r.bipolar() * 24.0f;
            p.octave = std::round (r.bipolar() * 2.0f);
            p.formant = r.bipolar() * 12.0f;
            p.harmonics = r.bipolar();
            p.alternator = r.uniform();
            p.fm = r.uniform();
            p.fmRatio = 0.25f + r.uniform() * 7.75f;
            p.smear = r.uniform();
            p.stereo = r.uniform();
            p.detuneCents = r.uniform() * 50.0f;
            p.glideMs = r.uniform() * 500.0f;
            p.mix = r.uniform();
            p.range = r.uniform() < 0.1f ? DetectionRange::Low : DetectionRange::High;
            p.midiMode = (MidiMode) (r.next() % 3);
            e.setParams (p);
            std::vector<MidiEvent> ev;
            if (r.uniform() < 0.3f)
                ev.push_back ({ (int) (r.next() % 512), r.uniform() < 0.6f ? MidiEvent::Type::NoteOn : MidiEvent::Type::NoteOff,
                                48 + (int) (r.next() % 36), r.uniform() });
            const float* in[1] = { phrase.samples.data() + pos };
            float* out[2] = { l.data(), rr.data() };
            e.process (in, out, 512, ev.data(), (int) ev.size());
            for (int i = 0; i < 512; ++i)
            {
                finite &= std::isfinite (l[(size_t) i]) && std::isfinite (rr[(size_t) i]);
                peak = std::max ({ peak, std::abs (l[(size_t) i]), std::abs (rr[(size_t) i]) });
            }
        }
    check (finite && peak < 8.0f, fmt ("random automation @ %.0f: finite output, peak %.2f", fs, peak));
}
} // namespace

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        std::printf ("-- %.0f Hz\n", fs);
        testLatencyAndDryPath (fs);
        testPitchShift (fs);
        testSteadyNotes (fs);
        testFormants (fs);
        testUnvoicedPassThrough (fs);
        testBlockSizeIndependence (fs);
        testRobustness (fs);
    }
    return finish ("test_shifter");
}
