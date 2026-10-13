#pragma once

#include "SnapshotPublisher.h"

#include "analysis/Analyzer.h"
#include "common/Audio.h"
#include "engine/EditModel.h"
#include "engine/Renderer.h"
#include "model/Project.h"
#include "model/UndoStack.h"

#include <juce_events/juce_events.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace tone::plugin
{
/** Audio the audio thread plays for a session: the render of the current edits (or the source). */
struct PlaybackAudio
{
    std::vector<float> samples;
    double sampleRate = 44100.0;
    uint64_t version = 0;
    bool isRender = false; // false while it is still the unprocessed source
};

/**
    Everything about one editable recording: source audio, analysis, the edit model with undo,
    and the background analysis / render workers.

    All public functions are for the message thread, except playback() which the audio thread
    reads through SnapshotPublisher::ReadScope.
*/
class EditSession : private juce::Timer
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        /** Model, selection, analysis or render state changed. */
        virtual void sessionChanged (EditSession&) = 0;
    };

    EditSession();
    ~EditSession() override;

    // Source & analysis -------------------------------------------------------------------
    /** Starts a fresh analysis of new source audio (clears edits and undo). */
    void setSource (std::shared_ptr<const SourceAudio>);
    /** Restores saved state without re-analysing. */
    void restore (std::shared_ptr<const SourceAudio>, std::shared_ptr<const AnalysisResult>, const Project&);
    /** Clears everything. */
    void clear();

    bool hasSource() const noexcept { return source != nullptr; }
    bool hasAnalysis() const noexcept { return analysis != nullptr; }
    bool isAnalysing() const noexcept { return analysing.load(); }
    float getAnalysisProgress() const noexcept { return analysisProgress.load(); }
    std::shared_ptr<const SourceAudio> getSource() const { return source; }
    std::shared_ptr<const AnalysisResult> getAnalysis() const { return analysis; }

    // Edit model ----------------------------------------------------------------------------
    const Project& getProject() const noexcept { return project; }
    /** Continuous view of the current project (null until analysed). */
    const EditModel* getEditModel() const noexcept { return editModel.get(); }
    uint64_t getVersion() const noexcept { return version; }

    /** Applies an undoable change. */
    void perform (const juce::String& label, const std::function<void (Project&)>& change);

    /** Interactive gestures: every update is applied to the state at beginGesture(). */
    void beginGesture();
    void updateGesture (const std::function<void (Project&)>& changeFromBase);
    void endGesture (const juce::String& label);
    bool isInGesture() const noexcept { return gestureBase.has_value(); }

    bool undo();
    bool redo();
    bool canUndo() const noexcept { return undoStack.canUndo(); }
    bool canRedo() const noexcept { return undoStack.canRedo(); }
    juce::String getUndoLabel() const { return undoStack.undoLabel(); }
    juce::String getRedoLabel() const { return undoStack.redoLabel(); }

    edits::CenterFn centreFunction() const;

    // Selection -----------------------------------------------------------------------------
    const NoteIdSet& getSelection() const noexcept { return selection; }
    void setSelection (NoteIdSet);
    /** Selection, or every note when nothing is selected. */
    NoteIdSet selectionOrAll() const;

    // Rendering -----------------------------------------------------------------------------
    void setEngine (SynthEngine);
    SynthEngine getEngine() const noexcept { return engine; }
    bool isRendering() const noexcept { return renderBusy.load() || renderRequested; }
    float getRenderProgress() const noexcept { return renderProgress.load(); }

    /** What the audio thread should play (source until the first render is ready). */
    const SnapshotPublisher<PlaybackAudio>& playback() const noexcept { return published; }

    /** Thread-safe copy of what needs saving (host state calls can come from any thread). */
    struct StateSnapshot
    {
        std::shared_ptr<const SourceAudio> source;
        std::shared_ptr<const AnalysisResult> analysis;
        std::shared_ptr<const Project> project;
        SynthEngine engine = SynthEngine::Psola;
    };
    StateSnapshot getStateSnapshot() const;

    /** Renders the current project synchronously (used for export). */
    std::vector<float> renderNow() const;

    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    /** Called (message thread) whenever a new render has been published. */
    std::function<void()> onRenderPublished;

private:
    void timerCallback() override;
    void modelChanged (bool requestRender = true);
    void notify();
    void updateStateSnapshot();
    void startAnalysis();
    void stopAnalysis();
    void requestRender();
    void renderLoop();

    std::shared_ptr<const SourceAudio> source;
    std::shared_ptr<const AnalysisResult> analysis;
    Project project;
    std::unique_ptr<EditModel> editModel;
    std::optional<Project> gestureBase;
    UndoStack undoStack;
    NoteIdSet selection;
    uint64_t version = 1;
    SynthEngine engine = SynthEngine::Psola;

    // analysis worker
    std::thread analysisThread;
    std::atomic<bool> analysing { false }, analysisCancel { false }, analysisDone { false };
    std::atomic<float> analysisProgress { 0.0f };
    std::mutex analysisMutex;
    std::shared_ptr<AnalysisResult> analysisResult;

    // render worker
    struct RenderJob
    {
        std::shared_ptr<const SourceAudio> source;
        std::shared_ptr<const AnalysisResult> analysis;
        std::shared_ptr<const Project> project;
        SynthEngine engine = SynthEngine::Psola;
        uint64_t version = 0;
    };
    std::thread renderThread;
    std::mutex renderMutex;
    std::condition_variable renderCv;
    std::optional<RenderJob> pendingJob;
    bool renderQuit = false;
    bool renderRequested = false;
    std::atomic<bool> renderBusy { false }, renderCancel { false }, renderPublished { false };
    std::atomic<float> renderProgress { 0.0f };

    SnapshotPublisher<PlaybackAudio> published;
    mutable std::mutex stateMutex;
    StateSnapshot stateSnapshot;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditSession)
};
} // namespace tone::plugin
