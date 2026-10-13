#pragma once

#include "PitchTrack.h"

#include <vector>

namespace tone
{
/** A detected note as a half-open frame range [startFrame, endFrame). */
struct NoteSegment
{
    int startFrame = 0;
    int endFrame = 0;
    float centerMidi = 0.0f;
};

struct SegmenterParams
{
    double maxHoleSeconds = 0.030;      // unvoiced gaps this short inside a phrase are filled
    double minVoicedSeconds = 0.040;    // voiced runs shorter than this are discarded
    double pitchWindowSeconds = 0.200;  // averaging window either side of a candidate pitch change
    float pitchChangeSemitones = 0.70f; // median jump needed to split
    float mergeSemitones = 0.50f;       // contiguous notes closer than this are merged
    double minNoteSeconds = 0.080;      // shorter notes are merged into a neighbour
    float energyDipDb = 6.0f;           // re-articulation dip needed for a split
    float onsetRatio = 4.0f;            // spectral-flux peak / local median needed for a split
};

/** Fills short unvoiced holes (interpolating pitch) and drops very short voiced fragments. */
void cleanVoicing (PitchTrack& track, const SegmenterParams& params = {});

/** Groups voiced frames into notes using pitch stability, energy dips and spectral-flux onsets. */
std::vector<NoteSegment> segmentNotes (const PitchTrack& track, const SegmenterParams& params = {});

/** Median MIDI pitch of the voiced frames in [start, end). */
float medianPitch (const PitchTrack& track, int startFrame, int endFrame);
} // namespace tone
