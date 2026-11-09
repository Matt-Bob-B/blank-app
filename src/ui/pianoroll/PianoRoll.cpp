#include "PianoRoll.h"
#include "engine/MidiTools.h"
#include "ui/theme/Theme.h"

namespace tess
{

using namespace theme;

namespace
{
    constexpr int keyboardWidth = 64;
    constexpr int toolbarHeight = 36;
    constexpr int pianoRulerHeight = 22;
    constexpr int edgeHandle = 6;

    bool isBlackKey (int pitch)
    {
        const int n = pitch % 12;
        return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
    }

    struct GridOption { const char* name; double beats; };
    const GridOption gridOptions[] = { { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/8T", 1.0 / 3.0 }, { "1/16", 0.25 },
                                       { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 } };
}

PianoRoll::PianoRoll (EditViewState& vs) : viewState (vs), edit (vs.edit)
{
    viewState.addChangeListener (this);
    setWantsKeyboardFocus (true);

    int id = 1;
    for (auto& o : gridOptions)
        gridBox.addItem (o.name, id++);

    gridBox.setSelectedId (4, juce::dontSendNotification);
    gridBox.setTooltip ("Note grid");
    gridBox.onChange = [this]
    {
        viewState.pianoRollGrid = gridOptions[gridBox.getSelectedId() - 1].beats;
        repaint();
    };
    viewState.pianoRollGrid = 0.25;

    quantiseButton.setTooltip ("Quantise selected notes, or all notes if none are selected (Q)");
    quantiseButton.onClick = [this] { quantise(); };

    velocitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    velocitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 20);
    velocitySlider.setRange (1, 127, 1);
    velocitySlider.setValue (100, juce::dontSendNotification);
    velocitySlider.onDragStart = [this] { edit.getUndoManager().beginNewTransaction ("Note Velocity"); };
    velocitySlider.onValueChange = [this] { applyVelocityToSelection ((int) velocitySlider.getValue()); };

    velocityLabel.setText ("Velocity", juce::dontSendNotification);
    velocityLabel.setFont (font (12.0f));
    velocityLabel.setColour (juce::Label::textColourId, colour::textSecondary);

    regionLabel.setFont (font (13.0f, true));

    for (auto* c : std::initializer_list<juce::Component*> { &gridBox, &quantiseButton, &velocitySlider, &regionLabel, &velocityLabel })
        addAndMakeVisible (c);

    changeListenerCallback (nullptr);
    startTimerHz (30);
}

PianoRoll::~PianoRoll()
{
    if (currentClip != nullptr)
        currentClip->state.removeListener (this);

    viewState.removeChangeListener (this);
}

te::MidiClip* PianoRoll::getClip() const  { return dynamic_cast<te::MidiClip*> (currentClip.get()); }

void PianoRoll::changeListenerCallback (juce::ChangeBroadcaster*)
{
    auto* selected = dynamic_cast<te::MidiClip*> (viewState.getSelectedClip());

    if (selected != currentClip.get() && selected != nullptr)
    {
        if (currentClip != nullptr)
            currentClip->state.removeListener (this);

        currentClip = selected;
        currentClip->state.addListener (this);
        selection.clear();
        centreOnNotes();
    }

    regionLabel.setText (getClip() != nullptr ? getClip()->getName() : juce::String ("No MIDI region selected"),
                         juce::dontSendNotification);
    repaint();
}

void PianoRoll::centreOnNotes()
{
    auto* clip = getClip();

    if (clip == nullptr)
        return;

    scrollBeats = juce::jmax (0.0, viewState.timeToBeats (clip->getPosition().getStart()) - 0.5);

    auto notes = clip->getSequence().getNotes();
    int lo = 60, hi = 72;

    if (! notes.isEmpty())
    {
        lo = 127; hi = 0;
        for (auto* n : notes) { lo = juce::jmin (lo, n->getNoteNumber()); hi = juce::jmax (hi, n->getNoteNumber()); }
    }

    const int visibleRows = juce::jmax (12, noteArea().getHeight() / noteHeight);
    topPitch = juce::jlimit (visibleRows - 1, 127, (lo + hi) / 2 + visibleRows / 2);
}

//==============================================================================
juce::Rectangle<int> PianoRoll::rulerArea() const     { return { keyboardWidth, toolbarHeight, getWidth() - keyboardWidth, pianoRulerHeight }; }
juce::Rectangle<int> PianoRoll::noteArea() const      { return getLocalBounds().withTrimmedTop (toolbarHeight + pianoRulerHeight).withTrimmedLeft (keyboardWidth); }
juce::Rectangle<int> PianoRoll::keyboardArea() const  { return getLocalBounds().withTrimmedTop (toolbarHeight + pianoRulerHeight).withWidth (keyboardWidth); }

double PianoRoll::clipContentStartBeat() const
{
    auto* clip = getClip();
    return clip == nullptr ? 0.0 : viewState.timeToBeats (clip->getPosition().getStart()) - clip->getOffsetInBeats().inBeats();
}

int PianoRoll::beatToX (double b) const     { return keyboardWidth + juce::roundToInt ((b - scrollBeats) * pixelsPerBeat); }
double PianoRoll::xToBeat (int x) const     { return scrollBeats + (x - keyboardWidth) / pixelsPerBeat; }
int PianoRoll::pitchToY (int pitch) const   { return noteArea().getY() + (topPitch - pitch) * noteHeight; }
int PianoRoll::yToPitch (int y) const       { return juce::jlimit (0, 127, topPitch - (y - noteArea().getY()) / noteHeight); }

juce::Rectangle<int> PianoRoll::noteBounds (te::MidiNote& n) const
{
    const double s = clipContentStartBeat() + n.getStartBeat().inBeats();
    const int x1 = beatToX (s), x2 = beatToX (s + n.getLengthBeats().inBeats());
    return { x1, pitchToY (n.getNoteNumber()) + 1, juce::jmax (3, x2 - x1), noteHeight - 1 };
}

te::MidiNote* PianoRoll::noteAt (juce::Point<int> p) const
{
    if (auto* clip = getClip())
    {
        auto notes = clip->getSequence().getNotes();

        for (int i = notes.size(); --i >= 0;)
            if (noteBounds (*notes[i]).contains (p))
                return notes[i];
    }

    return nullptr;
}

//==============================================================================
void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (colour::lane);

    // Toolbar
    auto bar = getLocalBounds().removeFromTop (toolbarHeight);
    g.setColour (colour::panel);
    g.fillRect (bar);
    g.setColour (colour::separator);
    g.drawHorizontalLine (bar.getBottom() - 1, 0.0f, (float) getWidth());

    auto area = noteArea();
    auto* clip = getClip();

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (area);

        // Rows
        for (int p = topPitch; p >= 0 && pitchToY (p) < area.getBottom(); --p)
        {
            g.setColour (isBlackKey (p) ? colour::laneAlt.darker (0.25f) : colour::laneAlt);
            g.fillRect (area.getX(), pitchToY (p), area.getWidth(), noteHeight);

            if (p % 12 == 0)
            {
                g.setColour (colour::gridBar.withMultipliedAlpha (0.6f));
                g.drawHorizontalLine (pitchToY (p) + noteHeight - 1, (float) area.getX(), (float) area.getRight());
            }
        }

        // Vertical grid
        const double bpb = viewState.beatsPerBar();
        const double grid = viewState.pianoRollGrid;
        const double first = std::floor (scrollBeats / grid) * grid;

        for (double b = first; beatToX (b) < area.getRight(); b += grid)
        {
            const bool isBar = std::abs (std::fmod (b + 1.0e-9, bpb)) < 1.0e-6;
            const bool isBeat = std::abs (b - std::round (b)) < 1.0e-6;
            g.setColour (isBar ? colour::gridBar.withMultipliedAlpha (1.6f) : isBeat ? colour::gridBar : colour::gridBeat);
            g.drawVerticalLine (beatToX (b), (float) area.getY(), (float) area.getBottom());
        }

        if (clip == nullptr)
        {
            g.setColour (colour::textTertiary);
            g.setFont (font (15.0f));
            g.drawText ("Double-click a MIDI region (or an empty spot on an instrument track) to edit notes",
                        area, juce::Justification::centred);
        }
        else
        {
            // Shade outside the region.
            const int rx1 = beatToX (viewState.timeToBeats (clip->getPosition().getStart()));
            const int rx2 = beatToX (viewState.timeToBeats (clip->getPosition().getEnd()));
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRect (area.withRight (rx1));
            g.fillRect (area.withLeft (rx2));

            auto trackCol = colour::trackColour (te::getAudioTracks (edit).indexOf (dynamic_cast<te::AudioTrack*> (clip->getTrack())));

            for (auto* n : clip->getSequence().getNotes())
            {
                auto r = noteBounds (*n).toFloat();
                const bool sel = selection.contains (n);
                const float velAmount = (float) n->getVelocity() / 127.0f;

                g.setColour (trackCol.withMultipliedBrightness (0.55f + 0.55f * velAmount).withAlpha (sel ? 1.0f : 0.9f));
                g.fillRoundedRectangle (r, 3.0f);
                g.setColour (sel ? juce::Colours::white : juce::Colours::black.withAlpha (0.45f));
                g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, sel ? 1.5f : 1.0f);

                if (r.getWidth() > 34 && noteHeight >= 12)
                {
                    g.setColour (juce::Colour (0xff1c1c1f).withAlpha (0.8f));
                    g.setFont (font (10.0f, true));
                    g.drawText (noteName (n->getNoteNumber()), r.reduced (4.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, false);
                }
            }
        }

        if (drag == Drag::marquee)
        {
            g.setColour (colour::accent.withAlpha (0.15f));
            g.fillRect (marquee);
            g.setColour (colour::accent);
            g.drawRect (marquee);
        }
    }

    drawRuler (g);
    drawKeyboard (g);

    g.setColour (colour::panel);
    g.fillRect (0, toolbarHeight, keyboardWidth, pianoRulerHeight);
    g.setColour (colour::separator);
    g.drawVerticalLine (keyboardWidth - 1, (float) toolbarHeight, (float) getHeight());
}

void PianoRoll::paintOverChildren (juce::Graphics& g)
{
    const int x = beatToX (viewState.timeToBeats (edit.getTransport().getPosition()));

    if (x >= noteArea().getX())
    {
        g.setColour (colour::playhead.withAlpha (0.85f));
        g.fillRect (x, rulerArea().getY(), 1, getHeight() - rulerArea().getY());
    }
}

void PianoRoll::drawRuler (juce::Graphics& g)
{
    auto r = rulerArea();
    g.setColour (colour::panel);
    g.fillRect (r);
    g.setColour (colour::separator);
    g.drawHorizontalLine (r.getBottom() - 1, (float) r.getX(), (float) r.getRight());

    const double bpb = viewState.beatsPerBar();
    g.setFont (font (11.0f));

    for (double b = std::floor (scrollBeats); beatToX (b) < r.getRight(); b += 1.0)
    {
        const int x = beatToX (b);
        const bool isBar = std::abs (std::fmod (b, bpb)) < 1.0e-6;
        g.setColour (isBar ? colour::textSecondary : colour::textTertiary);
        g.drawVerticalLine (x, (float) r.getBottom() - (isBar ? 10.0f : 5.0f), (float) r.getBottom());

        if (isBar)
            g.drawText (juce::String ((int) std::round (b / bpb) + 1), x + 3, r.getY() + 2, 40, 12, juce::Justification::centredLeft);
        else if (pixelsPerBeat > 60)
            g.drawText (juce::String ((int) std::floor (b / bpb + 1.0e-9) + 1) + "." + juce::String ((int) std::round (std::fmod (b, bpb)) + 1),
                        x + 3, r.getY() + 2, 40, 12, juce::Justification::centredLeft);
    }
}

void PianoRoll::drawKeyboard (juce::Graphics& g)
{
    auto kb = keyboardArea();
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (kb);
    g.fillAll (juce::Colour (0xffe8e8ec));

    for (int p = topPitch; p >= 0 && pitchToY (p) < kb.getBottom(); --p)
    {
        auto row = juce::Rectangle<int> (kb.getX(), pitchToY (p), kb.getWidth(), noteHeight);

        if (isBlackKey (p))
        {
            g.setColour (juce::Colour (0xff222226));
            g.fillRoundedRectangle (row.withWidth (kb.getWidth() * 62 / 100).toFloat().reduced (0.0f, 0.5f), 2.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xffb8b8c0));
            if (p % 12 == 0 || p % 12 == 5)
                g.drawHorizontalLine (row.getBottom() - 1, (float) kb.getX(), (float) kb.getRight());
        }

        if (p % 12 == 0)
        {
            g.setColour (juce::Colour (0xff55555c));
            g.setFont (font (10.0f));
            g.drawText (noteName (p), row.withTrimmedRight (4), juce::Justification::centredRight);
        }
    }
}

void PianoRoll::resized()
{
    auto bar = getLocalBounds().removeFromTop (toolbarHeight).reduced (10, 6);
    regionLabel.setBounds (bar.removeFromLeft (220));
    quantiseButton.setBounds (bar.removeFromRight (84));
    bar.removeFromRight (6);
    gridBox.setBounds (bar.removeFromRight (72));
    bar.removeFromRight (16);
    velocitySlider.setBounds (bar.removeFromRight (180));
    velocityLabel.setBounds (bar.removeFromRight (60));
}

void PianoRoll::timerCallback()
{
    const int x = beatToX (viewState.timeToBeats (edit.getTransport().getPosition()));

    if (x != lastPlayheadX)
    {
        repaint (juce::Rectangle<int> (juce::jmin (x, lastPlayheadX) - 2, 0, std::abs (x - lastPlayheadX) + 5, getHeight()));
        lastPlayheadX = x;
    }
}

//==============================================================================
void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    auto* n = noteAt (e.getPosition());
    setMouseCursor (n != nullptr && e.x >= noteBounds (*n).getRight() - edgeHandle ? juce::MouseCursor::LeftRightResizeCursor
                                                                                   : juce::MouseCursor::NormalCursor);
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    auto* clip = getClip();
    drag = Drag::none;

    if (clip == nullptr || ! noteArea().contains (e.getPosition()))
    {
        if (rulerArea().contains (e.getPosition()))
            edit.getTransport().setPosition (viewState.beatsToTime (juce::jmax (0.0, xToBeat (e.x))));
        return;
    }

    edit.getUndoManager().beginNewTransaction ("Edit Notes");
    dragStartBeat = xToBeat (e.x);
    dragStartPitch = yToPitch (e.y);

    if (auto* n = noteAt (e.getPosition()))
    {
        if (e.mods.isShiftDown())
        {
            if (selection.contains (n)) selection.removeFirstMatchingValue (n);
            else selection.add (n);
        }
        else if (! selection.contains (n))
        {
            selection = { n };
        }

        dragNote = n;
        drag = e.x >= noteBounds (*n).getRight() - edgeHandle ? Drag::resize : Drag::move;
        velocitySlider.setValue (n->getVelocity(), juce::dontSendNotification);
    }
    else if (e.mods.isShiftDown() || e.mods.isCommandDown())
    {
        drag = Drag::marquee;
        marquee = { e.x, e.y, 0, 0 };
    }
    else
    {
        // Click on empty grid: add a note, then let the drag set its length.
        const double grid = viewState.pianoRollGrid;
        const double editBeat = e.mods.isAltDown() ? dragStartBeat : std::floor (dragStartBeat / grid) * grid;
        auto* n = addNote (*clip, dragStartPitch, editBeat - clipContentStartBeat(), lastNoteLength, (int) velocitySlider.getValue());
        selection = { n };
        dragNote = n;
        drag = Drag::resize;
    }

    dragOriginals.clear();
    for (auto* n : selection)
        dragOriginals.push_back ({ n, n->getStartBeat().inBeats(), n->getLengthBeats().inBeats(), n->getNoteNumber() });

    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    auto* clip = getClip();
    if (clip == nullptr) return;

    auto* um = &edit.getUndoManager();
    const double grid = viewState.pianoRollGrid;
    const bool free = e.mods.isAltDown();
    const double deltaBeats = xToBeat (e.x) - dragStartBeat;
    const double snappedDelta = free ? deltaBeats : std::round (deltaBeats / grid) * grid;

    if (drag == Drag::move)
    {
        const int deltaPitch = yToPitch (e.y) - dragStartPitch;

        for (auto& o : dragOriginals)
        {
            o.note->setStartAndLength (te::BeatPosition::fromBeats (juce::jmax (0.0, o.start + snappedDelta)),
                                       te::BeatDuration::fromBeats (o.length), um);
            o.note->setNoteNumber (juce::jlimit (0, 127, o.pitch + deltaPitch), um);
        }
    }
    else if (drag == Drag::resize)
    {
        for (auto& o : dragOriginals)
        {
            double newLen = o.length + snappedDelta;
            newLen = free ? newLen : std::round (newLen / grid) * grid;
            newLen = juce::jmax (free ? 1.0 / 64.0 : grid, newLen);
            o.note->setStartAndLength (te::BeatPosition::fromBeats (o.start), te::BeatDuration::fromBeats (newLen), um);

            if (o.note == dragNote)
                lastNoteLength = newLen;
        }
    }
    else if (drag == Drag::marquee)
    {
        marquee = juce::Rectangle<int> (e.getMouseDownPosition(), e.getPosition());
        selection.clear();

        for (auto* n : clip->getSequence().getNotes())
            if (marquee.intersects (noteBounds (*n)))
                selection.add (n);
    }

    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    drag = Drag::none;
    dragNote = nullptr;
    dragOriginals.clear();
    repaint();
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (auto* n = noteAt (e.getPosition()))
    {
        selection = { n };
        deleteSelectedNotes();
    }
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const double anchor = xToBeat (e.x);
        pixelsPerBeat = juce::jlimit (12.0, 600.0, pixelsPerBeat * (w.deltaY > 0 ? 1.15 : 1.0 / 1.15));
        scrollBeats = juce::jmax (0.0, anchor - (e.x - keyboardWidth) / pixelsPerBeat);
    }
    else if (e.mods.isShiftDown() || w.deltaX != 0.0f)
    {
        const float d = e.mods.isShiftDown() ? w.deltaY : w.deltaX;
        scrollBeats = juce::jmax (0.0, scrollBeats - d * 250.0 / pixelsPerBeat);
    }
    else
    {
        const int visibleRows = noteArea().getHeight() / noteHeight;
        topPitch = juce::jlimit (visibleRows - 1, 127, topPitch + juce::roundToInt (w.deltaY * 20.0f));
    }

    repaint();
}

bool PianoRoll::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)
    {
        deleteSelectedNotes();
        return true;
    }

    if (k.getTextCharacter() == 'q' || k.getTextCharacter() == 'Q')
    {
        quantise();
        return true;
    }

    if (k == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0))
    {
        if (auto* clip = getClip())
            selection = clip->getSequence().getNotes();
        repaint();
        return true;
    }

    if (k == juce::KeyPress::upKey || k == juce::KeyPress::downKey)
    {
        const int step = (k.getModifiers().isShiftDown() ? 12 : 1) * (k == juce::KeyPress::upKey ? 1 : -1);
        edit.getUndoManager().beginNewTransaction ("Transpose");

        for (auto* n : selection)
            n->setNoteNumber (juce::jlimit (0, 127, n->getNoteNumber() + step), &edit.getUndoManager());

        repaint();
        return ! selection.isEmpty();
    }

    return false;
}

void PianoRoll::deleteSelectedNotes()
{
    if (auto* clip = getClip())
    {
        edit.getUndoManager().beginNewTransaction ("Delete Notes");

        for (auto* n : selection)
            clip->getSequence().removeNote (*n, &edit.getUndoManager());

        selection.clear();
        repaint();
    }
}

void PianoRoll::quantise()
{
    if (auto* clip = getClip())
    {
        edit.getUndoManager().beginNewTransaction ("Quantise");
        quantiseNotes (*clip, viewState.pianoRollGrid, 1.0, selection);
        repaint();
    }
}

void PianoRoll::applyVelocityToSelection (int velocity)
{
    for (auto* n : selection)
        n->setVelocity (velocity, &edit.getUndoManager());

    repaint();
}

} // namespace tess
