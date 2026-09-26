#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nsw
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline float semitonesToRatio (float semis) noexcept { return std::exp2 (semis * (1.0f / 12.0f)); }
inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float frequencyToMidi (float hz) noexcept { return 69.0f + 12.0f * std::log2 (std::max (hz, 1.0e-3f) / 440.0f); }
inline float midiToFrequency (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

inline int nextPowerOfTwo (int n) noexcept
{
    int p = 1;
    while (p < n)
        p <<= 1;
    return p;
}

/** Small, allocation-free xorshift RNG (audio-thread safe). */
class Random
{
public:
    explicit Random (uint32_t seed = 0x9E3779B9u) noexcept : state (seed ? seed : 1u) {}
    void seed (uint32_t s) noexcept { state = s ? s : 1u; }
    uint32_t next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    /** Uniform in [0, 1). */
    float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    /** Uniform in [-1, 1). */
    float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }

private:
    uint32_t state;
};

/** Transposed direct form II biquad. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() noexcept { z1 = z2 = 0.0f; }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setLowpass (double sampleRate, double freq, double q) noexcept
    {
        const double w = kTwoPi * std::clamp (freq, 1.0, 0.49 * sampleRate) / sampleRate;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) ((1.0 - cw) * 0.5 / a0);
        b1 = (float) ((1.0 - cw) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * cw / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    void setHighpass (double sampleRate, double freq, double q) noexcept
    {
        const double w = kTwoPi * std::clamp (freq, 1.0, 0.49 * sampleRate) / sampleRate;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) ((1.0 + cw) * 0.5 / a0);
        b1 = (float) (-(1.0 + cw) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * cw / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }
};

/**
    Two cascaded one-pole low-passes (critically damped): the smoothed value
    and its slope are both continuous, so parameter jumps never click.
    `timeMs` is roughly the time to cover ~90% of a step.
*/
class Smoother
{
public:
    void prepare (double sampleRate, double timeMs) noexcept
    {
        fs = sampleRate;
        setTime (timeMs);
    }
    void setTime (double timeMs) noexcept
    {
        const double tau = 0.001 * timeMs / 3.9; // two poles reach ~90% at ~3.9 tau
        coeff = timeMs <= 0.0 ? 0.0f : (float) std::exp (-1.0 / (tau * fs));
    }
    void reset (float v) noexcept { current = stage = target = v; }
    void setTarget (float v) noexcept { target = v; }
    float next() noexcept
    {
        stage = target + coeff * (stage - target);
        current = stage + coeff * (current - stage);
        return current;
    }
    /** Advance by n samples at once (for control-rate updates). */
    float advance (int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            next();
        return current;
    }
    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept { return target; }

private:
    double fs = 48000.0;
    float coeff = 0.0f, current = 0.0f, stage = 0.0f, target = 0.0f;
};

/** 4-point, 3rd-order Hermite interpolation. frac in [0, 1) between y0 and y1. */
inline float hermite (float ym1, float y0, float y1, float y2, float frac) noexcept
{
    const float c0 = y0;
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

/** Power-of-two circular buffer indexed by absolute sample time. */
class RingBuffer
{
public:
    void allocate (int minSize)
    {
        const int size = nextPowerOfTwo (std::max (minSize, 16));
        data.assign ((size_t) size, 0.0f);
        mask = size - 1;
    }
    void clear() noexcept { std::fill (data.begin(), data.end(), 0.0f); }
    int size() const noexcept { return mask + 1; }

    float& at (int64_t t) noexcept { return data[(size_t) (t & mask)]; }
    float at (int64_t t) const noexcept { return data[(size_t) (t & mask)]; }

    /** Read with Hermite interpolation at fractional absolute time. */
    float read (double t) const noexcept
    {
        const double fl = std::floor (t);
        const auto i = (int64_t) fl;
        const auto f = (float) (t - fl);
        return hermite (at (i - 1), at (i), at (i + 1), at (i + 2), f);
    }

private:
    std::vector<float> data;
    int64_t mask = 0;
};

/** Raised-cosine window lookup, w(x) for x in [-1, 1], zero outside. */
class HannTable
{
public:
    static constexpr int kSize = 2048;

    HannTable()
    {
        for (int i = 0; i <= kSize; ++i)
            table[(size_t) i] = (float) (0.5 * (1.0 + std::cos (kPi * (double) i / kSize)));
    }

    /** x = distance from centre in units of half-length (|x| < 1). */
    float operator() (float x) const noexcept
    {
        const float ax = std::abs (x) * (float) kSize;
        if (ax >= (float) kSize)
            return 0.0f;
        const int i = (int) ax;
        const float f = ax - (float) i;
        return table[(size_t) i] + f * (table[(size_t) i + 1] - table[(size_t) i]);
    }

private:
    float table[kSize + 1] {};
};

} // namespace nsw
