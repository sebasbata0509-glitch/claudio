#include "PluginEditor.h"

#include "PluginProcessor.h"

namespace
{
constexpr int headerHeight = 48;
constexpr int defaultWidth = 900;
constexpr int defaultHeight = 640;
} // namespace

NebulaEditor::NebulaEditor (NebulaProcessor& p)
    : AudioProcessorEditor (p), processor (p), generic (p)
{
    addAndMakeVisible (generic);

    setResizable (true, true);
    setResizeLimits (640, 420, 2400, 1600);

    auto extra = processor.getExtraState();
    setSize ((int) extra.getProperty (NebulaProcessor::uiWidthId, defaultWidth),
             (int) extra.getProperty (NebulaProcessor::uiHeightId, defaultHeight));
}

NebulaEditor::~NebulaEditor() = default;

void NebulaEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0d0f17));

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (16, 0);
    g.setColour (juce::Colour (0xffb9a6ff));
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("NEBULA", header, juce::Justification::centredLeft);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("v" NEBULA_VERSION_STRING "  -  dev build", header, juce::Justification::centredRight);
}

void NebulaEditor::resized()
{
    generic.setBounds (getLocalBounds().withTrimmedTop (headerHeight));

    auto extra = processor.getExtraState();
    extra.setProperty (NebulaProcessor::uiWidthId, getWidth(), nullptr);
    extra.setProperty (NebulaProcessor::uiHeightId, getHeight(), nullptr);
}
