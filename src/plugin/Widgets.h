#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "LookAndFeel.h"

namespace nsw::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

/** Rotary knob with name above and live value below. */
class Knob final : public juce::Component
{
public:
    enum class Size { Large, Medium, Small };

    Knob (APVTS& state, const juce::String& paramId, const juce::String& name, Size size, bool bipolar = false);

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider& getSlider() noexcept { return slider; }
    void setTooltip (const juce::String& t) { slider.setTooltip (t); }

private:
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    juce::String name;
    Size size;
};

/** Horizontal Dry/Wet slider with end labels. */
class MixSlider final : public juce::Component
{
public:
    MixSlider (APVTS& state, const juce::String& paramId);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

/** Row of mutually exclusive segments bound to a choice / int parameter. */
class SegmentedChoice final : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentedChoice (APVTS& state, const juce::String& paramId, juce::StringArray labels, juce::String caption = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int segmentAt (juce::Point<int>) const;
    juce::Rectangle<float> segmentArea() const;

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::StringArray labels;
    juce::String caption;
    float value = 0.0f;
    int hover = -1;
};

/** Toggle bound to a bool parameter. */
class ParamToggle final : public juce::Component
{
public:
    ParamToggle (APVTS& state, const juce::String& paramId, const juce::String& text);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton button;

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

/** Rounded panel with a small spaced-caps caption. */
class Section final : public juce::Component
{
public:
    explicit Section (juce::String text = {}) : caption (std::move (text)) { setInterceptsMouseClicks (false, true); }
    void paint (juce::Graphics&) override;
    void setSubCaptions (std::vector<std::pair<juce::String, juce::Rectangle<int>>> subs) { subCaptions = std::move (subs); repaint(); }

private:
    juce::String caption;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> subCaptions;
};

/** "NutSweller" wordmark with an original seed-and-swell emblem, all vector. */
class Wordmark final : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
};

/** Real-time detected note, cents offset and confidence meter. */
class PitchReadout final : public juce::Component
{
public:
    void paint (juce::Graphics&) override;

    /** Feed the latest detection (call from a timer). */
    void update (float frequency, float confidence, bool voiced, float outputShiftSemis, bool showOutput);

private:
    float shownHz = 0.0f, shownConfidence = 0.0f, shift = 0.0f;
    bool voiced = false, showOut = true;
    int holdFrames = 0;
};

juce::String noteName (float midi);
} // namespace nsw::ui
