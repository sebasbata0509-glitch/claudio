#include "Onsets.h"

#include "../common/Fft.h"
#include "../common/MathUtil.h"
#include "../common/Parallel.h"

#include <cmath>
#include <complex>

namespace tone
{
void computeOnsetStrength (const float* x, size_t n, PitchTrack& track)
{
    const int N = nextPowerOfTwo ((int) (track.sampleRate * 0.023));
    const int bins = N / 2 + 1;
    const size_t frames = track.frames.size();
    std::vector<float> window ((size_t) N);
    float wsum = 0.0f;
    for (int k = 0; k < N; ++k)
    {
        window[(size_t) k] = 0.5f - 0.5f * (float) std::cos (2.0 * kPi * k / N);
        wsum += window[(size_t) k];
    }

    std::vector<float> logMag (frames * (size_t) bins);
    parallelFor (frames, [&] (size_t begin, size_t end)
    {
        Fft fft (N);
        std::vector<std::complex<float>> buf ((size_t) N);
        for (size_t f = begin; f < end; ++f)
        {
            const int64_t start = (int64_t) f * track.hop - N / 2;
            for (int k = 0; k < N; ++k)
            {
                const int64_t idx = start + k;
                buf[(size_t) k] = { (idx >= 0 && idx < (int64_t) n) ? x[idx] * window[(size_t) k] : 0.0f, 0.0f };
            }
            fft.forward (buf.data());
            for (int b = 0; b < bins; ++b)
                logMag[f * (size_t) bins + (size_t) b] = std::log1p (1000.0f * std::abs (buf[(size_t) b]) / wsum);
        }
    });

    for (size_t f = 0; f < frames; ++f)
    {
        float flux = 0.0f;
        if (f > 0)
        {
            const float* a = &logMag[(f - 1) * (size_t) bins];
            const float* b = &logMag[f * (size_t) bins];
            for (int k = 1; k < bins; ++k)
                flux += std::max (0.0f, b[k] - a[k]);
        }
        track.frames[f].onset = flux / (float) bins;
    }
}
} // namespace tone
