#include "Inspector.h"

#include "ToneLookAndFeel.h"

#include "model/Edits.h"

namespace tone::ui
{
Inspector::Inspector()
{
    title.setText ("Notas", juce::dontSendNotification);
    title.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    addAndMakeVisible (title);
    info.setColour (juce::Label::textColourId, colours::textDim);
    info.setJustificationType (juce::Justification::topLeft);
    info.setFont (juce::FontOptions (13.0f));
    info.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (info);

    using P = Project;
    setupRow (centre, "Pitch Center", 0.0, 100.0, 0.0, " %",
              [] (P& p, const NoteIdSet& ids, double v) { edits::correctPitchCenter (p, ids, (float) v / 100.0f); }, true);
    setupRow (drift, "Pitch Drift", 0.0, 100.0, 100.0, " %",
              [] (P& p, const NoteIdSet& ids, double v) { edits::setDrift (p, ids, (float) v / 100.0f); }, false);
    setupRow (modulation, "Pitch Modulation", 0.0, 200.0, 100.0, " %",
              [] (P& p, const NoteIdSet& ids, double v) { edits::setModulation (p, ids, (float) v / 100.0f); }, false);
    setupRow (formant, "Formantes", -12.0, 12.0, 0.0, " st",
              [] (P& p, const NoteIdSet& ids, double v) { edits::setFormant (p, ids, (float) v); }, false);
    setupRow (gain, "Ganancia", -24.0, 12.0, 0.0, " dB",
              [] (P& p, const NoteIdSet& ids, double v) { edits::setGain (p, ids, (float) v); }, false);
    setupRow (scaleAmount, "Corregir a escala", 0.0, 100.0, 0.0, " %",
              [] (P& p, const NoteIdSet& ids, double v) { edits::correctToScale (p, ids, (float) v / 100.0f); }, true);

    centre.slider.setTooltip ("Arrastra para acercar el centro de cada nota al semitono (como la herramienta Pitch Center)");
    drift.slider.setTooltip ("100 % = deriva original. Baja para aplanar las variaciones lentas de afinacion");
    modulation.slider.setTooltip ("100 % = vibrato original, 0 % = sin vibrato, 200 % = vibrato exagerado");
    scaleAmount.slider.setTooltip ("Arrastra para llevar las notas (seleccion o todas) hacia la escala elegida");

    scaleLabel.setText ("Escala", juce::dontSendNotification);
    scaleLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (scaleLabel);
    for (int i = 0; i < 12; ++i)
        rootBox.addItem (Scale::rootName (i), i + 1);
    for (int i = 0; i < (int) ScaleType::NumTypes; ++i)
        typeBox.addItem (Scale::typeName ((ScaleType) i), i + 1);
    auto onScale = [this]
    {
        if (updating || session == nullptr)
            return;
        Scale s { rootBox.getSelectedId() - 1, (ScaleType) (typeBox.getSelectedId() - 1) };
        session->perform ("Set scale", [s] (Project& p) { p.scale = s; });
    };
    rootBox.onChange = onScale;
    typeBox.onChange = onScale;
    addAndMakeVisible (rootBox);
    addAndMakeVisible (typeBox);

    applyScale.setTooltip ("Corrige al 100 % las notas seleccionadas (o todas) a la escala");
    applyScale.onClick = [this]
    {
        if (session != nullptr)
            session->perform ("Correct to scale", [ids = targets (true)] (Project& p) { edits::correctToScale (p, ids, 1.0f); });
    };
    applyCentre.onClick = [this]
    {
        if (session != nullptr)
            session->perform ("Pitch center", [ids = targets (true)] (Project& p) { edits::correctPitchCenter (p, ids, 1.0f); });
    };
    mergeButton.setTooltip ("Une las notas contiguas seleccionadas (tecla M)");
    mergeButton.onClick = [this]
    {
        if (session != nullptr)
            session->perform ("Merge notes", [ids = targets (false), c = session->centreFunction()] (Project& p) { edits::merge (p, ids, c); });
    };
    splitButton.onClick = [this]
    {
        if (session == nullptr || ! getPlayhead)
            return;
        const double t = getPlayhead();
        const auto* model = session->getEditModel();
        const int i = model != nullptr ? model->noteAtDst (t) : -1;
        if (i >= 0)
        {
            const auto id = session->getProject().notes[(size_t) i].id;
            session->perform ("Split note", [id, t, c = session->centreFunction()] (Project& p) { edits::split (p, id, t, c); });
        }
    };
    resetButton.setTooltip ("Quita las ediciones de altura, formantes y ganancia (tecla Supr)");
    resetButton.onClick = [this]
    {
        if (session != nullptr)
            session->perform ("Reset notes", [ids = targets (true)] (Project& p) { edits::resetNotes (p, ids); });
    };
    resetTimeButton.onClick = [this]
    {
        if (session != nullptr)
            session->perform ("Reset timing", [] (Project& p) { edits::resetTiming (p); });
    };
    for (auto* b : { &applyScale, &applyCentre, &mergeButton, &splitButton, &resetButton, &resetTimeButton })
        addAndMakeVisible (b);

    refresh();
}

Inspector::~Inspector()
{
    setSession (nullptr);
}

void Inspector::setupRow (ParamRow& row, const juce::String& name, double lo, double hi, double neutral,
                          const juce::String& suffix, std::function<void (Project&, const NoteIdSet&, double)> apply, bool relative)
{
    row.label.setText (name, juce::dontSendNotification);
    row.slider.setRange (lo, hi, 0.1);
    row.slider.setDoubleClickReturnValue (true, neutral);
    row.slider.setTextValueSuffix (suffix);
    row.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    row.slider.setValue (neutral, juce::dontSendNotification);
    row.apply = std::move (apply);
    row.undoName = name;
    row.relative = relative;
    const bool allWhenEmpty = &row == &scaleAmount;

    row.slider.onDragStart = [this, &row]
    {
        if (session != nullptr)
            session->beginGesture();
    };
    row.slider.onValueChange = [this, &row, allWhenEmpty]
    {
        if (updating || session == nullptr)
            return;
        const auto ids = targets (allWhenEmpty);
        const double v = row.slider.getValue();
        if (session->isInGesture())
            session->updateGesture ([&] (Project& p) { row.apply (p, ids, v); });
        else if (! row.relative)
            session->perform (row.undoName, [&] (Project& p) { row.apply (p, ids, v); });
    };
    row.slider.onDragEnd = [this, &row]
    {
        if (session != nullptr)
            session->endGesture (row.undoName);
        if (row.relative)
        {
            updating = true;
            row.slider.setValue (row.slider.getDoubleClickReturnValue(), juce::dontSendNotification);
            updating = false;
        }
    };
    addAndMakeVisible (row.label);
    addAndMakeVisible (row.slider);
}

void Inspector::setSession (plugin::EditSession* s)
{
    if (session == s)
        return;
    if (session != nullptr)
        session->removeListener (this);
    session = s;
    if (session != nullptr)
        session->addListener (this);
    refresh();
}

void Inspector::sessionChanged (plugin::EditSession&)
{
    refresh();
}

NoteIdSet Inspector::targets (bool allWhenEmpty) const
{
    if (session == nullptr)
        return {};
    return allWhenEmpty ? session->selectionOrAll() : session->getSelection();
}

void Inspector::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    const bool ready = session != nullptr && session->hasAnalysis();
    const auto sel = ready ? session->getSelection() : NoteIdSet {};
    const Note* first = nullptr;
    if (ready)
        for (const auto& n : session->getProject().notes)
            if (sel.count (n.id) != 0) { first = &n; break; }

    juce::String text;
    if (! ready)
        text = "Sin analisis todavia.";
    else if (first == nullptr)
        text = juce::String (session->getProject().notes.size()) + " notas. Selecciona notas para editarlas.\n"
               "Las macros de escala actuan sobre todas si no hay seleccion.";
    else
    {
        const float t = first->targetMidi();
        const int m = (int) std::lround (t);
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        text << (int) sel.size() << (sel.size() == 1 ? " nota" : " notas") << " seleccionada" << (sel.size() == 1 ? "" : "s") << "\n"
             << names[((m % 12) + 12) % 12] << (m / 12 - 1) << "  " << juce::roundToInt ((t - (float) m) * 100.0f) << " cents\n"
             << "Desplazamiento " << juce::String (first->pitchShift, 2) << " st, duracion x" << juce::String (first->stretch(), 2);
    }
    info.setText (text, juce::dontSendNotification);

    for (auto* row : { &drift, &modulation, &formant, &gain })
        row->slider.setEnabled (first != nullptr);
    centre.slider.setEnabled (ready);
    scaleAmount.slider.setEnabled (ready);
    for (auto* b : { &applyScale, &applyCentre, &resetButton, &resetTimeButton, &splitButton })
        b->setEnabled (ready);
    mergeButton.setEnabled (ready && sel.size() >= 2);
    rootBox.setEnabled (ready);
    typeBox.setEnabled (ready);

    if (first != nullptr && (session == nullptr || ! session->isInGesture()))
    {
        drift.slider.setValue (first->driftAmount * 100.0, juce::dontSendNotification);
        modulation.slider.setValue (first->modulationAmount * 100.0, juce::dontSendNotification);
        formant.slider.setValue (first->formantShift, juce::dontSendNotification);
        gain.slider.setValue (first->gainDb, juce::dontSendNotification);
    }
    if (ready)
    {
        rootBox.setSelectedId (session->getProject().scale.root + 1, juce::dontSendNotification);
        typeBox.setSelectedId ((int) session->getProject().scale.type + 1, juce::dontSendNotification);
    }
}

void Inspector::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::gridStrong);
    g.fillRect (0, 0, 1, getHeight());
}

void Inspector::resized()
{
    auto r = getLocalBounds().reduced (12, 10);
    title.setBounds (r.removeFromTop (24));
    info.setBounds (r.removeFromTop (70));
    r.removeFromTop (6);

    auto layoutRow = [&r] (ParamRow& row)
    {
        row.label.setBounds (r.removeFromTop (18));
        row.slider.setBounds (r.removeFromTop (24));
        r.removeFromTop (6);
    };
    layoutRow (centre);
    applyCentre.setBounds (r.removeFromTop (24).removeFromRight (120));
    r.removeFromTop (6);
    layoutRow (drift);
    layoutRow (modulation);
    layoutRow (formant);
    layoutRow (gain);

    r.removeFromTop (4);
    auto buttons = r.removeFromTop (26);
    mergeButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 3));
    buttons.removeFromLeft (6);
    splitButton.setBounds (buttons);
    r.removeFromTop (6);
    buttons = r.removeFromTop (26);
    resetButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 3));
    buttons.removeFromLeft (6);
    resetTimeButton.setBounds (buttons);

    r.removeFromTop (14);
    scaleLabel.setBounds (r.removeFromTop (20));
    auto boxes = r.removeFromTop (26);
    rootBox.setBounds (boxes.removeFromLeft (64));
    boxes.removeFromLeft (6);
    typeBox.setBounds (boxes);
    r.removeFromTop (6);
    layoutRow (scaleAmount);
    applyScale.setBounds (r.removeFromTop (26));
}
} // namespace tone::ui
