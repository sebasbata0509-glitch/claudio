#include "EditModel.h"

#include "../analysis/Analyzer.h"
#include "../common/MathUtil.h"

#include <algorithm>
#include <cmath>

namespace tone
{
namespace
{
std::vector<float> movingAverage (const std::vector<float>& x, int half)
{
    const int n = (int) x.size();
    std::vector<double> prefix ((size_t) n + 1, 0.0);
    for (int i = 0; i < n; ++i)
        prefix[(size_t) i + 1] = prefix[(size_t) i] + x[(size_t) i];
    std::vector<float> y ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        // shrink the window symmetrically at the edges so the ends are not biased
        const int h = std::min ({ half, i, n - 1 - i });
        y[(size_t) i] = (float) ((prefix[(size_t) (i + h + 1)] - prefix[(size_t) (i - h)]) / (2 * h + 1));
    }
    return y;
}

SynthParams mix (const SynthParams& a, const SynthParams& b, float w) noexcept
{
    return { a.shift + w * (b.shift - a.shift), a.formant + w * (b.formant - a.formant), a.gainDb + w * (b.gainDb - a.gainDb) };
}
} // namespace

EditModel::EditModel (const Project& p, const PitchTrack& t)
    : proj (p), trk (t), map (TimeMap::fromProject (p))
{
    const int nf = (int) trk.frames.size();
    const int half = std::max (1, (int) std::lround (0.075 / trk.frameSeconds()));
    curves.resize (proj.notes.size());

    for (size_t k = 0; k < proj.notes.size(); ++k)
    {
        const auto& n = proj.notes[k];
        auto& c = curves[k];
        c.f0 = std::clamp (trk.frameBoundaryAt (n.srcStart), 0, std::max (0, nf - 1));
        c.f1 = std::clamp (trk.frameBoundaryAt (n.srcEnd), c.f0 + 1, std::max (c.f0 + 1, nf));
        const int len = c.f1 - c.f0;
        if (nf == 0 || len <= 0)
            continue;

        // Pitch with unvoiced frames bridged by interpolation.
        std::vector<float> m ((size_t) len, n.centerMidi);
        int lastVoiced = -1;
        for (int i = 0; i < len; ++i)
        {
            const auto& fr = trk.frames[(size_t) (c.f0 + i)];
            if (! fr.voiced)
                continue;
            m[(size_t) i] = fr.midi;
            if (lastVoiced < 0)
                std::fill (m.begin(), m.begin() + i, fr.midi);
            else
                for (int g = lastVoiced + 1; g < i; ++g)
                    m[(size_t) g] = m[(size_t) lastVoiced] + (fr.midi - m[(size_t) lastVoiced]) * (float) (g - lastVoiced) / (float) (i - lastVoiced);
            lastVoiced = i;
        }
        if (lastVoiced >= 0)
            std::fill (m.begin() + lastVoiced + 1, m.end(), m[(size_t) lastVoiced]);

        const auto slow = movingAverage (movingAverage (m, half), half);
        c.drift.resize ((size_t) len);
        c.mod.resize ((size_t) len);
        for (int i = 0; i < len; ++i)
        {
            c.drift[(size_t) i] = slow[(size_t) i] - n.centerMidi;
            c.mod[(size_t) i] = m[(size_t) i] - slow[(size_t) i];
        }
    }
}

std::pair<int, int> EditModel::frameRange (int k) const noexcept
{
    if (k < 0 || k >= (int) curves.size())
        return { 0, 0 };
    return { curves[(size_t) k].f0, curves[(size_t) k].f1 };
}

float EditModel::adjustment (int k, double src) const noexcept
{
    const auto& c = curves[(size_t) k];
    const auto& n = proj.notes[(size_t) k];
    if (c.drift.empty() || (n.driftAmount == 1.0f && n.modulationAmount == 1.0f))
        return 0.0f;
    const double fpos = std::clamp (src / trk.frameSeconds() - c.f0, 0.0, (double) (c.drift.size() - 1));
    const auto i = (size_t) fpos;
    const auto j = std::min (i + 1, c.drift.size() - 1);
    const float a = (float) (fpos - (double) i);
    const float drift = c.drift[i] + a * (c.drift[j] - c.drift[i]);
    const float mod = c.mod[i] + a * (c.mod[j] - c.mod[i]);
    return (n.driftAmount - 1.0f) * drift + (n.modulationAmount - 1.0f) * mod;
}

SynthParams EditModel::noteParams (int k, double src) const noexcept
{
    const auto& n = proj.notes[(size_t) k];
    return { n.pitchShift + adjustment (k, src), n.formantShift, n.gainDb };
}

int EditModel::noteAtDst (double t) const noexcept
{
    const auto& notes = proj.notes;
    auto it = std::upper_bound (notes.begin(), notes.end(), t, [] (double v, const Note& n) { return v < n.dstStart; });
    if (it == notes.begin())
        return -1;
    const auto k = (int) (it - notes.begin()) - 1;
    return t < notes[(size_t) k].dstEnd ? k : -1;
}

SynthParams EditModel::paramsAtDst (double t) const noexcept
{
    const auto& notes = proj.notes;
    if (notes.empty())
        return {};

    auto it = std::upper_bound (notes.begin(), notes.end(), t, [] (double v, const Note& n) { return v < n.dstStart; });
    const int prev = (int) (it - notes.begin()) - 1; // last note starting at or before t
    const double src = map.toSrc (t);

    if (prev >= 0 && t < notes[(size_t) prev].dstEnd)
    {
        const int k = prev;
        const auto& n = notes[(size_t) k];
        auto P = noteParams (k, src);

        if (k > 0 && Project::contiguous (notes[(size_t) k - 1], n))
        {
            const double X = std::min ({ transitionSeconds, 0.5 * n.dstDuration(), 0.5 * notes[(size_t) k - 1].dstDuration() });
            if (X > 0.0 && t < n.dstStart + X)
            {
                const auto w = (float) smoothStep ((t - (n.dstStart - X)) / (2.0 * X));
                P = mix (noteParams (k - 1, src), P, w);
            }
        }
        if (k + 1 < (int) notes.size() && Project::contiguous (n, notes[(size_t) k + 1]))
        {
            const double X = std::min ({ transitionSeconds, 0.5 * n.dstDuration(), 0.5 * notes[(size_t) k + 1].dstDuration() });
            if (X > 0.0 && t > n.dstEnd - X)
            {
                const auto w = (float) smoothStep ((t - (n.dstEnd - X)) / (2.0 * X));
                P = mix (P, noteParams (k + 1, src), w);
            }
        }
        return P;
    }

    // In a gap: only the gain ramps toward the neighbours to avoid level steps.
    SynthParams P;
    const double G = gapGainRampSeconds;
    if (prev >= 0)
    {
        const double d = t - notes[(size_t) prev].dstEnd;
        if (d < G)
            P.gainDb += notes[(size_t) prev].gainDb * (float) (1.0 - d / G);
    }
    if (prev + 1 < (int) notes.size())
    {
        const double d = notes[(size_t) prev + 1].dstStart - t;
        if (d < G)
            P.gainDb += notes[(size_t) prev + 1].gainDb * (float) (1.0 - d / G);
    }
    return P;
}

float EditModel::editedMidiAtFrame (int k, int frame) const noexcept
{
    const auto& n = proj.notes[(size_t) k];
    const auto& fr = trk.frames[(size_t) std::clamp (frame, 0, (int) trk.frames.size() - 1)];
    const float base = fr.voiced ? fr.midi : n.centerMidi;
    return base + n.pitchShift + adjustment (k, trk.timeOfFrame (frame));
}

void EditModel::editedCurve (int k, std::vector<std::pair<double, float>>& out) const
{
    out.clear();
    if (k < 0 || k >= (int) curves.size())
        return;
    const auto& c = curves[(size_t) k];
    for (int i = c.f0; i < c.f1; ++i)
        if (trk.frames[(size_t) i].voiced)
            out.emplace_back (map.toDst (trk.timeOfFrame (i)), editedMidiAtFrame (k, i));
}

void EditModel::originalCurve (int k, std::vector<std::pair<double, float>>& out) const
{
    out.clear();
    if (k < 0 || k >= (int) curves.size())
        return;
    const auto& c = curves[(size_t) k];
    for (int i = c.f0; i < c.f1; ++i)
        if (trk.frames[(size_t) i].voiced)
            out.emplace_back (map.toDst (trk.timeOfFrame (i)), trk.frames[(size_t) i].midi);
}

Project projectFromAnalysis (const AnalysisResult& a)
{
    Project p;
    p.sourceLength = a.sourceLengthSeconds;
    for (const auto& seg : a.notes)
    {
        Note n;
        n.id = p.nextId++;
        n.srcStart = n.dstStart = a.track.boundaryTime (seg.startFrame);
        n.srcEnd = n.dstEnd = std::min (a.track.boundaryTime (seg.endFrame), a.sourceLengthSeconds);
        n.centerMidi = seg.centerMidi;
        if (n.srcEnd > n.srcStart)
            p.notes.push_back (n);
    }
    return p;
}

edits::CenterFn makeCenterFn (const PitchTrack& track)
{
    return [&track] (double s0, double s1)
    {
        return medianPitch (track, track.frameBoundaryAt (s0), track.frameBoundaryAt (s1));
    };
}
} // namespace tone
