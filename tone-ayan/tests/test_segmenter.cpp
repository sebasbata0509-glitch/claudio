#include "support/Signals.h"

#include "analysis/Analyzer.h"

#include <catch2/catch_test_macros.hpp>

using namespace tone;
using namespace tone::test;

namespace
{
std::vector<NoteSegment> notesOf (const std::vector<float>& x, double sr, PitchTrack* trackOut = nullptr)
{
    auto r = analyse (x.data(), x.size(), sr);
    if (trackOut != nullptr)
        *trackOut = r.track;
    return r.notes;
}
} // namespace

TEST_CASE ("Legato note sequence is split at pitch changes", "[segmenter]")
{
    const double sr = 44100.0, dur = 0.3;
    const std::vector<float> midis { 60, 64, 67, 72, 71, 69 };
    auto x = vowel (noteSequence (midis, dur), dur * (double) midis.size(), sr);
    applyFades (x, sr);
    PitchTrack track;
    const auto notes = notesOf (x, sr, &track);

    REQUIRE (notes.size() == midis.size());
    for (size_t i = 0; i < notes.size(); ++i)
    {
        INFO ("note " << i << " centre " << notes[i].centerMidi);
        CHECK (std::abs (notes[i].centerMidi - midis[i]) < 0.10f);
        if (i > 0)
        {
            const double boundary = track.boundaryTime (notes[i].startFrame);
            CHECK (std::abs (boundary - dur * (double) i) < 0.025);
        }
    }
}

TEST_CASE ("Notes separated by silence and noise", "[segmenter]")
{
    const double sr = 44100.0;
    std::vector<float> x;
    const std::vector<float> midis { 57, 62, 62, 66 };
    for (size_t i = 0; i < midis.size(); ++i)
    {
        auto tone = saw ([m = midis[i]] (double) { return (double) midiToHz (m); }, 0.35, sr);
        applyFades (tone, sr);
        append (x, tone);
        append (x, i % 2 == 0 ? std::vector<float> ((size_t) (0.15 * sr), 0.0f) : noise (0.15, sr, 0.03f, (unsigned) i));
    }
    const auto notes = notesOf (x, sr);
    REQUIRE (notes.size() == midis.size());
    for (size_t i = 0; i < notes.size(); ++i)
        CHECK (std::abs (notes[i].centerMidi - midis[i]) < 0.10f);
}

TEST_CASE ("Re-articulated note at the same pitch is split by its energy dip", "[segmenter]")
{
    const double sr = 44100.0;
    auto x = saw ([] (double) { return 220.0; }, 1.0, sr);
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = (double) i / sr;
        const double d = std::abs (t - 0.5);
        x[i] *= (float) (d < 0.04 ? 0.08 + 0.92 * d / 0.04 : 1.0); // ~22 dB dip at 0.5 s
    }
    applyFades (x, sr);
    const auto notes = notesOf (x, sr);
    REQUIRE (notes.size() == 2);
}

TEST_CASE ("Vibrato stays a single note", "[segmenter]")
{
    const double sr = 44100.0;
    auto x = vowel ([] (double t) { return 261.63 * std::exp2 (0.6 / 12.0 * std::sin (2.0 * kPi * 5.5 * t)); }, 1.5, sr);
    applyFades (x, sr);
    const auto notes = notesOf (x, sr);
    REQUIRE (notes.size() == 1);
    CHECK (std::abs (notes[0].centerMidi - 60.0f) < 0.15f);
}

TEST_CASE ("Analysis blob round-trips", "[segmenter]")
{
    const double sr = 44100.0;
    auto x = saw (noteSequence ({ 60, 62 }, 0.3), 0.6, sr);
    const auto r = analyse (x.data(), x.size(), sr);
    const auto blob = r.serialise();
    const auto back = AnalysisResult::deserialise (blob.data(), blob.size());
    CHECK (back.track.frames.size() == r.track.frames.size());
    CHECK (back.marks.size() == r.marks.size());
    CHECK (back.notes.size() == r.notes.size());
    CHECK (back.sourceLengthSeconds == r.sourceLengthSeconds);
    CHECK_THROWS (AnalysisResult::deserialise (blob.data(), blob.size() / 2));
}
