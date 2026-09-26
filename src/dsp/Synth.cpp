#include "Synth.h"

namespace nsw
{

namespace
{
    const HannTable& hann()
    {
        static const HannTable table;
        return table;
    }
} // namespace

void SynthChannel::prepare (int accumulatorSize, uint32_t seed)
{
    acc.allocate (accumulatorSize);
    rng.seed (seed);
    (void) hann(); // build the table off the audio thread
}

void SynthChannel::reset (int64_t now, const SynthTiming& timing)
{
    acc.clear();
    nextS = (double) (now + timing.maxHalf);
    grainCount = 0;
    lastOutPeriod = (float) timing.maxPeriod * 0.5f;
    lastVoiced = false;
}

float SynthChannel::process (int64_t n, const Analyzer& an, const RingBuffer& src, const GrainSettings& gs,
                             const SynthTiming& timing, bool secondChannel) noexcept
{
    // If we fell behind (e.g. after a reset), jump forward rather than bursting grains.
    if (nextS < (double) n - timing.maxHalf)
        nextS = (double) (n + timing.maxHalf);

    while ((double) n >= nextS - timing.maxHalf)
        emit (n, an, src, gs, timing, secondChannel);

    float& slot = acc.at (n);
    const float y = slot;
    slot = 0.0f;
    return y;
}

void SynthChannel::emit (int64_t n, const Analyzer& an, const RingBuffer& src, const GrainSettings& gs,
                         const SynthTiming& timing, bool secondChannel) noexcept
{
    const double target = nextS - timing.latency;           // matching input instant
    const auto state = an.stateAt ((int64_t) target);
    const float f = std::clamp (gs.formant, 0.5f, 2.0f);
    const float maxP = (float) timing.maxPeriod;
    float hop = 0.0f;
    bool voicedGrain = false;

    if (state.voiced)
    {
        const float p = std::clamp (state.period, 8.0f, maxP);
        const float srcHalf = std::min (p, 1.25f * p * f);
        const int64_t latestAllowed = n - (int64_t) std::ceil (srcHalf) - 3;
        int64_t k = an.findMark (target, latestAllowed, p);

        if (k >= 0 && std::abs ((double) an.mark (k).pos - target) < 2.5 * p)
        {
            // Smear / stereo: reach back to earlier cycles for some grains.
            int back = 0;
            if (gs.smear > 0.0f && rng.uniform() < gs.smear)
                back += 1 + (int) (rng.uniform() * gs.smear * 3.0f);
            if (secondChannel && gs.stereo > 0.0f && rng.uniform() < gs.stereo * 0.5f)
                back += 1;
            k = std::max (k - back, an.oldestMark());

            const auto& m = an.mark (k);
            // By now the detector history covers this mark, so use the interpolated
            // estimate at the mark rather than the (older) one stored when it was placed.
            const float pa = std::clamp (an.stateAt (m.pos).period, 8.0f, maxP);

            // Pitch: blend of "follow" (fixed transposition) and "MIDI target" semitones.
            float semis = gs.shiftSemis;
            if (gs.midiWeight > 0.0f)
            {
                const float detected = frequencyToMidi ((float) timing.sampleRate / pa);
                semis += gs.midiWeight * ((gs.targetNote - detected) - gs.shiftSemis);
            }
            semis = std::clamp (semis + gs.detuneSemis, -48.0f, 48.0f);
            const float basePeriod = std::max (2.0f, pa / semitonesToRatio (semis));

            float outPeriod = basePeriod;
            if (gs.alternator > 0.0f && (grainCount & 1u))
                outPeriod *= 1.0f + gs.alternator; // every other cycle an octave lower at 100%

            float halfOut = std::min (pa / f, 1.25f * pa);
            if (gs.smear > 0.0f)
                halfOut *= 1.0f - 0.6f * gs.smear * rng.uniform();
            halfOut = std::max (halfOut, 4.0f);

            double centre = nextS;
            if (secondChannel && gs.stereo > 0.0f)
                centre += gs.stereo * (0.35f + 0.25f * rng.uniform()) * std::min (basePeriod, (float) timing.maxPeriod);

            // Energy-preserving gain for the grain density (sqrt(hop / half-length)).
            const float gain = std::min (3.0f, std::sqrt (outPeriod / halfOut)) * gs.voicedGain;
            writeGrain (n, centre, (double) m.pos, halfOut, f, gain, src);

            hop = outPeriod;
            if (gs.smear > 0.0f)
                hop *= 1.0f + 0.3f * gs.smear * rng.bipolar();
            lastOutPeriod = basePeriod;
            voicedGrain = true;
        }
    }

    if (! voicedGrain)
    {
        const auto halfOut = (float) timing.unvoicedHalf;
        double srcCentre = std::min (target, (double) n - halfOut * f - 3.0);
        if (gs.smear > 0.0f)
            srcCentre -= rng.uniform() * gs.smear * halfOut * 4.0f;
        double centre = nextS;
        if (secondChannel && gs.stereo > 0.0f)
        {
            srcCentre -= rng.uniform() * gs.stereo * halfOut;
            centre += gs.stereo * 0.5f * halfOut;
        }
        writeGrain (n, centre, srcCentre, halfOut, f, gs.unvoicedGain, src);
        hop = halfOut;
    }

    lastVoiced = voicedGrain;
    nextS += std::max (2.0f, hop);
    ++grainCount;
}

void SynthChannel::writeGrain (int64_t n, double centre, double srcCentre, float halfOut, float formant,
                               float gain, const RingBuffer& src) noexcept
{
    const auto& w = hann();
    const auto start = std::max (n, (int64_t) std::ceil (centre - halfOut));
    const auto end = std::min ((int64_t) std::floor (centre + halfOut), n + acc.size() - 1);
    const float inv = 1.0f / halfOut;
    for (int64_t m = start; m <= end; ++m)
    {
        const double k = (double) m - centre;
        const float win = w ((float) k * inv);
        if (win > 0.0f)
            acc.at (m) += gain * win * src.read (srcCentre + k * formant);
    }
}

//==============================================================================
void PostStage::prepare (double sampleRate, int maxPeriod, int fmBaseDelay)
{
    fs = sampleRate;
    fmBase = fmBaseDelay;
    combLine.allocate (8 * maxPeriod + 16);
    fmLine.allocate (2 * fmBase + 16);
    combDelay.prepare (fs, 15.0);
    combGain.prepare (fs, 20.0);
    fmDepth.prepare (fs, 20.0);
    fmFreq.prepare (fs, 15.0);
    reset();
}

void PostStage::reset()
{
    combLine.clear();
    fmLine.clear();
    t = 0;
    combDelay.reset (50.0f);
    combGain.reset (0.0f);
    fmDepth.reset (0.0f);
    fmFreq.reset (200.0f);
    fmPhase = 0.0;
}

float PostStage::process (float x, float outPeriod, bool voiced, float harmonics, float fmAmount,
                          float fmRatio) noexcept
{
    ++t;

    // Harmonics tilt.
    combLine.at (t) = x;
    combDelay.setTarget (std::clamp (outPeriod * 0.5f, 1.0f, (float) combLine.size() - 8.0f));
    combGain.setTarget (voiced ? -0.9f * std::clamp (harmonics, -1.0f, 1.0f) : 0.0f);
    const float d = combDelay.next();
    const float g = combGain.next();
    float y = x;
    if (std::abs (g) > 1.0e-5f)
        y = (x + g * combLine.read ((double) t - d)) / std::sqrt (1.0f + g * g);

    // FM (as phase modulation of the resynthesised voice).
    fmLine.at (t) = y;
    const float f0 = (float) fs / std::max (outPeriod, 2.0f);
    const float fmHz = std::clamp (fmRatio * f0, 1.0f, 0.45f * (float) fs);
    fmFreq.setTarget (fmHz);
    const float index = 4.0f * std::clamp (fmAmount, 0.0f, 1.0f) * (voiced ? 1.0f : 0.0f);
    const float depthByIndex = index * (float) fs / (float) (kTwoPi * f0);
    const float depthByRate = 2.0f * (float) fs / (float) (kTwoPi * fmHz); // keep |d delay/dt| <= 2
    fmDepth.setTarget (std::min ({ depthByIndex, depthByRate, (float) fmBase - 2.0f }));
    const float depth = fmDepth.next();
    const float freq = fmFreq.next();
    fmPhase += kTwoPi * freq / fs;
    if (fmPhase > kTwoPi)
        fmPhase -= kTwoPi;

    if (depth < 1.0e-4f)
        return fmLine.at (t - fmBase);
    return fmLine.read ((double) t - fmBase - depth * std::sin (fmPhase));
}

} // namespace nsw
