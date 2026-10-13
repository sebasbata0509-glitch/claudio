#pragma once

#include "analysis/Analyzer.h"
#include "common/Fft.h"
#include "common/MathUtil.h"

#include <cmath>
#include <functional>
#include <random>
#include <vector>

namespace tone::test
{
using F0Fn = std::function<double (double t)>;

/** Additive harmonic tone with an arbitrary f0 curve and harmonic amplitude function. */
inline std::vector<float> harmonicTone (const F0Fn& f0, double dur, double sr,
                                        const std::function<double (double freq, int h)>& amp, float gain = 0.5f)
{
    const auto n = (size_t) (dur * sr);
    std::vector<float> x (n);
    double phase = 0.0; // fundamental phase in radians
    for (size_t i = 0; i < n; ++i)
    {
        const double f = f0 ((double) i / sr);
        double v = 0.0;
        const int H = (int) (0.45 * sr / f);
        for (int h = 1; h <= H; ++h)
            v += amp (h * f, h) * std::sin (h * phase);
        x[i] = (float) (gain * v);
        phase += 2.0 * kPi * f / sr;
        if (phase > 2.0 * kPi * 1000.0)
            phase = std::fmod (phase, 2.0 * kPi);
    }
    return x;
}

inline std::vector<float> sine (double f, double dur, double sr, float gain = 0.5f)
{
    return harmonicTone ([f] (double) { return f; }, dur, sr, [] (double, int h) { return h == 1 ? 1.0 : 0.0; }, gain);
}

/** Band-limited sawtooth (1/h harmonics). */
inline std::vector<float> saw (const F0Fn& f0, double dur, double sr, float gain = 0.3f)
{
    return harmonicTone (f0, dur, sr, [] (double, int h) { return 1.0 / h; }, gain);
}

/** Formant envelope of an /a/ vowel (F1 700, F2 1220, F3 2600 Hz) with a gentle tilt. */
inline double vowelEnvelope (double f)
{
    const double F[3] = { 700.0, 1220.0, 2600.0 }, B[3] = { 110.0, 120.0, 160.0 }, A[3] = { 1.0, 0.6, 0.25 };
    double e = 0.02;
    for (int i = 0; i < 3; ++i)
        e += A[i] / (1.0 + std::pow ((f - F[i]) / B[i], 2.0));
    return e;
}

inline std::vector<float> vowel (const F0Fn& f0, double dur, double sr, float gain = 0.25f)
{
    return harmonicTone (f0, dur, sr, [] (double freq, int) { return vowelEnvelope (freq); }, gain);
}

inline std::vector<float> noise (double dur, double sr, float gain = 0.05f, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> d (-1.0f, 1.0f);
    std::vector<float> x ((size_t) (dur * sr));
    for (auto& v : x)
        v = gain * d (rng);
    return x;
}

inline void append (std::vector<float>& dst, const std::vector<float>& src)
{
    dst.insert (dst.end(), src.begin(), src.end());
}

inline void applyFades (std::vector<float>& x, double sr, double fadeSeconds = 0.01)
{
    const auto f = (size_t) (fadeSeconds * sr);
    for (size_t i = 0; i < f && i < x.size(); ++i)
    {
        const float g = (float) i / (float) f;
        x[i] *= g;
        x[x.size() - 1 - i] *= g;
    }
}

/** Step-wise f0 curve through MIDI notes with short glides between them. */
inline F0Fn noteSequence (std::vector<float> midis, double noteDur, double glide = 0.02)
{
    return [midis, noteDur, glide] (double t)
    {
        const int i = std::min ((int) (t / noteDur), (int) midis.size() - 1);
        const double local = t - i * noteDur;
        double m = midis[(size_t) i];
        if (i > 0 && local < glide)
            m = midis[(size_t) i - 1] + (midis[(size_t) i] - midis[(size_t) i - 1]) * smoothStep (local / glide);
        return (double) midiToHz ((float) m);
    };
}

/** Median MIDI pitch measured with pYIN over [t0, t1] seconds of a signal. */
inline float measuredMidi (const std::vector<float>& x, double sr, double t0, double t1)
{
    const auto track = runPYin (x.data(), x.size(), sr);
    std::vector<float> v;
    for (int i = track.frameAt (t0); i <= track.frameAt (t1); ++i)
        if (track.frames[(size_t) i].voiced)
            v.push_back (track.frames[(size_t) i].midi);
    return v.empty() ? -1000.0f : median (v);
}

inline double rmsDb (const float* x, size_t n)
{
    double e = 0.0;
    for (size_t i = 0; i < n; ++i)
        e += (double) x[i] * x[i];
    return 10.0 * std::log10 (std::max (e / std::max<size_t> (n, 1), 1.0e-20));
}

/** Average cepstrally-smoothed log spectrum (dB) of a signal segment, frequency resolution sr/N. */
inline std::vector<float> spectralEnvelopeDb (const std::vector<float>& x, double sr, double t0, double t1, int N = 4096)
{
    Fft fft (N);
    std::vector<std::complex<float>> buf ((size_t) N);
    std::vector<double> acc ((size_t) N / 2 + 1, 0.0);
    int frames = 0;
    for (auto s = (size_t) (t0 * sr); s + (size_t) N <= (size_t) (t1 * sr) && s + (size_t) N <= x.size(); s += (size_t) N / 4)
    {
        for (int k = 0; k < N; ++k)
            buf[(size_t) k] = { x[s + (size_t) k] * (float) (0.5 - 0.5 * std::cos (2.0 * kPi * k / N)), 0.0f };
        fft.forward (buf.data());
        for (int k = 0; k <= N / 2; ++k)
            acc[(size_t) k] += std::abs (buf[(size_t) k]);
        ++frames;
    }
    // log magnitude -> lifter -> envelope
    for (int k = 0; k < N; ++k)
    {
        const int b = k <= N / 2 ? k : N - k;
        buf[(size_t) k] = { (float) std::log (acc[(size_t) b] / std::max (frames, 1) + 1.0e-9), 0.0f };
    }
    fft.inverse (buf.data());
    const int lifter = (int) (0.0015 * sr);
    for (int k = 0; k < N; ++k)
    {
        const int q = std::min (k, N - k);
        if (q > lifter)
            buf[(size_t) k] = {};
    }
    fft.forward (buf.data());
    std::vector<float> env ((size_t) N / 2 + 1);
    for (int k = 0; k <= N / 2; ++k)
        env[(size_t) k] = (float) (20.0 / std::log (10.0) * buf[(size_t) k].real());
    return env;
}

/** Amplitude in dB of a sinusoid at frequency f, measured over [t0, t1] with a Hann window. */
inline double partialDb (const std::vector<float>& x, double sr, double f, double t0, double t1)
{
    const auto a = (size_t) (t0 * sr), b = std::min (x.size(), (size_t) (t1 * sr));
    double re = 0.0, im = 0.0, ws = 0.0;
    for (size_t i = a; i < b; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * (double) (i - a) / (double) (b - a));
        re += w * x[i] * std::cos (2.0 * kPi * f * (double) i / sr);
        im += w * x[i] * std::sin (2.0 * kPi * f * (double) i / sr);
        ws += w;
    }
    return 20.0 * std::log10 (2.0 * std::sqrt (re * re + im * im) / ws + 1.0e-12);
}

/**
    Mean absolute dB error between the measured harmonics of a steady tone (fundamental f0) and a
    reference envelope, after removing the average level difference. Harmonics up to maxHz.
*/
inline double envelopeErrorDb (const std::vector<float>& x, double sr, double f0, double t0, double t1,
                               const std::function<double (double)>& envelope, double maxHz = 3000.0)
{
    std::vector<double> diff;
    for (int h = 1; h * f0 <= maxHz; ++h)
        diff.push_back (partialDb (x, sr, h * f0, t0, t1) - 20.0 * std::log10 (envelope (h * f0)));
    double mean = 0.0;
    for (double d : diff)
        mean += d;
    mean /= (double) diff.size();
    double err = 0.0;
    for (double d : diff)
        err += std::abs (d - mean);
    return err / (double) diff.size();
}

/** Frequency of the envelope maximum within [lo, hi] Hz. */
inline double envelopePeakHz (const std::vector<float>& env, double sr, double lo, double hi)
{
    const int N = (int) (env.size() - 1) * 2;
    int best = (int) (lo * N / sr);
    for (int k = (int) (lo * N / sr); k <= (int) (hi * N / sr); ++k)
        if (env[(size_t) k] > env[(size_t) best])
            best = k;
    return best * sr / N;
}
} // namespace tone::test
