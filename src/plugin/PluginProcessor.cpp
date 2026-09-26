#include "PluginProcessor.h"
#include "PluginEditor.h"

NutSwellerProcessor::NutSwellerProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)) {}

bool NutSwellerProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

juce::AudioProcessorEditor* NutSwellerProcessor::createEditor() { return new NutSwellerEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NutSwellerProcessor(); }
