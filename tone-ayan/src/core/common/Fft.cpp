#include "Fft.h"

#include "MathUtil.h"

#include <cassert>
#include <stdexcept>
#include <utility>

namespace tone
{
Fft::Fft (int size) : n (size)
{
    if (size < 2 || (size & (size - 1)) != 0)
        throw std::invalid_argument ("FFT size must be a power of two");

    int bits = 0;
    while ((1 << bits) < n)
        ++bits;

    bitReverse.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b))
                r |= 1 << (bits - 1 - b);
        bitReverse[(size_t) i] = r;
    }

    twiddles.resize ((size_t) n / 2);
    for (int k = 0; k < n / 2; ++k)
    {
        const double a = -2.0 * kPi * k / n;
        twiddles[(size_t) k] = { (float) std::cos (a), (float) std::sin (a) };
    }
}

void Fft::transform (std::complex<float>* a, bool inv) const noexcept
{
    for (int i = 0; i < n; ++i)
        if (i < bitReverse[(size_t) i])
            std::swap (a[i], a[bitReverse[(size_t) i]]);

    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len / 2;
        const int step = n / len;
        for (int i = 0; i < n; i += len)
        {
            for (int j = 0; j < half; ++j)
            {
                auto w = twiddles[(size_t) (j * step)];
                if (inv)
                    w = std::conj (w);
                const auto u = a[i + j];
                const auto v = a[i + j + half] * w;
                a[i + j] = u + v;
                a[i + j + half] = u - v;
            }
        }
    }

    if (inv)
    {
        const float scale = 1.0f / (float) n;
        for (int i = 0; i < n; ++i)
            a[i] *= scale;
    }
}
} // namespace tone
