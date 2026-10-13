#include "PluginProcessor.h"

#include "Playback.h"
#include "PluginEditor.h"
#include "StateIO.h"
#include "ara/ToneARA.h"

#include "model/Serialize.h"

namespace tone::plugin
{
namespace
{
constexpr int kStateMagic = 0x4e594154; // "TAYN"
constexpr int kStateVersion = 1;
} // namespace

ToneAyanProcessor::ToneAyanProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    startTimerHz (20);
}

ToneAyanProcessor::~ToneAyanProcessor()
{
    stopTimer();
    cancelPendingUpdate();
    recorder.setArmed (false);
}

bool ToneAyanProcessor::isARA() const noexcept
{
#if JucePlugin_Enable_ARA
    return isBoundToARA();
#else
    return false;
#endif
}

void ToneAyanProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    monoScratch.assign ((size_t) std::max (1, samplesPerBlock), 0.0f);
#if JucePlugin_Enable_ARA
    prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
#endif
}

void ToneAyanProcessor::releaseResources()
{
#if JucePlugin_Enable_ARA
    releaseResourcesForARA();
#endif
}

bool ToneAyanProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == out;
}

double ToneAyanProcessor::getTailLengthSeconds() const
{
#if JucePlugin_Enable_ARA
    double tail = 0.0;
    if (getTailLengthSecondsForARA (tail))
        return tail;
#endif
    return 0.0;
}

//==============================================================================
void ToneAyanProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inputs = getTotalNumInputChannels();
    for (int c = inputs; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    const double sr = getSampleRate() > 0.0 ? getSampleRate() : currentRate.load();

#if JucePlugin_Enable_ARA
    if (isBoundToARA())
    {
        if (auto* ph = getPlayHead())
            if (const auto pos = ph->getPosition())
            {
                hostPlaying = pos->getIsPlaying();
                hostTime = pos->getTimeInSeconds().orFallback (0.0);
            }
        processBlockForARA (buffer, isRealtime(), getPlayHead());
        return;
    }
#endif

    if (isStandalone())
    {
        buffer.clear();
        if (! transportPlaying.load())
            return;
        double pos = transportPos.load();
        SnapshotPublisher<PlaybackAudio>::ReadScope clip (session.playback());
        if (clip)
        {
            readClip (*clip.get(), pos, sr, buffer, 0, n, true);
            pos += n / sr;
            if (pos >= clip->samples.size() / clip->sampleRate)
            {
                transportPlaying = false;
                pos = 0.0;
            }
        }
        transportPos = pos;
        return;
    }

    // Capture / playback inside a regular (non-ARA) host.
    bool playing = false;
    juce::int64 timeInSamples = 0;
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            timeInSamples = pos->getTimeInSamples().orFallback (0);
        }
    hostPlaying = playing;
    hostSample = timeInSamples;
    hostTime = (double) timeInSamples / sr;

    if (recorder.isArmed())
    {
        if (playing && n <= (int) monoScratch.size() && inputs > 0)
        {
            std::fill (monoScratch.begin(), monoScratch.begin() + n, 0.0f);
            for (int c = 0; c < inputs; ++c)
                juce::FloatVectorOperations::addWithMultiply (monoScratch.data(), buffer.getReadPointer (c), 1.0f / (float) inputs, n);
            recorder.write (monoScratch.data(), n, timeInSamples);
        }
        return; // pass the input through while capturing
    }

    if (! playing)
        return;

    SnapshotPublisher<PlaybackAudio>::ReadScope clip (session.playback());
    if (! clip)
        return;

    // Replace the input with the edited clip where the clip covers the timeline.
    const double clipStart = clipOffset.load();
    const double clipEnd = clipStart + (double) clip->samples.size() / clip->sampleRate;
    const double t0 = (double) timeInSamples / sr;
    const int first = std::clamp ((int) std::ceil ((clipStart - t0) * sr), 0, n);
    const int last = std::clamp ((int) std::ceil ((clipEnd - t0) * sr), 0, n);
    if (last > first)
    {
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            buffer.clear (c, first, last - first);
        readClip (*clip.get(), (double) (timeInSamples + first) / sr - clipStart, sr, buffer, first, last - first, true);
    }
}

//==============================================================================
double ToneAyanProcessor::getSessionPlayheadSeconds() const noexcept
{
    if (isStandalone())
        return transportPos.load();
    return hostTime.load() - clipOffset.load();
}

bool ToneAyanProcessor::isSessionPlaying() const noexcept
{
    return isStandalone() ? transportPlaying.load() : hostPlaying.load();
}

void ToneAyanProcessor::setTransportPlaying (bool shouldPlay)
{
    transportPlaying = shouldPlay;
}

void ToneAyanProcessor::setTransportPosition (double seconds)
{
    transportPos = std::max (0.0, seconds);
}

void ToneAyanProcessor::setCaptureArmed (bool shouldArm)
{
    if (shouldArm)
    {
        recorder.reset();
        recorder.ensureAllocatedAround (hostSample.load());
    }
    recorder.setArmed (shouldArm);
}

bool ToneAyanProcessor::importAudioFile (const juce::File& file, juce::String& error)
{
    auto audio = loadAudioFile (file, error);
    if (audio == nullptr)
        return false;
    recorder.setArmed (false);
    clipOffset = 0.0;
    transportPlaying = false;
    transportPos = 0.0;
    session.setSource (std::move (audio));
    return true;
}

void ToneAyanProcessor::timerCallback()
{
    if (! recorder.isArmed())
        return;

    recorder.ensureAllocatedAround (hostSample.load());

    const auto now = juce::Time::getMillisecondCounter();
    if (hostPlaying.load())
        lastPlayingMs = now;
    else if (recorder.hasData() && now - lastPlayingMs > 250)
    {
        // Transport stopped after a pass: take the recording and analyse it.
        std::vector<float> samples;
        const auto start = recorder.takeRecording (samples);
        if (samples.size() > (size_t) (0.1 * currentRate.load()))
        {
            auto audio = std::make_shared<SourceAudio>();
            audio->samples = std::move (samples);
            audio->sampleRate = currentRate.load();
            clipOffset = (double) start / audio->sampleRate;
            session.setSource (std::move (audio));
        }
    }
}

//==============================================================================
void ToneAyanProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::MemoryOutputStream out (dest, false);
    out.writeInt (kStateMagic);
    out.writeInt (kStateVersion);

    const auto snap = session.getStateSnapshot();
    const bool hasClip = snap.source != nullptr && snap.analysis != nullptr && snap.project != nullptr && ! isARA();
    out.writeBool (hasClip);
    if (! hasClip)
        return;

    out.writeDouble (clipOffset.load());
    out.writeInt ((int) snap.engine);
    const auto flac = encodeFlac (*snap.source);
    out.writeInt64 ((juce::int64) flac.getSize());
    out.write (flac.getData(), flac.getSize());
    const auto blob = snap.analysis->serialise();
    out.writeInt64 ((juce::int64) blob.size());
    out.write (blob.data(), blob.size());
    out.writeString (juce::String (projectToJson (*snap.project)));
}

void ToneAyanProcessor::setStateInformation (const void* data, int size)
{
    {
        std::lock_guard<std::mutex> lock (pendingStateMutex);
        pendingState.replaceAll (data, (size_t) size);
    }
    if (juce::MessageManager::existsAndIsCurrentThread())
        handleAsyncUpdate();
    else
        triggerAsyncUpdate();
}

void ToneAyanProcessor::handleAsyncUpdate()
{
    juce::MemoryBlock state;
    {
        std::lock_guard<std::mutex> lock (pendingStateMutex);
        state.swapWith (pendingState);
    }
    if (state.getSize() > 0)
        applyState (state);
}

void ToneAyanProcessor::applyState (const juce::MemoryBlock& state)
{
    juce::MemoryInputStream in (state, false);
    if (in.readInt() != kStateMagic || in.readInt() > kStateVersion || ! in.readBool())
        return;

    try
    {
        const double offset = in.readDouble();
        const auto engine = (SynthEngine) juce::jlimit (0, 1, in.readInt());

        const auto flacSize = in.readInt64();
        if (flacSize <= 0 || flacSize > in.getNumBytesRemaining())
            return;
        juce::MemoryBlock flac;
        in.readIntoMemoryBlock (flac, (int) flacSize);

        const auto blobSize = in.readInt64();
        if (blobSize <= 0 || blobSize > in.getNumBytesRemaining())
            return;
        std::vector<uint8_t> blob ((size_t) blobSize);
        in.read (blob.data(), (int) blobSize);
        const auto json = in.readString();

        auto audio = decodeFlac (flac.getData(), flac.getSize());
        if (audio == nullptr)
            return;
        auto analysis = std::make_shared<AnalysisResult> (AnalysisResult::deserialise (blob.data(), blob.size()));
        const auto project = projectFromJson (json.toStdString());

        clipOffset = offset;
        session.setEngine (engine);
        session.restore (std::move (audio), std::move (analysis), project);
    }
    catch (const std::exception&)
    {
        // Unreadable state: start empty rather than crash the host.
    }
}

juce::AudioProcessorEditor* ToneAyanProcessor::createEditor()
{
    return new ToneAyanEditor (*this);
}
} // namespace tone::plugin

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new tone::plugin::ToneAyanProcessor();
}

#if JucePlugin_Enable_ARA
const ARA::ARAFactory* JUCE_CALLTYPE createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<tone::plugin::ToneDocumentController>();
}
#endif
