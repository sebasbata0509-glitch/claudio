#include "ToneARA.h"

#if JucePlugin_Enable_ARA

#include "../Playback.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <thread>

#include "model/Serialize.h"

namespace tone::plugin
{
namespace
{
/** Reads the whole host audio source (mixed to mono) on a pool thread. */
class SourceLoadJob : public juce::ThreadPoolJob
{
public:
    explicit SourceLoadJob (ToneAudioModification& m)
        : ThreadPoolJob ("Tone-ayan source loader"),
          reader (std::make_unique<juce::ARAAudioSourceReader> (m.getAudioSource())),
          target (&m),
          rate (m.getAudioSource()->getSampleRate()),
          length (m.getAudioSource()->getSampleCount())
    {
    }

    JobStatus runJob() override
    {
        auto audio = std::make_shared<SourceAudio>();
        audio->sampleRate = rate;
        audio->samples.assign ((size_t) std::max<juce::int64> (0, length), 0.0f);

        const int channels = (int) reader->numChannels;
        constexpr int block = 1 << 16;
        juce::AudioBuffer<float> buf (std::max (1, channels), block);
        bool ok = channels > 0;
        for (juce::int64 pos = 0; ok && pos < length; pos += block)
        {
            if (shouldExit())
            {
                ok = false;
                break;
            }
            const int n = (int) std::min<juce::int64> (block, length - pos);
            ok = reader->read (&buf, 0, n, pos, true, true);
            for (int c = 0; ok && c < channels; ++c)
                juce::FloatVectorOperations::addWithMultiply (audio->samples.data() + pos, buf.getReadPointer (c), 1.0f / (float) channels, n);
        }

        // Deliver (and destroy the reader) on the message thread.
        juce::MessageManager::callAsync ([weak = target, ok, audio, r = std::shared_ptr<juce::ARAAudioSourceReader> (reader.release())]() mutable
        {
            r.reset();
            if (auto* m = weak.get())
            {
                m->loadJob = nullptr;
                if (ok)
                    m->sourceLoaded (audio);
                else
                    m->loadStarted = false; // retry when access is enabled again
            }
        });
        return jobHasFinished;
    }

private:
    std::unique_ptr<juce::ARAAudioSourceReader> reader;
    juce::WeakReference<ToneAudioModification> target;
    double rate;
    juce::int64 length;
};
} // namespace

//==============================================================================
ToneAudioModification::ToneAudioModification (juce::ARAAudioSource* source, ARA::ARAAudioModificationHostRef hostRef,
                                              const ToneAudioModification* toClone)
    : ARAAudioModification (source, hostRef, toClone), session (std::make_unique<EditSession>())
{
    if (toClone != nullptr && toClone->session->hasAnalysis())
    {
        pendingAnalysis = toClone->session->getAnalysis();
        pendingProject = toClone->session->getProject();
    }

    session->onRenderPublished = [this]
    {
        // Let the host know the modification's audio changed (for bouncing / freezing).
        notifyContentChanged (juce::ARAContentUpdateScopes::samplesAreAffected(), true);
    };
}

ToneAudioModification::~ToneAudioModification()
{
    session->onRenderPublished = nullptr;
}

void ToneAudioModification::sourceLoaded (std::shared_ptr<const SourceAudio> audio)
{
    if (pendingAnalysis != nullptr && pendingProject.has_value())
        session->restore (std::move (audio), pendingAnalysis, *pendingProject);
    else
        session->setSource (std::move (audio));
    pendingAnalysis.reset();
    pendingProject.reset();
}

void ToneAudioModification::setRestoredState (std::shared_ptr<const AnalysisResult> analysis, const Project& project)
{
    if (session->hasSource())
    {
        session->restore (session->getSource(), std::move (analysis), project);
        return;
    }
    pendingAnalysis = std::move (analysis);
    pendingProject = project;
}

std::pair<std::shared_ptr<const AnalysisResult>, std::shared_ptr<const Project>> ToneAudioModification::persistableState() const
{
    if (pendingAnalysis != nullptr && pendingProject.has_value())
        return { pendingAnalysis, std::make_shared<const Project> (*pendingProject) };
    const auto snap = session->getStateSnapshot();
    if (snap.analysis == nullptr)
        return {};
    return { snap.analysis, snap.project };
}

//==============================================================================
ToneDocumentController::~ToneDocumentController()
{
    loaderPool.removeAllJobs (true, 5000);
}

void ToneDocumentController::willBeginEditing (juce::ARADocument*)
{
    editing.store (true);
    while (activeRenderers.load() > 0) // a renderer finishes its current block
        std::this_thread::yield();
}

void ToneDocumentController::didEndEditing (juce::ARADocument*)
{
    editing.store (false);
    loadAllPending();
    sendChangeMessage();
}

juce::ARAAudioModification* ToneDocumentController::doCreateAudioModification (juce::ARAAudioSource* source,
                                                                               ARA::ARAAudioModificationHostRef hostRef,
                                                                               const juce::ARAAudioModification* toClone) noexcept
{
    auto* m = new ToneAudioModification (source, hostRef, static_cast<const ToneAudioModification*> (toClone));
    modifications.push_back (m);
    return m;
}

juce::ARAPlaybackRenderer* ToneDocumentController::doCreatePlaybackRenderer() noexcept
{
    return new TonePlaybackRenderer (getDocumentController(), *this);
}

void ToneDocumentController::willDestroyAudioModification (juce::ARAAudioModification* mod)
{
    auto* m = static_cast<ToneAudioModification*> (mod);
    if (m->loadJob != nullptr)
        loaderPool.removeJob (m->loadJob, true, 5000);
    modifications.erase (std::remove (modifications.begin(), modifications.end(), m), modifications.end());
    sendChangeMessage();
}

void ToneDocumentController::didEnableAudioSourceSamplesAccess (juce::ARAAudioSource*, bool enable)
{
    if (enable)
        loadAllPending();
}

void ToneDocumentController::loadAllPending()
{
    for (auto* m : modifications)
        requestLoad (*m);
}

void ToneDocumentController::requestLoad (ToneAudioModification& m)
{
    if (m.loadStarted || m.getSession().hasSource() || ! m.getAudioSource()->isSampleAccessEnabled())
        return;
    m.loadStarted = true;
    auto* job = new SourceLoadJob (m);
    m.loadJob = job;
    loaderPool.addJob (job, true);
}

//==============================================================================
bool ToneDocumentController::doStoreObjectsToStream (juce::ARAOutputStream& out, const juce::ARAStoreObjectsFilter* filter) noexcept
{
    const auto& mods = filter->getAudioModificationsToStore<ToneAudioModification>();
    if (! out.writeInt64 ((juce::int64) mods.size()))
        return false;

    for (auto* m : mods)
    {
        const auto [analysis, project] = m->persistableState();
        const auto json = project != nullptr ? projectToJson (*project) : std::string();
        const auto blob = analysis != nullptr ? analysis->serialise() : std::vector<uint8_t>();
        if (! out.writeString (m->getPersistentID())
            || ! out.writeString (juce::String (json))
            || ! out.writeInt64 ((juce::int64) blob.size())
            || (! blob.empty() && ! out.write (blob.data(), blob.size())))
            return false;
    }
    return true;
}

bool ToneDocumentController::doRestoreObjectsFromStream (juce::ARAInputStream& in, const juce::ARARestoreObjectsFilter* filter) noexcept
{
    const auto count = in.readInt64();
    for (juce::int64 i = 0; i < count && ! in.failed(); ++i)
    {
        const auto id = in.readString();
        const auto json = in.readString();
        const auto size = in.readInt64();
        if (size < 0 || size > (juce::int64) 1 << 31)
            return false;
        std::vector<uint8_t> blob ((size_t) size);
        if (size > 0 && in.read (blob.data(), (int) size) != (int) size)
            return false;

        auto* m = filter->getAudioModificationToRestoreStateWithID<ToneAudioModification> (id.toRawUTF8());
        if (m == nullptr || json.isEmpty() || blob.empty())
            continue;
        try
        {
            auto analysis = std::make_shared<AnalysisResult> (AnalysisResult::deserialise (blob.data(), blob.size()));
            m->setRestoredState (std::move (analysis), projectFromJson (json.toStdString()));
        }
        catch (const std::exception&)
        {
            // Corrupt entry: the take is simply re-analysed.
        }
    }
    return ! in.failed();
}

//==============================================================================
TonePlaybackRenderer::TonePlaybackRenderer (ARA::PlugIn::DocumentController* dc, ToneDocumentController& o)
    : ARAPlaybackRenderer (dc), owner (o)
{
}

void TonePlaybackRenderer::prepareToPlay (double sr, int maxBlock, int numChannels,
                                          juce::AudioProcessor::ProcessingPrecision precision, AlwaysNonRealtime nonRealtime)
{
    ARAPlaybackRenderer::prepareToPlay (sr, maxBlock, numChannels, precision, nonRealtime);
    sampleRate = sr;
}

bool TonePlaybackRenderer::processBlock (juce::AudioBuffer<float>& buffer, juce::AudioProcessor::Realtime,
                                         const juce::AudioPlayHead::PositionInfo& info) noexcept
{
    buffer.clear();
    const ToneDocumentController::RenderScope scope (owner);
    if (! scope.canRender() || ! info.getIsPlaying())
        return true;

    const auto blockStart = info.getTimeInSamples().orFallback (0);
    const int n = buffer.getNumSamples();
    const double t0 = (double) blockStart / sampleRate, t1 = (double) (blockStart + n) / sampleRate;

    for (auto* region : getPlaybackRegions())
    {
        const double pStart = region->getStartInPlaybackTime(), pEnd = region->getEndInPlaybackTime();
        if (pEnd <= t0 || pStart >= t1)
            continue;

        auto* mod = region->getAudioModification<ToneAudioModification>();
        SnapshotPublisher<PlaybackAudio>::ReadScope clip (mod->getSession().playback());
        if (! clip)
            continue;

        const int first = std::clamp ((int) std::ceil ((pStart - t0) * sampleRate), 0, n);
        const int last = std::clamp ((int) std::ceil ((pEnd - t0) * sampleRate), 0, n);
        const double clipTime = (double) (blockStart + first) / sampleRate - pStart + region->getStartInAudioModificationTime();
        readClip (*clip.get(), clipTime, sampleRate, buffer, first, last - first, false);
    }
    return true;
}
} // namespace tone::plugin

#endif
