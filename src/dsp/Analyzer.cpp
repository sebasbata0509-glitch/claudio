#include "Analyzer.h"

namespace nsw
{

void Analyzer::prepare (double sampleRate, int channels, int lookbackSamples)
{
    fs = sampleRate;
    numChannels = std::clamp (channels, 1, kMaxChannels);
    det.prepare (fs);
    for (auto& b : in)
        b.allocate (lookbackSamples);
    lpSignal.allocate (lookbackSamples);
    maxPeriod = det.getMaxPeriodSamples();
    reset();
}

void Analyzer::setRange (DetectionRange r)
{
    if (r == det.getRange())
        return;
    det.setRange (r);
    maxPeriod = det.getMaxPeriodSamples();
    reset();
}

void Analyzer::reset()
{
    det.reset();
    for (auto& b : in)
        b.clear();
    lpSignal.clear();
    markLp1.reset();
    markLp2.reset();
    markLpFreq = 0.0f;
    markLp1.setLowpass (fs, 400.0, 0.707);
    markLp2.setLowpass (fs, 400.0, 0.707);
    historyCount = 0;
    markCount = 0;
    tracking = false;
    current = -1;
}

void Analyzer::push (const float* frame) noexcept
{
    ++current;
    float mono = 0.0f;
    for (int c = 0; c < numChannels; ++c)
    {
        in[(size_t) c].at (current) = frame[c];
        mono += frame[c];
    }
    mono /= (float) numChannels;

    lpSignal.at (current) = markLp2.process (markLp1.process (mono));

    if (det.process (mono))
    {
        const auto& e = det.latest();
        history[(size_t) (historyCount % kHistory)] = e;
        ++historyCount;

        if (e.voiced)
        {
            // Keep the peak-picking low-pass just above the fundamental so each
            // cycle shows one dominant, stable peak.
            const float target = std::clamp (e.frequency * 1.5f, 80.0f, 1600.0f);
            if (std::abs (target - markLpFreq) > markLpFreq * 0.08f)
            {
                markLpFreq = target;
                markLp1.setLowpass (fs, target, 0.6);
                markLp2.setLowpass (fs, target, 0.6);
            }
        }
    }

    trackMarks();
}

AnalysisState Analyzer::stateAt (int64_t t) const noexcept
{
    AnalysisState s;
    if (historyCount == 0)
    {
        s.period = (float) (fs / 200.0);
        return s;
    }

    const int64_t newest = historyCount - 1;
    const int64_t oldest = std::max<int64_t> (0, historyCount - kHistory);
    auto entry = [this] (int64_t k) -> const PitchEstimate& { return history[(size_t) (k % kHistory)]; };

    if (t >= entry (newest).time)
    {
        const auto& e = entry (newest);
        return { e.voiced, std::min (e.period, (float) maxPeriod), e.confidence };
    }

    int64_t k = newest;
    while (k > oldest && entry (k).time > t)
        --k;

    const auto& a = entry (k);
    if (k == newest || a.time > t)
        return { a.voiced, std::min (a.period, (float) maxPeriod), a.confidence };

    const auto& b = entry (k + 1);
    const float frac = (float) (t - a.time) / (float) std::max<int64_t> (1, b.time - a.time);
    s.voiced = frac < 0.5f ? a.voiced : b.voiced;
    s.confidence = a.confidence + frac * (b.confidence - a.confidence);
    if (a.voiced && b.voiced)
        s.period = a.period + frac * (b.period - a.period);
    else
        s.period = frac < 0.5f ? a.period : b.period;
    s.period = std::min (s.period, (float) maxPeriod);
    return s;
}

int64_t Analyzer::peakIn (int64_t from, int64_t to) const noexcept
{
    from = std::max (from, current - (int64_t) lpSignal.size() + 4);
    int64_t best = from;
    float bestVal = lpSignal.at (from);
    for (int64_t i = from + 1; i <= to; ++i)
    {
        const float v = lpSignal.at (i);
        if (v > bestVal)
        {
            bestVal = v;
            best = i;
        }
    }
    return best;
}

void Analyzer::addMark (int64_t pos, float period) noexcept
{
    marks[(size_t) (markCount & (kMaxMarks - 1))] = { pos, period };
    ++markCount;
}

void Analyzer::trackMarks() noexcept
{
    if (historyCount == 0)
        return;

    if (! tracking)
    {
        const auto& e = history[(size_t) ((historyCount - 1) % kHistory)];
        if (! e.voiced)
            return;
        const auto p = (int64_t) std::lround (std::min (e.period, (float) maxPeriod));
        if (current < p + 2)
            return;
        // Onset: anchor on the strongest peak of the most recent cycle, but never
        // before the previous mark (keeps marks monotonic across short gaps).
        int64_t from = current - p + 1;
        if (markCount > 0)
            from = std::max (from, mark (markCount - 1).pos + p / 2);
        if (from >= current)
            return;
        addMark (peakIn (from, current), e.period);
        tracking = true;
        return;
    }

    const auto& last = mark (markCount - 1);
    const float period = stateAt (last.pos + (int64_t) last.period).period;
    const auto predicted = last.pos + (int64_t) std::lround (period);
    const auto radius = (int64_t) std::lround (period * 0.3f);

    if (current < predicted + radius)
        return;

    const auto s = stateAt (predicted);
    if (! s.voiced)
    {
        tracking = false;
        return;
    }

    const int64_t lo = std::max (predicted - radius, last.pos + (int64_t) (period * 0.5f));
    addMark (peakIn (lo, predicted + radius), s.period);
}

int64_t Analyzer::findMark (double target, int64_t latestAllowed, float period) const noexcept
{
    int64_t best = -1;
    double bestDist = 1.0e30;
    const int64_t oldest = oldestMark();
    for (int64_t k = markCount - 1; k >= oldest; --k)
    {
        const auto pos = mark (k).pos;
        if (pos > latestAllowed)
            continue;
        const double d = std::abs ((double) pos - target);
        if (d < bestDist)
        {
            bestDist = d;
            best = k;
        }
        if ((double) pos < target - 2.0 * period)
            break;
    }
    return best;
}

} // namespace nsw
