#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

namespace tone::plugin
{
/**
    Single-writer / multi-reader publication of immutable snapshots to the audio thread without
    locks or allocations on the reading side.

    Readers wrap their access in a ReadScope (two atomic increments). Replaced snapshots are kept
    in a retired list and freed by a non-audio thread once no reader is active, so a reader can
    never see a deleted object.
*/
template <typename T>
class SnapshotPublisher
{
public:
    SnapshotPublisher() = default;
    ~SnapshotPublisher() { delete current.exchange (nullptr); }

    class ReadScope
    {
    public:
        explicit ReadScope (const SnapshotPublisher& p) noexcept : owner (p)
        {
            owner.readers.fetch_add (1, std::memory_order_seq_cst);
            ptr = owner.current.load (std::memory_order_seq_cst);
        }
        ~ReadScope() { owner.readers.fetch_sub (1, std::memory_order_seq_cst); }
        const T* get() const noexcept { return ptr; }
        const T* operator->() const noexcept { return ptr; }
        explicit operator bool() const noexcept { return ptr != nullptr; }

        ReadScope (const ReadScope&) = delete;
        ReadScope& operator= (const ReadScope&) = delete;

    private:
        const SnapshotPublisher& owner;
        const T* ptr = nullptr;
    };

    /** Replaces the snapshot (never call from the audio thread). */
    void publish (std::unique_ptr<T> next)
    {
        std::lock_guard<std::mutex> lock (gcMutex);
        if (T* old = current.exchange (next.release(), std::memory_order_seq_cst))
            retired.emplace_back (old);
        collectLocked();
    }

    /** Frees retired snapshots that no reader can still hold (call periodically, not from audio). */
    void collectGarbage()
    {
        std::lock_guard<std::mutex> lock (gcMutex);
        collectLocked();
    }

    /** Non-audio-thread peek (the caller must not outlive the next publish without a ReadScope). */
    const T* peek() const noexcept { return current.load (std::memory_order_acquire); }

private:
    void collectLocked()
    {
        if (! retired.empty() && readers.load (std::memory_order_seq_cst) == 0)
            retired.clear();
    }

    std::atomic<T*> current { nullptr };
    mutable std::atomic<int> readers { 0 };
    std::mutex gcMutex;
    std::vector<std::unique_ptr<T>> retired;
};
} // namespace tone::plugin
