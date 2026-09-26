#pragma once

#include "PitchDetector.h"

#include <array>

namespace nsw
{

struct PitchMark
{
    int64_t pos = 0;     // absolute input sample index (glottal-cycle anchor)
    float period = 0.0f; // local period in samples
};

struct AnalysisState
{
    bool voiced = false;
    float period = 0.0f;
    float confidence = 0.0f;
};

/**
    Input side of the shifter, shared by every synthesis voice and channel:

    - stores the raw input of each channel (grains are read from here, and the
      dry path is read from here with the latency delay)
    - runs the YIN detector on the mono sum and keeps a short history of
      estimates so any past instant can be queried
    - places pitch marks (one per glottal cycle) by predicting the next cycle
      from the detected period and snapping to the peak of a low-passed copy
      of the signal. Marks are shared across channels so the stereo image
      stays coherent.
*/
class Analyzer
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate, int numChannels, int lookbackSamples);
    void setRange (DetectionRange r);
    void reset();

    /** Push one frame. `frame` holds numChannels samples. */
    void push (const float* frame) noexcept;

    /** Absolute index of the most recently pushed sample. */
    int64_t now() const noexcept { return current; }

    const RingBuffer& input (int ch) const noexcept { return in[(size_t) std::min (ch, numChannels - 1)]; }
    const PitchDetector& detector() const noexcept { return det; }

    /** Detector state at an absolute time (interpolated, clamped to the known history). */
    AnalysisState stateAt (int64_t t) const noexcept;

    /** Returns the id of the mark nearest to `target` among marks with pos <= latestAllowed, or -1. */
    int64_t findMark (double target, int64_t latestAllowed, float period) const noexcept;
    bool isValidMark (int64_t id) const noexcept { return id >= 0 && id < markCount && id >= markCount - kMaxMarks; }
    int64_t oldestMark() const noexcept { return std::max<int64_t> (0, markCount - kMaxMarks); }
    const PitchMark& mark (int64_t id) const noexcept { return marks[(size_t) (id & (kMaxMarks - 1))]; }

    int getMaxPeriod() const noexcept { return maxPeriod; }

private:
    void trackMarks() noexcept;
    int64_t peakIn (int64_t from, int64_t to) const noexcept;
    void addMark (int64_t pos, float period) noexcept;

    static constexpr int kHistory = 256;
    static constexpr int kMaxMarks = 1024;

    double fs = 48000.0;
    int numChannels = 1;
    int maxPeriod = 480;
    PitchDetector det;
    std::array<RingBuffer, kMaxChannels> in;
    RingBuffer lpSignal;
    Biquad markLp1, markLp2;
    float markLpFreq = 0.0f;

    std::array<PitchEstimate, kHistory> history {};
    int64_t historyCount = 0;

    std::array<PitchMark, kMaxMarks> marks {};
    int64_t markCount = 0;
    bool tracking = false;

    int64_t current = -1;
};

} // namespace nsw
