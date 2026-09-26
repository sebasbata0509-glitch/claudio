#pragma once

#include "PluginProcessor.h"
#include "ModPanel.h"
#include "Widgets.h"

/** All controls live in a fixed-size "canvas" that is scaled as a whole, so the vector UI stays crisp at any size. */
class NutSwellerEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NutSwellerEditor (NutSwellerProcessor&);
    ~NutSwellerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseWidth = 1000;
    static constexpr int kBaseHeight = 830;

private:
    class Canvas final : public juce::Component
    {
    public:
        explicit Canvas (NutSwellerProcessor&);
        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        void resized() override;
        void refresh();

    private:
        void refreshPresetList();

        NutSwellerProcessor& proc;
        juce::AudioProcessorValueTreeState& state;

        nsw::ui::Wordmark wordmark;
        juce::ComboBox presetBox;
        juce::TextButton prevPreset { "<" }, nextPreset { ">" };
        nsw::ui::SegmentedChoice range, mode;

        nsw::ui::Section pitchSection {}, centreSection {}, formantSection {},
            textureSection { "Texture" };
        nsw::ui::Knob pitch, formant;
        nsw::ui::SegmentedChoice octave;
        nsw::ui::ParamToggle snap;
        nsw::ui::PitchReadout readout;
        nsw::ui::MixSlider mix;
        nsw::ui::Knob glide, output;
        nsw::ui::Knob harmonics, alternator, fm, ratio, smear, stereo, detune;
        nsw::ui::ModPanel modPanel;
        int lastProgram = -1;
    };

    void timerCallback() override;

    NutSwellerProcessor& proc;
    nsw::ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    Canvas canvas;
};
