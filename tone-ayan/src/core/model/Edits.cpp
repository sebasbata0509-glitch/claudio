#include "Edits.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tone::edits
{
namespace
{
template <typename Fn>
void forEach (Project& p, const NoteIdSet& ids, Fn&& fn)
{
    for (auto& n : p.notes)
        if (ids.count (n.id) != 0)
            fn (n);
}
} // namespace

void transpose (Project& p, const NoteIdSet& ids, float s)
{
    forEach (p, ids, [s] (Note& n) { n.pitchShift += s; });
}

void setPitchShift (Project& p, NoteId id, float s)
{
    if (auto* n = p.find (id))
        n->pitchShift = s;
}

void correctPitchCenter (Project& p, const NoteIdSet& ids, float amount)
{
    amount = std::clamp (amount, 0.0f, 1.0f);
    forEach (p, ids, [amount] (Note& n)
    {
        const float t = n.targetMidi();
        n.pitchShift += amount * (std::round (t) - t);
    });
}

void correctToScale (Project& p, const NoteIdSet& ids, float amount)
{
    amount = std::clamp (amount, 0.0f, 1.0f);
    const auto scale = p.scale;
    forEach (p, ids, [amount, &scale] (Note& n)
    {
        const float t = n.targetMidi();
        n.pitchShift += amount * (scale.nearest (t) - t);
    });
}

void setDrift (Project& p, const NoteIdSet& ids, float v)      { forEach (p, ids, [v] (Note& n) { n.driftAmount = std::clamp (v, 0.0f, 1.0f); }); }
void setModulation (Project& p, const NoteIdSet& ids, float v) { forEach (p, ids, [v] (Note& n) { n.modulationAmount = std::clamp (v, 0.0f, 2.0f); }); }
void setFormant (Project& p, const NoteIdSet& ids, float v)    { forEach (p, ids, [v] (Note& n) { n.formantShift = std::clamp (v, -12.0f, 12.0f); }); }
void setGain (Project& p, const NoteIdSet& ids, float v)       { forEach (p, ids, [v] (Note& n) { n.gainDb = std::clamp (v, -48.0f, 12.0f); }); }

void resetNotes (Project& p, const NoteIdSet& ids)
{
    forEach (p, ids, [] (Note& n)
    {
        n.pitchShift = 0.0f;
        n.driftAmount = 1.0f;
        n.modulationAmount = 1.0f;
        n.formantShift = 0.0f;
        n.gainDb = 0.0f;
    });
}

//==============================================================================
namespace
{
/** For a move by dt, which edges of each note shift (0 or 1 times dt). */
struct EdgeCoefficients
{
    std::vector<int> start, end;
};

EdgeCoefficients edgeCoefficients (const Project& p, const NoteIdSet& ids)
{
    const size_t n = p.notes.size();
    EdgeCoefficients c { std::vector<int> (n, 0), std::vector<int> (n, 0) };
    for (size_t i = 0; i < n; ++i)
    {
        if (ids.count (p.notes[i].id) != 0)
        {
            c.start[i] = c.end[i] = 1;
            continue;
        }
        if (i > 0 && ids.count (p.notes[i - 1].id) != 0 && Project::contiguous (p.notes[i - 1], p.notes[i]))
            c.start[i] = 1;
        if (i + 1 < n && ids.count (p.notes[i + 1].id) != 0 && Project::contiguous (p.notes[i], p.notes[i + 1]))
            c.end[i] = 1;
    }
    return c;
}
} // namespace

double clampMove (const Project& p, const NoteIdSet& ids, double dt)
{
    const auto c = edgeCoefficients (p, ids);
    double lo = -std::numeric_limits<double>::infinity(), hi = std::numeric_limits<double>::infinity();

    // constraint: value + k * dt >= 0
    auto constrain = [&] (double value, int k)
    {
        if (k > 0) lo = std::max (lo, -value / k);
        if (k < 0) hi = std::min (hi, value / -k);
    };

    const auto& notes = p.notes;
    for (size_t i = 0; i < notes.size(); ++i)
    {
        constrain (notes[i].dstDuration() - kMinNoteSeconds, c.end[i] - c.start[i]);
        if (i == 0)
            constrain (notes[i].dstStart, c.start[i]);
        else if (! Project::contiguous (notes[i - 1], notes[i]))
            constrain (notes[i].dstStart - notes[i - 1].dstEnd, c.start[i] - c.end[i - 1]);
    }

    lo = std::min (lo, 0.0);
    hi = std::max (hi, 0.0);
    return std::clamp (dt, lo, hi);
}

double moveTime (Project& p, const NoteIdSet& ids, double dt)
{
    dt = clampMove (p, ids, dt);
    if (dt == 0.0)
        return 0.0;
    const auto c = edgeCoefficients (p, ids);
    for (size_t i = 0; i < p.notes.size(); ++i)
    {
        p.notes[i].dstStart += c.start[i] * dt;
        p.notes[i].dstEnd += c.end[i] * dt;
    }
    return dt;
}

double resizeEdge (Project& p, NoteId id, Edge edge, double t)
{
    const int i = p.indexOf (id);
    if (i < 0)
        return t;
    auto& notes = p.notes;
    auto& n = notes[(size_t) i];

    if (edge == Edge::End)
    {
        double lo = n.dstStart + kMinNoteSeconds, hi = std::numeric_limits<double>::infinity();
        const bool hasNext = (size_t) i + 1 < notes.size();
        const bool attached = hasNext && Project::contiguous (n, notes[(size_t) i + 1]);
        if (hasNext)
            hi = attached ? notes[(size_t) i + 1].dstEnd - kMinNoteSeconds : notes[(size_t) i + 1].dstStart;
        t = std::clamp (t, lo, std::max (lo, hi));
        n.dstEnd = t;
        if (attached)
            notes[(size_t) i + 1].dstStart = t;
    }
    else
    {
        double lo = 0.0, hi = n.dstEnd - kMinNoteSeconds;
        const bool attached = i > 0 && Project::contiguous (notes[(size_t) i - 1], n);
        if (i > 0)
            lo = attached ? notes[(size_t) i - 1].dstStart + kMinNoteSeconds : notes[(size_t) i - 1].dstEnd;
        t = std::clamp (t, std::min (lo, hi), hi);
        n.dstStart = t;
        if (attached)
            notes[(size_t) i - 1].dstEnd = t;
    }
    return t;
}

void resetTiming (Project& p)
{
    for (auto& n : p.notes)
    {
        n.dstStart = n.srcStart;
        n.dstEnd = n.srcEnd;
    }
}

bool split (Project& p, NoteId id, double dstTime, const CenterFn& centerOf)
{
    const int i = p.indexOf (id);
    if (i < 0)
        return false;
    const Note orig = p.notes[(size_t) i];
    if (dstTime - orig.dstStart < kMinNoteSeconds || orig.dstEnd - dstTime < kMinNoteSeconds)
        return false;

    const double a = (dstTime - orig.dstStart) / orig.dstDuration();
    const double srcSplit = orig.srcStart + a * orig.srcDuration();
    if (srcSplit - orig.srcStart < kMinNoteSeconds || orig.srcEnd - srcSplit < kMinNoteSeconds)
        return false;

    Note left = orig, right = orig;
    left.srcEnd = srcSplit;
    left.dstEnd = dstTime;
    right.id = p.nextId++;
    right.srcStart = srcSplit;
    right.dstStart = dstTime;
    left.centerMidi = centerOf (left.srcStart, left.srcEnd);
    right.centerMidi = centerOf (right.srcStart, right.srcEnd);

    p.notes[(size_t) i] = left;
    p.notes.insert (p.notes.begin() + i + 1, right);
    return true;
}

bool merge (Project& p, const NoteIdSet& ids, const CenterFn& centerOf)
{
    bool mergedAny = false;
    for (size_t i = 0; i + 1 < p.notes.size();)
    {
        auto& a = p.notes[i];
        const auto& b = p.notes[i + 1];
        if (ids.count (a.id) == 0 || ids.count (b.id) == 0 || ! Project::contiguous (a, b))
        {
            ++i;
            continue;
        }

        const double da = a.srcDuration(), db = b.srcDuration();
        const float target = (float) ((a.targetMidi() * da + b.targetMidi() * db) / (da + db));
        a.srcEnd = b.srcEnd;
        a.dstEnd = b.dstEnd;
        a.centerMidi = centerOf (a.srcStart, a.srcEnd);
        a.pitchShift = target - a.centerMidi;
        p.notes.erase (p.notes.begin() + (std::ptrdiff_t) i + 1);
        mergedAny = true;
    }
    return mergedAny;
}
} // namespace tone::edits
