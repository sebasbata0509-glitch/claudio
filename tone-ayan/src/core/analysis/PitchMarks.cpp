#include "PitchMarks.h"

#include <algorithm>
#include <cmath>

namespace tone
{
namespace
{
double periodAt (const PitchTrack& t, double samplePos, int runStart, int runEnd)
{
    const double fpos = samplePos / t.hop;
    int i = std::clamp ((int) std::floor (fpos), runStart, runEnd - 1);
    int j = std::min (i + 1, runEnd - 1);
    const double a = std::clamp (fpos - i, 0.0, 1.0);
    const double f0 = (1.0 - a) * t.frames[(size_t) i].f0Hz + a * t.frames[(size_t) j].f0Hz;
    return t.sampleRate / std::max (f0, 20.0);
}
} // namespace

std::vector<PitchMark> findPitchMarks (const float* x, size_t n, const PitchTrack& t)
{
    std::vector<PitchMark> marks;
    const int nf = (int) t.frames.size();

    // Light low-pass so marks lock onto the period structure rather than upper harmonics.
    std::vector<float> y (n);
    for (size_t k = 0; k < n; ++k)
    {
        const float a = k > 0 ? x[k - 1] : 0.0f, c = k + 1 < n ? x[k + 1] : 0.0f;
        y[k] = 0.25f * a + 0.5f * x[k] + 0.25f * c;
    }

    for (int a = 0; a < nf;)
    {
        if (! t.frames[(size_t) a].voiced) { ++a; continue; }
        int b = a;
        while (b < nf && t.frames[(size_t) b].voiced)
            ++b;

        const auto s0 = (int64_t) std::max (0.0, (a - 0.5) * t.hop);
        const auto s1 = (int64_t) std::min ((double) n, (b - 0.5) * t.hop);
        if (s1 - s0 < 8)
        {
            a = b;
            continue;
        }

        // Polarity: follow whichever sign carries the larger peaks in this run.
        float pos = 0.0f, neg = 0.0f;
        for (auto k = s0; k < s1; ++k)
        {
            pos = std::max (pos, y[(size_t) k]);
            neg = std::max (neg, -y[(size_t) k]);
        }
        const float sign = pos >= neg ? 1.0f : -1.0f;
        auto val = [&] (int64_t k) { return sign * y[(size_t) k]; };

        // Anchor on the loudest frame of the run.
        int loud = a;
        for (int i = a; i < b; ++i)
            if (t.frames[(size_t) i].rmsDb > t.frames[(size_t) loud].rmsDb)
                loud = i;
        const double centre = std::clamp ((double) loud * t.hop, (double) s0, (double) (s1 - 1));
        const double T0 = periodAt (t, centre, a, b);
        int64_t anchor = (int64_t) centre;
        {
            const auto lo = std::max (s0, (int64_t) (centre - 0.5 * T0));
            const auto hi = std::min (s1 - 1, (int64_t) (centre + 0.5 * T0));
            for (auto k = lo; k <= hi; ++k)
                if (val (k) > val (anchor))
                    anchor = k;
        }

        auto search = [&] (double expected, double T) -> int64_t
        {
            const auto lo = std::max (s0, (int64_t) std::ceil (expected - 0.3 * T));
            const auto hi = std::min (s1 - 1, (int64_t) std::floor (expected + 0.3 * T));
            if (lo > hi)
                return -1;
            float range = 1.0e-9f;
            for (auto k = lo; k <= hi; ++k)
                range = std::max (range, std::abs (val (k)));
            int64_t best = lo;
            double bestScore = -1.0e30;
            for (auto k = lo; k <= hi; ++k)
            {
                const double dev = ((double) k - expected) / (0.3 * T);
                const double score = val (k) - 0.35 * range * dev * dev;
                if (score > bestScore) { bestScore = score; best = k; }
            }
            return best;
        };

        std::vector<double> fwd { (double) anchor }, bwd;
        for (;;)
        {
            const double p = fwd.back();
            const double T = periodAt (t, p, a, b);
            if (p + T >= (double) s1)
                break;
            const auto m = search (p + T, T);
            if (m < 0 || (double) m <= p + 0.5 * T)
                break;
            fwd.push_back ((double) m);
        }
        for (double p = (double) anchor;;)
        {
            const double T = periodAt (t, p, a, b);
            if (p - T < (double) s0)
                break;
            const auto m = search (p - T, T);
            if (m < 0 || (double) m >= p - 0.5 * T)
                break;
            bwd.push_back ((double) m);
            p = (double) m;
        }

        std::vector<double> run (bwd.rbegin(), bwd.rend());
        run.insert (run.end(), fwd.begin(), fwd.end());
        for (size_t k = 0; k < run.size(); ++k)
        {
            float period;
            if (run.size() == 1)
                period = (float) periodAt (t, run[k], a, b);
            else if (k == 0)
                period = (float) (run[1] - run[0]);
            else if (k + 1 == run.size())
                period = (float) (run[k] - run[k - 1]);
            else
                period = (float) (0.5 * (run[k + 1] - run[k - 1]));
            marks.push_back ({ run[k], period });
        }

        a = b;
    }

    return marks;
}
} // namespace tone
