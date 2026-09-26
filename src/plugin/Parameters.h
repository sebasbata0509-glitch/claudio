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
};

/** Write engine-level values into the parameters (used for factory presets). */
void applyParams (juce::AudioProcessorValueTreeState& state, const Params& p);
} // namespace nsw
