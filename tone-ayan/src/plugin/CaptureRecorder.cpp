#include "CaptureRecorder.h"

#include <algorithm>
#include <thread>

namespace tone::plugin
{
CaptureRecorder::CaptureRecorder() : chunks (new std::atomic<float*>[(size_t) maxChunks])
{
    for (int64_t i = 0; i < maxChunks; ++i)
        chunks[(size_t) i].store (nullptr);
}

CaptureRecorder::~CaptureRecorder()
{
    setArmed (false);
    freeAll();
}

void CaptureRecorder::setArmed (bool shouldBeArmed)
{
    armed = shouldBeArmed;
    if (! shouldBeArmed)
        while (writing.load()) // an in-flight write() finishes within one block
            std::this_thread::yield();
}

void CaptureRecorder::ensureAllocatedAround (int64_t hostSample)
{
    const int64_t first = std::max<int64_t> (0, hostSample / chunkSize - 1);
    const int64_t last = std::min<int64_t> (maxChunks - 1, hostSample / chunkSize + 8);
    for (auto c = first; c <= last; ++c)
        if (chunks[(size_t) c].load (std::memory_order_acquire) == nullptr)
        {
            auto* mem = new float[(size_t) chunkSize];
            std::fill (mem, mem + chunkSize, 0.0f);
            chunks[(size_t) c].store (mem, std::memory_order_release);
        }
}

void CaptureRecorder::write (const float* mono, int n, int64_t pos) noexcept
{
    writing.store (true);
    if (! armed.load() || pos < 0)
    {
        writing.store (false);
        return;
    }

    for (int i = 0; i < n;)
    {
        const int64_t p = pos + i;
        const int64_t c = p / chunkSize;
        const int64_t offset = p % chunkSize;
        const int count = (int) std::min<int64_t> (n - i, chunkSize - offset);
        float* mem = c < maxChunks ? chunks[(size_t) c].load (std::memory_order_acquire) : nullptr;
        if (mem != nullptr)
            std::copy (mono + i, mono + i + count, mem + offset);
        else
            dropped.fetch_add (count);
        i += count;
    }

    // monotonic min / max of the recorded span
    int64_t lo = minWritten.load();
    while (pos < lo && ! minWritten.compare_exchange_weak (lo, pos)) {}
    int64_t hi = maxWritten.load();
    while (pos + n > hi && ! maxWritten.compare_exchange_weak (hi, pos + n)) {}
    writing.store (false);
}

int64_t CaptureRecorder::takeRecording (std::vector<float>& out)
{
    setArmed (false);
    const int64_t lo = minWritten.load(), hi = maxWritten.load();
    out.clear();
    if (hi <= lo)
        return 0;
    out.resize ((size_t) (hi - lo), 0.0f);
    for (int64_t p = lo; p < hi;)
    {
        const int64_t c = p / chunkSize, offset = p % chunkSize;
        const int64_t count = std::min (hi - p, chunkSize - offset);
        if (const float* mem = chunks[(size_t) c].load())
            std::copy (mem + offset, mem + offset + count, out.begin() + (p - lo));
        p += count;
    }
    reset();
    return lo;
}

void CaptureRecorder::reset()
{
    setArmed (false);
    freeAll();
    minWritten = INT64_MAX;
    maxWritten = INT64_MIN;
    dropped = 0;
}

void CaptureRecorder::freeAll()
{
    for (int64_t i = 0; i < maxChunks; ++i)
        delete[] chunks[(size_t) i].exchange (nullptr);
}
} // namespace tone::plugin
