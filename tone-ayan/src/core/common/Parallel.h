#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

namespace tone
{
/** Splits [0, count) into contiguous blocks processed on worker threads (offline use only). */
inline void parallelFor (size_t count, const std::function<void (size_t begin, size_t end)>& body, unsigned maxThreads = 0)
{
    if (count == 0)
        return;

    unsigned threads = maxThreads != 0 ? maxThreads : std::max (1u, std::thread::hardware_concurrency());
    threads = (unsigned) std::min<size_t> (threads, std::max<size_t> (1, count / 64));

    if (threads <= 1)
    {
        body (0, count);
        return;
    }

    // Dynamic scheduling over small blocks keeps all workers busy.
    const size_t block = std::max<size_t> (16, count / (threads * 8));
    std::atomic<size_t> next { 0 };
    auto worker = [&]
    {
        for (;;)
        {
            const size_t begin = next.fetch_add (block);
            if (begin >= count)
                return;
            body (begin, std::min (count, begin + block));
        }
    };

    std::vector<std::thread> pool;
    pool.reserve (threads - 1);
    for (unsigned t = 1; t < threads; ++t)
        pool.emplace_back (worker);
    worker();
    for (auto& t : pool)
        t.join();
}

/** Progress / cancellation hooks used by long offline jobs. */
struct JobControl
{
    std::function<void (float)> onProgress;   // 0..1, may be called from worker threads
    const std::atomic<bool>* cancel = nullptr;

    bool cancelled() const noexcept { return cancel != nullptr && cancel->load (std::memory_order_relaxed); }
    void progress (float p) const
    {
        if (onProgress)
            onProgress (p);
    }
};
} // namespace tone
