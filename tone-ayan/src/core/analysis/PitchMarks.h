#pragma once

#include "PitchTrack.h"

#include <cstddef>
#include <vector>

namespace tone
{
/**
    Places one mark per glottal period inside every voiced run of the track.

    Marks sit on the dominant waveform peak of each period (polarity chosen per run), are tracked
    forwards and backwards from the strongest period so that they stay phase-consistent, and carry
    the local period used to size PSOLA grains.
*/
std::vector<PitchMark> findPitchMarks (const float* samples, size_t numSamples, const PitchTrack& track);
} // namespace tone
