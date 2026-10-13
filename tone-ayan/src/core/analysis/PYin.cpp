#include "PYin.h"

#include "../common/Fft.h"
#include "../common/MathUtil.h"

#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>

namespace tone
{
namespace
{
struct Candidate
{
    float tau = 0.0f;     // refined lag in samples
    float prob = 0.0f;
    bool fromDip = false; // found by a threshold dip (vs. the absolute-minimum fallback)
};

std::vector<float> betaPrior (int n, float a, float b)
{
    std::vector<float> w ((size_t) n);
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double x = (i + 0.5) / n;
        const double p = std::pow (x, a - 1.0) * std::pow (1.0 - x, b - 1.0);
        w[(size_t) i] = (float) p;
        sum += p;
    }
    for (auto& v : w)
        v = (float) (v / sum);
    return w;
}

/**
    Computes a symmetric YIN difference function for one frame:
        d(tau) = sum_j (x_j - x_{j+tau})^2 + sum_j (x_j - x_{j-tau})^2,  j over a window centred on the frame.
    Using both lag directions keeps the effective analysis centre on the frame for every lag, so the
    estimate does not lag behind fast pitch movement (vibrato, glides). One FFT cross-correlation
    gives both terms.
*/
class FrameAnalyser
{
public:
    FrameAnalyser (int window, int minLagIn, int maxLagIn, const std::vector<float>& priorIn)
        : W (window), minLag (minLagIn), maxLag (maxLagIn),
          fft (nextPowerOfTwo (window + 2 * maxLagIn + 2)), prior (priorIn)
    {
        const auto len = (size_t) (W + 2 * maxLag + 1);
        frame.resize (len);
        prefix.resize (len + 1);
        spec.resize ((size_t) fft.size());
        prod.resize ((size_t) fft.size());
        diff.resize ((size_t) maxLag + 2);
        cmnd.resize ((size_t) maxLag + 2);
    }

    void analyse (const float* x, int64_t n, int64_t centre, std::vector<Candidate>& out, float& rmsDb)
    {
        out.clear();
        const int len = W + 2 * maxLag + 1;
        const int64_t start = centre - W / 2 - maxLag;
        for (int k = 0; k < len; ++k)
        {
            const auto idx = start + k;
            frame[(size_t) k] = (idx >= 0 && idx < n) ? x[idx] : 0.0f;
        }

        prefix[0] = 0.0;
        for (int k = 0; k < len; ++k)
            prefix[(size_t) k + 1] = prefix[(size_t) k] + (double) frame[(size_t) k] * frame[(size_t) k];
        auto energy = [this] (int from) { return prefix[(size_t) (from + W)] - prefix[(size_t) from]; };

        const double ea = energy (maxLag);
        rmsDb = (float) (10.0 * std::log10 (std::max (ea / W, 1.0e-14)));

        // a = centred window (zero elsewhere), b = whole frame; packed into one complex FFT.
        const int N = fft.size();
        for (int k = 0; k < N; ++k)
        {
            const float b = k < len ? frame[(size_t) k] : 0.0f;
            const float a = (k >= maxLag && k < maxLag + W) ? frame[(size_t) k] : 0.0f;
            spec[(size_t) k] = { a, b };
        }
        fft.forward (spec.data());
        for (int k = 0; k < N; ++k)
        {
            const auto zk = spec[(size_t) k];
            const auto zn = std::conj (spec[(size_t) ((N - k) & (N - 1))]);
            const auto A = 0.5f * (zk + zn);
            const auto B = std::complex<float> (0.0f, -0.5f) * (zk - zn);
            prod[(size_t) k] = std::conj (A) * B;
        }
        fft.inverse (prod.data()); // prod[lag mod N] = sum_k a_k b_{k+lag}

        diff[0] = 0.0f;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            const double fwd = ea + energy (maxLag + tau) - 2.0 * prod[(size_t) tau].real();
            const double bwd = ea + energy (maxLag - tau) - 2.0 * prod[(size_t) (N - tau)].real();
            diff[(size_t) tau] = (float) std::max (0.0, fwd + bwd);
        }
        diff[(size_t) maxLag + 1] = diff[(size_t) maxLag];

        cmnd[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            running += diff[(size_t) tau];
            cmnd[(size_t) tau] = running > 0.0 ? (float) ((double) diff[(size_t) tau] * tau / running) : 1.0f;
        }

        // Local minima of the CMNDF in lag order.
        minima.clear();
        int globalMin = minLag;
        for (int tau = std::max (2, minLag); tau < maxLag; ++tau)
        {
            if (cmnd[(size_t) tau] < cmnd[(size_t) globalMin])
                globalMin = tau;
            if (cmnd[(size_t) tau] < cmnd[(size_t) tau - 1] && cmnd[(size_t) tau] <= cmnd[(size_t) tau + 1])
                minima.push_back (tau);
        }

        // Each threshold votes for the first dip below it (prior-weighted).
        probs.assign (minima.size(), 0.0f);
        float fallback = 0.0f;
        const int numT = (int) prior.size();
        for (int i = 0; i < numT; ++i)
        {
            const float thr = (float) (i + 1) / (float) numT;
            // thresholds ascend, so the first qualifying dip index can only move earlier
            size_t found = minima.size();
            for (size_t m = 0; m < minima.size(); ++m)
                if (cmnd[(size_t) minima[m]] < thr) { found = m; break; }
            if (found < minima.size())
                probs[found] += prior[(size_t) i];
            else
                fallback += prior[(size_t) i];
        }

        for (size_t m = 0; m < minima.size(); ++m)
            if (probs[m] > 0.0f)
                out.push_back ({ refine (minima[m]), probs[m], true });

        if (fallback > 0.0f)
            out.push_back ({ refine (globalMin), fallback * 0.01f, false });
    }

private:
    float refine (int tau) const noexcept
    {
        if (tau <= 1 || tau >= maxLag)
            return (float) tau;
        const double a = diff[(size_t) tau - 1], b = diff[(size_t) tau], c = diff[(size_t) tau + 1];
        const double den = a - 2.0 * b + c;
        if (den <= 0.0)
            return (float) tau;
        const double delta = std::clamp (0.5 * (a - c) / den, -1.0, 1.0);
        return (float) (tau + delta);
    }

    int W, minLag, maxLag;
    Fft fft;
    const std::vector<float>& prior;
    std::vector<float> frame;
    std::vector<double> prefix;
    std::vector<std::complex<float>> spec, prod;
    std::vector<float> diff, cmnd;
    std::vector<int> minima;
    std::vector<float> probs;
};

/** Two-state voicing Viterbi. */
std::vector<bool> decodeVoicing (const std::vector<float>& pv, float switchProb)
{
    const size_t n = pv.size();
    std::vector<bool> voiced (n, false);
    if (n == 0)
        return voiced;

    const double stay = std::log (1.0 - switchProb), change = std::log (switchProb);
    std::vector<uint8_t> back (n * 2);
    double dU = std::log (1.0 - pv[0]), dV = std::log (pv[0]);
    for (size_t i = 1; i < n; ++i)
    {
        const double oV = std::log (pv[i]), oU = std::log (1.0 - pv[i]);
        const double uFromU = dU + stay, uFromV = dV + change;
        const double vFromV = dV + stay, vFromU = dU + change;
        back[i * 2 + 0] = uFromV > uFromU ? 1 : 0;
        back[i * 2 + 1] = vFromU > vFromV ? 0 : 1;
        dU = std::max (uFromU, uFromV) + oU;
        dV = std::max (vFromV, vFromU) + oV;
    }
    uint8_t s = dV > dU ? 1 : 0;
    for (size_t i = n; i-- > 0;)
    {
        voiced[i] = s == 1;
        if (i > 0)
            s = back[i * 2 + s];
    }
    return voiced;
}
} // namespace

PitchTrack runPYin (const float* x, size_t numSamples, double sr, const PYinParams& p, const JobControl& job)
{
    PitchTrack track;
    track.sampleRate = sr;
    track.hop = std::max (1, (int) std::lround (sr * p.hopSeconds));
    const size_t numFrames = numSamples / (size_t) track.hop + 1;
    track.frames.resize (numFrames);

    const int maxLag = (int) std::ceil (sr / p.minHz) + 1;
    const int minLag = std::max (2, (int) std::floor (sr / p.maxHz));
    const int window = maxLag;
    const auto prior = betaPrior (p.numThresholds, p.betaA, p.betaB);

    std::vector<std::vector<Candidate>> candidates (numFrames);
    std::atomic<size_t> done { 0 };

    parallelFor (numFrames, [&] (size_t begin, size_t end)
    {
        FrameAnalyser fa (window, minLag, maxLag, prior);
        for (size_t i = begin; i < end; ++i)
        {
            if (job.cancelled())
                return;
            float rms = -120.0f;
            fa.analyse (x, (int64_t) numSamples, (int64_t) i * track.hop, candidates[i], rms);
            track.frames[i].rmsDb = rms;
        }
        const auto d = done.fetch_add (end - begin) + (end - begin);
        job.progress (0.8f * (float) d / (float) numFrames);
    });

    if (job.cancelled())
        return track;

    // Voicing decision.
    std::vector<float> pv (numFrames);
    for (size_t i = 0; i < numFrames; ++i)
    {
        float v = 0.0f;
        for (const auto& c : candidates[i])
            if (c.fromDip)
                v += c.prob;
        if (track.frames[i].rmsDb < p.silenceDb)
            v = 0.0f;
        track.frames[i].voicedProb = std::clamp (v, 0.0f, 1.0f);
        pv[i] = std::clamp (v, 0.005f, 0.995f);
    }
    const auto voiced = decodeVoicing (pv, p.voicingSwitchProb);

    // Pitch Viterbi inside each voiced run.
    const float midiMin = hzToMidi ((float) p.minHz), midiMax = hzToMidi ((float) p.maxHz);
    const float binsPerSemi = 100.0f / p.binCents;
    const int numBins = (int) std::ceil ((midiMax - midiMin) * binsPerSemi) + 1;
    const int maxJump = std::max (1, (int) std::lround (p.maxJumpSemitones * binsPerSemi));

    std::vector<float> logTrans ((size_t) maxJump + 1);
    {
        double sum = 0.0;
        for (int d = -maxJump; d <= maxJump; ++d)
            sum += maxJump + 1 - std::abs (d);
        for (int d = 0; d <= maxJump; ++d)
            logTrans[(size_t) d] = (float) std::log ((maxJump + 1 - d) / sum);
    }

    auto candidateMidi = [&] (const Candidate& c) { return hzToMidi ((float) (sr / c.tau)); };
    auto binOf = [&] (float midi) { return std::clamp ((int) std::lround ((midi - midiMin) * binsPerSemi), 0, numBins - 1); };

    std::vector<float> prev ((size_t) numBins), cur ((size_t) numBins), obs ((size_t) numBins);
    std::vector<int16_t> back;

    size_t i = 0;
    while (i < numFrames)
    {
        if (! voiced[i]) { ++i; continue; }
        size_t j = i;
        while (j < numFrames && voiced[j])
            ++j;

        const size_t len = j - i;
        back.assign (len * (size_t) numBins, 0);

        auto fillObs = [&] (size_t frame)
        {
            std::fill (obs.begin(), obs.end(), 1.0e-4f);
            for (const auto& c : candidates[frame])
            {
                const int b = binOf (candidateMidi (c));
                obs[(size_t) b] += c.prob;
                if (b > 0) obs[(size_t) b - 1] += 0.3f * c.prob;
                if (b + 1 < numBins) obs[(size_t) b + 1] += 0.3f * c.prob;
            }
            for (auto& o : obs)
                o = std::log (o);
        };

        fillObs (i);
        prev = obs;
        for (size_t k = 1; k < len; ++k)
        {
            fillObs (i + k);
            auto* bk = back.data() + k * (size_t) numBins;
            for (int b = 0; b < numBins; ++b)
            {
                float best = -std::numeric_limits<float>::infinity();
                int arg = b;
                const int lo = std::max (0, b - maxJump), hi = std::min (numBins - 1, b + maxJump);
                for (int q = lo; q <= hi; ++q)
                {
                    const float v = prev[(size_t) q] + logTrans[(size_t) std::abs (q - b)];
                    if (v > best) { best = v; arg = q; }
                }
                cur[(size_t) b] = best + obs[(size_t) b];
                bk[b] = (int16_t) arg;
            }
            std::swap (prev, cur);
        }

        int s = (int) (std::max_element (prev.begin(), prev.end()) - prev.begin());
        for (size_t k = len; k-- > 0;)
        {
            const size_t f = i + k;
            auto& fr = track.frames[f];
            const float binMidi = midiMin + (float) s / binsPerSemi;
            float best = 1.0e9f, midi = binMidi;
            for (const auto& c : candidates[f])
            {
                const float m = candidateMidi (c);
                const float dist = std::abs (m - binMidi) * binsPerSemi;
                if (dist <= 3.0f && dist < best) { best = dist; midi = m; }
            }
            fr.voiced = true;
            fr.midi = midi;
            fr.f0Hz = midiToHz (midi);
            if (k > 0)
                s = back[k * (size_t) numBins + (size_t) s];
        }
        i = j;
        job.progress (0.8f + 0.2f * (float) i / (float) numFrames);
    }

    job.progress (1.0f);
    return track;
}
} // namespace tone
