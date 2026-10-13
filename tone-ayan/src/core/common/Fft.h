#pragma once

#include <complex>
#include <vector>

namespace tone
{
/** Iterative radix-2 complex FFT with precomputed twiddles. Not real-time safe to construct. */
class Fft
{
public:
    explicit Fft (int size);

    int size() const noexcept { return n; }

    /** In-place forward transform (no scaling). */
    void forward (std::complex<float>* data) const noexcept { transform (data, false); }

    /** In-place inverse transform, scaled by 1/N. */
    void inverse (std::complex<float>* data) const noexcept { transform (data, true); }

private:
    void transform (std::complex<float>* data, bool inverse) const noexcept;

    int n;
    std::vector<std::complex<float>> twiddles;
    std::vector<int> bitReverse;
};
} // namespace tone
