#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "params/Parameters.h"

class NebulaProcessor final : public juce::AudioProcessor
{
public:
    NebulaProcessor();
    ~NebulaProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nebula"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    /** Non-parameter state (sample path, FX order, UI size). Message thread only. */
    juce::ValueTree getExtraState() { return apvts.state.getOrCreateChildWithName (extraStateId, nullptr); }

    /** Current FX chain order as FxSlot indices. Safe to call from any thread. */
    std::array<int, nebula::kNumFxSlots> getFxOrder() const noexcept;
    void setFxOrder (const std::array<int, nebula::kNumFxSlots>& order);

    static inline const juce::Identifier extraStateId { "EXTRA" };
    static inline const juce::Identifier fxOrderId { "fxOrder" };
    static inline const juce::Identifier samplePathId { "samplePath" };
    static inline const juce::Identifier uiWidthId { "uiWidth" };
    static inline const juce::Identifier uiHeightId { "uiHeight" };
    static constexpr int stateVersion = 1;

private:
    static bool isValidFxOrder (const std::array<int, nebula::kNumFxSlots>& order) noexcept;
    void syncFxOrderFromState();

    juce::AudioProcessorValueTreeState apvts;

    // FX order packed into one atomic word (4 bits per slot), so the audio
    // thread can read it lock-free.
    std::atomic<uint32_t> packedFxOrder { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NebulaProcessor)
};
