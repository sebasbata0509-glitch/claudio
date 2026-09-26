#pragma once

#include <array>
#include <cstdint>

namespace nsw
{

/** Parameters a modulator can drive. */
enum class ModDest
{
    None = 0,
    Pitch,
    Formant,
    Harmonics,
    Alternator,
    FM,
    Ratio,
    Smear,
    Stereo,
    Detune,
    Mix,
    Output,
    Count
};

constexpr int kNumModDests = (int) ModDest::Count - 1; // excluding None
inline const char* modDestName (ModDest d)
{
    static const char* names[] = { "Off",   "Pitch",  "Formant", "Harmonics", "Alternator", "FM",
                                   "Ratio", "Smear",  "Stereo",  "Detune",    "Mix",        "Output" };
    return names[(int) d];
}

enum class SeqRate
{
    Quarter,
    Eighth,
    Sixteenth,
    ThirtySecond,
    EighthTriplet,
    SixteenthTriplet
};

inline double seqRateBeats (SeqRate r)
{
    switch (r)
    {
        case SeqRate::Quarter:          return 1.0;
        case SeqRate::Eighth:           return 0.5;
        case SeqRate::Sixteenth:        return 0.25;
        case SeqRate::ThirtySecond:     return 0.125;
        case SeqRate::EighthTriplet:    return 1.0 / 3.0;
        case SeqRate::SixteenthTriplet: return 1.0 / 6.0;
    }
    return 0.25;
}

enum class EnvTrigger
{
    Midi,
    InputLevel
};

struct ModParams
{
    // Macro: one knob, each destination has its own bipolar depth (-1..1 of that destination's span).
    float macro = 0.0f; // 0..1
    std::array<float, kNumModDests> macroDepth {};

    // 16-step sequencer, tempo synced, per-step value (-1..1) and glide (0..1).
    bool seqOn = false;
    SeqRate seqRate = SeqRate::Sixteenth;
    bool seqToFormant = false; // false: Pitch, true: Formant
    float seqDepth = 12.0f;    // semitones at step value 1
    int seqLength = 16;
    std::array<float, 16> seqValue {};
    std::array<float, 16> seqGlide {};

    // ADSR.
    ModDest envDest = ModDest::None;
    float envDepth = 0.5f; // -1..1 of the destination span
    float attackMs = 10.0f, decayMs = 250.0f, sustain = 0.6f, releaseMs = 300.0f;
    EnvTrigger envTrigger = EnvTrigger::Midi;
    float envThresholdDb = -35.0f;
};

struct Transport
{
    bool hasTempo = false;
    double bpm = 120.0;
    bool isPlaying = false;
    bool hasPosition = false;
    double ppqAtBlockStart = 0.0;
};

/** Full span a depth of 1.0 moves each destination by (in that destination's units). */
float modSpan (ModDest d);

/** Sequencer + ADSR state, advanced at control rate by the engine. */
class Modulators
{
public:
    void prepare (double sampleRate);
    void reset();

    /** Advance by `samples`. `ppq` < 0 means free-running at `bpm`. */
    void advance (const ModParams& m, int samples, double ppq, double bpm, bool gate) noexcept;

    /** Sequencer output in semitones (already scaled by depth), 0 when off. */
    float sequencerSemis() const noexcept { return seqOut; }
    int currentStep() const noexcept { return step; }
    float envelope() const noexcept { return env; }

    /** Feed input level (linear peak of the last control period) for level triggering. */
    void setInputLevel (float peak) noexcept { inputPeak = peak; }
    bool levelGate (float thresholdDb) noexcept;
    void retrigger() noexcept { retrig = true; }

private:
    double fs = 48000.0;
    double freePpq = 0.0;
    float seqOut = 0.0f;
    int step = 0;

    enum class Stage { Idle, Attack, Decay, Sustain, Release } stage = Stage::Idle;
    float env = 0.0f;
    bool lastGate = false, retrig = false;
    float follower = 0.0f, inputPeak = 0.0f;
    bool levelOpen = false;
};

} // namespace nsw
