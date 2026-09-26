#pragma once

// Radix-2 FFT and long-term average spectrum helpers for tests.

#include "dsp/DspUtil.h"

#include <complex>
#include <vector>

namespace nsw::test
{

inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -kTwoPi / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

/** Long-term average power spectrum of x[from, to) (Hann, 50% overlap). */
inline std::vector<double> ltas (const std::vector<float>& x, size_t from, size_t to, size_t n = 4096)
{
    std::vector<double> psd (n / 2 + 1, 0.0);
    std::vector<std::complex<double>> buf (n);
    for (size_t start = from; start + n <= to; start += n / 2)
    {
        for (size_t i = 0; i < n; ++i)
            buf[i] = x[start + i] * 0.5 * (1.0 - std::cos (kTwoPi * (double) i / (double) n));
        fft (buf);
        for (size_t k = 0; k <= n / 2; ++k)
            psd[k] += std::norm (buf[k]);
    }
    return psd;
}

/** Power-weighted spectral centroid between lo and hi Hz. */
inline double centroid (const std::vector<double>& psd, double fs, double lo, double hi)
{
    const size_t n = (psd.size() - 1) * 2;
    double num = 0, den = 0;
    for (size_t k = 0; k < psd.size(); ++k)
    {
        const double f = (double) k * fs / (double) n;
        if (f < lo || f > hi)
            continue;
        num += f * psd[k];
        den += psd[k];
    }
    return den > 0 ? num / den : 0.0;
}
} // namespace nsw::test

namespace nsw::test
{
/** Log spectral envelope: moving max over +-maxWidthHz (bridges harmonics), then smoothed. */
inline std::vector<double> logEnvelope (const std::vector<double>& psd, double fs, double maxWidthHz, double smoothHz)
{
    const size_t n = psd.size();
    const double binHz = fs / (2.0 * (double) (n - 1));
    const auto wMax = (long) std::max (1.0, maxWidthHz / binHz), wSm = (long) std::max (1.0, smoothHz / binHz);
    std::vector<double> peak (n), env (n);
    for (long k = 0; k < (long) n; ++k)
    {
        double m = 0;
        for (long j = std::max (0L, k - wMax); j <= std::min ((long) n - 1, k + wMax); ++j)
            m = std::max (m, psd[(size_t) j]);
        peak[(size_t) k] = std::log (m + 1e-30);
    }
    for (long k = 0; k < (long) n; ++k)
    {
        double s = 0;
        int c = 0;
        for (long j = std::max (0L, k - wSm); j <= std::min ((long) n - 1, k + wSm); ++j, ++c)
            s += peak[(size_t) j];
        env[(size_t) k] = s / c;
    }
    return env;
}

/** Frequency-scale factor a that best maps envIn(f / a) onto envOut(f) over [lo, hi] Hz. */
inline double envelopeScale (const std::vector<double>& envIn, const std::vector<double>& envOut, double fs,
                             double lo, double hi)
{
    const size_t n = envIn.size();
    const double binHz = fs / (2.0 * (double) (n - 1));
    double bestA = 1, bestScore = -1e30;
    for (int i = 0; i <= 600; ++i)
    {
        const double a = std::exp (std::log (0.4) + (std::log (2.6) - std::log (0.4)) * i / 600.0);
        std::vector<double> x, y;
        for (double f = lo; f <= hi; f += binHz)
        {
            const double src = f / a / binHz;
            const auto k = (size_t) src;
            if (k + 1 >= n)
                break;
            const double fr = src - (double) k;
            x.push_back (envOut[(size_t) (f / binHz)]);
            y.push_back (envIn[k] + fr * (envIn[k + 1] - envIn[k]));
        }
        if (x.size() < 16)
            continue;
        double mx = 0, my = 0;
        for (size_t k = 0; k < x.size(); ++k)
        {
            mx += x[k];
            my += y[k];
        }
        mx /= (double) x.size();
        my /= (double) y.size();
        double sxy = 0, sxx = 0, syy = 0;
        for (size_t k = 0; k < x.size(); ++k)
        {
            sxy += (x[k] - mx) * (y[k] - my);
            sxx += (x[k] - mx) * (x[k] - mx);
            syy += (y[k] - my) * (y[k] - my);
        }
        const double score = sxy / std::sqrt (sxx * syy + 1e-30);
        if (score > bestScore)
        {
            bestScore = score;
            bestA = a;
        }
    }
    return bestA;
}
} // namespace nsw::test
