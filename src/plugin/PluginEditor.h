#pragma once
#include "PluginProcessor.h"

class NutSwellerEditor final : public juce::AudioProcessorEditor
{
public:
    explicit NutSwellerEditor (NutSwellerProcessor& p) : AudioProcessorEditor (p) { setSize (600, 400); }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
};
