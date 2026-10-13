#pragma once

#include "../analysis/Analyzer.h"
#include "../common/Audio.h"
#include "../common/Parallel.h"
#include "../model/Project.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tone
{
enum class SynthEngine
{
    Psola = 0,        // TD-PSOLA voiced layer + time-mapped OLA for unvoiced parts (default)
    PhaseVocoder = 1  // phase vocoder with cepstral envelope preservation (alternative)
};

struct RenderStats
{
    int unitsTotal = 0;
    int unitsRendered = 0; // cache misses
};

/**
    Offline renderer. The output timeline is cut into units (one per note and one per gap). Each
    unit is rendered with a small overlap and stored in a cache keyed by a hash of everything that
    can influence it (its range, the notes around it and their parameters), so after an edit only
    the units touched by the change are re-rendered. Units are joined with complementary
    raised-cosine cross-fades.

    Not thread-safe: use one Renderer per worker thread.
*/
class Renderer
{
public:
    Renderer (std::shared_ptr<const SourceAudio>, std::shared_ptr<const AnalysisResult>);

    void setEngine (SynthEngine e) { engine = e; }
    SynthEngine getEngine() const noexcept { return engine; }

    /** Renders the whole project into out (resized). Returns false if cancelled. */
    bool render (const Project&, std::vector<float>& out, const JobControl& job = {}, RenderStats* stats = nullptr);

    void clearCache() { cache.clear(); }
    size_t cachedUnits() const noexcept { return cache.size(); }

private:
    struct CacheEntry
    {
        std::vector<float> samples; // covers [start, start + size)
        int64_t start = 0;
        uint64_t lastUsed = 0;
    };

    std::shared_ptr<const SourceAudio> source;
    std::shared_ptr<const AnalysisResult> analysis;
    SynthEngine engine = SynthEngine::Psola;
    std::unordered_map<uint64_t, CacheEntry> cache;
    uint64_t generation = 0;
};
} // namespace tone
