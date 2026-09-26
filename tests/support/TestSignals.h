#pragma once

// Deterministic test signals with ground-truth pitch, shared by tests and tools.

#include "dsp/DspUtil.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace nsw::test
{

struct Signal
{
    double sampleRate = 48000.0;
    std::vector<float> samples;
    std::vector<float> f0;     // ground-truth fundamental per sample (0 = unvoiced)
};

inline Signal sine (double fs, double hz, double seconds, float amp = 0.5f)
{
    Signal s;
    s.sampleRate = fs;
    const auto n = (size_t) (seconds * fs);
    s.samples.resize (n);
    s.f0.assign (n, (float) hz);
    for (size_t i = 0; i < n; ++i)
        s.samples[i] = amp * (float) std::sin (kTwoPi * hz * (double) i / fs);
    return s;
}

/** Exponential sine sweep from f1 to f2. */
inline Signal chirp (double fs, double f1, double f2, double seconds, float amp = 0.5f)
{
    Signal s;
    s.sampleRate = fs;
    const auto n = (size_t) (seconds * fs);
    s.samples.resize (n);
    s.f0.resize (n);
    const double k = std::log (f2 / f1) / seconds;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / fs;
        const double phase = kTwoPi * f1 * (std::exp (k * t) - 1.0) / k;
        s.samples[i] = amp * (float) std::sin (phase);
        s.f0[i] = (float) (f1 * std::exp (k * t));
    }
    return s;
}

inline Signal noise (double fs, double seconds, float amp, uint32_t seed = 1234)
{
    Signal s;
    s.sampleRate = fs;
    const auto n = (size_t) (seconds * fs);
    Random r (seed);
    s.samples.resize (n);
    s.f0.assign (n, 0.0f);
    for (auto& x : s.samples)
        x = amp * r.bipolar();
    return s;
}

/**
    Source-filter "sung vowel" model: Rosenberg glottal pulses with vibrato and
    jitter, radiated (differentiated) and shaped by a cascade of formant
    resonators. Segments of breath noise and an "s" fricative are included so
    the unvoiced path gets exercised. This is a stand-in for a real vocal
    recording where the true pitch is known exactly.
*/
class VowelSynth
{
public:
    struct Formants { float f[4]; float bw[4]; };
    static Formants vowelA() { return { { 730, 1090, 2440, 3400 }, { 90, 110, 170, 250 } }; }
    static Formants vowelI() { return { { 270, 2290, 3010, 3700 }, { 60, 100, 170, 250 } }; }
    static Formants vowelU() { return { { 300, 870, 2240, 3300 }, { 70, 100, 170, 250 } }; }

    explicit VowelSynth (double fs) : fs (fs), rng (777) {}

    void setFormants (const Formants& fm)
    {
        for (int k = 0; k < 4; ++k)
        {
            const double r = std::exp (-kPi * fm.bw[k] / fs);
            const double c = 2.0 * r * std::cos (kTwoPi * fm.f[k] / fs);
            res[k].a1 = (float) -c;
            res[k].a2 = (float) (r * r);
            res[k].g = (float) (1.0 - c + r * r);
        }
    }

    /** Render `seconds` of voiced sound gliding f0 from `hzStart` to `hzEnd`. */
    void voiced (Signal& s, double hzStart, double hzEnd, double seconds, float amp = 0.4f,
                 double vibratoCents = 35.0, double vibratoHz = 5.5)
    {
        const auto n = (size_t) (seconds * fs);
        for (size_t i = 0; i < n; ++i)
        {
            const double t = (double) i / (double) n;
            const double base = hzStart * std::pow (hzEnd / hzStart, t);
            vibPhase += kTwoPi * vibratoHz / fs;
            const double hz = base * std::exp2 (vibratoCents / 1200.0 * std::sin (vibPhase));
            // Attack/release envelope over the note.
            const double env = std::min ({ 1.0, t * seconds / 0.03, (1.0 - t) * seconds / 0.03 });

            phase += hz / fs;
            if (phase >= 1.0)
            {
                phase -= 1.0;
                jitter = 1.0 + 0.004 * rng.bipolar();
            }
            const float g = glottal (phase * jitter);
            const float d = (g - lastG);
            lastG = g;
            const float aspiration = 0.02f * rng.bipolar() * g;
            s.samples.push_back (amp * (float) env * formantFilter (d * 8.0f + aspiration));
            s.f0.push_back ((float) hz);
        }
    }

    /** Unvoiced noise; `bright` selects fricative ("s") vs breath colour. */
    void unvoiced (Signal& s, double seconds, float amp, bool bright)
    {
        const auto n = (size_t) (seconds * fs);
        Biquad colour;
        if (bright)
            colour.setHighpass (fs, 4500.0, 0.7);
        else
            colour.setLowpass (fs, 1800.0, 0.5);
        for (size_t i = 0; i < n; ++i)
        {
            const double t = (double) i / (double) n;
            const double env = std::min ({ 1.0, t * seconds / 0.02, (1.0 - t) * seconds / 0.02 });
            s.samples.push_back (amp * (float) env * colour.process (rng.bipolar()));
            s.f0.push_back (0.0f);
        }
        lastG = 0.0f;
    }

    void silence (Signal& s, double seconds)
    {
        const auto n = (size_t) (seconds * fs);
        s.samples.insert (s.samples.end(), n, 0.0f);
        s.f0.insert (s.f0.end(), n, 0.0f);
    }

private:
    static float glottal (double ph)
    {
        constexpr double tp = 0.40, tn = 0.16;
        if (ph < tp)
            return (float) (0.5 * (1.0 - std::cos (kPi * ph / tp)));
        if (ph < tp + tn)
            return (float) std::cos (kPi * (ph - tp) / (2.0 * tn));
        return 0.0f;
    }

    float formantFilter (float x)
    {
        for (auto& r : res)
        {
            const float y = r.g * x - r.a1 * r.y1 - r.a2 * r.y2;
            r.y2 = r.y1;
            r.y1 = y;
            x = y;
        }
        return x * 0.4f; // peaks around -16 dBFS, like a typical vocal take
    }

    struct Resonator { float a1 = 0, a2 = 0, g = 1, y1 = 0, y2 = 0; };

    double fs;
    Random rng;
    Resonator res[4];
    double phase = 0.0, vibPhase = 0.0, jitter = 1.0;
    float lastG = 0.0f;
};

/** A short sung phrase: several notes/vowels with breaths and an "s". */
inline Signal vocalPhrase (double fs)
{
    Signal s;
    s.sampleRate = fs;
    VowelSynth v (fs);
    v.silence (s, 0.15);
    v.unvoiced (s, 0.25, 0.08f, false);            // breath in
    v.setFormants (VowelSynth::vowelA());
    v.voiced (s, 220.0, 220.0, 0.6);               // A3
    v.voiced (s, 220.0, 262.0, 0.15, 0.4f, 0.0);   // scoop up
    v.voiced (s, 262.0, 262.0, 0.5);               // C4
    v.unvoiced (s, 0.18, 0.15f, true);             // "s"
    v.setFormants (VowelSynth::vowelI());
    v.voiced (s, 330.0, 330.0, 0.55);              // E4
    v.setFormants (VowelSynth::vowelU());
    v.voiced (s, 294.0, 196.0, 0.7, 0.4f, 20.0);   // falling phrase end
    v.unvoiced (s, 0.3, 0.06f, false);             // breath out
    v.silence (s, 0.2);
    return s;
}

inline float centsError (float estHz, float trueHz)
{
    return 1200.0f * std::log2 (estHz / trueHz);
}

inline float percentile (std::vector<float> v, float p)
{
    if (v.empty())
        return 0.0f;
    std::sort (v.begin(), v.end());
    const auto idx = (size_t) std::clamp (p * (float) (v.size() - 1), 0.0f, (float) (v.size() - 1));
    return v[idx];
}

} // namespace nsw::test
