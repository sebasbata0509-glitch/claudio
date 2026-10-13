#include "support/Signals.h"

#include "analysis/PYin.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace tone;
using namespace tone::test;

namespace
{
struct Accuracy
{
    float maxErrCents = 0.0f;
    float voicedFraction = 0.0f;
};

Accuracy accuracyOn (const std::vector<float>& x, double sr, const F0Fn& truth, double t0, double t1)
{
    const auto track = runPYin (x.data(), x.size(), sr);
    Accuracy a;
    int frames = 0, voiced = 0;
    for (int i = track.frameAt (t0); i <= track.frameAt (t1); ++i)
    {
        ++frames;
        const auto& f = track.frames[(size_t) i];
        if (! f.voiced)
            continue;
        ++voiced;
        const float expected = hzToMidi ((float) truth (track.timeOfFrame (i)));
        a.maxErrCents = std::max (a.maxErrCents, 100.0f * std::abs (f.midi - expected));
    }
    a.voicedFraction = (float) voiced / (float) std::max (frames, 1);
    return a;
}
} // namespace

TEST_CASE ("pYIN tracks sines within 5 cents", "[pyin]")
{
    const double sr = 44100.0;
    const double f = GENERATE (82.41, 110.0, 196.0, 261.63, 440.0, 659.26, 880.0, 1046.5);
    const auto x = sine (f, 0.6, sr);
    const auto a = accuracyOn (x, sr, [f] (double) { return f; }, 0.05, 0.55);
    INFO ("f0 = " << f << " Hz, max error = " << a.maxErrCents << " cents");
    CHECK (a.voicedFraction > 0.98f);
    CHECK (a.maxErrCents < 5.0f);
}

TEST_CASE ("pYIN tracks band-limited sawtooths within 5 cents", "[pyin]")
{
    const double sr = 48000.0;
    const double f = GENERATE (87.31, 146.83, 220.0, 329.63, 523.25, 783.99);
    const auto x = saw ([f] (double) { return f; }, 0.6, sr);
    const auto a = accuracyOn (x, sr, [f] (double) { return f; }, 0.05, 0.55);
    INFO ("f0 = " << f << " Hz, max error = " << a.maxErrCents << " cents");
    CHECK (a.voicedFraction > 0.98f);
    CHECK (a.maxErrCents < 5.0f);
}

TEST_CASE ("pYIN follows vibrato and glides", "[pyin]")
{
    const double sr = 44100.0;
    auto vib = [] (double t) { return 220.0 * std::exp2 (0.5 / 12.0 * std::sin (2.0 * kPi * 5.5 * t)); };
    const auto x = saw (vib, 1.0, sr);
    const auto a = accuracyOn (x, sr, vib, 0.05, 0.95);
    INFO ("max error = " << a.maxErrCents << " cents");
    CHECK (a.voicedFraction > 0.98f);
    CHECK (a.maxErrCents < 10.0f);

    auto glide = [] (double t) { return 200.0 * std::exp2 (t); }; // one octave per second
    const auto g = saw (glide, 1.0, sr);
    const auto b = accuracyOn (g, sr, glide, 0.05, 0.95);
    INFO ("glide max error = " << b.maxErrCents << " cents");
    CHECK (b.maxErrCents < 10.0f);
}

TEST_CASE ("pYIN reports silence and noise as unvoiced", "[pyin]")
{
    const double sr = 44100.0;
    std::vector<float> silence ((size_t) sr / 2, 0.0f);
    auto t1 = runPYin (silence.data(), silence.size(), sr);
    for (const auto& f : t1.frames)
        CHECK_FALSE (f.voiced);

    const auto n = noise (1.0, sr, 0.3f);
    auto t2 = runPYin (n.data(), n.size(), sr);
    int voiced = 0;
    for (const auto& f : t2.frames)
        voiced += f.voiced ? 1 : 0;
    CHECK ((float) voiced / (float) t2.frames.size() < 0.05f);
}
