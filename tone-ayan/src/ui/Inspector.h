#pragma once

#include "../plugin/EditSession.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace tone::ui
{
/**
    Right-hand panel: per-note tools (Pitch Center, Pitch Drift, Pitch Modulation, formant, gain),
    the scale macro, and structural actions. Every control acts on the selected notes (the macros
    act on all notes when nothing is selected).
*/
class Inspector : public juce::Component, private plugin::EditSession::Listener
{
public:
    Inspector();
    ~Inspector() override;

    void setSession (plugin::EditSession*);
    std::function<double()> getPlayhead; // for "split at playhead"

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    /** A slider that edits the selection as one undoable gesture per drag. */
    struct ParamRow
    {
        juce::Label label;
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        std::function<void (Project&, const NoteIdSet&, double)> apply;
        juce::String undoName;
        bool relative = false; // tool-style slider that snaps back to 0 after each use
    };

    void sessionChanged (plugin::EditSession&) override;
    void setupRow (ParamRow&, const juce::String& name, double lo, double hi, double neutral, const juce::String& suffix,
                   std::function<void (Project&, const NoteIdSet&, double)> apply, bool relative);
    void refresh();
    NoteIdSet targets (bool allWhenEmpty) const;

    plugin::EditSession* session = nullptr;

    juce::Label title, info;
    ParamRow centre, drift, modulation, formant, gain, scaleAmount;
    juce::Label scaleLabel;
    juce::ComboBox rootBox, typeBox;
    juce::TextButton applyScale { "Corregir a escala" }, applyCentre { "Centrar 100 %" };
    juce::TextButton mergeButton { "Unir" }, splitButton { "Dividir en cursor" };
    juce::TextButton resetButton { "Restablecer notas" }, resetTimeButton { "Restablecer tiempo" };
    bool updating = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Inspector)
};
} // namespace tone::ui
