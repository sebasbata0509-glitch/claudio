#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

namespace tone
{
inline constexpr double kPi = std::numbers::pi;

inline float hzToMidi (float hz) noexcept     { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }
inline float midiToHz (float midi) noexcept   { return 440.0f * std::exp2 ((midi - 69.0f) / 12.0f); }
inline double semitonesToRatio (double s) noexcept { return std::exp2 (s / 12.0); }
inline float dbToGain (float db) noexcept     { return std::pow (10.0f, db / 20.0f); }
inline float gainToDb (float g) noexcept      { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

/** Median of a copy of the values (empty input returns 0). */
inline float median (std::vector<float> v)
{
    if (v.empty())
        return 0.0f;
    const auto mid = v.begin() + (std::ptrdiff_t) (v.size() / 2);
    std::nth_element (v.begin(), mid, v.end());
    if (v.size() % 2 == 1)
        return *mid;
    const float upper = *mid;
    const float lower = *std::max_element (v.begin(), mid);
    return 0.5f * (lower + upper);
}

/** Raised-cosine fade 0..1 -> 0..1 (smooth at both ends). */
inline double smoothStep (double x) noexcept
{
    x = std::clamp (x, 0.0, 1.0);
    return 0.5 - 0.5 * std::cos (kPi * x);
}

inline int nextPowerOfTwo (int n) noexcept
{
    int p = 1;
    while (p < n)
        p <<= 1;
    return p;
}

/** Catmull-Rom interpolation of x at a fractional position; out-of-range samples read as 0. */
inline float interpolateCubic (const float* x, int64_t n, double pos) noexcept
{
    const auto i = (int64_t) std::floor (pos);
    const auto f = (float) (pos - (double) i);
    auto at = [x, n] (int64_t k) { return (k >= 0 && k < n) ? x[k] : 0.0f; };
    const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

/** FNV-1a style hash combining, used for render-cache keys. */
struct Hasher
{
    uint64_t h = 1469598103934665603ull;

    void addBytes (const void* data, size_t size) noexcept
    {
        auto* p = static_cast<const unsigned char*> (data);
        for (size_t i = 0; i < size; ++i)
        {
            h ^= p[i];
            h *= 1099511628211ull;
        }
    }

    template <typename T>
    Hasher& add (const T& v) noexcept
    {
        addBytes (&v, sizeof (T));
        return *this;
    }
};
} // namespace tone
