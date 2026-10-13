#pragma once

#include "CaptureRecorder.h"
#include "EditSession.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <mutex>

namespace tone::plugin
{
/**
    Tone-ayan audio processor.

    Three ways of getting audio in:
      - ARA2 (Studio One, Cubase, Reaper, Cakewalk/Sonar, Logic...): the host hands us whole clips;
        each audio modification has its own EditSession (see ara/ToneARA.h).
      - Capture ("Transfer"): in any VST3 host, arm capture and play the song; the audio passing
        through is recorded against the host timeline, analysed, and from then on replaced by the
        edited render at the same timeline position.
      - Standalone: open an audio file and play it with the built-in transport.
*/
class ToneAyanProcessor : public juce::AudioProcessor,
#if JucePlugin_Enable_ARA
                          public juce::AudioProcessorARAExtension,
#endif
                          private juce::Timer,
                          private juce::AsyncUpdater
{
public:
    ToneAyanProcessor();
    ~ToneAyanProcessor() override;

    // AudioProcessor ----------------------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Tone-ayan ---------------------------------------------------------------------------
    bool isStandalone() const noexcept { return wrapperType == wrapperType_Standalone; }
    bool isARA() const noexcept;

    /** Session for capture / standalone mode. */
    EditSession& getSession() noexcept { return session; }

    /** Host transport as last seen by the audio thread. */
    bool isHostPlaying() const noexcept { return hostPlaying.load(); }
    double getHostTimeSeconds() const noexcept { return hostTime.load(); }

    /** Start of the captured / imported clip on the host timeline (seconds). */
    double getClipOffsetSeconds() const noexcept { return clipOffset.load(); }

    /** Playhead in session (clip) time for capture / standalone mode. */
    double getSessionPlayheadSeconds() const noexcept;
    bool isSessionPlaying() const noexcept;

    // Standalone transport
    void setTransportPlaying (bool);
    void setTransportPosition (double sessionSeconds);

    // Capture
    void setCaptureArmed (bool);
    bool isCaptureArmed() const noexcept { return recorder.isArmed(); }
    bool isCapturing() const noexcept { return recorder.isArmed() && hostPlaying.load() && recorder.hasData(); }

    /** Loads an audio file into the capture / standalone session (clip placed at timeline 0). */
    bool importAudioFile (const juce::File&, juce::String& error);

private:
    void timerCallback() override;
    void handleAsyncUpdate() override;
    void applyState (const juce::MemoryBlock&);

    EditSession session;
    CaptureRecorder recorder;

    std::atomic<bool> hostPlaying { false }, transportPlaying { false };
    std::atomic<double> hostTime { 0.0 }, transportPos { 0.0 }, clipOffset { 0.0 }, currentRate { 44100.0 };
    std::atomic<juce::int64> hostSample { 0 };
    std::vector<float> monoScratch;
    juce::uint32 lastPlayingMs = 0;

    std::mutex pendingStateMutex;
    juce::MemoryBlock pendingState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneAyanProcessor)
};
} // namespace tone::plugin
