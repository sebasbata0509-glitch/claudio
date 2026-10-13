#include "Segmenter.h"

#include "../common/MathUtil.h"

#include <algorithm>
#include <cmath>

namespace tone
{
namespace
{
int framesFor (const PitchTrack& t, double seconds) { return std::max (1, (int) std::lround (seconds / t.frameSeconds())); }

float rangeMean (const PitchTrack& t, int a, int b)
{
    double sum = 0.0;
    int count = 0;
    for (int i = std::max (0, a); i < std::min ((int) t.frames.size(), b); ++i)
        if (t.frames[(size_t) i].voiced)
        {
            sum += t.frames[(size_t) i].midi;
            ++count;
        }
    return count > 0 ? (float) (sum / count) : 0.0f;
}

float rangeMedian (const PitchTrack& t, int a, int b)
{
    std::vector<float> v;
    v.reserve ((size_t) std::max (0, b - a));
    for (int i = std::max (0, a); i < std::min ((int) t.frames.size(), b); ++i)
        if (t.frames[(size_t) i].voiced)
            v.push_back (t.frames[(size_t) i].midi);
    return median (std::move (v));
}

struct Boundary
{
    int frame;
    bool strong; // articulation (energy / onset) rather than a pure pitch change
};
} // namespace

float medianPitch (const PitchTrack& track, int startFrame, int endFrame)
{
    return rangeMedian (track, startFrame, endFrame);
}

void cleanVoicing (PitchTrack& track, const SegmenterParams& p)
{
    auto& f = track.frames;
    const int n = (int) f.size();
    const int maxHole = framesFor (track, p.maxHoleSeconds);
    const int minRun = framesFor (track, p.minVoicedSeconds);

    for (int i = 1; i < n;)
    {
        if (f[(size_t) i].voiced || ! f[(size_t) i - 1].voiced) { ++i; continue; }
        int j = i;
        while (j < n && ! f[(size_t) j].voiced)
            ++j;
        if (j < n && j - i <= maxHole)
        {
            const float m0 = f[(size_t) i - 1].midi, m1 = f[(size_t) j].midi;
            for (int k = i; k < j; ++k)
            {
                const float a = (float) (k - i + 1) / (float) (j - i + 1);
                f[(size_t) k].voiced = true;
                f[(size_t) k].midi = m0 + a * (m1 - m0);
                f[(size_t) k].f0Hz = midiToHz (f[(size_t) k].midi);
            }
        }
        i = j;
    }

    for (int i = 0; i < n;)
    {
        if (! f[(size_t) i].voiced) { ++i; continue; }
        int j = i;
        while (j < n && f[(size_t) j].voiced)
            ++j;
        if (j - i < minRun)
            for (int k = i; k < j; ++k)
            {
                f[(size_t) k].voiced = false;
                f[(size_t) k].f0Hz = 0.0f;
            }
        i = j;
    }
}

std::vector<NoteSegment> segmentNotes (const PitchTrack& track, const SegmenterParams& p)
{
    const auto& f = track.frames;
    const int n = (int) f.size();
    const int w = framesFor (track, p.pitchWindowSeconds);
    const int minNote = framesFor (track, p.minNoteSeconds);
    const int dipReach = framesFor (track, 0.1);

    float globalOnsetMax = 0.0f;
    for (const auto& fr : f)
        globalOnsetMax = std::max (globalOnsetMax, fr.onset);

    std::vector<NoteSegment> notes;

    for (int a = 0; a < n;)
    {
        if (! f[(size_t) a].voiced) { ++a; continue; }
        int b = a;
        while (b < n && f[(size_t) b].voiced)
            ++b;

        std::vector<Boundary> cands;

        // 1) Pitch changes: compare mean pitch before / after each frame. Windows span about one
        //    vibrato period, so vibrato averages out while real note changes stand out.
        if (b - a >= 2 * minNote)
        {
            std::vector<float> D ((size_t) (b - a), 0.0f);
            for (int i = a + minNote / 2; i <= b - minNote / 2; ++i)
            {
                if (i >= b) break;
                const int l0 = std::max (a, i - w), r1 = std::min (b, i + w);
                if (i - l0 < minNote / 2 || r1 - i < minNote / 2)
                    continue;
                D[(size_t) (i - a)] = std::abs (rangeMean (track, l0, i) - rangeMean (track, i, r1));
            }
            for (int i = a + 1; i < b - 1; ++i)
            {
                const float d = D[(size_t) (i - a)];
                if (d < p.pitchChangeSemitones)
                    continue;
                bool isPeak = true;
                for (int k = std::max (a, i - w / 2); k <= std::min (b - 1, i + w / 2) && isPeak; ++k)
                    if (D[(size_t) (k - a)] > d || (D[(size_t) (k - a)] == d && k < i))
                        isPeak = false;
                if (! isPeak)
                    continue;

                // Snap to the steepest pitch step near the candidate.
                int best = i;
                float bestStep = -1.0f;
                for (int k = std::max (a + 1, i - w / 4); k <= std::min (b - 1, i + w / 4); ++k)
                {
                    const float step = std::abs (f[(size_t) k].midi - f[(size_t) k - 1].midi);
                    if (step > bestStep) { bestStep = step; best = k; }
                }
                cands.push_back ({ best, false });
            }
        }

        // 2) Energy dips (re-articulated notes at the same pitch).
        for (int i = a + minNote; i < b - minNote; ++i)
        {
            const float e = f[(size_t) i].rmsDb;
            bool isMin = true;
            for (int k = i - 4; k <= i + 4 && isMin; ++k)
                if (f[(size_t) k].rmsDb < e)
                    isMin = false;
            if (! isMin)
                continue;
            float leftMax = -200.0f, rightMax = -200.0f;
            for (int k = std::max (a, i - dipReach); k < i; ++k) leftMax = std::max (leftMax, f[(size_t) k].rmsDb);
            for (int k = i + 1; k <= std::min (b - 1, i + dipReach); ++k) rightMax = std::max (rightMax, f[(size_t) k].rmsDb);
            if (leftMax - e >= p.energyDipDb && rightMax - e >= p.energyDipDb)
                cands.push_back ({ i, true });
        }

        // 3) Spectral-flux onsets.
        if (globalOnsetMax > 0.0f)
        {
            for (int i = a + minNote; i < b - minNote; ++i)
            {
                const float o = f[(size_t) i].onset;
                if (o < 0.15f * globalOnsetMax)
                    continue;
                bool isPeak = true;
                for (int k = i - 6; k <= i + 6 && isPeak; ++k)
                    if (k >= 0 && k < n && f[(size_t) k].onset > o)
                        isPeak = false;
                if (! isPeak)
                    continue;
                std::vector<float> local;
                for (int k = std::max (0, i - 40); k <= std::min (n - 1, i + 40); ++k)
                    local.push_back (f[(size_t) k].onset);
                if (o > p.onsetRatio * median (std::move (local)))
                    cands.push_back ({ i, true });
            }
        }

        // Thin out boundaries closer than a minimum note, preferring articulation boundaries.
        std::sort (cands.begin(), cands.end(), [] (auto& x, auto& y) { return x.frame < y.frame; });
        std::vector<Boundary> bounds;
        for (const auto& c : cands)
        {
            if (c.frame - a < minNote / 2 || b - c.frame < minNote / 2)
                continue;
            if (! bounds.empty() && c.frame - bounds.back().frame < minNote)
            {
                if (c.strong && ! bounds.back().strong)
                    bounds.back() = c;
                continue;
            }
            bounds.push_back (c);
        }

        // Build segments then merge too-short / same-pitch neighbours.
        struct Seg { int s, e; bool strongStart; float m; };
        std::vector<Seg> segs;
        int prevEdge = a;
        bool prevStrong = true;
        for (const auto& bd : bounds)
        {
            segs.push_back ({ prevEdge, bd.frame, prevStrong, 0.0f });
            prevEdge = bd.frame;
            prevStrong = bd.strong;
        }
        segs.push_back ({ prevEdge, b, prevStrong, 0.0f });
        for (auto& s : segs)
            s.m = rangeMedian (track, s.s, s.e);

        auto mergeAt = [&] (size_t k) // merge segs[k] and segs[k+1]
        {
            segs[k].e = segs[k + 1].e;
            segs[k].m = rangeMedian (track, segs[k].s, segs[k].e);
            segs.erase (segs.begin() + (std::ptrdiff_t) k + 1);
        };

        for (bool changed = true; changed && segs.size() > 1;)
        {
            changed = false;
            size_t shortest = segs.size();
            int shortestLen = minNote;
            for (size_t k = 0; k < segs.size(); ++k)
                if (segs[k].e - segs[k].s < shortestLen) { shortestLen = segs[k].e - segs[k].s; shortest = k; }
            if (shortest == segs.size())
                break;
            const bool hasPrev = shortest > 0, hasNext = shortest + 1 < segs.size();
            const float dPrev = hasPrev ? std::abs (segs[shortest - 1].m - segs[shortest].m) : 1.0e9f;
            const float dNext = hasNext ? std::abs (segs[shortest + 1].m - segs[shortest].m) : 1.0e9f;
            mergeAt (dPrev <= dNext ? shortest - 1 : shortest);
            changed = true;
        }

        for (size_t k = 0; k + 1 < segs.size();)
        {
            if (! segs[k + 1].strongStart && std::abs (segs[k].m - segs[k + 1].m) < p.mergeSemitones)
                mergeAt (k);
            else
                ++k;
        }

        for (const auto& s : segs)
            notes.push_back ({ s.s, s.e, s.m });

        a = b;
    }

    return notes;
}
} // namespace tone
