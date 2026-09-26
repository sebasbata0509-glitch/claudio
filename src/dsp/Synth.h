#pragma once

#include "Analyzer.h"

namespace nsw
{

/** Fixed timing of the shifter, derived from the detection range and sample rate. */
struct SynthTiming
{
    int maxPeriod = 480;     // longest analysis period (samples)
    int maxHalf = 600;       // longest output grain half-length (samples)
    int unvoicedHalf = 240;  // output half-length of unvoiced grains
    int latency = 1088;      // analysis -> synthesis delay (samples)
    double sampleRate = 48000.0;
};

/** Per-grain settings for one synthesis channel. */
struct GrainSettings
{
    float shiftSemis = 0.0f;  // fixed transposition ("follow" mode)
    float targetNote = 60.0f; // MIDI target note (absolute pitch)
    float midiWeight = 0.0f;  // 0 = follow transposition, 1 = lock to targetNote
    float detuneSemis = 0.0f; // per-channel detune, added on top
    float formant = 1.0f;     // formant ratio: >1 smaller/brighter, <1 larger/darker
    float alternator = 0.0f;  // 0..1
    float smear = 0.0f;       // 0..1
    float stereo = 0.0f;      // 0..1
    float voicedGain = 1.0f;
    float unvoicedGain = 1.0f;
};

/**
    TD-PSOLA synthesis for one output channel of one voice.

    Synthesis marks are spaced at the output period. For each one, the
    analysis mark nearest to the matching input instant (minus the fixed
    latency) is picked, a Hann-windowed two-period grain is cut around it and
    written into an overlap-add accumulator. Formants move independently of
    pitch by resampling the grain: reading the source at `formant` times the
    output rate compresses (brighter) or stretches (darker) the spectral
    envelope, while grain spacing alone sets the pitch.

    Unvoiced input (no reliable marks) falls back to time-aligned granular
    resynthesis at 50% overlap, which is an identity at formant = 1, so
    breaths and consonants pass through cleanly.
*/
class SynthChannel
{
public:
    void prepare (int accumulatorSize, uint32_t seed);
    void reset (int64_t now, const SynthTiming& timing);

    /** Produce output sample n. Call once per sample with increasing n. */
    float process (int64_t n, const Analyzer& an, const RingBuffer& src, const GrainSettings& gs,
                   const SynthTiming& timing, bool secondChannel) noexcept;

    /** Output period of the latest voiced grain (samples), and whether output is currently voiced. */
    float outputPeriod() const noexcept { return lastOutPeriod; }
    bool outputVoiced() const noexcept { return lastVoiced; }

private:
    void emit (int64_t n, const Analyzer& an, const RingBuffer& src, const GrainSettings& gs,
               const SynthTiming& timing, bool secondChannel) noexcept;
    void writeGrain (int64_t n, double centre, double srcCentre, float halfOut, float formant, float gain,
                     const RingBuffer& src) noexcept;

    RingBuffer acc;
    Random rng;
    double nextS = 0.0;
    uint64_t grainCount = 0;
    float lastOutPeriod = 100.0f;
    bool lastVoiced = false;
};

/**
    Per-voice post stage on the resynthesised signal:
    - Harmonics: feed-forward comb at half the output period. At harmonic k
      the delayed copy has phase (-1)^k, so the sign of the gain tilts the
      balance between odd and even harmonics.
    - FM: phase modulation by a modulated delay whose modulator runs at
      `ratio` x the output fundamental. A fixed base delay keeps it causal and
      is included in the reported latency.
*/
class PostStage
{
public:
    void prepare (double sampleRate, int maxPeriod, int fmBaseDelay);
    void reset();
    float process (float x, float outPeriod, bool voiced, float harmonics, float fmAmount, float fmRatio) noexcept;

private:
    double fs = 48000.0;
    int fmBase = 72;
    RingBuffer combLine, fmLine;
    int64_t t = 0;
    Smoother combDelay, combGain, fmDepth, fmFreq;
    double fmPhase = 0.0;
};

} // namespace nsw
