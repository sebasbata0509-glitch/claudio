#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

/*  Nebula parameter system.

    Every parameter of every phase is declared here, up front, so parameter IDs
    (and therefore host automation and saved projects) stay stable while the
    DSP is filled in. IDs are versioned with juce::ParameterID { id, 1 }.

    Rules:
      - Never rename or remove an ID once released.
      - Only append to choice lists (presets store the choice index).
*/
namespace nebula
{
constexpr int kNumLayers   = 4;
constexpr int kNumLfos     = 4;
constexpr int kNumModEnvs  = 3;
constexpr int kNumMacros   = 4;
constexpr int kNumModSlots = 16;
constexpr int kMaxVoices   = 16;
constexpr int kMaxUnison   = 16;
constexpr int kMaxPartials = 256;

//==============================================================================
// Enumerations (order == choice index; append only)
enum class FilterType   { LP12, LP24, BP12, BP24, HP12, HP24 };
enum class Wavetable    { Saw, Square, Vocal, Glass, Metallic };
enum class GranularMode { Granular, Texture, Stretch };
enum class NoiseType    { White, Pink, Vinyl, Wind };
enum class LfoShape     { Sine, Triangle, Saw, Square, Random, SmoothRandom };
enum class SpectralMode { Gate, Freeze, Smear, Mp3ify };
enum class ChorusMode   { Chorus, Ensemble };
enum class ReverbMode   { Normal, BigStereo, Reverse };

/** Effects in the reorderable global chain. The compressor/limiter is always last. */
enum class FxSlot { Spectral, Resonator, GrainDelay, Vocoder, Chorus, Shimmer };
constexpr int kNumFxSlots = 6;

const juce::StringArray& filterTypeNames();
const juce::StringArray& wavetableNames();
const juce::StringArray& granularModeNames();
const juce::StringArray& noiseTypeNames();
const juce::StringArray& lfoShapeNames();
const juce::StringArray& spectralModeNames();
const juce::StringArray& chorusModeNames();
const juce::StringArray& reverbModeNames();
const juce::StringArray& fxSlotNames();
const juce::StringArray& syncDivisionNames();
const juce::StringArray& layerNames();
const juce::StringArray& macroNames();

/** Length of a tempo-sync division in quarter notes (beats). */
double syncDivisionBeats (int index) noexcept;

//==============================================================================
// Modulation matrix vocabulary
const juce::StringArray& modSourceNames();

enum class ModSource
{
    None,
    Lfo1, Lfo2, Lfo3, Lfo4,
    Env1, Env2, Env3,
    Velocity, ModWheel, Aftertouch, Note, Random,
    Macro1, Macro2, Macro3, Macro4
};

struct ModDestination
{
    const char* name;     // shown in the UI
    const char* paramId;  // parameter the modulation is added to (normalised domain)
};

/** Index 0 is "None". Append only. */
const std::vector<ModDestination>& modDestinations();
const juce::StringArray& modDestinationNames();

//==============================================================================
// Parameter IDs
namespace ids
{
    /** Per-layer ID, e.g. layer (0, "vol") == "l1_vol". */
    juce::String layer (int layerIndex, const char* name);
    juce::String lfo (int lfoIndex, const char* name);      // "lfo1_rate"
    juce::String modEnv (int envIndex, const char* name);   // "env1_attack"
    juce::String modSlot (int slotIndex, const char* name); // "mm1_src"
    juce::String macro (int macroIndex);                    // "macro1"

    // Global
    inline constexpr const char* masterGain  = "master_gain";
    inline constexpr const char* masterWidth = "master_width";
    inline constexpr const char* bendRange   = "bend_range";
    inline constexpr const char* glide       = "glide";
} // namespace ids

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace nebula
