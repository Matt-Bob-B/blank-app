#include "ArrangeView.h"
#include "engine/TrackOps.h"

namespace tess
{

using namespace theme;

namespace
{
    constexpr int clipHeaderHeight = 15;
    constexpr int resizeHandle = 7;

    juce::Colour colourForTrack (te::AudioTrack& t, int index)
    {
        auto c = t.getColour();
        return c.isTransparent() ? colour::trackColour (index) : c;
    }

    void styleSmallToggle (juce::TextButton& b, juce::Colour on)
    {
        b.setClickingTogglesState (false);
        b.setColour (juce::TextButton::buttonColourId, colour::control);
        b.setColour (juce::TextButton::buttonOnColourId, on);
        b.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1c1c1f));
        b.setColour (juce::TextButton::textColourOffId, colour::textSecondary);
        b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
}

//==============================================================================
TrackHeader::TrackHeader (EditViewState& vs, te::AudioTrack& t, int i)
    : viewState (vs), track (&t), index (i)
{
    name.setFont (font (14.0f, true));
    name.setColour (juce::Label::textColourId, colour::text);
    name.setEditable (false, true, false);
    name.onTextChange = [this]
    {
        track->edit.getUndoManager().beginNewTransaction ("Rename Track");
        track->setName (name.getText());
    };
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (name);

    styleSmallToggle (muteButton, colour::mute);
    styleSmallToggle (soloButton, colour::solo);
    styleSmallToggle (armButton, colour::record);
    muteButton.setTooltip ("Mute");
    soloButton.setTooltip ("Solo");
    armButton.setTooltip ("Record Enable");

    muteButton.onClick = [this] { track->setMute (! track->isMuted (false)); refresh(); };
    soloButton.onClick = [this] { track->setSolo (! track->isSolo (false)); refresh(); };
    armButton.onClick = [this]
    {
        const bool arm = ! isTrackArmed (*track);

        if (getTrackKind (*track) == TrackKind::instrument)
            armTrackForMidi (*track, arm);
        else
            armTrackForAudio (*track, arm);

        refresh();
    };

    for (auto* b : { &muteButton, &soloButton, &armButton })
        addAndMakeVisible (b);

    refresh();
}

void TrackHeader::refresh()
{
    name.setText (track->getName(), juce::dontSendNotification);
    muteButton.setToggleState (track->isMuted (false), juce::dontSendNotification);
    soloButton.setToggleState (track->isSolo (false), juce::dontSendNotification);
    armButton.setToggleState (isTrackArmed (*track), juce::dontSendNotification);
    repaint();
}

void TrackHeader::paint (juce::Graphics& g)
{
    const bool selected = viewState.getSelectedTrack() == track.get();
    auto r = getLocalBounds();

    g.setColour (selected ? colour::panelRaised.brighter (0.06f) : colour::panel);
    g.fillRect (r);

    g.setColour (colourForTrack (*track, index));
    g.fillRoundedRectangle (r.removeFromLeft (5).reduced (0, 3).toFloat().withX (2.0f), 1.5f);

    if (selected)
    {
        g.setColour (colour::accent.withAlpha (0.18f));
        g.fillRect (getLocalBounds());
    }

    // Instrument / input caption under the name.
    auto caption = getTrackKind (*track) == TrackKind::instrument
                     ? (getInstrument (*track) != nullptr ? getInstrument (*track)->getName() : juce::String ("No Instrument"))
                     : juce::String ("Audio In 1-2");

    g.setColour (colour::textSecondary);
    g.setFont (font (11.5f));
    g.drawText (caption, 16, getHeight() / 2 + 2, getWidth() - 110, 16, juce::Justification::centredLeft, true);

    g.setColour (colour::separator);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void TrackHeader::resized()
{
    auto r = getLocalBounds().reduced (10, 0).withTrimmedLeft (4);
    auto buttons = r.removeFromRight (84).withSizeKeepingCentre (84, 20);
    armButton.setBounds (buttons.removeFromRight (24));
    buttons.removeFromRight (6);
    soloButton.setBounds (buttons.removeFromRight (24));
    buttons.removeFromRight (6);
    muteButton.setBounds (buttons.removeFromRight (24));

    name.setBounds (r.withHeight (22).withY (getHeight() / 2 - 20));
}

void TrackHeader::mouseDown (const juce::MouseEvent& e)
{
    viewState.selectTrack (track.get());

    if (e.getNumberOfClicks() == 2 && name.getBounds().contains (e.getPosition()))
        name.showEditor();
}

//==============================================================================
ArrangeView::ArrangeView (EditViewState& vs) : viewState (vs), edit (vs.edit)
{
    edit.state.addListener (this);
    viewState.addChangeListener (this);

    addAndMakeVisible (addTrackButton);
    addTrackButton.onClick = [this] { if (onAddTrack) onAddTrack(); };

    for (auto* sb : { &hScroll, &vScroll })
    {
        sb->setAutoHide (false);
        sb->addListener (this);
        addAndMakeVisible (sb);
    }

    setWantsKeyboardFocus (false);
    rebuildHeaders();
    startTimerHz (60);
}

ArrangeView::~ArrangeView()
{
    edit.state.removeListener (this);
    viewState.removeChangeListener (this);
}

//==============================================================================
juce::Rectangle<int> ArrangeView::rulerArea() const
{
    return getLocalBounds().removeFromTop (size::rulerHeight).withTrimmedLeft (size::trackHeaderWidth).withTrimmedRight (vScroll.getWidth());
}

juce::Rectangle<int> ArrangeView::lanesArea() const
{
    return getLocalBounds().withTrimmedTop (size::rulerHeight).withTrimmedLeft (size::trackHeaderWidth)
                           .withTrimmedRight (vScroll.getWidth()).withTrimmedBottom (hScroll.getHeight());
}

int ArrangeView::trackY (int index) const        { return size::rulerHeight + index * size::trackHeight - scrollY; }
int ArrangeView::trackIndexAt (int y) const      { return (y - size::rulerHeight + scrollY) / size::trackHeight; }

te::AudioTrack* ArrangeView::trackAt (int y) const
{
    if (y < size::rulerHeight)
        return nullptr;

    return te::getAudioTracks (edit)[trackIndexAt (y)];
}

double ArrangeView::xToBeats (int x) const
{
    return viewState.scrollBeats + (x - size::trackHeaderWidth) / viewState.pixelsPerBeat;
}

int ArrangeView::beatsToX (double beats) const
{
    return size::trackHeaderWidth + juce::roundToInt ((beats - viewState.scrollBeats) * viewState.pixelsPerBeat);
}

juce::Rectangle<int> ArrangeView::clipBounds (te::Clip& clip, int trackIndex) const
{
    auto range = clip.getEditTimeRange();
    const int x1 = beatsToX (viewState.timeToBeats (range.getStart()));
    const int x2 = beatsToX (viewState.timeToBeats (range.getEnd()));
    return { x1, trackY (trackIndex) + 2, juce::jmax (4, x2 - x1), size::trackHeight - 4 };
}

ArrangeView::Hit ArrangeView::hitTest (juce::Point<int> p) const
{
    if (! lanesArea().contains (p))
        return {};

    const int index = trackIndexAt (p.y);
    auto tracks = te::getAudioTracks (edit);

    if (auto* track = tracks[index])
    {
        auto clips = track->getClips();

        for (int i = clips.size(); --i >= 0;)   // topmost (last drawn) first
        {
            auto* clip = clips.getUnchecked (i);
            auto b = clipBounds (*clip, index);

            if (b.contains (p))
            {
                if (p.x >= b.getRight() - resizeHandle)                         return { clip, DragMode::resizeEnd };
                if (p.x <= b.getX() + resizeHandle && b.getWidth() > 3 * resizeHandle) return { clip, DragMode::resizeStart };
                return { clip, DragMode::move };
            }
        }
    }

    return {};
}

//==============================================================================
void ArrangeView::paint (juce::Graphics& g)
{
    g.fillAll (colour::lane);

    auto lanes = lanesArea();
    auto tracks = te::getAudioTracks (edit);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (lanes);

        for (int i = 0; i < tracks.size(); ++i)
        {
            auto row = juce::Rectangle<int> (lanes.getX(), trackY (i), lanes.getWidth(), size::trackHeight);
            g.setColour ((i % 2) == 0 ? colour::lane : colour::laneAlt);
            g.fillRect (row);

            if (tracks[i] == viewState.getSelectedTrack())
            {
                g.setColour (juce::Colours::white.withAlpha (0.025f));
                g.fillRect (row);
            }
        }

        drawGrid (g, lanes);

        // Cycle range shading
        auto& transport = edit.getTransport();
        if (transport.looping.get())
        {
            auto loop = transport.getLoopRange();
            const int x1 = beatsToX (viewState.timeToBeats (loop.getStart()));
            const int x2 = beatsToX (viewState.timeToBeats (loop.getEnd()));
            g.setColour (colour::cycle.withAlpha (0.05f));
            g.fillRect (x1, lanes.getY(), x2 - x1, lanes.getHeight());
        }

        for (int i = 0; i < tracks.size(); ++i)
        {
            auto* track = tracks[i];
            auto col = colourForTrack (*track, i);

            for (auto* clip : track->getClips())
            {
                auto b = clipBounds (*clip, i);

                if (b.getRight() >= lanes.getX() && b.getX() <= lanes.getRight())
                    drawClip (g, *clip, b, col);
            }

            // Live recording preview: a growing red region on armed tracks.
            if (recordStartBeat.has_value() && isTrackArmed (*track))
            {
                const int x1 = beatsToX (*recordStartBeat);
                const int x2 = beatsToX (viewState.timeToBeats (edit.getTransport().getPosition()));
                auto r = juce::Rectangle<int> (x1, trackY (i) + 2, juce::jmax (2, x2 - x1), size::trackHeight - 4).toFloat();
                g.setColour (colour::record.withAlpha (0.35f));
                g.fillRoundedRectangle (r, 4.0f);
                g.setColour (colour::record);
                g.drawRoundedRectangle (r, 4.0f, 1.0f);
            }
        }
    }

    drawRuler (g);

    // Header column background below the last track
    auto headerCol = getLocalBounds().removeFromLeft (size::trackHeaderWidth).withTrimmedTop (size::rulerHeight);
    g.setColour (colour::panel);
    g.fillRect (headerCol.withTop (juce::jmax (size::rulerHeight, trackY (tracks.size()))));

    // Top-left corner
    g.setColour (colour::panel);
    g.fillRect (0, 0, size::trackHeaderWidth, size::rulerHeight);
    g.setColour (colour::separator);
    g.drawVerticalLine (size::trackHeaderWidth - 1, 0.0f, (float) getHeight());
    g.drawHorizontalLine (size::rulerHeight - 1, 0.0f, (float) getWidth());

    if (tracks.isEmpty())
    {
        g.setColour (colour::textTertiary);
        g.setFont (font (15.0f));
        g.drawText ("Click + to add a track, or drag audio files here", lanes, juce::Justification::centred);
    }
}

void ArrangeView::paintOverChildren (juce::Graphics& g)
{
    // Playhead
    auto lanes = lanesArea().withTop (0);
    const double beat = viewState.timeToBeats (edit.getTransport().getPosition());
    const int x = beatsToX (beat);

    if (x >= lanes.getX() && x <= lanes.getRight())
    {
        g.setColour (colour::playhead.withAlpha (0.9f));
        g.fillRect (x, size::rulerHeight - 8, 1, lanesArea().getBottom() - size::rulerHeight + 8);

        juce::Path head;
        head.addTriangle ((float) x - 6.0f, (float) size::rulerHeight - 12.0f, (float) x + 7.0f,
                          (float) size::rulerHeight - 12.0f, (float) x + 0.5f, (float) size::rulerHeight - 3.0f);
        g.fillPath (head);
    }
}

void ArrangeView::drawGrid (juce::Graphics& g, juce::Rectangle<int> lanes)
{
    const double bpb = viewState.beatsPerBar();
    const double firstBeat = std::floor (viewState.scrollBeats);
    const double lastBeat = xToBeats (lanes.getRight()) + 1.0;
    const bool showBeats = viewState.pixelsPerBeat >= 10.0;
    const int barStep = viewState.pixelsPerBeat * bpb < 24.0 ? 4 : 1;

    for (double b = firstBeat; b <= lastBeat; b += 1.0)
    {
        const bool isBar = std::fmod (b, bpb) < 1.0e-6;
        const int barNum = (int) std::round (b / bpb);

        if (isBar && (barNum % barStep) == 0)
            g.setColour (colour::gridBar);
        else if (showBeats && ! isBar)
            g.setColour (colour::gridBeat);
        else
            continue;

        g.drawVerticalLine (beatsToX (b), (float) lanes.getY(), (float) lanes.getBottom());
    }
}

void ArrangeView::drawRuler (juce::Graphics& g)
{
    auto r = rulerArea();
    g.setColour (colour::panel);
    g.fillRect (r);

    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (r);

    auto& transport = edit.getTransport();
    auto loop = transport.getLoopRange();

    if (! loop.isEmpty())
    {
        const int x1 = beatsToX (viewState.timeToBeats (loop.getStart()));
        const int x2 = beatsToX (viewState.timeToBeats (loop.getEnd()));
        auto band = juce::Rectangle<int> (x1, r.getY() + 2, x2 - x1, 9).toFloat();
        g.setColour (transport.looping.get() ? colour::cycle : colour::textTertiary.withAlpha (0.6f));
        g.fillRoundedRectangle (band, 3.0f);
    }

    const double bpb = viewState.beatsPerBar();
    const int barStep = viewState.pixelsPerBeat * bpb < 24.0 ? 4 : (viewState.pixelsPerBeat * bpb < 40.0 ? 2 : 1);
    const int firstBar = (int) std::floor (viewState.scrollBeats / bpb);
    const int lastBar = (int) std::ceil (xToBeats (r.getRight()) / bpb);

    g.setFont (font (11.5f));

    for (int bar = firstBar; bar <= lastBar; ++bar)
    {
        const int x = beatsToX (bar * bpb);

        if (bar % barStep == 0)
        {
            g.setColour (colour::textSecondary);
            g.drawVerticalLine (x, (float) r.getBottom() - 12.0f, (float) r.getBottom());
            g.drawText (juce::String (bar + 1), x + 4, r.getY() + 11, 40, 14, juce::Justification::centredLeft);
        }

        if (viewState.pixelsPerBeat >= 12.0)
        {
            g.setColour (colour::textTertiary);

            for (int beat = 1; beat < (int) bpb; ++beat)
                g.drawVerticalLine (beatsToX (bar * bpb + beat), (float) r.getBottom() - 5.0f, (float) r.getBottom());
        }
    }
}

void ArrangeView::drawClip (juce::Graphics& g, te::Clip& clip, juce::Rectangle<int> b, juce::Colour col)
{
    const bool selected = viewState.getSelectedClip() == &clip;
    auto r = b.toFloat();

    g.setColour (col.withAlpha (selected ? 0.55f : 0.38f));
    g.fillRoundedRectangle (r, 4.0f);

    auto header = r.withHeight ((float) clipHeaderHeight);
    g.setColour (col.withAlpha (selected ? 1.0f : 0.85f));
    g.fillRoundedRectangle (header, 4.0f);
    g.fillRect (header.withTrimmedTop (4.0f));

    g.setColour (juce::Colour (0xff1c1c1f).withAlpha (0.9f));
    g.setFont (font (11.0f, true));
    g.drawText (clip.getName(), header.reduced (5.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, true);

    auto body = b.withTrimmedTop (clipHeaderHeight).reduced (2, 2);

    if (auto* midi = dynamic_cast<te::MidiClip*> (&clip))
        drawMidiPreview (g, *midi, body, col);
    else if (auto* wave = dynamic_cast<te::WaveAudioClip*> (&clip))
        drawAudioPreview (g, *wave, body, col);

    g.setColour (selected ? juce::Colours::white : col.brighter (0.2f).withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, selected ? 1.5f : 1.0f);
}

void ArrangeView::drawMidiPreview (juce::Graphics& g, te::MidiClip& clip, juce::Rectangle<int> body, juce::Colour col)
{
    auto& seq = clip.getSequence();
    auto notes = seq.getNotes();

    if (notes.isEmpty() || body.getHeight() < 6)
        return;

    int lo = 127, hi = 0;
    for (auto* n : notes)
    {
        lo = juce::jmin (lo, n->getNoteNumber());
        hi = juce::jmax (hi, n->getNoteNumber());
    }

    lo -= 2; hi += 2;
    const float rowH = juce::jmax (1.5f, (float) body.getHeight() / (float) (hi - lo + 1));
    const double clipStartBeat = viewState.timeToBeats (clip.getPosition().getStart());
    const double offsetBeats = clip.getOffsetInBeats().inBeats();

    g.setColour (col.brighter (0.6f));

    for (auto* n : notes)
    {
        const double s = clipStartBeat + n->getStartBeat().inBeats() - offsetBeats;
        const double e = s + n->getLengthBeats().inBeats();
        const float x1 = (float) beatsToX (s), x2 = (float) beatsToX (e);
        const float y = (float) body.getBottom() - (float) (n->getNoteNumber() - lo + 1) * rowH;

        if (x2 >= (float) body.getX() && x1 <= (float) body.getRight())
            g.fillRect (juce::Rectangle<float> (x1, y, juce::jmax (1.5f, x2 - x1 - 1.0f), juce::jmin (rowH, 3.0f))
                          .getIntersection (body.toFloat()));
    }
}

void ArrangeView::drawAudioPreview (juce::Graphics& g, te::WaveAudioClip& clip, juce::Rectangle<int> body, juce::Colour col)
{
    auto& thumb = thumbnails[clip.itemID];

    if (thumb == nullptr)
        thumb = std::make_unique<te::SmartThumbnail> (edit.engine, clip.getAudioFile(), *this, &edit);

    if (thumb->isGeneratingProxy())
        return;

    auto pos = clip.getPosition();
    const auto sourceStart = te::TimePosition() + pos.getOffset();
    g.setColour (col.brighter (0.55f));
    thumb->drawChannels (g, body, { sourceStart, sourceStart + pos.getLength() }, juce::Decibels::decibelsToGain (clip.getGainDB()));
}

//==============================================================================
void ArrangeView::resized()
{
    const int sb = 10;
    vScroll.setBounds (getWidth() - sb, size::rulerHeight, sb, getHeight() - size::rulerHeight - sb);
    hScroll.setBounds (size::trackHeaderWidth, getHeight() - sb, getWidth() - size::trackHeaderWidth - sb, sb);
    addTrackButton.setBounds (size::trackHeaderWidth - 34, 4, 26, size::rulerHeight - 8);

    for (int i = 0; i < headers.size(); ++i)
        headers[i]->setBounds (0, trackY (i), size::trackHeaderWidth - 1, size::trackHeight);

    updateScrollBars();
}

void ArrangeView::updateScrollBars()
{
    const double visibleBeats = lanesArea().getWidth() / viewState.pixelsPerBeat;
    const double contentBeats = juce::jmax (viewState.timeToBeats (te::TimePosition() + edit.getLength()) + viewState.beatsPerBar() * 16.0,
                                            viewState.scrollBeats + visibleBeats, 64.0 * viewState.beatsPerBar());
    hScroll.setRangeLimits (0.0, contentBeats, juce::dontSendNotification);
    hScroll.setCurrentRange (viewState.scrollBeats, visibleBeats, juce::dontSendNotification);

    const int contentH = (headers.size() + 2) * size::trackHeight;
    const int visibleH = lanesArea().getHeight();
    scrollY = juce::jlimit (0, juce::jmax (0, contentH - visibleH), scrollY);
    vScroll.setRangeLimits (0.0, (double) juce::jmax (contentH, visibleH), juce::dontSendNotification);
    vScroll.setCurrentRange (scrollY, visibleH, juce::dontSendNotification);
}

void ArrangeView::scrollBarMoved (juce::ScrollBar* sb, double start)
{
    if (sb == &hScroll)
        viewState.scrollBeats = juce::jmax (0.0, start);
    else
        scrollY = (int) start;

    resized();
    repaint();
}

void ArrangeView::zoom (double factor, std::optional<int> anchorX)
{
    const int ax = anchorX.value_or (lanesArea().getCentreX());
    const double anchorBeat = xToBeats (ax);
    viewState.pixelsPerBeat = juce::jlimit (2.0, 400.0, viewState.pixelsPerBeat * factor);
    viewState.scrollBeats = juce::jmax (0.0, anchorBeat - (ax - size::trackHeaderWidth) / viewState.pixelsPerBeat);
    updateScrollBars();
    repaint();
}

void ArrangeView::rebuildHeaders()
{
    headers.clear();
    auto tracks = te::getAudioTracks (edit);

    for (int i = 0; i < tracks.size(); ++i)
        addAndMakeVisible (headers.add (new TrackHeader (viewState, *tracks[i], i)));

    addTrackButton.toFront (false);

    // Drop thumbnails for clips that no longer exist.
    for (auto it = thumbnails.begin(); it != thumbnails.end();)
        it = edit.clipCache.findItem (it->first) == nullptr ? thumbnails.erase (it) : std::next (it);

    resized();
    repaint();
}

void ArrangeView::handleAsyncUpdate()
{
    auto tracks = te::getAudioTracks (edit);
    bool sameTracks = tracks.size() == headers.size();

    for (int i = 0; sameTracks && i < tracks.size(); ++i)
        sameTracks = &headers[i]->getTrack() == tracks[i];

    if (sameTracks)
    {
        for (auto* h : headers)
            h->refresh();

        updateScrollBars();
        repaint();
    }
    else
    {
        rebuildHeaders();
    }
}

void ArrangeView::timerCallback()
{
    auto& transport = edit.getTransport();

    if (transport.isRecording() && ! recordStartBeat.has_value())
        recordStartBeat = viewState.timeToBeats (transport.getPosition());
    else if (! transport.isRecording())
        recordStartBeat.reset();

    const double x = beatsToX (viewState.timeToBeats (transport.getPosition()));

    // Follow the playhead: page forward when it leaves the visible area.
    if (transport.isPlaying() && dragMode == DragMode::none && x > lanesArea().getRight() - 20)
    {
        viewState.scrollBeats = viewState.timeToBeats (transport.getPosition()) - 4.0;
        updateScrollBars();
        repaint();
    }
    else if (x != lastPlayheadX || recordStartBeat.has_value())
    {
        repaint();
    }

    lastPlayheadX = x;
}

//==============================================================================
void ArrangeView::mouseMove (const juce::MouseEvent& e)
{
    auto hit = hitTest (e.getPosition());
    setMouseCursor (hit.mode == DragMode::resizeEnd || hit.mode == DragMode::resizeStart
                        ? juce::MouseCursor::LeftRightResizeCursor
                        : juce::MouseCursor::NormalCursor);
}

void ArrangeView::mouseDown (const juce::MouseEvent& e)
{
    dragMode = DragMode::none;
    dragClip = nullptr;
    dragStartBeat = xToBeats (e.x);

    if (rulerArea().contains (e.getPosition()))
    {
        // Upper strip of the ruler sets the cycle range; the rest moves the playhead.
        if (e.y < rulerArea().getY() + 12 || e.mods.isShiftDown())
        {
            dragMode = DragMode::cycle;
            cycleAnchorBeat = viewState.snap (dragStartBeat);
        }
        else
        {
            dragMode = DragMode::scrub;
            edit.getTransport().setPosition (viewState.beatsToTime (juce::jmax (0.0, dragStartBeat)));
        }

        return;
    }

    auto hit = hitTest (e.getPosition());

    if (hit.clip != nullptr)
    {
        viewState.selectClip (hit.clip);
        dragMode = hit.mode;
        dragClip = hit.clip;
        dragClipStartBeat = viewState.timeToBeats (hit.clip->getPosition().getStart());
        dragClipEndBeat = viewState.timeToBeats (hit.clip->getPosition().getEnd());
        dragStartTrack = trackIndexAt (e.y);
        edit.getUndoManager().beginNewTransaction ("Edit Region");

        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem ("Delete", [this, c = dragClip] { viewState.selectClip (c.get()); edit.getUndoManager().beginNewTransaction(); viewState.selectClip (nullptr); c->removeFromParent(); });
            m.addItem ("Mute", true, hit.clip->isMuted(), [c = dragClip] { c->setMuted (! c->isMuted()); });
            m.showMenuAsync ({});
            dragMode = DragMode::none;
        }

        return;
    }

    if (lanesArea().contains (e.getPosition()))
    {
        viewState.selectClip (nullptr);

        if (auto* t = trackAt (e.y))
            viewState.selectTrack (t);
    }
}

void ArrangeView::mouseDrag (const juce::MouseEvent& e)
{
    const double beat = xToBeats (e.x);
    const bool bypassSnap = e.mods.isAltDown();

    switch (dragMode)
    {
        case DragMode::scrub:
            edit.getTransport().setPosition (viewState.beatsToTime (juce::jmax (0.0, beat)));
            break;

        case DragMode::cycle:
        {
            auto a = cycleAnchorBeat, b = viewState.snap (beat, bypassSnap);
            if (std::abs (b - a) > 1.0e-6)
            {
                edit.getTransport().setLoopRange ({ viewState.beatsToTime (juce::jmax (0.0, juce::jmin (a, b))),
                                                    viewState.beatsToTime (juce::jmax (a, b)) });
                edit.getTransport().looping = true;
            }
            break;
        }

        case DragMode::move:
        {
            if (dragClip == nullptr) break;

            const double newStart = juce::jmax (0.0, viewState.snap (dragClipStartBeat + (beat - dragStartBeat), bypassSnap));
            dragClip->setStart (viewState.beatsToTime (newStart), false, true);

            // Vertical drag moves the region to another track of a compatible kind.
            auto tracks = te::getAudioTracks (edit);
            auto* target = tracks[juce::jlimit (0, tracks.size() - 1, trackIndexAt (e.y))];

            if (target != nullptr && target != dragClip->getTrack()
                 && (dynamic_cast<te::MidiClip*> (dragClip.get()) != nullptr) == (getTrackKind (*target) == TrackKind::instrument))
                dragClip->moveTo (*target);

            break;
        }

        case DragMode::resizeEnd:
        {
            if (dragClip == nullptr) break;

            const double minLen = viewState.snapEnabled && viewState.gridBeats > 0 ? viewState.gridBeats : 0.25;
            const double newEnd = juce::jmax (dragClipStartBeat + minLen, viewState.snap (dragClipEndBeat + (beat - dragStartBeat), bypassSnap));
            dragClip->setEnd (viewState.beatsToTime (newEnd), true);
            break;
        }

        case DragMode::resizeStart:
        {
            if (dragClip == nullptr) break;

            const double newStart = juce::jlimit (0.0, dragClipEndBeat - 0.25,
                                                  viewState.snap (dragClipStartBeat + (beat - dragStartBeat), bypassSnap));
            dragClip->setStart (viewState.beatsToTime (newStart), true, false);
            break;
        }

        case DragMode::none:
            break;
    }

    repaint();
}

void ArrangeView::mouseUp (const juce::MouseEvent&)
{
    dragMode = DragMode::none;
    dragClip = nullptr;
    updateScrollBars();
}

void ArrangeView::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto hit = hitTest (e.getPosition());

    if (hit.clip != nullptr)
    {
        if (onOpenRegion)
            onOpenRegion (*hit.clip);

        return;
    }

    if (! lanesArea().contains (e.getPosition()))
        return;

    auto* track = trackAt (e.y);

    if (track == nullptr)
        return;

    const double bpb = viewState.beatsPerBar();
    const int bar = (int) std::floor (juce::jmax (0.0, xToBeats (e.x)) / bpb);

    if (getTrackKind (*track) == TrackKind::instrument)
    {
        edit.getUndoManager().beginNewTransaction ("New MIDI Region");

        if (auto* clip = addMidiRegion (*track, bar, 1))
        {
            viewState.selectClip (clip);

            if (onOpenRegion)
                onOpenRegion (*clip);
        }
    }
    else
    {
        importAudio (*track, bar * bpb);
    }
}

void ArrangeView::importAudio (te::AudioTrack& track, double beat)
{
    auto& formats = edit.engine.getAudioFileFormatManager().readFormatManager;
    chooser = std::make_unique<juce::FileChooser> ("Import Audio File",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   formats.getWildcardForAllFormats());

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, t = te::AudioTrack::Ptr (&track), beat] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();

                              if (f.existsAsFile())
                              {
                                  edit.getUndoManager().beginNewTransaction ("Import Audio");

                                  if (auto* clip = addAudioRegion (*t, f, viewState.beatsToTime (beat)))
                                      viewState.selectClip (clip);
                              }
                          });
}

void ArrangeView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        zoom (w.deltaY > 0 ? 1.15 : 1.0 / 1.15, e.x);
        return;
    }

    const float dx = e.mods.isShiftDown() ? w.deltaY : w.deltaX;
    const float dy = e.mods.isShiftDown() ? 0.0f : w.deltaY;

    if (dx != 0.0f)
        viewState.scrollBeats = juce::jmax (0.0, viewState.scrollBeats - dx * 300.0 / viewState.pixelsPerBeat);

    if (dy != 0.0f)
        scrollY = juce::jmax (0, scrollY - juce::roundToInt (dy * 300.0f));

    resized();
    repaint();
}

void ArrangeView::mouseMagnify (const juce::MouseEvent& e, float scale)
{
    zoom (scale, e.x);
}

//==============================================================================
bool ArrangeView::isInterestedInFileDrag (const juce::StringArray& files)
{
    auto& formats = edit.engine.getAudioFileFormatManager().readFormatManager;

    for (auto& f : files)
        if (formats.findFormatForFileExtension (juce::File (f).getFileExtension()) != nullptr)
            return true;

    return false;
}

void ArrangeView::filesDropped (const juce::StringArray& files, int x, int y)
{
    auto* track = trackAt (y);

    if (track == nullptr || getTrackKind (*track) != TrackKind::audio)
    {
        edit.getUndoManager().beginNewTransaction ("New Audio Track");
        track = addAudioTrack (edit, "Audio " + juce::String (te::getAudioTracks (edit).size() + 1));
    }

    if (track == nullptr)
        return;

    edit.getUndoManager().beginNewTransaction ("Import Audio");
    double beat = juce::jmax (0.0, viewState.snap (xToBeats (x)));

    for (auto& path : files)
    {
        if (auto* clip = addAudioRegion (*track, juce::File (path), viewState.beatsToTime (beat)))
        {
            viewState.selectClip (clip);
            beat = viewState.timeToBeats (clip->getPosition().getEnd());
        }
    }
}

bool ArrangeView::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& d)
{
    return dynamic_cast<juce::FileTreeComponent*> (d.sourceComponent.get()) != nullptr;
}

void ArrangeView::itemDropped (const juce::DragAndDropTarget::SourceDetails& d)
{
    if (auto* tree = dynamic_cast<juce::FileTreeComponent*> (d.sourceComponent.get()))
    {
        juce::StringArray files;

        for (int i = 0; i < tree->getNumSelectedFiles(); ++i)
            if (tree->getSelectedFile (i).existsAsFile())
                files.add (tree->getSelectedFile (i).getFullPathName());

        if (! files.isEmpty())
            filesDropped (files, d.localPosition.x, d.localPosition.y);
    }
}

} // namespace tess
