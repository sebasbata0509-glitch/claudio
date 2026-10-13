#pragma once

#include "../common/Parallel.h"
#include "PYin.h"
#include "PitchMarks.h"
#include "Segmenter.h"

#include <cstdint>
#include <vector>

namespace tone
{
struct AnalysisParams
{
    PYinParams pyin;
    SegmenterParams segmenter;
};

/** Everything the offline analysis produces for one mono source. Immutable once shared. */
struct AnalysisResult
{
    PitchTrack track;               // voicing already cleaned
    std::vector<PitchMark> marks;   // PSOLA epochs inside voiced runs
    std::vector<NoteSegment> notes; // initial note segmentation
    double sourceLengthSeconds = 0.0;

    std::vector<uint8_t> serialise() const;
    static AnalysisResult deserialise (const uint8_t* data, size_t size); // throws on corrupt data
};

/** Runs pYIN -> voicing cleanup -> onsets -> segmentation -> pitch marks. Progress 0..1 via job. */
AnalysisResult analyse (const float* samples, size_t numSamples, double sampleRate,
                        const AnalysisParams& params = {}, const JobControl& job = {});
} // namespace tone
