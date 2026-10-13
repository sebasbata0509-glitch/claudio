#include "TimeMap.h"

#include <algorithm>
#include <cmath>

namespace tone
{
void TimeMap::add (double src, double dst)
{
    if (! s.empty() && src - s.back() < 1.0e-9)
    {
        d.back() = std::max (d.back(), dst);
        return;
    }
    if (! d.empty())
        dst = std::max (dst, d.back());
    s.push_back (src);
    d.push_back (dst);
}

TimeMap TimeMap::fromProject (const Project& p)
{
    TimeMap m;
    if (p.notes.empty())
        return m;

    if (p.notes.front().srcStart > 1.0e-9)
        m.add (0.0, 0.0);
    for (const auto& n : p.notes)
    {
        m.add (n.srcStart, n.dstStart);
        m.add (n.srcEnd, n.dstEnd);
    }
    const auto& last = p.notes.back();
    if (p.sourceLength > last.srcEnd + 1.0e-9)
        m.add (p.sourceLength, p.sourceLength + (last.dstEnd - last.srcEnd));
    return m;
}

double TimeMap::map (const std::vector<double>& from, const std::vector<double>& to, double x) noexcept
{
    if (from.empty())
        return x;
    if (x <= from.front())
        return to.front() + (x - from.front());
    if (x >= from.back())
        return to.back() + (x - from.back());

    const auto it = std::upper_bound (from.begin(), from.end(), x);
    const auto k = (size_t) (it - from.begin()) - 1;
    const double span = from[k + 1] - from[k];
    if (span <= 1.0e-12)
        return to[k + 1];
    return to[k] + (x - from[k]) * (to[k + 1] - to[k]) / span;
}

double TimeMap::toSrc (double dst) const noexcept { return map (d, s, dst); }
double TimeMap::toDst (double src) const noexcept { return map (s, d, src); }

double TimeMap::srcRate (double dst) const noexcept
{
    if (d.size() < 2 || dst <= d.front() || dst >= d.back())
        return 1.0;
    const auto it = std::upper_bound (d.begin(), d.end(), dst);
    const auto k = (size_t) (it - d.begin()) - 1;
    const double span = d[k + 1] - d[k];
    return span <= 1.0e-12 ? 1.0 : (s[k + 1] - s[k]) / span;
}

bool TimeMap::isIdentity() const noexcept
{
    for (size_t i = 0; i < s.size(); ++i)
        if (std::abs (s[i] - d[i]) > 1.0e-9)
            return false;
    return true;
}
} // namespace tone
