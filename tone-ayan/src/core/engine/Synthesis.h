#pragma once

#include "../analysis/PitchTrack.h"
#include "EditModel.h"

#include <cstdint>
#include <vector>

namespace tone
{
/** A voiced source region with its pitch marks [firstMark, lastMark). Sample units. */
struct VoicedRegion
{
    int64_t start = 0, end = 0;
    int firstMark = 0, lastMark = 0;
};

/** Everything the synthesis kernels need; all references must outlive the context. */
struct SynthContext
{
    const float* x = nullptr;
    int64_t n = 0;
    double sr = 44100.0;
    const std::vector<PitchMark>* marks = nullptr;
    const EditModel* model = nullptr;
    std::vector<VoicedRegion> regions;

    /** Voiced-layer weight (0..1) at a source sample position. */
    float voicedMask (double srcSample) const noexcept;
    /** Region containing a source sample position, or -1. */
    int regionAt (double srcSample) const noexcept;
};

SynthContext makeSynthContext (const float* x, int64_t n, double sr, const std::vector<PitchMark>& marks,
                               const PitchTrack& track, const EditModel& model);

/** TD-PSOLA voiced layer for output samples [a, b); adds into out (length b - a). */
void renderPsolaVoiced (const SynthContext&, int64_t a, int64_t b, float* out);

/** Time-mapped overlap-add of the source with no pitch change (consonants, breaths, noise). */
void renderUnvoiced (const SynthContext&, int64_t a, int64_t b, float* out);

/** Alternative engine: phase vocoder with cepstral spectral-envelope preservation (whole signal). */
void renderPhaseVocoder (const SynthContext&, int64_t a, int64_t b, float* out);
} // namespace tone
