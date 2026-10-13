#pragma once

#include "PitchTrack.h"

#include <cstddef>

namespace tone
{
/** Fills PitchFrame::onset with a log-magnitude spectral-flux onset strength aligned to the track frames. */
void computeOnsetStrength (const float* samples, size_t numSamples, PitchTrack& track);
} // namespace tone
