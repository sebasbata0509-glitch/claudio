#pragma once

#include "Scale.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace tone
{
using NoteId = uint32_t;
using NoteIdSet = std::set<NoteId>;

/**
    One editable note blob. The analysed (source) data never changes; every edit is stored as a
    parameter so rendering is always non-destructive and fully undoable.
*/
struct Note
{
    NoteId id = 0;

    // Analysed source range (seconds into the source audio) and its median pitch.
    double srcStart = 0.0, srcEnd = 0.0;
    float centerMidi = 60.0f;

    // Placement on the output timeline (seconds). Defaults to the source range.
    double dstStart = 0.0, dstEnd = 0.0;

    // Edit parameters.
    float pitchShift = 0.0f;      // semitones (fractions = cents)
    float driftAmount = 1.0f;     // 1 = original slow pitch drift, 0 = flattened
    float modulationAmount = 1.0f;// 1 = original vibrato, 0 = removed, 2 = doubled
    float formantShift = 0.0f;    // semitones
    float gainDb = 0.0f;

    float targetMidi() const noexcept { return centerMidi + pitchShift; }
    double srcDuration() const noexcept { return srcEnd - srcStart; }
    double dstDuration() const noexcept { return dstEnd - dstStart; }
    double stretch() const noexcept { return srcDuration() > 0 ? dstDuration() / srcDuration() : 1.0; }
    bool operator== (const Note&) const = default;
};

/** The complete edit model for one source: notes (sorted by source time) plus the scale. */
struct Project
{
    std::vector<Note> notes;
    Scale scale;
    double sourceLength = 0.0;
    NoteId nextId = 1;

    const Note* find (NoteId id) const;
    Note* find (NoteId id);
    int indexOf (NoteId id) const;

    /** True when two notes (adjacent in source order) share a boundary in the source. */
    static bool contiguous (const Note& a, const Note& b) noexcept { return b.srcStart - a.srcEnd < 1.0e-6; }

    /** End of the output timeline (source tail keeps the last note's time offset). */
    double outputLength() const;

    bool operator== (const Project&) const = default;
};
} // namespace tone
