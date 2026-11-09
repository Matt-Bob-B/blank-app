#include "ControlBar.h"
#include "ui/Commands.h"

namespace tess
{

//==============================================================================
TransportDisplay::TransportDisplay (te::Edit& e) : edit (e)
{
    startTimerHz (30);
}

juce::Rectangle<int> TransportDisplay::positionArea() const   { return getLocalBounds().withWidth (getWidth() * 50 / 100); }
juce::Rectangle<int> TransportDisplay::tempoArea() const      { return getLocalBounds().withTrimmedLeft (getWidth() * 50 / 100).withWidth (getWidth() * 27 / 100); }
juce::Rectangle<int> TransportDisplay::signatureArea() const  { return getLocalBounds().withTrimmedLeft (getWidth() * 77 / 100); }

void TransportDisplay::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = getLocalBounds().toFloat();

    g.setColour (colour::lcdBackground);
    g.fillRoundedRectangle (r, (float) size::radius);
    g.setColour (colour::hairline);
    g.drawRoundedRectangle (r.reduced (0.5f), (float) size::radius, 1.0f);

    auto& transport = edit.getTransport();
    auto& ts = edit.tempoSequence;
    const auto pos = transport.getPosition();
    const auto bb = ts.toBarsAndBeats (pos);

    const int bar = bb.bars + 1;
    const int beat = bb.getWholeBeats() + 1;
    const double frac = bb.getFractionalBeats().inBeats();
    const int sixteenth = (int) (frac * 4.0) + 1;
    const int ticks = (int) std::fmod (frac * 960.0, 240.0);

    const double secs = pos.inSeconds();
    const int mins = (int) (secs / 60.0);
    const double remSecs = secs - mins * 60.0;

    // Position: bars.beats.div.ticks large, clock time small underneath.
    auto posArea = positionArea().reduced (12, 6);
    g.setColour (colour::lcdText);
    g.setFont (monoFont (21.0f));
    g.drawText (juce::String::formatted ("%3d  %d  %d  %3d", bar, beat, sixteenth, ticks),
                posArea.removeFromTop (posArea.getHeight() * 62 / 100), juce::Justification::centredLeft);
    g.setColour (colour::textSecondary);
    g.setFont (monoFont (11.5f));
    g.drawText (juce::String::formatted ("%02d:%06.3f", mins, remSecs), posArea, juce::Justification::centredLeft);

    auto drawField = [&] (juce::Rectangle<int> area, const juce::String& value, const juce::String& caption)
    {
        g.setColour (colour::hairline);
        g.drawVerticalLine (area.getX(), 8.0f, (float) getHeight() - 8.0f);
        area = area.reduced (10, 6);
        g.setColour (colour::lcdText);
        g.setFont (monoFont (19.0f));
        g.drawText (value, area.removeFromTop (area.getHeight() * 62 / 100), juce::Justification::centred);
        g.setColour (colour::textSecondary);
        g.setFont (font (10.5f));
        g.drawText (caption, area, juce::Justification::centred);
    };

    auto* tempo = ts.getTempo (0);
    auto* sig = ts.getTimeSig (0);
    drawField (tempoArea(), juce::String (tempo->getBpm(), tempo->getBpm() == std::floor (tempo->getBpm()) ? 0 : 1), "BPM");
    drawField (signatureArea(), sig->getStringTimeSig(), "SIGNATURE");

    if (transport.isRecording())
    {
        g.setColour (colour::record);
        g.fillEllipse (r.getRight() - 12.0f, 6.0f, 6.0f, 6.0f);
    }
}

void TransportDisplay::mouseDown (const juce::MouseEvent& e)
{
    draggingTempo = tempoArea().contains (e.getPosition());
    tempoAtDragStart = edit.tempoSequence.getTempo (0)->getBpm();

    if (draggingTempo)
        edit.getUndoManager().beginNewTransaction ("Change Tempo");
}

void TransportDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingTempo)
        return;

    const double step = e.mods.isShiftDown() ? 0.1 : 0.5;
    auto bpm = juce::jlimit (20.0, 300.0, tempoAtDragStart - e.getDistanceFromDragStartY() * step);
    edit.tempoSequence.getTempo (0)->setBpm (e.mods.isShiftDown() ? bpm : std::round (bpm));
    repaint();
}

void TransportDisplay::mouseUp (const juce::MouseEvent&)  { draggingTempo = false; }

void TransportDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (tempoArea().contains (e.getPosition()))
    {
        editValue (tempoArea(), juce::String (edit.tempoSequence.getTempo (0)->getBpm()), [this] (const juce::String& s)
        {
            auto bpm = s.getDoubleValue();
            if (bpm >= 20.0 && bpm <= 300.0)
                edit.tempoSequence.getTempo (0)->setBpm (bpm);
        });
    }
    else if (signatureArea().contains (e.getPosition()))
    {
        editValue (signatureArea(), edit.tempoSequence.getTimeSig (0)->getStringTimeSig(), [this] (const juce::String& s)
        {
            const int num = s.upToFirstOccurrenceOf ("/", false, false).getIntValue();
            const int den = s.fromFirstOccurrenceOf ("/", false, false).getIntValue();

            if (num >= 1 && num <= 32 && (den == 2 || den == 4 || den == 8 || den == 16))
                edit.tempoSequence.getTimeSig (0)->setStringTimeSig (juce::String (num) + "/" + juce::String (den));
        });
    }
}

void TransportDisplay::editValue (juce::Rectangle<int> area, const juce::String& current,
                                  std::function<void (const juce::String&)> apply)
{
    inlineEditor = std::make_unique<juce::TextEditor>();
    inlineEditor->setFont (theme::monoFont (18.0f));
    inlineEditor->setJustification (juce::Justification::centred);
    inlineEditor->setText (current, false);
    inlineEditor->selectAll();
    inlineEditor->setBounds (area.reduced (6, 8));
    addAndMakeVisible (*inlineEditor);
    inlineEditor->grabKeyboardFocus();

    auto finish = [this, apply] (bool commit)
    {
        if (inlineEditor == nullptr)
            return;

        auto text = inlineEditor->getText();
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<TransportDisplay> (this)] { if (sp) sp->inlineEditor.reset(); });

        if (commit)
        {
            edit.getUndoManager().beginNewTransaction();
            apply (text);
        }

        repaint();
    };

    inlineEditor->onReturnKey = [finish] { finish (true); };
    inlineEditor->onEscapeKey = [finish] { finish (false); };
    inlineEditor->onFocusLost = [finish] { finish (true); };
}

//==============================================================================
ControlBar::ControlBar (EditViewState& vs)
    : viewState (vs), edit (vs.edit), display (vs.edit)
{
    for (auto* b : { &libraryButton, &toStartButton, &stopButton, &playButton, &recordButton,
                     &cycleButton, &metronomeButton, &pianoRollButton, &mixerButton })
        addAndMakeVisible (b);

    addAndMakeVisible (display);

    recordButton.setIdleGlyphColour (theme::colour::record);

    libraryButton.onClick   = [this] { if (onToggleLibrary) onToggleLibrary(); };
    pianoRollButton.onClick = [this] { if (onTogglePianoRoll) onTogglePianoRoll(); };
    mixerButton.onClick     = [this] { if (onToggleMixer) onToggleMixer(); };

    toStartButton.onClick   = [this] { commands::goToStart (edit); };
    stopButton.onClick      = [this] { commands::stop (edit); };
    playButton.onClick      = [this] { commands::togglePlay (edit); };
    recordButton.onClick    = [this] { commands::toggleRecord (edit); };
    cycleButton.onClick     = [this] { commands::toggleCycle (viewState); };
    metronomeButton.onClick = [this] { commands::toggleMetronome (edit); };

    startTimerHz (15);
}

void ControlBar::setPanelStates (bool libraryShown, bool pianoRollShown, bool mixerShown)
{
    libraryButton.setToggleState (libraryShown, juce::dontSendNotification);
    pianoRollButton.setToggleState (pianoRollShown, juce::dontSendNotification);
    mixerButton.setToggleState (mixerShown, juce::dontSendNotification);
}

void ControlBar::timerCallback()
{
    auto& t = edit.getTransport();
    playButton.setToggleState (t.isPlaying() && ! t.isRecording(), juce::dontSendNotification);
    recordButton.setToggleState (t.isRecording(), juce::dontSendNotification);
    cycleButton.setToggleState (t.looping.get(), juce::dontSendNotification);
    metronomeButton.setToggleState (edit.clickTrackEnabled.get(), juce::dontSendNotification);
}

void ControlBar::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (theme::colour::panelRaised, 0, 0,
                                             theme::colour::panel, 0, (float) getHeight(), false));
    g.fillAll();
    g.setColour (theme::colour::separator);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void ControlBar::resized()
{
    auto r = getLocalBounds().reduced (12, 9);
    const int b = r.getHeight();
    const int gap = 6;

    libraryButton.setBounds (r.removeFromLeft (b + 6));

    mixerButton.setBounds (r.removeFromRight (b + 6));
    r.removeFromRight (gap);
    pianoRollButton.setBounds (r.removeFromRight (b + 6));

    // Centre group: transport | LCD | metronome
    const int lcdW = juce::jmin (380, juce::jmax (280, r.getWidth() / 3));
    const int groupW = 5 * (b + 8) + gap * 5 + lcdW + gap * 2 + b + 8;
    auto group = r.withSizeKeepingCentre (juce::jmin (groupW, r.getWidth()), r.getHeight());

    for (auto* btn : std::initializer_list<juce::Component*> { &toStartButton, &stopButton, &playButton, &recordButton, &cycleButton })
    {
        btn->setBounds (group.removeFromLeft (b + 8));
        group.removeFromLeft (gap);
    }

    group.removeFromLeft (gap);
    metronomeButton.setBounds (group.removeFromRight (b + 8));
    group.removeFromRight (gap * 2);
    display.setBounds (group.expanded (0, 3));
}

} // namespace tess
