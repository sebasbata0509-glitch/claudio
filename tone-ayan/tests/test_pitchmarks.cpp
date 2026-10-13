#include "support/Signals.h"

#include "analysis/Analyzer.h"

#include <catch2/catch_test_macros.hpp>

using namespace tone;
using namespace tone::test;

TEST_CASE ("Pitch marks are one per period and evenly spaced", "[marks]")
{
    const double sr = 44100.0;
    for (double f : { 110.0, 220.0, 440.0 })
    {
        auto x = vowel ([f] (double) { return f; }, 0.5, sr);
        applyFades (x, sr);
        const auto r = analyse (x.data(), x.size(), sr);
        REQUIRE (r.marks.size() > 10);
        const double T = sr / f;
        int bad = 0;
        for (size_t k = 1; k < r.marks.size(); ++k)
            if (std::abs (r.marks[k].position - r.marks[k - 1].position - T) > 0.05 * T)
                ++bad;
        INFO ("f0 " << f << ", marks " << r.marks.size() << ", irregular " << bad);
        CHECK (bad == 0);
        CHECK (std::abs ((double) r.marks.size() - 0.5 * f) < 0.1 * 0.5 * f);
    }
}
