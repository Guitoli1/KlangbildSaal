#include "PluginEditor.h"

using namespace vintage;

namespace
{
juce::String u8 (const char* t) { return juce::String (juce::CharPointer_UTF8 (t)); }
juce::String num (double v, int d) { return d <= 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, d).replaceCharacter ('.', ','); }
const juce::Colour glassTop { 0xff141a1f }, glassBottom { 0xff0c1013 };
const juce::Colour lineDim { 0x30e8dcc0 };
const juce::Colour colLow { 0xffe3a456 }, colMid { 0xffefe3c6 }, colHigh { 0xff9cc8ea };
}

//==============================================================================
void RoomView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (glassTop, 0, 0, glassBottom, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 8.0f);

    auto inner = r.reduced (14.0f, 12.0f);
    auto left = inner.removeFromLeft (inner.getWidth() * 0.44f);
    inner.removeFromLeft (16.0f);
    if (data.custom) paintCustom (g, left); else paintPlan (g, left);
    paintDecay (g, inner);

    // Glasreflex
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), 0, 0, juce::Colours::transparentWhite, 0, r.getHeight() * 0.45f, false));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f), 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

void RoomView::paintPlan (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& room = saal::rooms()[(size_t) juce::jlimit (0, (int) saal::rooms().size() - 1, data.room)];
    g.setColour (cream.withAlpha (0.55f));
    g.setFont (lnf.labelFont (10.5f, 0.25f));
    g.drawText ("GRUNDRISS", area.removeFromTop (14.0f), juce::Justification::centredLeft);
    auto foot = area.removeFromBottom (16.0f);
    area.removeFromBottom (4.0f);

    const float sc = juce::jmin (area.getWidth() / (float) room.L, area.getHeight() / (float) room.W);
    const auto box = juce::Rectangle<float> ((float) room.L * sc, (float) room.W * sc).withCentre (area.getCentre());
    // Boden
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a2e22), box.getX(), box.getY(), juce::Colour (0xff241c15), box.getRight(), box.getBottom(), false));
    g.fillRect (box);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (float x = box.getX() + 6.0f; x < box.getRight(); x += 9.0f)
        g.drawVerticalLine ((int) x, box.getY(), box.getBottom());

    auto toPx = [&] (double x, double y) { return juce::Point<float> (box.getX() + (float) x * sc, box.getY() + (float) y * sc); };
    const double sx = room.L * 0.28, sy = room.W * 0.5;
    const double mx = sx + data.micDistance, my = sy;
    const auto S = toPx (sx, sy), M = toPx (mx, my);

    // erste Reflexionen (vier Wände)
    const std::array<juce::Point<double>, 4> images { juce::Point<double> (-sx, sy), { 2 * room.L - sx, sy }, { sx, -sy }, { sx, 2 * room.W - sy } };
    for (size_t i = 0; i < images.size(); ++i)
    {
        const auto im = images[i];
        // Schnittpunkt der Linie Spiegelquelle–Mikro mit der Wand
        double t = 0.5;
        if (i == 0) t = (0 - im.x) / (mx - im.x);
        if (i == 1) t = (room.L - im.x) / (mx - im.x);
        if (i == 2) t = (0 - im.y) / (my - im.y);
        if (i == 3) t = (room.W - im.y) / (my - im.y);
        const auto W = toPx (im.x + (mx - im.x) * t, im.y + (my - im.y) * t);
        juce::Path p; p.startNewSubPath (S); p.lineTo (W); p.lineTo (M);
        g.setColour (colHigh.withAlpha (0.35f));
        g.strokePath (p, juce::PathStrokeType (1.1f), juce::AffineTransform());
    }
    // direkter Weg
    g.setColour (cream.withAlpha (0.25f));
    g.drawLine ({ S, M }, 1.0f);

    // Wände
    g.setColour (cream.withAlpha (0.85f));
    g.drawRect (box, 2.0f);

    // Lautsprecher
    const auto spk = juce::Rectangle<float> (12.0f, 16.0f).withCentre (S);
    g.setColour (juce::Colour (0xff111111)); g.fillRoundedRectangle (spk, 2.0f);
    g.setColour (gold); g.drawRoundedRectangle (spk, 2.0f, 1.0f);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (S));
    // Mikrofone (ORTF-Paar)
    for (float dy : { -4.0f, 4.0f })
    {
        const auto mc = M.translated (0.0f, dy);
        g.setColour (lampGreen.withAlpha (0.25f)); g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (mc));
        g.setColour (lampGreen); g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (mc));
    }

    g.setColour (cream.withAlpha (0.7f));
    g.setFont (lnf.labelFontRegular (11.5f));
    g.drawText (num (room.L, room.L < 10 ? 1 : 0) + juce::String (" x ") + num (room.W, room.W < 10 ? 1 : 0) + juce::String (" x ") + num (room.H, 1) + " m",
                foot, juce::Justification::centredLeft);
    g.drawText ("Mikro " + num (data.micDistance, data.micDistance < 10 ? 1 : 0) + " m", foot, juce::Justification::centredRight);
}

void RoomView::paintCustom (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (cream.withAlpha (0.55f));
    g.setFont (lnf.labelFont (10.5f, 0.25f));
    g.drawText ("EIGENE IMPULSANTWORT", area.removeFromTop (14.0f), juce::Justification::centredLeft);
    auto mid = area.reduced (4.0f, 10.0f);
    // stilisierte Impulsantwort aus der Abklingkurve
    if (! data.decay[1].empty())
    {
        juce::Random rnd (7);
        const auto& c = data.decay[1];
        const int n = (int) c.size();
        const float x0 = mid.getX(), w = mid.getWidth(), cy = mid.getCentreY() - 10.0f, hh = mid.getHeight() * 0.36f;
        g.setColour (colMid.withAlpha (0.8f));
        for (int i = 0; i < (int) w; i += 2)
        {
            const int k = juce::jlimit (0, n - 1, (int) ((float) i / w * (float) n));
            const float a = std::pow (10.0f, c[(size_t) k] / 40.0f) * (0.35f + 0.65f * rnd.nextFloat());
            g.drawVerticalLine ((int) (x0 + (float) i), cy - a * hh, cy + a * hh);
        }
    }
    g.setColour (gold);
    g.setFont (lnf.labelFont (15.0f, 0.12f));
    g.drawText (data.name, mid.removeFromBottom (22.0f), juce::Justification::centred, true);
}

void RoomView::paintDecay (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (cream.withAlpha (0.55f));
    g.setFont (lnf.labelFont (10.5f, 0.25f));
    auto head = area.removeFromTop (14.0f);
    g.drawText ("AUSKLANG", head, juce::Justification::centredLeft);

    // Legende
    {
        auto lg = head;
        const std::array<std::pair<const char*, juce::Colour>, 3> items { std::pair<const char*, juce::Colour> { "HOCH", colHigh }, { "MITTE", colMid }, { "TIEF", colLow } };
        for (auto& [t, col] : items)
        {
            auto it = lg.removeFromRight (54.0f);
            g.setColour (col);
            g.fillRect (it.getX(), it.getCentreY() - 1.0f, 12.0f, 2.0f);
            g.drawText (t, it.withTrimmedLeft (16.0f), juce::Justification::centredLeft);
        }
    }

    auto plot = area.withTrimmedTop (6.0f).withTrimmedBottom (16.0f).withTrimmedLeft (26.0f);
    const float maxT = juce::jmax (0.5f, std::ceil (juce::jmax (0.5f, data.t60 * 1.25f) * 2.0f) / 2.0f);
    g.setFont (lnf.labelFontRegular (10.0f));
    for (int db = 0; db >= -60; db -= 20)
    {
        const float y = plot.getY() + plot.getHeight() * (float) (-db) / 60.0f;
        g.setColour (lineDim);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (cream.withAlpha (0.45f));
        g.drawText (juce::String (db), juce::Rectangle<float> (plot.getX() - 28.0f, y - 6.0f, 24.0f, 12.0f), juce::Justification::centredRight);
    }
    const float stepT = maxT <= 1.0f ? 0.25f : (maxT <= 3.0f ? 0.5f : 1.0f);
    for (float t = 0.0f; t <= maxT + 1e-3f; t += stepT)
    {
        const float x = plot.getX() + plot.getWidth() * t / maxT;
        g.setColour (lineDim);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (cream.withAlpha (0.45f));
        g.drawText (num (t, stepT < 0.5f ? 2 : 1) + " s", juce::Rectangle<float> (x - 22.0f, plot.getBottom() + 2.0f, 44.0f, 12.0f), juce::Justification::centred);
    }

    const std::array<juce::Colour, 3> cols { colLow, colMid, colHigh };
    for (size_t b = 0; b < 3; ++b)
    {
        const auto& c = data.decay[b];
        if (c.size() < 2) continue;
        juce::Path p;
        for (size_t i = 0; i < c.size(); ++i)
        {
            const float t = (float) i * 0.01f;
            if (t > maxT) break;
            const float x = plot.getX() + plot.getWidth() * t / maxT;
            const float y = plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, -c[i] / 60.0f);
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (cols[b].withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved));
        g.setColour (cols[b]);
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
    }

    // Nachhallzeit
    auto badge = juce::Rectangle<float> (128.0f, 40.0f).withPosition (plot.getRight() - 132.0f, plot.getY() + 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (badge, 5.0f);
    g.setColour (gold);
    g.setFont (lnf.labelFont (10.0f, 0.25f));
    g.drawText ("NACHHALL T60", badge.removeFromTop (16.0f).translated (0.0f, 3.0f), juce::Justification::centred);
    g.setColour (logo);
    g.setFont (lnf.labelFont (18.0f, 0.06f));
    g.drawText (data.t60 > 0 ? num (data.t60, 2) + " s" : u8 ("\xe2\x80\x93"), badge, juce::Justification::centred);
}

//==============================================================================
MeasurePanel::MeasurePanel (SaalAudioProcessor& p, VintageLookAndFeel& l) : proc (p), lnf (l)
{
    name.setText ("Mein Raum", juce::dontSendNotification);
    name.setFont (lnf.labelFontRegular (15.0f));
    name.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1b1815));
    name.setColour (juce::TextEditor::textColourId, cream);
    name.setColour (juce::TextEditor::outlineColourId, gold.withAlpha (0.5f));
    addAndMakeVisible (name);

    level.setSliderStyle (juce::Slider::LinearHorizontal);
    level.setRange (-40.0, -6.0, 1.0);
    level.setValue (-20.0, juce::dontSendNotification);
    level.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);
    level.setTextValueSuffix (" dBFS");
    level.setColour (juce::Slider::trackColourId, gold);
    level.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff2a2622));
    level.setColour (juce::Slider::thumbColourId, cream);
    level.setColour (juce::Slider::textBoxTextColourId, cream);
    level.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (level);

    startBtn.setButtonText ("Messung starten");
    closeBtn.setButtonText ("Fertig");
    startBtn.onClick = [this]
    {
        if (proc.measureState.load() == SaalAudioProcessor::running) { proc.cancelMeasurement(); status = "Abgebrochen."; repaint(); return; }
        if (proc.startMeasurement ((float) level.getValue(), name.getText()))
            status = u8 ("Messung l\xc3\xa4uft \xe2\x80\x93 bitte ruhig sein \xe2\x80\xa6");
        repaint();
    };
    closeBtn.onClick = [this] { proc.cancelMeasurement(); if (onClose) onClose(); };
    addAndMakeVisible (startBtn);
    addAndMakeVisible (closeBtn);
}

void MeasurePanel::tick()
{
    const int st = proc.measureState.load();
    const bool busy = st == SaalAudioProcessor::running || st == SaalAudioProcessor::analysing || st == SaalAudioProcessor::finishing;
    startBtn.setButtonText (st == SaalAudioProcessor::running ? "Abbrechen" : "Messung starten");
    startBtn.setEnabled (! (st == SaalAudioProcessor::analysing || st == SaalAudioProcessor::finishing));
    name.setEnabled (! busy);
    level.setEnabled (! busy);
    if (st == SaalAudioProcessor::analysing || st == SaalAudioProcessor::finishing) status = u8 ("Werte aus \xe2\x80\xa6");
    if (st == SaalAudioProcessor::done || st == SaalAudioProcessor::failed) status = proc.measureMessage;
    repaint();
}

void MeasurePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (3.0f, 5.0f), 10.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff27221d), 0, 0, juce::Colour (0xff15120f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (gold.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (4.0f), 8.0f, 1.0f);

    g.setColour (gold);
    g.setFont (lnf.labelFont (18.0f, 0.4f));
    g.drawText ("RAUM MESSEN", r.withHeight (48.0f).withTrimmedLeft (26.0f), juce::Justification::centredLeft);

    juce::AttributedString as;
    const auto f = lnf.labelFontRegular (14.5f);
    auto add = [&] (const juce::String& s, juce::Colour c) { as.append (s, f, c); };
    add (u8 ("1  Lautsprecher dorthin stellen, wo das Instrument stehen soll \xe2\x80\x93 Mikrofon(e) an die H\xc3\xb6rposition.\n"), cream);
    add (u8 ("2  In Logic: Saal auf eine Audiospur legen, deren Eingang das Mikrofon ist, und Input-Monitoring einschalten. "
             "Einfacher: die Standalone-App \xe2\x80\x9eKlangbild Saal\xe2\x80\x9c (Ein- und Ausgang unter Optionen).\n"), cream);
    add (u8 ("3  Abh\xc3\xb6re leise stellen, Pegel w\xc3\xa4hlen, starten: 8 s Sweep, danach 4 s Stille \xe2\x80\x93 bitte ruhig sein.\n"), cream);
    add (u8 ("Die Impulsantwort wird unter \xe2\x80\x9eR\xc3\xa4ume\xe2\x80\x9c gesichert und sofort geladen."), cream.withAlpha (0.65f));
    as.setLineSpacing (5.0f);
    as.draw (g, textArea.toFloat());

    g.setColour (cream.withAlpha (0.75f));
    g.setFont (lnf.labelFont (11.0f, 0.2f));
    g.drawText ("NAME", nameLabel.toFloat(), juce::Justification::centredLeft);
    g.drawText ("SWEEP-PEGEL", levelLabel.toFloat(), juce::Justification::centredLeft);

    // Fortschritt
    auto pa = progressArea.toFloat();
    g.setColour (juce::Colour (0xff0d0b09)); g.fillRoundedRectangle (pa, 4.0f);
    g.setColour (gold); g.fillRoundedRectangle (pa.withWidth (pa.getWidth() * proc.measureProgress.load()), 4.0f);
    g.setColour (cream.withAlpha (0.75f));
    g.drawText ("FORTSCHRITT", pa.translated (0.0f, -16.0f).withHeight (14.0f), juce::Justification::centredLeft);

    // Eingangspegel
    auto ia = inputArea.toFloat();
    const float in = proc.measureInput.load();
    const float frac = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (in, -60.0f) + 60.0f) / 60.0f);
    g.setColour (juce::Colour (0xff0d0b09)); g.fillRoundedRectangle (ia, 4.0f);
    g.setColour (in > 0.98f ? lampRed : (in > 0.5f ? lampAmber : lampGreen));
    g.fillRoundedRectangle (ia.withWidth (ia.getWidth() * frac), 4.0f);
    g.setColour (cream.withAlpha (0.75f));
    g.drawText ("EINGANG", ia.translated (0.0f, -16.0f).withHeight (14.0f), juce::Justification::centredLeft);

    g.setColour (proc.measureState.load() == SaalAudioProcessor::failed ? lampAmber : logo);
    g.setFont (lnf.labelFontRegular (14.0f));
    g.drawFittedText (status, statusArea, juce::Justification::centredLeft, 2);
}

void MeasurePanel::resized()
{
    auto r = getLocalBounds().reduced (26, 0);
    r.removeFromTop (48);
    textArea = r.removeFromTop (112);
    r.removeFromTop (8);
    auto row = r.removeFromTop (48);
    nameLabel = row.removeFromLeft (280).removeFromTop (14);
    levelLabel = row.removeFromLeft (360).removeFromTop (14);
    auto row2 = juce::Rectangle<int> (26, nameLabel.getBottom() + 4, getWidth() - 52, 30);
    name.setBounds (row2.removeFromLeft (260));
    row2.removeFromLeft (20);
    level.setBounds (row2.removeFromLeft (340));
    r.removeFromTop (14);
    auto bars = r.removeFromTop (34);
    progressArea = bars.removeFromLeft (getWidth() / 2 - 40).withTrimmedTop (18);
    bars.removeFromLeft (28);
    inputArea = bars.withTrimmedTop (18);
    r.removeFromTop (8);
    auto bottom = r.removeFromBottom (54).withTrimmedBottom (16);
    closeBtn.setBounds (bottom.removeFromRight (130));
    bottom.removeFromRight (12);
    startBtn.setBounds (bottom.removeFromRight (180));
    statusArea = bottom.withTrimmedRight (12);
}

//==============================================================================
SaalAudioProcessorEditor::SaalAudioProcessorEditor (SaalAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    addKnob ("mix",      "MIX (%)",          true, "0|25|50|75|100");
    addKnob ("out",      "AUSGANG (DB)",     false, "-12|-6|0|+6|+12");
    addKnob ("distance", "ABSTAND",          false, "nah||||fern");
    addKnob ("length",   u8 ("L\xc3\x84NGE (%)"), false, "50||100||||200");
    addKnob ("damping",  u8 ("D\xc3\x84MPFUNG"),  false);
    addKnob ("early",    "ERSTREFLEXIONEN",  false, "-12|-6|0|+6");
    addKnob ("predelay", "PRE-DELAY",        false);
    addKnob ("lowcut",   "LOW CUT",          false);
    addKnob ("highcut",  "HIGH CUT",         false);
    addKnob ("width",    "BREITE",           false);

    onSwitch.setButtonText ("x");
    onSwitch.getProperties().set ("onText", "AN");
    onSwitch.getProperties().set ("offText", "AUS");
    onSwitch.getProperties().set ("lamp", (int) lampRed.getARGB());
    addAndMakeVisible (onSwitch);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (proc.apvts, "on", onSwitch));
    rack::styleToggle (onSwitch);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    roomBox.beforePopup = [this] { fillRoomBox(); };
    roomBox.onChange = [this] { roomChosen(); };
    addAndMakeVisible (roomBox);
    fillRoomBox();

    loadBtn.setButtonText ("IR laden");
    loadBtn.setTooltip (u8 ("Eigene Impulsantwort (WAV, AIFF) laden \xe2\x80\x93 wird in den Ordner \xe2\x80\x9eR\xc3\xa4ume\xe2\x80\x9c kopiert"));
    loadBtn.onClick = [this] { chooseFile(); };
    measureBtn.setButtonText ("Raum messen");
    measureBtn.onClick = [this] { openMeasurePanel(); };
    addAndMakeVisible (loadBtn);
    addAndMakeVisible (measureBtn);

    addAndMakeVisible (view);
    addAndMakeVisible (vu);
    setSize (rackW, rackH);
    startTimerHz (30);
}

SaalAudioProcessorEditor::~SaalAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

SaalAudioProcessorEditor::Knob& SaalAudioProcessorEditor::addKnob (const juce::String& paramID, const juce::String& name, bool big, const juce::String& steps)
{
    auto k = std::make_unique<Knob>();
    k->name = name;
    k->big = big;
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    k->slider.setMouseDragSensitivity (big ? 320 : 220);
    if (steps.isNotEmpty())
        k->slider.getProperties().set ("stepLabels", steps);
    addAndMakeVisible (k->slider);
    k->attachment = std::make_unique<SliderAttachment> (proc.apvts, paramID, k->slider);
    if (auto* rp = proc.apvts.getParameter (paramID))
    {
        k->slider.setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));
        k->slider.setTooltip (rp->getName (64));
    }
    knobs.push_back (std::move (k));
    return *knobs.back();
}

//==============================================================================
void SaalAudioProcessorEditor::fillRoomBox()
{
    roomBox.clear (juce::dontSendNotification);
    const auto& rs = saal::rooms();
    roomBox.addSectionHeading (u8 ("EINGEBAUTE R\xc3\x84UME"));
    for (int i = 0; i < (int) rs.size(); ++i)
        roomBox.addItem (u8 (rs[(size_t) i].name), i + 1);

    roomFiles = SaalAudioProcessor::roomsFolder().findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");
    std::sort (roomFiles.begin(), roomFiles.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
    const auto cur = proc.customFile();
    if (cur.existsAsFile() && ! roomFiles.contains (cur))
        roomFiles.add (cur);
    roomBox.addSeparator();
    roomBox.addSectionHeading (u8 ("EIGENE R\xc3\x84UME"));
    if (roomFiles.isEmpty())
        roomBox.addItem (u8 ("(noch keine \xe2\x80\x93 IR laden oder Raum messen)"), 99), roomBox.setItemEnabled (99, false);
    for (int i = 0; i < roomFiles.size(); ++i)
        roomBox.addItem (roomFiles[i].getFileNameWithoutExtension(), 101 + i);
    roomBox.addSeparator();
    roomBox.addItem (u8 ("Impulsantwort laden \xe2\x80\xa6"), 901);
    roomBox.addItem (u8 ("Raum messen \xe2\x80\xa6"), 902);
    roomBox.addItem (u8 ("Ordner \xe2\x80\x9eR\xc3\xa4ume\xe2\x80\x9c zeigen"), 903);
    syncRoomBox();
}

void SaalAudioProcessorEditor::syncRoomBox()
{
    int id = (int) proc.apvts.getRawParameterValue ("room")->load() + 1;
    if (proc.isCustom())
    {
        const int idx = roomFiles.indexOf (proc.customFile());
        id = idx >= 0 ? 101 + idx : 0;
        if (idx < 0) { fillRoomBox(); return; }
    }
    if (roomBox.getSelectedId() != id)
        roomBox.setSelectedId (id, juce::dontSendNotification);
}

void SaalAudioProcessorEditor::roomChosen()
{
    const int id = roomBox.getSelectedId();
    if (id >= 1 && id <= (int) saal::rooms().size())
        proc.useBuiltIn (id - 1);
    else if (id >= 101 && id < 101 + roomFiles.size())
    {
        juce::String err;
        if (! proc.loadIrFile (roomFiles[id - 101], err))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Klangbild Saal", err);
    }
    else if (id == 901) chooseFile();
    else if (id == 902) openMeasurePanel();
    else if (id == 903) { SaalAudioProcessor::roomsFolder().createDirectory(); SaalAudioProcessor::roomsFolder().revealToUser(); }
    syncRoomBox();
}

void SaalAudioProcessorEditor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Impulsantwort laden", juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.wav;*.aif;*.aiff;*.flac");
    juce::Component::SafePointer<SaalAudioProcessorEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr) return;
        const auto f = fc.getResult();
        if (! f.existsAsFile()) { safe->syncRoomBox(); return; }
        // Kopie in den Ordner „Räume“, damit sie immer in der Liste steht
        auto folder = SaalAudioProcessor::roomsFolder();
        folder.createDirectory();
        auto target = f;
        if (! f.isAChildOf (folder))
        {
            target = folder.getChildFile (f.getFileName());
            if (! target.existsAsFile() && ! f.copyFileTo (target))
                target = f;
        }
        juce::String err;
        if (! safe->proc.loadIrFile (target, err))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Klangbild Saal", err);
        safe->fillRoomBox();
    });
}

void SaalAudioProcessorEditor::openMeasurePanel()
{
    if (measure != nullptr) return;
    if (proc.measureState.load() == SaalAudioProcessor::done || proc.measureState.load() == SaalAudioProcessor::failed)
        proc.measureState = SaalAudioProcessor::idle;
    measure = std::make_unique<MeasurePanel> (proc, lnf);
    measure->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<SaalAudioProcessorEditor> (this)] { if (safe != nullptr) { safe->measure.reset(); safe->dimmer.setVisible (false); safe->fillRoomBox(); } }); };
    dimmer.setInterceptsMouseClicks (true, true);
    addAndMakeVisible (dimmer);
    dimmer.setBounds (getLocalBounds());
    addAndMakeVisible (*measure);
    measure->setBounds (juce::Rectangle<int> (820, 380).withCentre (getLocalBounds().getCentre()));
    syncRoomBox();
}

void SaalAudioProcessorEditor::timerCallback()
{
    constexpr float dt = 1.0f / 30.0f;
    vu.update (juce::jmax (proc.vuL.load(), proc.vuR.load()),
               juce::jmax (proc.meterL.exchange (0.0f), proc.meterR.exchange (0.0f)), dt);

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);

    if (proc.displayVersion.load() != lastDisplayVersion)
    {
        lastDisplayVersion = proc.displayVersion.load();
        const auto d = proc.getDisplay();
        view.setData (d);
        const auto desc = d.custom ? u8 ("Eigene Impulsantwort \xe2\x80\x93 Abstand gilt nur f\xc3\xbcr eingebaute R\xc3\xa4ume")
                                   : u8 (saal::rooms()[(size_t) juce::jlimit (0, (int) saal::rooms().size() - 1, d.room)].short_);
        if (desc != description) { description = desc; renderPanel(); repaint(); }
        knobs[2]->slider.setEnabled (! d.custom);
        knobs[2]->slider.setAlpha (d.custom ? 0.4f : 1.0f);
        syncRoomBox();
    }
    if (measure != nullptr)
        measure->tick();
}

//==============================================================================
void SaalAudioProcessorEditor::renderPanel()
{
    panelImage = rack::renderPanel (rackW, rackH, 23);
    juce::Graphics g (panelImage);
    const juce::String titles[] = { "RAUM", "AUSGANG", "MISCHUNG", "KLANG" };
    for (size_t i = 0; i < sections.size(); ++i)
        rack::drawSection (g, sections[i], titles[i], lnf);
    rack::drawBezel (g, vuBezel);
    rack::drawBezel (g, viewBezel, 10.0f);
    for (auto& k : knobs)
        rack::drawKnobLabel (g, k->slider, k->name, lnf, k->big ? 14.0f : 12.0f);
    g.setColour (rack::silkDim);
    g.setFont (lnf.labelFontRegular (13.0f));
    g.drawText (description, descArea.toFloat(), juce::Justification::centredLeft, true);
    rack::drawLabel (g, "RAUM", switchLabel.toFloat(), lnf, 11.0f, juce::Justification::centredLeft);
    rack::drawNameplate (g, nameplate, "SAAL", rack::u8 ("STUDIO-RAUM \xc2\xb7 FALTUNG"), lnf);
    rack::drawLabel (g, "PROGRAMM", presetBox.getBounds().toFloat().withY ((float) presetBox.getY() - 16.0f).withHeight (14.0f), lnf, 11.0f, juce::Justification::centredLeft);
    rack::drawTypeTag (g, rack::u8 ("TYP KB-250  \xc2\xb7  NR. 1112"), tagArea, lnf);
}

void SaalAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (panelImage, 0, 0);
}

void SaalAudioProcessorEditor::resized()
{
    const int top = 30;
    sections[0] = { 72, top, 708, 300 };
    sections[1] = { 792, top, 262, 300 };
    sections[2] = { 1066, top, 150, 300 };
    sections[3] = { 72, top + 320, 1144, rackH - 34 - (top + 320) };

    roomBox.setBounds (92, top + 20, 400, 30);
    loadBtn.setBounds (504, top + 20, 120, 30);
    measureBtn.setBounds (636, top + 20, 124, 30);
    descArea = { 94, top + 56, 660, 18 };
    viewBezel = { 88, top + 80, 676, 206 };
    view.setBounds (viewBezel.reduced (6));

    vuBezel = juce::Rectangle<int> (232, 162).withCentre ({ sections[1].getCentreX(), top + 108 });
    vu.setBounds (vuBezel.reduced (9));
    knobs[1]->slider.setBounds (juce::Rectangle<int> (92, 92).withCentre ({ sections[1].getCentreX(), top + 238 }));
    knobs[0]->slider.setBounds (juce::Rectangle<int> (138, 138).withCentre ({ sections[2].getCentreX(), top + 140 }));

    const auto rs = sections[3];
    const int nk = 8, kw = 112, gap = (rs.getWidth() - nk * kw) / (nk + 1);
    for (int i = 0; i < nk; ++i)
        knobs[(size_t) i + 2]->slider.setBounds (rs.getX() + gap + i * (kw + gap), rs.getY() + 24, kw, kw);

    const int colX = 1230, colW = rackW - 56 - 14 - colX;
    nameplate = { colX, top - 8, colW, 88 };
    presetBox.setBounds (colX, top + 112, colW, 28);
    switchLabel = { colX + 4, top + 168, colW, 14 };
    onSwitch.setBounds (colX + 4, top + 186, 150, 40);
    tagArea = { colX - 60, rackH - 28, colW + 60, 16 };
    dimmer.setBounds (getLocalBounds());
    if (measure != nullptr)
        measure->setBounds (juce::Rectangle<int> (820, 380).withCentre (getLocalBounds().getCentre()));
    renderPanel();
}
