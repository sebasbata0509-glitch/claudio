#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Engine.h"

namespace nsw::ids
{
// Core
inline constexpr auto pitch = "pitch";
inline constexpr auto snap = "snap";
inline constexpr auto octave = "octave";
inline constexpr auto formant = "formant";
inline constexpr auto harmonics = "harmonics";
inline constexpr auto alternator = "alternator";
inline constexpr auto fm = "fm";
inline constexpr auto ratio = "ratio";
inline constexpr auto glide = "glide";
inline constexpr auto smear = "smear";
inline constexpr auto stereo = "stereo";
inline constexpr auto detune = "detune";
inline constexpr auto mix = "mix";
inline constexpr auto output = "output";
inline constexpr auto mode = "mode";
inline constexpr auto range = "range";
inline constexpr auto velTarget = "velTarget";
inline constexpr auto velAmount = "velAmount";

// Modulation
inline constexpr auto macro = "macro";
inline constexpr auto seqOn = "seqOn";
inline constexpr auto seqRate = "seqRate";
inline constexpr auto seqDest = "seqDest";
inline constexpr auto seqDepth = "seqDepth";
inline constexpr auto seqLength = "seqLength";
inline constexpr auto envDest = "envDest";
inline constexpr auto envDepth = "envDepth";
inline constexpr auto envAttack = "envAttack";
inline constexpr auto envDecay = "envDecay";
inline constexpr auto envSustain = "envSustain";
inline constexpr auto envRelease = "envRelease";
inline constexpr auto envTrigger = "envTrigger";
inline constexpr auto envThreshold = "envThreshold";

/** "macroDepth_pitch", ... one per modulation destination. */
juce::String macroDepth (ModDest d);
/** "seqStep1".."seqStep16" and "seqGlide1".."seqGlide16". */
juce::String seqStep (int index);
juce::String seqGlide (int index);
} // namespace nsw::ids

namespace nsw
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Snapshot of the parameter values the engine needs (audio-thread safe). */
class ParameterReader
{
public:
    explicit ParameterReader (juce::AudioProcessorValueTreeState& state);
    Params read() const noexcept;

private:
    struct Raw
    {
        std::atomic<float>* p = nullptr;
        float get() const noexcept { return p->load (std::memory_order_relaxed); }
    };
    Raw pitch, snap, octave, formant, harmonics, alternator, fm, ratio, glide, smear, stereo, detune, mix, output,
        mode, range, velTarget, velAmount;
    Raw macro, seqOn, seqRate, seqDest, seqDepth, seqLength, envDest, envDepth, envAttack, envDecay, envSustain,
        envRelease, envTrigger, envThreshold;
    std::array<Raw, kNumModDests> macroDepth;
    std::array<Raw, 16> seqStep, seqGlide;
};

/** Write engine-level values into the parameters (used for factory presets). */
void applyParams (juce::AudioProcessorValueTreeState& state, const Params& p);
} // namespace nsw
