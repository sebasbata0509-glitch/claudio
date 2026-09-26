#pragma once

#include "DspUtil.h"

namespace nsw
{

enum class DetectionRange
{
    Low,  // ~50 - 500 Hz
    High  // ~100 - 1000 Hz
};

struct PitchEstimate
{
    int64_t time = 0;        // absolute input sample index the estimate refers to (window centre)
    float period = 0.0f;     // in input-rate samples; valid even when unvoiced (last best guess)
    float frequency = 0.0f;  // Hz
    float confidence = 0.0f; // 1 - YIN aperiodicity, 0..1
    float levelDb = -120.0f; // RMS of the analysis window
    bool voiced = false;
};

/**
    Real-time YIN pitch detector.

    The mono input is band-limited and decimated to ~16-22 kHz, where the YIN
    cumulative-mean-normalised difference function is evaluated over the full
    lag range. The winning lag is then refined at the full sample rate with a
    handful of difference-function evaluations and parabolic interpolation, so
    accuracy is sub-cent for clean tones while the cost stays flat across
    44.1 - 192 kHz.

    Voicing uses hysteresis on the aperiodicity plus a level gate, so breaths
    and fricatives are reported as unvoiced instead of producing random pitches.

    Allocation happens only in prepare(); process() is real-time safe.
*/
class PitchDetector
{
public:
    void prepare (double sampleRate);
    void setRange (DetectionRange newRange);
    DetectionRange getRange() const noexcept { return range; }
    void reset();

    /** Push one input sample. Returns true when a new estimate is available. */
    bool process (float x) noexcept;

    const PitchEstimate& latest() const noexcept { return estimate; }
    int64_t getSampleCount() const noexcept { return sampleCount; }

    float getMinFrequency() const noexcept { return range == DetectionRange::Low ? 50.0f : 100.0f; }
    float getMaxFrequency() const noexcept { return range == DetectionRange::Low ? 500.0f : 1000.0f; }

    /** Longest period the current range can report, in input samples. */
    int getMaxPeriodSamples() const noexcept { return (int) std::ceil (fs / getMinFrequency()); }

    /** Samples between successive estimates (input rate). */
    int getHopSamples() const noexcept { return hopDecimated * decimation; }

    /** CMND threshold used for picking the lag (classic YIN absolute threshold). */
    void setYinThreshold (float t) noexcept { yinThreshold = t; }
    /** Voicing hysteresis on aperiodicity: become voiced below `on`, unvoiced above `off`. */
    void setVoicingThresholds (float on, float off) noexcept { voicedOn = on; voicedOff = off; }
    void setGateDb (float db) noexcept { gateDb = db; }

private:
    void configure();
    void analyse() noexcept;
    float refineAtFullRate (float decimatedLag) const noexcept;

    double fs = 48000.0;
    int decimation = 1;
    double decimatedRate = 48000.0;
    DetectionRange range = DetectionRange::High;

    // Filters: DC blocker + 4th-order Butterworth anti-alias low-pass.
    Biquad hp, lp1, lp2;

    // Contiguous-read circular buffers (each sample written twice).
    std::vector<float> fullBuf, decBuf;
    int fullLen = 0, decLen = 0;
    int fullWrite = 0, decWrite = 0;
    int decimPhase = 0;
    int hopCounter = 0;

    int window = 0;         // YIN integration window (decimated samples)
    int tauMin = 2, tauMax = 2;
    int hopDecimated = 64;
    std::vector<float> diff, cmnd;

    float yinThreshold = 0.15f;
    float voicedOn = 0.25f, voicedOff = 0.40f;
    float gateDb = -60.0f;

    int64_t sampleCount = 0;
    PitchEstimate estimate;
    float lastVoicedLag = 0.0f; // decimated lag of last voiced estimate (octave-jump guard)
};

} // namespace nsw
