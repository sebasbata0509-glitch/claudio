#pragma once

#include "../model/Project.h"

#include <vector>

namespace tone
{
/**
    Monotonic piecewise-linear mapping between output (dst) time and source (src) time, built
    from the note boundaries. Notes stretch linearly; gaps between notes stretch to fit; the tail
    after the last note keeps its offset. Times are in seconds.
*/
class TimeMap
{
public:
    TimeMap() = default; // identity

    static TimeMap fromProject (const Project&);

    double toSrc (double dst) const noexcept;
    double toDst (double src) const noexcept;

    /** d(src)/d(dst) at an output time (1 = untouched, < 1 = slowed down). */
    double srcRate (double dst) const noexcept;

    bool isIdentity() const noexcept;

    const std::vector<double>& srcAnchors() const noexcept { return s; }
    const std::vector<double>& dstAnchors() const noexcept { return d; }

private:
    static double map (const std::vector<double>& from, const std::vector<double>& to, double x) noexcept;
    void add (double src, double dst);

    std::vector<double> s, d;
};
} // namespace tone
