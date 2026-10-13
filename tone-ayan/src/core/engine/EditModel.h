#pragma once

#include "../analysis/PitchTrack.h"
#include "../model/Edits.h"
#include "../model/Project.h"
#include "TimeMap.h"

#include <utility>
#include <vector>

namespace tone
{
/** Per-output-time synthesis parameters. */
struct SynthParams
{
    float shift = 0.0f;   // semitones relative to the source
    float formant = 0.0f; // semitones
    float gainDb = 0.0f;
};

/**
    Read-only view that turns a Project + PitchTrack into continuous synthesis parameters.

    The pitch of each note is decomposed as  midi(t) = centre + drift(t) + modulation(t), where
    drift is a slow (< ~3 Hz) component and modulation the rest (vibrato). The rendered pitch is
        centre + pitchShift + driftAmount * drift(t) + modulationAmount * modulation(t)
    so the shift relative to the source is pitchShift + (driftAmount-1) drift + (modAmount-1) mod.
    Adjacent notes cross-fade their parameters over transitionSeconds around shared boundaries.
*/
class EditModel
{
public:
    static constexpr double transitionSeconds = 0.030;
    static constexpr double gapGainRampSeconds = 0.010;

    EditModel (const Project&, const PitchTrack&);

    const Project& project() const noexcept { return proj; }
    const PitchTrack& track() const noexcept { return trk; }
    const TimeMap& timeMap() const noexcept { return map; }

    SynthParams paramsAtDst (double dstSeconds) const noexcept;

    /** Index of the note under an output time, or -1 when in a gap. */
    int noteAtDst (double dstSeconds) const noexcept;

    /** (dst seconds, midi) points of the edited pitch curve of a note, one per voiced frame. */
    void editedCurve (int noteIndex, std::vector<std::pair<double, float>>& out) const;
    /** Same, for the analysed (unedited) pitch, placed on the output timeline. */
    void originalCurve (int noteIndex, std::vector<std::pair<double, float>>& out) const;

    /** Edited-curve value with no transition blending, by source frame (for tests / UI). */
    float editedMidiAtFrame (int noteIndex, int frame) const noexcept;

    /** Analysed frame range [first, last) covered by a note. */
    std::pair<int, int> frameRange (int noteIndex) const noexcept;

private:
    struct NoteCurve
    {
        int f0 = 0, f1 = 0;
        std::vector<float> drift, mod; // per frame in [f0, f1)
    };

    SynthParams noteParams (int k, double srcSeconds) const noexcept;
    float adjustment (int k, double srcSeconds) const noexcept;

    Project proj;
    const PitchTrack& trk;
    TimeMap map;
    std::vector<NoteCurve> curves;
};

/** Builds the initial project (one note per analysed segment, unedited). */
struct AnalysisResult;
Project projectFromAnalysis (const AnalysisResult&);

/** Median-pitch lookup for split / merge, bound to a track. */
edits::CenterFn makeCenterFn (const PitchTrack&);
} // namespace tone
