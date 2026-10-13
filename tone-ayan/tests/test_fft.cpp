#include "common/Fft.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <vector>

TEST_CASE ("FFT round trip and single-bin spectrum", "[fft]")
{
    const int N = 256;
    tone::Fft fft (N);
    std::vector<std::complex<float>> x ((size_t) N);
    for (int i = 0; i < N; ++i)
        x[(size_t) i] = { (float) std::cos (2.0 * 3.141592653589793 * 5 * i / N), 0.0f };
    auto orig = x;

    fft.forward (x.data());
    CHECK_THAT (std::abs (x[5]), Catch::Matchers::WithinAbs (N / 2.0, 1e-2));
    CHECK_THAT (std::abs (x[6]), Catch::Matchers::WithinAbs (0.0, 1e-2));

    fft.inverse (x.data());
    for (int i = 0; i < N; ++i)
        CHECK_THAT (x[(size_t) i].real(), Catch::Matchers::WithinAbs (orig[(size_t) i].real(), 1e-4));
}
