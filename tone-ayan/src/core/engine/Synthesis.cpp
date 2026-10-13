#include "Synthesis.h"

#include "../common/Fft.h"
#include "../common/MathUtil.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace tone
{
//==============================================================================
SynthContext makeSynthContext (const float* x, int64_t n, double sr, const std::vector<PitchMark>& marks,
                               const PitchTrack& track, const EditModel& model)
{
    SynthContext c;
    c.x = x;
    c.n = n;
    c.sr = sr;
    c.marks = &marks;
    c.model = &model;

    const int nf = (int) track.frames.size();
    for (int a = 0; a < nf;)
    {
        if (! track.frames[(size_t) a].voiced) { ++a; continue; }
        int b = a;
        while (b < nf && track.frames[(size_t) b].voiced)
            ++b;
        VoicedRegion r;
        r.start = (int64_t) std::max (0.0, (a - 0.5) * track.hop);
        r.end = (int64_t) std::min ((double) n, (b - 0.5) * track.hop);
        const auto lo = std::lower_bound (marks.begin(), marks.end(), (double) r.start,
                                          [] (const PitchMark& m, double v) { return m.position < v; });
        const auto hi = std::lower_bound (marks.begin(), marks.end(), (double) r.end,
                                          [] (const PitchMark& m, double v) { return m.position < v; });
        r.firstMark = (int) (lo - marks.begin());
        r.lastMark = (int) (hi - marks.begin());
        if (r.lastMark - r.firstMark >= 2)
            c.regions.push_back (r);
        a = b;
    }
    return c;
}

int SynthContext::regionAt (double s) const noexcept
{
    const auto it = std::upper_bound (regions.begin(), regions.end(), s,
                                      [] (double v, const VoicedRegion& r) { return v < (double) r.start; });
    if (it == regions.begin())
        return -1;
    const auto k = (int) (it - regions.begin()) - 1;
    return s < (double) regions[(size_t) k].end ? k : -1;
}

float SynthContext::voicedMask (double s) const noexcept
{
    const int k = regionAt (s);
    if (k < 0)
        return 0.0f;
    const auto& r = regions[(size_t) k];
    const double pF = (*marks)[(size_t) r.firstMark].position;
    const double pL = (*marks)[(size_t) r.lastMark - 1].position;
    if (s <= pF || s >= pL)
        return 0.0f;
    const double ramp = std::min (0.005 * sr, 0.5 * (pL - pF));
    if (ramp <= 0.0)
        return 1.0f;
    return (float) (smoothStep ((s - pF) / ramp) * smoothStep ((pL - s) / ramp));
}

//==============================================================================
namespace
{
inline float hannHalf (double u) noexcept // u in [-1, 1], 1 at 0
{
    return u <= -1.0 || u >= 1.0 ? 0.0f : (float) (0.5 + 0.5 * std::cos (kPi * u));
}

int nearestMark (const std::vector<PitchMark>& m, const VoicedRegion& r, double s) noexcept
{
    const auto first = m.begin() + r.firstMark, last = m.begin() + r.lastMark;
    auto it = std::lower_bound (first, last, s, [] (const PitchMark& pm, double v) { return pm.position < v; });
    if (it == last)
        return r.lastMark - 1;
    if (it != first && s - (it - 1)->position < it->position - s)
        --it;
    return (int) (it - m.begin());
}
} // namespace

void renderPsolaVoiced (const SynthContext& c, int64_t a, int64_t b, float* out)
{
    const auto& marks = *c.marks;
    const auto& map = c.model->timeMap();
    const double sr = c.sr;

    for (const auto& r : c.regions)
    {
        const double pF = marks[(size_t) r.firstMark].position, pL = marks[(size_t) r.lastMark - 1].position;
        double maxT = 0.0;
        for (int k = r.firstMark; k < r.lastMark; ++k)
            maxT = std::max (maxT, (double) marks[(size_t) k].period);
        const double margin = 4.0 * maxT + 2.0;

        const double dA = map.toDst (pF / sr) * sr, dB = map.toDst (pL / sr) * sr;
        if (dB + margin < (double) a || dA - margin > (double) b)
            continue;

        // First synthesis mark: aligned with an analysis mark so untouched audio is reproduced exactly.
        double t;
        if (dA >= (double) a - margin)
            t = dA;
        else
        {
            const int k = nearestMark (marks, r, map.toSrc (((double) a - margin) / sr) * sr);
            t = map.toDst (marks[(size_t) k].position / sr) * sr;
        }
        const double tEnd = std::min (dB + maxT, (double) b + margin);

        while (t <= tEnd)
        {
            const double tSec = t / sr;
            const double s = map.toSrc (tSec) * sr;
            const int k = nearestMark (marks, r, s);
            const auto& mk = marks[(size_t) k];
            const auto P = c.model->paramsAtDst (tSec);
            const double ratio = semitonesToRatio (P.shift);
            const double fr = semitonesToRatio (P.formant);
            const double T = std::max (2.0, (double) mk.period);

            const double L = k > r.firstMark ? std::clamp (mk.position - marks[(size_t) k - 1].position, 0.5 * T, 2.0 * T) : T;
            const double R = k + 1 < r.lastMark ? std::clamp (marks[(size_t) k + 1].position - mk.position, 0.5 * T, 2.0 * T) : T;
            const float amp = (float) (1.0 / std::sqrt (ratio)); // keep loudness when pulse density changes

            const auto n0 = std::max (a, (int64_t) std::ceil (t - L / fr));
            const auto n1 = std::min (b - 1, (int64_t) std::floor (t + R / fr));
            for (auto idx = n0; idx <= n1; ++idx)
            {
                const double j = ((double) idx - t) * fr;
                const float w = j < 0.0 ? hannHalf (j / L) : hannHalf (j / R);
                if (w <= 0.0f)
                    continue;
                out[idx - a] += amp * w * interpolateCubic (c.x, c.n, mk.position + j);
            }

            // Advance one synthesis period. Untouched regions lock back onto the analysis marks.
            const bool untouched = std::abs (P.shift) < 1.0e-4f && std::abs (P.formant) < 1.0e-4f
                                   && std::abs (map.srcRate (tSec) - 1.0) < 1.0e-9;
            double next = t + T / ratio;
            if (untouched && k + 1 < r.lastMark)
            {
                const double locked = map.toDst (marks[(size_t) k + 1].position / sr) * sr;
                if (locked > t + 0.25 * T && std::abs (locked - next) < 0.5 * T)
                    next = locked;
            }
            t = next;
        }
    }
}

//==============================================================================
void renderUnvoiced (const SynthContext& c, int64_t a, int64_t b, float* out)
{
    const int N = nextPowerOfTwo ((int) (0.0116 * c.sr));
    const int H = N / 4;
    const auto& map = c.model->timeMap();

    thread_local std::vector<float> win;
    if ((int) win.size() != N)
    {
        win.resize ((size_t) N);
        for (int k = 0; k < N; ++k)
            win[(size_t) k] = 0.5f * (0.5f - 0.5f * (float) std::cos (2.0 * kPi * k / N)); // includes 1/2 OLA gain
    }

    const auto m0 = (int64_t) std::floor ((double) (a - N / 2) / H);
    const auto m1 = (int64_t) std::ceil ((double) (b + N / 2) / H);
    for (auto m = m0; m <= m1; ++m)
    {
        const auto centre = m * H;
        const double tSec = (double) centre / c.sr;
        if (map.srcRate (tSec) < 0.05)
            continue; // degenerate stretch of an empty region: leave silent
        const auto s = (int64_t) std::llround (map.toSrc (tSec) * c.sr);
        const auto i0 = std::max (a, centre - N / 2), i1 = std::min (b, centre + N / 2);
        for (auto i = i0; i < i1; ++i)
        {
            const auto j = i - centre;
            const auto si = s + j;
            if (si >= 0 && si < c.n)
                out[i - a] += win[(size_t) (j + N / 2)] * c.x[si];
        }
    }
}

//==============================================================================
void renderPhaseVocoder (const SynthContext& c, int64_t a, int64_t b, float* out)
{
    const int N = nextPowerOfTwo ((int) (0.046 * c.sr));
    const int H = N / 4;
    const int bins = N / 2 + 1;
    const int lifter = std::max (8, (int) (0.0008 * c.sr));
    const auto& map = c.model->timeMap();
    Fft fft (N);

    std::vector<float> win ((size_t) N);
    for (int k = 0; k < N; ++k)
        win[(size_t) k] = 0.5f - 0.5f * (float) std::cos (2.0 * kPi * k / N);
    const float olaNorm = 1.0f / 1.5f; // sum of hann^2 at 75 % overlap

    std::vector<std::complex<float>> X ((size_t) N), Xp ((size_t) N), cep ((size_t) N), Y ((size_t) N);
    std::vector<float> logMag ((size_t) bins), env ((size_t) bins), target ((size_t) bins), omega ((size_t) bins), phase ((size_t) bins);
    std::vector<float> synPhase ((size_t) bins, 0.0f);
    bool havePrev = false;

    auto frameAt = [&] (double centre, std::vector<std::complex<float>>& dst)
    {
        const auto s = (int64_t) std::llround (centre);
        for (int k = 0; k < N; ++k)
        {
            const auto i = s - N / 2 + k;
            dst[(size_t) k] = { (i >= 0 && i < c.n) ? c.x[i] * win[(size_t) k] : 0.0f, 0.0f };
        }
        fft.forward (dst.data());
    };

    const auto m0 = (int64_t) std::floor ((double) (a - N / 2) / H);
    const auto m1 = (int64_t) std::ceil ((double) (b + N / 2) / H);
    for (auto m = m0; m <= m1; ++m)
    {
        const auto centre = m * H;
        const double tSec = (double) centre / c.sr;
        const double s = map.toSrc (tSec) * c.sr;
        const auto P = c.model->paramsAtDst (tSec);
        const float v = c.voicedMask (s);
        const double r = semitonesToRatio (P.shift * v);
        const double fr = semitonesToRatio (P.formant * v);

        frameAt (s, X);
        frameAt (s - H, Xp);
        for (int k = 0; k < bins; ++k)
        {
            const float mag = std::abs (X[(size_t) k]);
            phase[(size_t) k] = std::arg (X[(size_t) k]);
            const double expected = 2.0 * kPi * k * H / N;
            double dphi = phase[(size_t) k] - std::arg (Xp[(size_t) k]) - expected;
            dphi -= 2.0 * kPi * std::round (dphi / (2.0 * kPi));
            omega[(size_t) k] = (float) ((expected + dphi) / H);
            logMag[(size_t) k] = std::log (mag + 1.0e-9f);
        }

        const bool identity = std::abs (r - 1.0) < 1.0e-6 && std::abs (fr - 1.0) < 1.0e-6;
        if (identity)
        {
            for (int k = 0; k < bins; ++k)
            {
                synPhase[(size_t) k] = havePrev ? synPhase[(size_t) k] + (float) H * omega[(size_t) k] : phase[(size_t) k];
                Y[(size_t) k] = std::polar (std::exp (logMag[(size_t) k]), synPhase[(size_t) k]);
            }
        }
        else
        {
            // "True envelope" (iterative cepstral smoothing that hugs the harmonic peaks). The
            // lifter follows the local period so harmonics are never mistaken for formants.
            int q = lifter;
            {
                const auto& tr = c.model->track();
                const auto& fr0 = tr.frames[(size_t) tr.frameAt (s / c.sr)];
                if (fr0.voiced && fr0.f0Hz > 0.0f)
                    q = std::clamp ((int) (0.75 * c.sr / fr0.f0Hz), 8, N / 8);
            }
            std::copy (logMag.begin(), logMag.end(), target.begin());
            for (int iter = 0; iter < 6; ++iter)
            {
                for (int k = 0; k < N; ++k)
                    cep[(size_t) k] = { target[(size_t) (k < bins ? k : N - k)], 0.0f };
                fft.inverse (cep.data());
                for (int k = 0; k < N; ++k)
                {
                    const int qq = std::min (k, N - k);
                    const float w = qq < q ? 1.0f : (qq == q ? 0.5f : 0.0f);
                    cep[(size_t) k] = { cep[(size_t) k].real() * w, 0.0f };
                }
                fft.forward (cep.data());
                for (int k = 0; k < bins; ++k)
                {
                    env[(size_t) k] = cep[(size_t) k].real();
                    target[(size_t) k] = std::max (logMag[(size_t) k], env[(size_t) k]);
                }
            }

            auto lerp = [&] (const std::vector<float>& arr, double pos)
            {
                pos = std::clamp (pos, 0.0, (double) (bins - 1));
                const auto i = (int) pos;
                const auto j = std::min (i + 1, bins - 1);
                const auto f = (float) (pos - i);
                return arr[(size_t) i] + f * (arr[(size_t) j] - arr[(size_t) i]);
            };

            for (int k = 0; k < bins; ++k)
            {
                const double kf = k / r;
                if (kf > bins - 1)
                {
                    Y[(size_t) k] = {};
                    continue;
                }
                const int ks = std::clamp ((int) std::lround (kf), 0, bins - 1);
                const float fine = lerp (logMag, kf) - lerp (env, kf);
                const float outLog = fine + lerp (env, k / fr);
                const float w = (float) (omega[(size_t) ks] * r);
                synPhase[(size_t) k] = havePrev ? synPhase[(size_t) k] + (float) H * w : phase[(size_t) ks];
                Y[(size_t) k] = std::polar (std::exp (outLog), synPhase[(size_t) k]);
            }
        }
        havePrev = true;

        for (int k = 1; k < N / 2; ++k)
            Y[(size_t) (N - k)] = std::conj (Y[(size_t) k]);
        Y[(size_t) (N / 2)] = { Y[(size_t) (N / 2)].real(), 0.0f };
        Y[0] = { Y[0].real(), 0.0f };
        fft.inverse (Y.data());

        const auto i0 = std::max (a, centre - N / 2), i1 = std::min (b, centre + N / 2);
        for (auto i = i0; i < i1; ++i)
        {
            const auto k = (size_t) (i - (centre - N / 2));
            out[i - a] += Y[k].real() * win[k] * olaNorm;
        }
    }
}
} // namespace tone
