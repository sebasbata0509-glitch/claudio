#pragma once

#include "Widgets.h"
#include "dsp/Engine.h"

namespace nsw::ui
{
/** ComboBox bound to a choice parameter. */
class ParamCombo final : public juce::Component
{
public:
    ParamCombo (APVTS& state, const juce::String& paramId, const juce::String& caption);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::ComboBox box;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    juce::String caption;
};

/** 16 bipolar bars with a glide strip underneath; drag to draw. */
class StepEditor final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepEditor (APVTS& state);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void setPlayingStep (int step, bool running);

private:
    void edit (const juce::MouseEvent&, bool reset);
    juce::Rectangle<float> barArea() const;
    juce::Rectangle<float> glideArea() const;

    APVTS& state;
    std::array<juce::RangedAudioParameter*, 16> values {}, glides {};
    juce::RangedAudioParameter *depth = nullptr, *length = nullptr, *dest = nullptr;
    int playing = -1;
    bool running = false;
    bool editingGlide = false;
};

/** ADSR shape preview with a live level dot. */
class EnvelopeView final : public juce::Component
{
public:
    explicit EnvelopeView (APVTS& state);
    void paint (juce::Graphics&) override;
    void setLevel (float l);

private:
    std::atomic<float>*a, *d, *s, *r;
    float level = 0.0f;
};

/** Tabbed modulation section: Macro, Sequencer, Envelope, MIDI. */
class ModPanel final : public juce::Component
{
public:
    explicit ModPanel (APVTS& state);
    void paint (juce::Graphics&) override;
    void resized() override;
    void updateFromEngine (const Engine::Display& d);

private:
    void showTab (int index);

    APVTS& state;
    juce::OwnedArray<juce::TextButton> tabs;
    int current = 0;

    // Macro
    juce::Component macroPage;
    std::unique_ptr<Knob> macro;
    juce::OwnedArray<Knob> depths;

    // Sequencer
    juce::Component seqPage;
    ParamToggle seqOn;
    ParamCombo seqRate;
    SegmentedChoice seqDest;
    Knob seqDepth, seqLength;
    StepEditor steps;

    // Envelope
    juce::Component envPage;
    Knob attack, decay, sustain, release, envDepth, threshold;
    SegmentedChoice trigger;
    ParamCombo envDest;
    EnvelopeView envView;

    // MIDI
    juce::Component midiPage;
    SegmentedChoice midiMode;
    ParamCombo velTarget;
    Knob velAmount;
    juce::Label midiHelp;
};
} // namespace nsw::ui
