#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class NebulaProcessor;

/** Development editor: a header plus JUCE's generic parameter editor, so every
    parameter can be tweaked while the DSP is built. Replaced by the full UI in
    phase 7. */
class NebulaEditor final : public juce::AudioProcessorEditor
{
public:
    explicit NebulaEditor (NebulaProcessor&);
    ~NebulaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    NebulaProcessor& processor;
    juce::GenericAudioProcessorEditor generic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NebulaEditor)
};
