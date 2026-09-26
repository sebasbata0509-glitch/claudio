#include "PitchDetector.h"

namespace nsw
{

namespace
{
    float dot (const float* a, const float* b, int n) noexcept
    {
        float s0 = 0, s1 = 0, s2 = 0, s3 = 0;
        int i = 0;
        for (; i + 4 <= n; i += 4)
        {
            s0 += a[i] * b[i];
            s1 += a[i + 1] * b[i + 1];
            s2 += a[i + 2] * b[i + 2];
            s3 += a[i + 3] * b[i + 3];
        }
        for (; i < n; ++i)
            s0 += a[i] * b[i];
        return (s0 + s1) + (s2 + s3);
    }

    float sumSquaredDiff (const float* a, const float* b, int n) noexcept
    {
        float s0 = 0, s1 = 0, s2 = 0, s3 = 0;
        int i = 0;
        for (; i + 4 <= n; i += 4)
        {
            const float d0 = a[i] - b[i], d1 = a[i + 1] - b[i + 1];
            const float d2 = a[i + 2] - b[i + 2], d3 = a[i + 3] - b[i + 3];
            s0 += d0 * d0;
            s1 += d1 * d1;
            s2 += d2 * d2;
            s3 += d3 * d3;
        }
        for (; i < n; ++i)
        {
            const float d = a[i] - b[i];
            s0 += d * d;
        }
        return (s0 + s1) + (s2 + s3);
    }

    /** Vertex offset of the parabola through (-1, a), (0, b), (1, c). */
    float parabolicOffset (float a, float b, float c) noexcept
    {
        const float denom = a - 2.0f * b + c;
        if (std::abs (denom) < 1.0e-12f)
            return 0.0f;
        return std::clamp (0.5f * (a - c) / denom, -0.5f, 0.5f);
    }
} // namespace

void PitchDetector::prepare (double sampleRate)
{
    fs = sampleRate;
    decimation = std::max (1, (int) std::floor (fs / 16000.0));
    decimatedRate = fs / decimation;

    // Allocate for the widest range so switching never allocates.
    const int maxTau = (int) std::ceil (decimatedRate / 50.0) + 2;
    decLen = 2 * maxTau + 8;
    fullLen = (2 * maxTau + 8) * decimation + 2 * decimation + 8;
    decBuf.assign ((size_t) decLen * 2, 0.0f);
    fullBuf.assign ((size_t) fullLen * 2, 0.0f);
    diff.assign ((size_t) maxTau + 4, 0.0f);
    cmnd.assign ((size_t) maxTau + 4, 0.0f);

    const double aa = std::min (0.42 * decimatedRate, 5000.0);
    hp.setHighpass (fs, 40.0, 0.707);
    lp1.setLowpass (fs, aa, 0.5412);
    lp2.setLowpass (fs, aa, 1.3066);

    configure();
    reset();
}

void PitchDetector::setRange (DetectionRange newRange)
{
    if (newRange == range)
        return;
    range = newRange;
    configure();
    reset();
}

void PitchDetector::configure()
{
    tauMin = std::max (2, (int) std::floor (decimatedRate / getMaxFrequency()));
    tauMax = (int) std::ceil (decimatedRate / getMinFrequency());
    window = tauMax;
    hopDecimated = std::max (8, (int) std::round (decimatedRate * 0.004));
}

void PitchDetector::reset()
{
    hp.reset();
    lp1.reset();
    lp2.reset();
    std::fill (decBuf.begin(), decBuf.end(), 0.0f);
    std::fill (fullBuf.begin(), fullBuf.end(), 0.0f);
    fullWrite = decWrite = decimPhase = hopCounter = 0;
    sampleCount = 0;
    estimate = {};
    estimate.period = (float) (fs / 200.0);
    estimate.frequency = 200.0f;
    lastVoicedLag = 0.0f;
}

bool PitchDetector::process (float x) noexcept
{
    const float y = lp2.process (lp1.process (hp.process (x)));
    ++sampleCount;

    fullBuf[(size_t) fullWrite] = y;
    fullBuf[(size_t) (fullWrite + fullLen)] = y;
    if (++fullWrite == fullLen)
        fullWrite = 0;

    if (++decimPhase < decimation)
        return false;
    decimPhase = 0;

    decBuf[(size_t) decWrite] = y;
    decBuf[(size_t) (decWrite + decLen)] = y;
    if (++decWrite == decLen)
        decWrite = 0;

    if (++hopCounter < hopDecimated)
        return false;
    hopCounter = 0;

    analyse();
    return true;
}

void PitchDetector::analyse() noexcept
{
    // Most recent window + tauMax + 1 decimated samples, oldest first.
    const int span = window + tauMax + 2;
    const float* x = decBuf.data() + decWrite + decLen - span;

    // Difference function via energy terms and cross-correlation.
    double e0 = 0.0;
    for (int j = 0; j < window; ++j)
        e0 += (double) x[j] * x[j];

    double eTau = e0;
    diff[0] = 0.0f;
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        eTau += (double) x[tau + window - 1] * x[tau + window - 1] - (double) x[tau - 1] * x[tau - 1];
        const double r = dot (x, x + tau, window);
        diff[(size_t) tau] = (float) std::max (0.0, e0 + eTau - 2.0 * r);
    }

    // Cumulative mean normalised difference.
    cmnd[0] = 1.0f;
    double running = 0.0;
    for (int tau = 1; tau <= tauMax + 1; ++tau)
    {
        running += diff[(size_t) tau];
        cmnd[(size_t) tau] = running > 0.0 ? (float) (diff[(size_t) tau] * tau / running) : 1.0f;
    }

    // Absolute threshold: first dip below threshold, then descend to its local minimum.
    int best = -1;
    for (int tau = tauMin; tau <= tauMax; ++tau)
    {
        if (cmnd[(size_t) tau] < yinThreshold)
        {
            while (tau + 1 <= tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0)
    {
        best = tauMin;
        for (int tau = tauMin + 1; tau <= tauMax; ++tau)
            if (cmnd[(size_t) tau] < cmnd[(size_t) best])
                best = tau;
    }

    // Octave-down guard: a sudden jump to a much longer lag (sub-harmonic) while
    // the previous lag still dips below the threshold is almost always an error.
    // Upward jumps are left to YIN's first-dip rule, which already favours the
    // shortest valid lag.
    if (estimate.voiced && lastVoicedLag > 0.0f)
    {
        const float jump = std::log2 ((float) best / lastVoicedLag);
        if (jump > 0.75f)
        {
            const int centre = (int) std::lround (lastVoicedLag);
            const int radius = std::max (2, (int) (lastVoicedLag * 0.12f));
            int cand = -1;
            for (int tau = std::max (tauMin, centre - radius); tau <= std::min (tauMax, centre + radius); ++tau)
                if (cand < 0 || cmnd[(size_t) tau] < cmnd[(size_t) cand])
                    cand = tau;
            if (cand > 0 && cmnd[(size_t) cand] < yinThreshold)
                best = cand;
        }
    }

    const float aperiodicity = cmnd[(size_t) best];
    float lag = (float) best;
    if (best > 1 && best <= tauMax)
        lag += parabolicOffset (cmnd[(size_t) best - 1], cmnd[(size_t) best], cmnd[(size_t) best + 1]);

    const float period = decimation > 1 ? refineAtFullRate (lag) : lag;

    const double meanSquare = e0 / std::max (1, window);
    const float levelDb = 10.0f * (float) std::log10 (meanSquare + 1.0e-14);
    const bool loudEnough = levelDb > gateDb;

    bool voiced;
    if (! loudEnough)
        voiced = false;
    else if (estimate.voiced)
        voiced = aperiodicity < voicedOff;
    else
        voiced = aperiodicity < voicedOn;

    // The difference function at lag L compares x[0, W) with x[L, L + W), so its
    // effective centre is (W + L) / 2 samples after the start of the span.
    estimate.time = sampleCount - (int64_t) span * decimation + (int64_t) (0.5f * ((float) window + lag) * (float) decimation);
    estimate.confidence = std::clamp (1.0f - aperiodicity, 0.0f, 1.0f);
    estimate.levelDb = levelDb;
    estimate.voiced = voiced;
    if (voiced || ! loudEnough || aperiodicity < voicedOff)
    {
        estimate.period = period;
        estimate.frequency = (float) (fs / period);
    }
    if (voiced)
        lastVoicedLag = lag;
}

float PitchDetector::refineAtFullRate (float decimatedLag) const noexcept
{
    const float centre = decimatedLag * (float) decimation;
    const int w = window * decimation;
    const int maxLag = tauMax * decimation + decimation;
    const int lo = std::max (2, (int) std::floor (centre) - decimation);
    const int hi = std::min (maxLag, (int) std::ceil (centre) + decimation);
    if (hi - lo < 2)
        return centre;

    // Start at the same instant as the decimated analysis span so both stages
    // describe the same stretch of signal (and the reported timestamp holds).
    const int span = (window + tauMax + 2) * decimation;
    const float* x = fullBuf.data() + fullWrite + fullLen - span;

    float values[64];
    const int count = std::min (hi - lo + 1, 64);
    int best = 0;
    for (int k = 0; k < count; ++k)
    {
        values[k] = sumSquaredDiff (x, x + lo + k, w);
        if (values[k] < values[best])
            best = k;
    }
    if (best == 0 || best == count - 1)
        return centre;

    return (float) (lo + best) + parabolicOffset (values[best - 1], values[best], values[best + 1]);
}

} // namespace nsw
