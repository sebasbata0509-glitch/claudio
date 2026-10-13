#include "EditSession.h"

namespace tone::plugin
{
EditSession::EditSession()
{
    renderThread = std::thread ([this] { renderLoop(); });
    startTimerHz (30);
}

EditSession::~EditSession()
{
    stopTimer();
    stopAnalysis();
    {
        std::lock_guard<std::mutex> lock (renderMutex);
        renderQuit = true;
        renderCancel = true;
    }
    renderCv.notify_all();
    renderThread.join();
}

//==============================================================================
void EditSession::setSource (std::shared_ptr<const SourceAudio> newSource)
{
    stopAnalysis();
    source = std::move (newSource);
    analysis.reset();
    project = {};
    editModel.reset();
    gestureBase.reset();
    undoStack.clear();
    selection.clear();
    ++version;

    if (source != nullptr)
    {
        auto pa = std::make_unique<PlaybackAudio>();
        pa->samples = source->samples;
        pa->sampleRate = source->sampleRate;
        pa->version = version;
        published.publish (std::move (pa));
        startAnalysis();
    }
    else
    {
        published.publish (nullptr);
    }
    notify();
}

void EditSession::restore (std::shared_ptr<const SourceAudio> src, std::shared_ptr<const AnalysisResult> an, const Project& p)
{
    stopAnalysis();
    source = std::move (src);
    analysis = std::move (an);
    project = p;
    gestureBase.reset();
    undoStack.clear();
    selection.clear();

    if (source != nullptr)
    {
        auto pa = std::make_unique<PlaybackAudio>();
        pa->samples = source->samples;
        pa->sampleRate = source->sampleRate;
        published.publish (std::move (pa));
    }
    if (analysis == nullptr && source != nullptr)
        startAnalysis();
    modelChanged();
}

void EditSession::clear()
{
    setSource (nullptr);
}

//==============================================================================
void EditSession::startAnalysis()
{
    analysisProgress = 0.0f;
    analysisCancel = false;
    analysisDone = false;
    analysing = true;
    analysisThread = std::thread ([this, src = source]
    {
        JobControl job;
        job.cancel = &analysisCancel;
        job.onProgress = [this] (float p) { analysisProgress = p; };
        auto result = std::make_shared<AnalysisResult> (analyse (src->samples.data(), src->samples.size(), src->sampleRate, {}, job));
        if (! analysisCancel)
        {
            std::lock_guard<std::mutex> lock (analysisMutex);
            analysisResult = std::move (result);
            analysisDone = true;
        }
        analysing = false;
    });
}

void EditSession::stopAnalysis()
{
    analysisCancel = true;
    if (analysisThread.joinable())
        analysisThread.join();
    analysisCancel = false;
    analysing = false;
    analysisDone = false;
    analysisResult.reset();
}

//==============================================================================
void EditSession::modelChanged (bool render)
{
    ++version;
    if (analysis != nullptr)
        editModel = std::make_unique<EditModel> (project, analysis->track);
    else
        editModel.reset();

    // Drop selected ids that no longer exist (after merge, undo, ...).
    for (auto it = selection.begin(); it != selection.end();)
        it = project.find (*it) == nullptr ? selection.erase (it) : std::next (it);

    if (render)
        requestRender();
    notify();
}

void EditSession::updateStateSnapshot()
{
    std::lock_guard<std::mutex> lock (stateMutex);
    stateSnapshot = { source, analysis, std::make_shared<const Project> (project), engine };
}

EditSession::StateSnapshot EditSession::getStateSnapshot() const
{
    std::lock_guard<std::mutex> lock (stateMutex);
    return stateSnapshot;
}

void EditSession::notify()
{
    updateStateSnapshot();
    listeners.call ([this] (Listener& l) { l.sessionChanged (*this); });
}

void EditSession::perform (const juce::String& label, const std::function<void (Project&)>& change)
{
    if (analysis == nullptr || isInGesture())
        return;
    const Project before = project;
    change (project);
    if (project == before)
        return;
    undoStack.push (before, label.toStdString());
    modelChanged();
}

void EditSession::beginGesture()
{
    if (analysis != nullptr)
        gestureBase = project;
}

void EditSession::updateGesture (const std::function<void (Project&)>& change)
{
    if (! gestureBase)
        return;
    Project next = *gestureBase;
    change (next);
    if (next == project)
        return;
    project = std::move (next);
    modelChanged();
}

void EditSession::endGesture (const juce::String& label)
{
    if (! gestureBase)
        return;
    if (! (project == *gestureBase))
        undoStack.push (*gestureBase, label.toStdString());
    gestureBase.reset();
    notify();
}

bool EditSession::undo()
{
    if (isInGesture() || ! undoStack.undo (project))
        return false;
    modelChanged();
    return true;
}

bool EditSession::redo()
{
    if (isInGesture() || ! undoStack.redo (project))
        return false;
    modelChanged();
    return true;
}

edits::CenterFn EditSession::centreFunction() const
{
    if (analysis == nullptr)
        return [] (double, double) { return 60.0f; };
    return [an = analysis] (double s0, double s1)
    {
        return medianPitch (an->track, an->track.frameBoundaryAt (s0), an->track.frameBoundaryAt (s1));
    };
}

void EditSession::setSelection (NoteIdSet s)
{
    if (s == selection)
        return;
    selection = std::move (s);
    notify();
}

NoteIdSet EditSession::selectionOrAll() const
{
    if (! selection.empty())
        return selection;
    NoteIdSet all;
    for (const auto& n : project.notes)
        all.insert (n.id);
    return all;
}

//==============================================================================
void EditSession::setEngine (SynthEngine e)
{
    if (e == engine)
        return;
    engine = e;
    requestRender();
    notify();
}

void EditSession::requestRender()
{
    if (analysis == nullptr || source == nullptr)
        return;
    RenderJob job { source, analysis, std::make_shared<const Project> (project), engine, version };
    {
        std::lock_guard<std::mutex> lock (renderMutex);
        pendingJob = std::move (job);
        renderCancel = true; // abandon an outdated render; finished units stay cached
        renderRequested = true;
    }
    renderCv.notify_one();
}

void EditSession::renderLoop()
{
    std::unique_ptr<Renderer> renderer;
    const SourceAudio* rendererSource = nullptr;
    const AnalysisResult* rendererAnalysis = nullptr;

    for (;;)
    {
        RenderJob job;
        {
            std::unique_lock<std::mutex> lock (renderMutex);
            renderCv.wait (lock, [this] { return renderQuit || pendingJob.has_value(); });
            if (renderQuit)
                return;
            job = std::move (*pendingJob);
            pendingJob.reset();
            renderCancel = false;
            renderBusy = true;
        }

        if (renderer == nullptr || job.source.get() != rendererSource || job.analysis.get() != rendererAnalysis)
        {
            renderer = std::make_unique<Renderer> (job.source, job.analysis);
            rendererSource = job.source.get();
            rendererAnalysis = job.analysis.get();
        }
        renderer->setEngine (job.engine);

        std::vector<float> out;
        JobControl control;
        control.cancel = &renderCancel;
        control.onProgress = [this] (float p) { renderProgress = p; };
        renderProgress = 0.0f;

        if (renderer->render (*job.project, out, control))
        {
            auto pa = std::make_unique<PlaybackAudio>();
            pa->samples = std::move (out);
            pa->sampleRate = job.source->sampleRate;
            pa->version = job.version;
            pa->isRender = true;
            published.publish (std::move (pa));
            renderPublished = true;
        }
        renderBusy = false;
    }
}

std::vector<float> EditSession::renderNow() const
{
    std::vector<float> out;
    if (source == nullptr)
        return out;
    if (analysis == nullptr)
        return source->samples;
    Renderer r (source, analysis);
    r.setEngine (engine);
    r.render (project, out);
    return out;
}

//==============================================================================
void EditSession::timerCallback()
{
    published.collectGarbage();

    if (analysisDone.exchange (false))
    {
        std::shared_ptr<AnalysisResult> r;
        {
            std::lock_guard<std::mutex> lock (analysisMutex);
            r = std::move (analysisResult);
        }
        if (analysisThread.joinable())
            analysisThread.join();
        if (r != nullptr)
        {
            analysis = r;
            const auto restored = project; // keep notes restored before the analysis finished
            if (restored.notes.empty())
                project = projectFromAnalysis (*analysis);
            modelChanged();
        }
    }

    bool stateChanged = false;
    if (renderPublished.exchange (false))
    {
        if (onRenderPublished)
            onRenderPublished();
        stateChanged = true;
    }

    {
        std::lock_guard<std::mutex> lock (renderMutex);
        const bool nowRendering = renderBusy || pendingJob.has_value();
        if (renderRequested != nowRendering)
        {
            renderRequested = nowRendering;
            stateChanged = true;
        }
    }

    if (stateChanged)
        notify();
}
} // namespace tone::plugin
