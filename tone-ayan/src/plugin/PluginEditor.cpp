#include "PluginEditor.h"

#include "StateIO.h"
#include "ara/ToneARA.h"

namespace tone::plugin
{
ToneAyanEditor::ToneAyanEditor (ToneAyanProcessor& p)
    : AudioProcessorEditor (&p),
#if JucePlugin_Enable_ARA
      AudioProcessorEditorARAExtension (&p),
#endif
      processor (p)
{
    setLookAndFeel (&lookAndFeel);

    logo.setText ("Tone-ayan", juce::dontSendNotification);
    logo.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    logo.setColour (juce::Label::textColourId, ui::colours::accent);
    status.setColour (juce::Label::textColourId, ui::colours::textDim);
    status.setJustificationType (juce::Justification::centredRight);
    hint.setColour (juce::Label::textColourId, ui::colours::textDim);
    for (auto* c : std::initializer_list<juce::Component*> { &logo, &status, &hint, &pianoRoll, &inspector, &progress, &engineBox })
        addAndMakeVisible (c);

    openButton.setTooltip ("Carga un archivo de audio (WAV, AIFF, FLAC, MP3...) para editarlo");
    exportButton.setTooltip ("Guarda el resultado editado como WAV de 24 bits");
    captureButton.setTooltip ("Arma la captura y reproduce el proyecto en tu DAW: Tone-ayan graba lo que entra, "
                              "lo analiza al detener y despues reproduce la version editada en la misma posicion");
    playButton.setTooltip ("Reproducir / detener (barra espaciadora)");
    mainTool.setTooltip ("Herramienta principal: arrastra en vertical (altura) o en horizontal (tiempo); bordes = estirar; doble clic = dividir");
    pitchTool.setTooltip ("Solo altura");
    timeTool.setTooltip ("Solo tiempo y duracion");
    splitTool.setTooltip ("Clic sobre una nota para dividirla");
    snapButton.setTooltip ("Ajuste a semitonos al arrastrar (manten Alt para mover libre en cents)");
    origButton.setTooltip ("Muestra la curva de altura original bajo la editada");
    followButton.setTooltip ("La vista sigue al cursor de reproduccion");
    fitButton.setTooltip ("Ajusta el zoom a todas las notas (tecla F)");

    for (auto* b : { &openButton, &exportButton, &captureButton, &playButton, &undoButton, &redoButton, &mainTool, &pitchTool,
                     &timeTool, &splitTool, &snapButton, &origButton, &followButton, &fitButton })
        addAndMakeVisible (b);

    captureButton.setClickingTogglesState (false);
    captureButton.setColour (juce::TextButton::buttonOnColourId, ui::colours::record);
    captureButton.onClick = [this] { processor.setCaptureArmed (! processor.isCaptureArmed()); updateButtons(); };
    playButton.onClick = [this]
    {
        if (processor.isStandalone())
            processor.setTransportPlaying (! processor.isSessionPlaying());
    };
    openButton.onClick = [this] { openFile(); };
    exportButton.onClick = [this] { exportFile(); };
    undoButton.onClick = [this] { if (session != nullptr) session->undo(); };
    redoButton.onClick = [this] { if (session != nullptr) session->redo(); };

    auto setTool = [this] (ui::Tool t) { view.tool = t; updateButtons(); };
    mainTool.onClick = [setTool] { setTool (ui::Tool::Main); };
    pitchTool.onClick = [setTool] { setTool (ui::Tool::Pitch); };
    timeTool.onClick = [setTool] { setTool (ui::Tool::Time); };
    splitTool.onClick = [setTool] { setTool (ui::Tool::Split); };
    snapButton.onClick = [this] { view.snapToSemitone = ! view.snapToSemitone; updateButtons(); };
    origButton.onClick = [this] { view.showOriginalCurve = ! view.showOriginalCurve; updateButtons(); pianoRoll.repaint(); };
    followButton.onClick = [this] { view.followPlayhead = ! view.followPlayhead; updateButtons(); };
    fitButton.onClick = [this] { pianoRoll.zoomToFit(); };

    engineBox.addItem ("Motor: TD-PSOLA", 1);
    engineBox.addItem ("Motor: Vocoder de fase", 2);
    engineBox.setSelectedId (1, juce::dontSendNotification);
    engineBox.setTooltip ("TD-PSOLA (recomendado para voz) o vocoder de fase con envolvente espectral");
    engineBox.onChange = [this]
    {
        if (session != nullptr)
            session->setEngine (engineBox.getSelectedId() == 2 ? SynthEngine::PhaseVocoder : SynthEngine::Psola);
    };

    pianoRoll.onSeek = [this] (double t)
    {
        if (processor.isStandalone())
            processor.setTransportPosition (t);
    };
    pianoRoll.onTogglePlay = [this] { playButton.triggerClick(); };
    pianoRoll.onHint = [this] (const juce::String& s) { hint.setText (s, juce::dontSendNotification); };
    inspector.getPlayhead = [this] { return currentPlayhead(); };

#if JucePlugin_Enable_ARA
    if (auto* editorView = getARAEditorView())
    {
        editorView->addListener (this);
        if (auto* dc = documentController())
            dc->addChangeListener (this);
        pickARASession (nullptr);
    }
    else
#endif
    {
        setSession (&processor.getSession());
    }

    setResizable (true, true);
    setResizeLimits (820, 480, 4000, 3000);
    setSize (1320, 760);
    startTimerHz (30);
    updateButtons();
}

ToneAyanEditor::~ToneAyanEditor()
{
    stopTimer();
#if JucePlugin_Enable_ARA
    if (auto* editorView = getARAEditorView())
    {
        editorView->removeListener (this);
        if (auto* dc = documentController())
            dc->removeChangeListener (this);
    }
#endif
    setSession (nullptr);
    setLookAndFeel (nullptr);
}

void ToneAyanEditor::setSession (EditSession* s)
{
    if (session == s)
        return;
    if (session != nullptr)
        session->removeListener (this);
    session = s;
    if (session != nullptr)
    {
        session->addListener (this);
        engineBox.setSelectedId (session->getEngine() == SynthEngine::PhaseVocoder ? 2 : 1, juce::dontSendNotification);
    }
    pianoRoll.setSession (session);
    inspector.setSession (session);
    updateButtons();
}

void ToneAyanEditor::sessionChanged (EditSession&)
{
    updateButtons();
}

#if JucePlugin_Enable_ARA
ToneDocumentController* ToneAyanEditor::documentController() const
{
    if (auto* editorView = getARAEditorView())
        return juce::ARADocumentControllerSpecialisation::getSpecialisedDocumentController<ToneDocumentController> (
            editorView->getDocumentController());
    return nullptr;
}

void ToneAyanEditor::onNewSelection (const juce::ARAViewSelection& selection)
{
    for (auto* region : selection.getPlaybackRegions<juce::ARAPlaybackRegion>())
    {
        pickARASession (region->getAudioModification<ToneAudioModification>());
        return;
    }
}

void ToneAyanEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    pickARASession (araModification);
}

void ToneAyanEditor::pickARASession (ToneAudioModification* preferred)
{
    auto* dc = documentController();
    if (dc == nullptr)
        return;
    const auto& mods = dc->getModifications();
    if (preferred == nullptr || std::find (mods.begin(), mods.end(), preferred) == mods.end())
        preferred = mods.empty() ? nullptr : mods.front();
    araModification = preferred;
    if (araModification != nullptr)
        dc->requestLoad (*araModification);
    setSession (araModification != nullptr ? &araModification->getSession() : nullptr);
}
#endif

//==============================================================================
double ToneAyanEditor::currentPlayhead() const
{
#if JucePlugin_Enable_ARA
    if (processor.isARA())
    {
        if (araModification == nullptr)
            return -1.0;
        const double t = processor.getHostTimeSeconds();
        for (auto* region : araModification->getPlaybackRegions<juce::ARAPlaybackRegion>())
            if (t >= region->getStartInPlaybackTime() && t < region->getEndInPlaybackTime())
                return t - region->getStartInPlaybackTime() + region->getStartInAudioModificationTime();
        return -1.0;
    }
#endif
    return processor.getSessionPlayheadSeconds();
}

bool ToneAyanEditor::currentlyPlaying() const
{
    return processor.isARA() ? processor.isHostPlaying() : processor.isSessionPlaying();
}

void ToneAyanEditor::timerCallback()
{
    pianoRoll.setPlayhead (currentPlayhead(), currentlyPlaying());

    juce::String text;
    double prog = -1.0;
    if (processor.isCaptureArmed())
        text = processor.isCapturing() ? "Grabando... detén el DAW para analizar" : "Captura armada: reproduce tu DAW";
    else if (session != nullptr && session->isAnalysing())
    {
        text = "Analizando";
        prog = session->getAnalysisProgress();
    }
    else if (session != nullptr && session->isRendering())
    {
        text = "Renderizando";
        prog = session->getRenderProgress();
    }
    else if (session != nullptr && session->hasAnalysis())
        text = juce::String (session->getProject().notes.size()) + " notas  |  "
               + (processor.isARA() ? "ARA" : (processor.isStandalone() ? "Standalone" : "Modo captura"));
    else
        text = processor.isARA() ? "ARA: selecciona un clip en el DAW" : (processor.isStandalone() ? "Standalone" : "Modo captura");

    status.setText (text, juce::dontSendNotification);
    progressValue = prog < 0.0 ? 0.0 : prog;
    progress.setVisible (prog >= 0.0);

    const bool playing = currentlyPlaying();
    if (playButton.getToggleState() != playing)
    {
        playButton.setToggleState (playing, juce::dontSendNotification);
        playButton.setButtonText (playing ? "Detener" : "Reproducir");
    }
    if (captureButton.getToggleState() != processor.isCaptureArmed())
        captureButton.setToggleState (processor.isCaptureArmed(), juce::dontSendNotification);
    if (session != nullptr && session->isAnalysing())
        pianoRoll.repaint();
}

void ToneAyanEditor::updateButtons()
{
    const bool ara = processor.isARA();
    const bool standalone = processor.isStandalone();
    captureButton.setVisible (! ara && ! standalone);
    playButton.setVisible (standalone);
    openButton.setVisible (! ara);

    const bool ready = session != nullptr && session->hasAnalysis();
    exportButton.setEnabled (ready);
    undoButton.setEnabled (session != nullptr && session->canUndo());
    redoButton.setEnabled (session != nullptr && session->canRedo());
    undoButton.setTooltip (session != nullptr && session->canUndo() ? "Deshacer " + session->getUndoLabel() + " (Ctrl+Z)" : "Deshacer (Ctrl+Z)");
    redoButton.setTooltip (session != nullptr && session->canRedo() ? "Rehacer " + session->getRedoLabel() + " (Ctrl+Y)" : "Rehacer (Ctrl+Y)");

    mainTool.setToggleState (view.tool == ui::Tool::Main, juce::dontSendNotification);
    pitchTool.setToggleState (view.tool == ui::Tool::Pitch, juce::dontSendNotification);
    timeTool.setToggleState (view.tool == ui::Tool::Time, juce::dontSendNotification);
    splitTool.setToggleState (view.tool == ui::Tool::Split, juce::dontSendNotification);
    snapButton.setToggleState (view.snapToSemitone, juce::dontSendNotification);
    origButton.setToggleState (view.showOriginalCurve, juce::dontSendNotification);
    followButton.setToggleState (view.followPlayhead, juce::dontSendNotification);
    resized();
}

void ToneAyanEditor::openFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Abrir audio", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file == juce::File())
                                  return;
                              juce::String error;
                              if (! processor.importAudioFile (file, error))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Tone-ayan", error);
                          });
}

void ToneAyanEditor::exportFile()
{
    if (session == nullptr || ! session->hasAnalysis())
        return;
    chooser = std::make_unique<juce::FileChooser> ("Exportar WAV", juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                                                       .getChildFile ("tone-ayan-export.wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file == juce::File() || session == nullptr)
                                  return;
                              const auto audio = session->renderNow();
                              if (! writeWav (file.withFileExtension ("wav"), audio, session->getSource()->sampleRate))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Tone-ayan",
                                                                          "No se pudo escribir " + file.getFullPathName());
                          });
}

//==============================================================================
void ToneAyanEditor::paint (juce::Graphics& g)
{
    g.fillAll (ui::colours::background);
    g.setColour (ui::colours::panel);
    g.fillRect (getLocalBounds().removeFromTop (44));
    g.setColour (ui::colours::gridStrong);
    g.fillRect (0, 44, getWidth(), 1);
    g.setColour (ui::colours::panel);
    g.fillRect (getLocalBounds().removeFromBottom (22));
}

void ToneAyanEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (44).reduced (8, 8);
    auto bottom = r.removeFromBottom (22).reduced (8, 2);

    logo.setBounds (top.removeFromLeft (92));
    auto place = [&top] (juce::Component& c, int w)
    {
        if (! c.isVisible())
            return;
        c.setBounds (top.removeFromLeft (w));
        top.removeFromLeft (4);
    };
    place (openButton, 72);
    place (exportButton, 82);
    place (captureButton, 84);
    place (playButton, 92);
    top.removeFromLeft (8);
    place (undoButton, 76);
    place (redoButton, 70);
    top.removeFromLeft (8);
    place (mainTool, 78);
    place (pitchTool, 60);
    place (timeTool, 64);
    place (splitTool, 64);
    top.removeFromLeft (8);
    place (snapButton, 50);
    place (origButton, 96);
    place (followButton, 56);
    place (fitButton, 58);
    top.removeFromLeft (8);
    engineBox.setBounds (top.removeFromRight (180));

    progress.setBounds (bottom.removeFromRight (160).reduced (0, 2));
    status.setBounds (bottom.removeFromRight (360));
    hint.setBounds (bottom);

    inspector.setBounds (r.removeFromRight (276));
    pianoRoll.setBounds (r);
}
} // namespace tone::plugin
