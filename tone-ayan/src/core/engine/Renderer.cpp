#include "Renderer.h"

#include "../common/MathUtil.h"
#include "EditModel.h"
#include "Synthesis.h"

#include <algorithm>
#include <cmath>

namespace tone
{
namespace
{
constexpr double kFadeSeconds = 0.010;
constexpr double kContextSeconds = 0.12; // notes this close to a unit can influence it

struct Unit
{
    int64_t a = 0, b = 0;       // core range [a, b)
    int64_t fadeA = 0, fadeB = 0;
    uint64_t key = 0;
};
} // namespace

Renderer::Renderer (std::shared_ptr<const SourceAudio> src, std::shared_ptr<const AnalysisResult> an)
    : source (std::move (src)), analysis (std::move (an))
{
}

bool Renderer::render (const Project& project, std::vector<float>& out, const JobControl& job, RenderStats* stats)
{
    ++generation;
    const double sr = source->sampleRate;
    const auto total = (int64_t) std::ceil (project.outputLength() * sr);
    out.assign ((size_t) std::max<int64_t> (total, 0), 0.0f);
    if (total <= 0)
        return true;

    const EditModel model (project, analysis->track);
    const auto ctx = makeSynthContext (source->samples.data(), (int64_t) source->samples.size(), sr,
                                       analysis->marks, analysis->track, model);

    // Unit boundaries: every note edge on the output timeline.
    std::vector<int64_t> edges { 0, total };
    for (const auto& n : project.notes)
    {
        edges.push_back (std::clamp ((int64_t) std::llround (n.dstStart * sr), (int64_t) 0, total));
        edges.push_back (std::clamp ((int64_t) std::llround (n.dstEnd * sr), (int64_t) 0, total));
    }
    std::sort (edges.begin(), edges.end());
    edges.erase (std::unique (edges.begin(), edges.end()), edges.end());

    const auto fade = (int64_t) (kFadeSeconds * sr);
    std::vector<int64_t> boundaryFade (edges.size(), 0);
    for (size_t i = 1; i + 1 < edges.size(); ++i)
        boundaryFade[i] = std::min ({ fade, (edges[i] - edges[i - 1]) / 2, (edges[i + 1] - edges[i]) / 2 });

    std::vector<Unit> units;
    for (size_t i = 0; i + 1 < edges.size(); ++i)
    {
        Unit u { edges[i], edges[i + 1], boundaryFade[i], boundaryFade[i + 1], 0 };

        Hasher h;
        h.add ((int) engine).add (u.a).add (u.b).add (u.fadeA).add (u.fadeB).add (project.sourceLength).add (sr);
        const double t0 = (double) u.a / sr - kContextSeconds, t1 = (double) u.b / sr + kContextSeconds;
        const auto& notes = project.notes;
        for (size_t k = 0; k < notes.size(); ++k)
        {
            const auto& n = notes[k];
            // A gap is shaped by the notes bounding it, so include the nearest note on each side too.
            const bool near = n.dstEnd >= t0 && n.dstStart <= t1;
            const bool boundsGapBefore = k + 1 < notes.size() && n.dstEnd <= t0 && notes[k + 1].dstStart >= t0;
            const bool boundsGapAfter = k > 0 && n.dstStart >= t1 && notes[k - 1].dstEnd <= t1;
            if (! (near || boundsGapBefore || boundsGapAfter))
                continue;
            h.add (n.srcStart).add (n.srcEnd).add (n.dstStart).add (n.dstEnd).add (n.centerMidi)
             .add (n.pitchShift).add (n.driftAmount).add (n.modulationAmount).add (n.formantShift).add (n.gainDb);
        }
        if (notes.empty() || (double) u.b / sr > notes.back().dstEnd)
            h.add (notes.empty() ? 0.0 : notes.back().dstEnd - notes.back().srcEnd);
        u.key = h.h;
        units.push_back (u);
    }

    std::vector<size_t> dirty;
    for (size_t i = 0; i < units.size(); ++i)
        if (cache.find (units[i].key) == cache.end())
            dirty.push_back (i);

    std::vector<CacheEntry> rendered (dirty.size());
    std::vector<char> completed (dirty.size(), 0);
    std::atomic<size_t> done { 0 };
    parallelFor (dirty.size(), [&] (size_t begin, size_t end)
    {
        for (size_t d = begin; d < end; ++d)
        {
            if (job.cancelled())
                return;
            const auto& u = units[dirty[d]];
            const auto a = u.a - u.fadeA, b = u.b + u.fadeB;
            auto& e = rendered[d];
            e.start = a;
            e.samples.assign ((size_t) (b - a), 0.0f);
            const auto len = (size_t) (b - a);

            if (engine == SynthEngine::Psola)
            {
                std::vector<float> voiced (len, 0.0f), unvoiced (len, 0.0f);
                renderPsolaVoiced (ctx, a, b, voiced.data());
                renderUnvoiced (ctx, a, b, unvoiced.data());
                for (size_t i = 0; i < len; ++i)
                {
                    const double s = model.timeMap().toSrc ((double) (a + (int64_t) i) / sr) * sr;
                    const float v = ctx.voicedMask (s);
                    e.samples[i] = v * voiced[i] + (1.0f - v) * unvoiced[i];
                }
            }
            else
            {
                renderPhaseVocoder (ctx, a, b, e.samples.data());
            }

            // Gain is evaluated every 32 samples and interpolated.
            constexpr int64_t step = 32;
            float g0 = dbToGain (model.paramsAtDst ((double) a / sr).gainDb);
            for (int64_t i = 0; i < (int64_t) len; i += step)
            {
                const auto j = std::min<int64_t> ((int64_t) len, i + step);
                const float g1 = dbToGain (model.paramsAtDst ((double) (a + j) / sr).gainDb);
                for (auto k = i; k < j; ++k)
                    e.samples[(size_t) k] *= g0 + (g1 - g0) * (float) (k - i) / (float) (j - i);
                g0 = g1;
            }

            completed[d] = 1;
            const auto finished = done.fetch_add (1) + 1;
            job.progress ((float) finished / (float) dirty.size());
        }
    });

    // Finished units are cached even when cancelled, so a restarted render resumes cheaply.
    for (size_t d = 0; d < dirty.size(); ++d)
        if (completed[d] != 0)
            cache[units[dirty[d]].key] = std::move (rendered[d]);

    if (job.cancelled())
        return false;

    // Assemble with complementary raised-cosine cross-fades at every unit boundary.
    for (const auto& u : units)
    {
        auto& e = cache[u.key];
        e.lastUsed = generation;
        const auto a = u.a - u.fadeA, b = u.b + u.fadeB;
        for (auto i = std::max<int64_t> (a, 0); i < std::min (b, total); ++i)
        {
            float w = 1.0f;
            if (u.fadeA > 0 && i < u.a + u.fadeA)
                w *= (float) smoothStep (((double) (i - (u.a - u.fadeA)) + 0.5) / (double) (2 * u.fadeA));
            if (u.fadeB > 0 && i >= u.b - u.fadeB)
                w *= 1.0f - (float) smoothStep (((double) (i - (u.b - u.fadeB)) + 0.5) / (double) (2 * u.fadeB));
            out[(size_t) i] += w * e.samples[(size_t) (i - e.start)];
        }
    }

    // Keep units from the last few renders so undo / redo stays cheap.
    for (auto it = cache.begin(); it != cache.end();)
        it = generation - it->second.lastUsed > 8 ? cache.erase (it) : std::next (it);

    if (stats != nullptr)
        *stats = { (int) units.size(), (int) dirty.size() };
    return true;
}
} // namespace tone
