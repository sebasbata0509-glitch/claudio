#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "dsp/Presets.h"

NutSwellerProcessor::NutSwellerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "NutSweller", nsw::createParameterLayout()),
      reader (state)
{
    midiEvents.reserve (1024);
}

bool NutSwellerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    // Mono -> mono, mono -> stereo, stereo -> stereo.
    return in.size() <= out.size();
}

void NutSwellerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numIn = std::max (1, getTotalNumInputChannels());
    const int numOut = std::max (1, getTotalNumOutputChannels());
    engine.setParams (reader.read());
    engine.prepare (sampleRate, samplesPerBlock, numIn, numOut);
    scratch.setSize (std::max (numIn, numOut), std::max (1, samplesPerBlock));
    setLatencySamples (engine.getLatencySamples());
}

double NutSwellerProcessor::getTailLengthSeconds() const
{
    // Grains still overlapping after the input stops, plus note release.
    return 0.1;
}

void NutSwellerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    process (buffer, midi, false);
}

void NutSwellerProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    process (buffer, midi, true);
}

void NutSwellerProcessor::process (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = std::min (getTotalNumOutputChannels(), buffer.getNumChannels());
    if (numSamples == 0 || numOut == 0)
        return;

    // Hosts may exceed the announced block size; the engine itself has no block limit,
    // but a disabled input needs a buffer of silence.
    if (numIn == 0 && scratch.getNumSamples() < numSamples)
        scratch.setSize (scratch.getNumChannels(), numSamples, false, false, true);

    const float* in[2] {};
    float* out[2] {};
    for (int c = 0; c < 2; ++c)
    {
        if (numIn > 0)
            in[c] = buffer.getReadPointer (std::min (c, numIn - 1));
        else
        {
            scratch.clear (0, 0, numSamples);
            in[c] = scratch.getReadPointer (0);
        }
        out[c] = buffer.getWritePointer (std::min (c, numOut - 1));
    }

    engine.setParams (reader.read());

    nsw::Transport transport;
    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            if (const auto bpm = pos->getBpm())
            {
                transport.hasTempo = true;
                transport.bpm = *bpm;
            }
            transport.isPlaying = pos->getIsPlaying();
            if (const auto ppq = pos->getPpqPosition())
            {
                transport.hasPosition = true;
                transport.ppqAtBlockStart = *ppq;
            }
        }
    }
    engine.setTransport (transport);

    if (bypassed)
    {
        engine.processBypassed (in, out, numSamples);
    }
    else
    {
        midiEvents.clear();
        for (const auto meta : midi)
        {
            if (midiEvents.size() == midiEvents.capacity())
                break; // never allocate on the audio thread
            const auto m = meta.getMessage();
            const int pos = juce::jlimit (0, numSamples - 1, meta.samplePosition);
            if (m.isNoteOn())
                midiEvents.push_back ({ pos, nsw::MidiEvent::Type::NoteOn, m.getNoteNumber(), m.getFloatVelocity() });
            else if (m.isNoteOff())
                midiEvents.push_back ({ pos, nsw::MidiEvent::Type::NoteOff, m.getNoteNumber(), 0.0f });
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                midiEvents.push_back ({ pos, nsw::MidiEvent::Type::AllNotesOff, 0, 0.0f });
        }
        engine.process (in, out, numSamples, midiEvents.data(), (int) midiEvents.size());
    }

    for (int c = numOut; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    // The detection range sets the latency; tell the host when it changes.
    if (engine.getLatencySamples() != getLatencySamples())
        setLatencySamples (engine.getLatencySamples());
}

int NutSwellerProcessor::getNumPrograms()
{
    return (int) nsw::factoryPresets().size();
}

const juce::String NutSwellerProcessor::getProgramName (int index)
{
    const auto presets = nsw::factoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? juce::String (presets[(size_t) index].name)
                                                                   : juce::String();
}

void NutSwellerProcessor::setCurrentProgram (int index)
{
    const auto presets = nsw::factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;
    currentProgram.store (index);
    nsw::applyParams (state, presets[(size_t) index].params);
}

void NutSwellerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto tree = state.copyState();
    tree.setProperty ("program", currentProgram.load(), nullptr);
    tree.setProperty ("uiScale", (double) uiScale.load(), nullptr);
    tree.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = tree.createXml())
        copyXmlToBinary (*xml, destData);
}

void NutSwellerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (state.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    currentProgram.store (juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0)));
    uiScale.store ((float) (double) tree.getProperty ("uiScale", 1.0));
    state.replaceState (tree);
}

juce::AudioProcessorEditor* NutSwellerProcessor::createEditor()
{
    return new NutSwellerEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NutSwellerProcessor();
}
