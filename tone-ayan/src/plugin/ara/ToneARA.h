#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#if JucePlugin_Enable_ARA

#include "../EditSession.h"

#include <atomic>
#include <optional>
#include <utility>

namespace tone::plugin
{
class ToneDocumentController;

/** One editable take in ARA mode: an audio modification owns its edit session. */
class ToneAudioModification : public juce::ARAAudioModification
{
public:
    ToneAudioModification (juce::ARAAudioSource*, ARA::ARAAudioModificationHostRef, const ToneAudioModification* toClone);
    ~ToneAudioModification() override;

    EditSession& getSession() noexcept { return *session; }
    const EditSession& getSession() const noexcept { return *session; }

    /** Called on the message thread once the host audio has been read. */
    void sourceLoaded (std::shared_ptr<const SourceAudio>);
    /** Saved state waiting for the source audio (or applied right away if it is already loaded). */
    void setRestoredState (std::shared_ptr<const AnalysisResult>, const Project&);
    /** What to archive: the live session, or restored state not yet applied. */
    std::pair<std::shared_ptr<const AnalysisResult>, std::shared_ptr<const Project>> persistableState() const;

    bool loadStarted = false;
    juce::ThreadPoolJob* loadJob = nullptr;

private:
    std::unique_ptr<EditSession> session;
    std::shared_ptr<const AnalysisResult> pendingAnalysis;
    std::optional<Project> pendingProject;

    JUCE_DECLARE_WEAK_REFERENCEABLE (ToneAudioModification)
};

/** ARA document controller: creates modifications, loads host audio, persists edits. */
class ToneDocumentController : public juce::ARADocumentControllerSpecialisation,
                               public juce::ChangeBroadcaster
{
public:
    using ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;
    ~ToneDocumentController() override;

    /**
        Lock-free guard between host graph edits (message thread) and playback renderers (audio
        thread): renderers skip a block while the document is being edited, and editing waits for
        renderers that are mid-block. No locks or allocations on the audio thread.
    */
    class RenderScope
    {
    public:
        explicit RenderScope (ToneDocumentController& dc) noexcept : owner (dc)
        {
            owner.activeRenderers.fetch_add (1);
            ok = ! owner.editing.load();
        }
        ~RenderScope() { owner.activeRenderers.fetch_sub (1); }
        bool canRender() const noexcept { return ok; }

    private:
        ToneDocumentController& owner;
        bool ok = false;
    };
    const std::vector<ToneAudioModification*>& getModifications() const noexcept { return modifications; }

    /** Starts reading the host audio for a modification if access is enabled and not yet loaded. */
    void requestLoad (ToneAudioModification&);

protected:
    void willBeginEditing (juce::ARADocument*) override;
    void didEndEditing (juce::ARADocument*) override;

    juce::ARAAudioModification* doCreateAudioModification (juce::ARAAudioSource*, ARA::ARAAudioModificationHostRef,
                                                           const juce::ARAAudioModification*) noexcept override;
    juce::ARAPlaybackRenderer* doCreatePlaybackRenderer() noexcept override;

    bool doRestoreObjectsFromStream (juce::ARAInputStream&, const juce::ARARestoreObjectsFilter*) noexcept override;
    bool doStoreObjectsToStream (juce::ARAOutputStream&, const juce::ARAStoreObjectsFilter*) noexcept override;

    void didEnableAudioSourceSamplesAccess (juce::ARAAudioSource*, bool enable) override;
    void willDestroyAudioModification (juce::ARAAudioModification*) override;

private:
    void loadAllPending();

    std::atomic<bool> editing { false };
    std::atomic<int> activeRenderers { 0 };
    std::vector<ToneAudioModification*> modifications;
    juce::ThreadPool loaderPool { 1 };
};

/** Plays the rendered audio of every modification the host assigns to this renderer. */
class TonePlaybackRenderer : public juce::ARAPlaybackRenderer
{
public:
    TonePlaybackRenderer (ARA::PlugIn::DocumentController*, ToneDocumentController&);

    void prepareToPlay (double sampleRate, int maxBlock, int numChannels, juce::AudioProcessor::ProcessingPrecision,
                        AlwaysNonRealtime) override;
    bool processBlock (juce::AudioBuffer<float>&, juce::AudioProcessor::Realtime,
                       const juce::AudioPlayHead::PositionInfo&) noexcept override;
    using juce::ARAPlaybackRenderer::processBlock;

private:
    ToneDocumentController& owner;
    double sampleRate = 44100.0;
};
} // namespace tone::plugin

#endif
