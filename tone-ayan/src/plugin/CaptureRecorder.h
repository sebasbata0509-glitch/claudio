#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace tone::plugin
{
/**
    Records the audio passing through the plugin, anchored to the host timeline ("capture" mode).

    Memory is a table of fixed-size chunks indexed by host sample position. A non-audio thread
    keeps chunks allocated around the playhead (ensureAllocatedAround), so write() on the audio
    thread never allocates or locks; it only drops samples if a chunk was not ready in time.
*/
class CaptureRecorder
{
public:
    static constexpr int64_t chunkSize = 1 << 15;  // ~0.7 s at 48 kHz
    static constexpr int64_t maxChunks = 1 << 13;  // ~1.5 h at 48 kHz

    CaptureRecorder();
    ~CaptureRecorder();

    // message thread ---------------------------------------------------------------
    void setArmed (bool shouldBeArmed);
    bool isArmed() const noexcept { return armed.load(); }
    bool hasData() const noexcept { return maxWritten.load() > minWritten.load(); }
    /** Allocates chunks from a little before to several seconds after the host position. */
    void ensureAllocatedAround (int64_t hostSample);
    /** Copies out [first written, last written) and resets. Returns the start position. */
    int64_t takeRecording (std::vector<float>& out);
    void reset();
    int64_t droppedSamples() const noexcept { return dropped.load(); }

    // audio thread -----------------------------------------------------------------
    void write (const float* mono, int numSamples, int64_t hostSample) noexcept;

private:
    void freeAll();

    std::unique_ptr<std::atomic<float*>[]> chunks;
    std::atomic<bool> armed { false }, writing { false };
    std::atomic<int64_t> minWritten { INT64_MAX }, maxWritten { INT64_MIN }, dropped { 0 };
};
} // namespace tone::plugin
