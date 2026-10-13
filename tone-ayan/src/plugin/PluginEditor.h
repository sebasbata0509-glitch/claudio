#pragma once

#include "PluginProcessor.h"

#include "../ui/Inspector.h"
#include "../ui/PianoRoll.h"
#include "../ui/ToneLookAndFeel.h"
#include "../ui/ViewState.h"

namespace tone::plugin
{
#if JucePlugin_Enable_ARA
class ToneAudioModification;
class ToneDocumentController;
#endif

class ToneAyanEditor : public juce::AudioProcessorEditor,
#if JucePlugin_Enable_ARA
                       public juce::AudioProcessorEditorARAExtension,
                       private juce::ARAEditorView::Listener,
                       private juce::ChangeListener,
#endif
                       private EditSession::Listener,
                       private juce::Timer
{
public:
    explicit ToneAyanEditor (ToneAyanProcessor&);
    ~ToneAyanEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void sessionChanged (EditSession&) override;
    void setSession (EditSession*);
    void updateButtons();
    void openFile();
    void exportFile();
    double currentPlayhead() const;
    bool currentlyPlaying() const;

#if JucePlugin_Enable_ARA
    void onNewSelection (const juce::ARAViewSelection&) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void pickARASession (ToneAudioModification* preferred);
    ToneDocumentController* documentController() const;
    ToneAudioModification* araModification = nullptr;
#endif

    ToneAyanProcessor& processor;
    ui::ToneLookAndFeel lookAndFeel;
    ui::ViewState view;
    ui::PianoRoll pianoRoll { view };
    ui::Inspector inspector;
    EditSession* session = nullptr;

    juce::Label logo, status, hint;
    juce::TextButton openButton { "Abrir..." }, exportButton { "Exportar..." }, captureButton { "Capturar" },
                     playButton { "Reproducir" }, undoButton { "Deshacer" }, redoButton { "Rehacer" },
                     mainTool { "Principal" }, pitchTool { "Altura" }, timeTool { "Tiempo" }, splitTool { "Dividir" },
                     snapButton { "Snap" }, origButton { "Curva original" }, followButton { "Seguir" }, fitButton { "Ajustar" };
    juce::ComboBox engineBox;
    double progressValue = 0.0;
    juce::ProgressBar progress { progressValue };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneAyanEditor)
};
} // namespace tone::plugin
