#include "MixerView.h"
#include "engine/TrackOps.h"
#include "ui/PluginMenus.h"
#include "ui/theme/Theme.h"

namespace tess
{

using namespace theme;

namespace
{
    constexpr int stripWidth = 92;
    constexpr int numSlots = 6;

    void styleSlot (juce::TextButton& b, bool filled)
    {
        b.setColour (juce::TextButton::buttonColourId, filled ? colour::accent.withAlpha (0.35f) : colour::lane);
        b.setColour (juce::TextButton::textColourOffId, filled ? colour::text : colour::textTertiary);
    }

    float dbToMeterProportion (float db)
    {
        // -60 dB .. +6 dB, with more resolution near the top like a hardware meter.
        const float clamped = juce::jlimit (-60.0f, 6.0f, db);
        return std::pow ((clamped + 60.0f) / 66.0f, 1.6f);
    }
}

//==============================================================================
void LevelMeter::setMeasurer (te::LevelMeasurer* m)
{
    if (measurer.get() == m)
        return;

    if (auto* old = measurer.get())
        old->removeClient (client);

    measurer = m;

    if (m != nullptr)
        m->addClient (client);
}

void LevelMeter::timerCallback()
{
    bool changed = false;

    for (int ch = 0; ch < 2; ++ch)
    {
        float db = -100.0f;

        if (measurer.get() != nullptr)
            db = client.getAndClearAudioLevel (ch).dB;

        // Fast attack, smooth release.
        const float next = db > levels[ch] ? db : juce::jmax (db, levels[ch] - 1.5f);

        if (next > peaks[ch]) { peaks[ch] = next; peakHold[ch] = 45; }
        else if (--peakHold[ch] <= 0) peaks[ch] = juce::jmax (-100.0f, peaks[ch] - 1.0f);

        changed = changed || std::abs (next - levels[ch]) > 0.05f;
        levels[ch] = next;
    }

    if (changed)
        repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colour::separator);
    g.fillRoundedRectangle (r, 2.0f);

    const float barW = (r.getWidth() - 3.0f) / 2.0f;

    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = juce::Rectangle<float> (r.getX() + 1.0f + ch * (barW + 1.0f), r.getY() + 1.0f, barW, r.getHeight() - 2.0f);
        const float h = bar.getHeight() * dbToMeterProportion (levels[ch]);

        juce::ColourGradient grad (colour::meterHigh, 0, bar.getY(), colour::meterLow, 0, bar.getBottom(), false);
        grad.addColour (1.0 - dbToMeterProportion (0.0f), colour::meterHigh);
        grad.addColour (1.0 - dbToMeterProportion (-6.0f), colour::meterMid);
        grad.addColour (1.0 - dbToMeterProportion (-18.0f), colour::meterLow);
        g.setGradientFill (grad);
        g.fillRect (bar.withTop (bar.getBottom() - h));

        if (peaks[ch] > -60.0f)
        {
            const float py = bar.getBottom() - bar.getHeight() * dbToMeterProportion (peaks[ch]);
            g.setColour (peaks[ch] > 0.0f ? colour::meterHigh : colour::text.withAlpha (0.8f));
            g.fillRect (bar.getX(), py, bar.getWidth(), 1.5f);
        }
    }
}

//==============================================================================
ChannelStrip::ChannelStrip (EditViewState& vs, te::AudioTrack* t, int i)
    : viewState (vs), track (t), index (i)
{
    name.setJustificationType (juce::Justification::centred);
    name.setFont (font (12.0f, true));
    name.setText (track != nullptr ? track->getName() : juce::String ("Stereo Out"), juce::dontSendNotification);
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (name);

    if (track != nullptr && getTrackKind (*track) == TrackKind::instrument)
    {
        instrumentSlot.setTooltip ("Instrument — click to open, right-click to change");
        instrumentSlot.onClick = [this]
        {
            if (juce::ModifierKeys::currentModifiers.isPopupMenu() || getInstrument (*track) == nullptr)
            {
                plugin_menus::chooseInstrument (track->edit, instrumentSlot, [this] (te::Plugin::Ptr p)
                {
                    track->edit.getUndoManager().beginNewTransaction ("Change Instrument");
                    setInstrument (*track, p);
                    plugin_menus::showEditor (*p);
                });
            }
            else if (auto* inst = getInstrument (*track))
            {
                plugin_menus::showEditor (*inst);
            }
        };
        addAndMakeVisible (instrumentSlot);
    }

    pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pan.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pan.setRange (-1.0, 1.0, 0.01);
    pan.setDoubleClickReturnValue (true, 0.0);
    pan.setTooltip ("Pan (double-click to centre)");
    pan.onDragStart = [this] { viewState.edit.getUndoManager().beginNewTransaction ("Pan"); };
    pan.onValueChange = [this] { if (auto* v = getVolumePlugin()) v->setPan ((float) pan.getValue()); };
    addAndMakeVisible (pan);

    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange (0.0, 1.0);
    fader.setDoubleClickReturnValue (true, te::decibelsToVolumeFaderPosition (0.0f));
    fader.setTooltip ("Volume (double-click for 0 dB)");
    fader.onDragStart = [this] { viewState.edit.getUndoManager().beginNewTransaction ("Volume"); };
    fader.onValueChange = [this]
    {
        if (auto* v = getVolumePlugin())
            v->setSliderPos ((float) fader.getValue());
    };
    addAndMakeVisible (fader);

    dbLabel.setJustificationType (juce::Justification::centred);
    dbLabel.setFont (monoFont (11.0f));
    dbLabel.setColour (juce::Label::textColourId, colour::textSecondary);
    addAndMakeVisible (dbLabel);

    if (track != nullptr)
    {
        for (auto [b, c] : { std::pair { &muteButton, colour::mute }, std::pair { &soloButton, colour::solo } })
        {
            b->setColour (juce::TextButton::buttonOnColourId, c);
            b->setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1c1c1f));
            b->setColour (juce::TextButton::textColourOffId, colour::textSecondary);
            addAndMakeVisible (b);
        }

        muteButton.onClick = [this] { track->setMute (! track->isMuted (false)); refresh(); };
        soloButton.onClick = [this] { track->setSolo (! track->isSolo (false)); refresh(); };
        meter.setMeasurer (&track->getLevelMeterPlugin()->measurer);
    }

    addAndMakeVisible (meter);
    refresh();
    startTimerHz (10);
}

te::VolumeAndPanPlugin* ChannelStrip::getVolumePlugin() const
{
    return track != nullptr ? track->getVolumePlugin() : viewState.edit.getMasterVolumePlugin().get();
}

juce::Array<te::Plugin*> ChannelStrip::getInserts() const
{
    juce::Array<te::Plugin*> result;
    auto& list = track != nullptr ? track->pluginList : viewState.edit.getMasterPluginList();
    auto* instrument = track != nullptr ? getInstrument (*track) : nullptr;

    for (auto* p : list.getPlugins())
        if (! plugin_menus::isHiddenPlugin (*p) && p != instrument)
            result.add (p);

    return result;
}

void ChannelStrip::rebuildSlots()
{
    slots.clear();
    auto inserts = getInserts();

    for (int i = 0; i < numSlots; ++i)
    {
        auto* b = slots.add (new juce::TextButton());
        te::Plugin::Ptr p = i < inserts.size() ? inserts[i] : nullptr;

        b->setButtonText (p != nullptr ? p->getName() : juce::String());
        b->setTooltip (p != nullptr ? "Click to open, right-click for options" : "Add audio effect");
        styleSlot (*b, p != nullptr);
        b->setVisible (i <= inserts.size());   // existing inserts + one empty slot
        addChildComponent (b);

        b->onClick = [this, b, p]
        {
            auto& edit = viewState.edit;
            auto& list = track != nullptr ? track->pluginList : edit.getMasterPluginList();

            if (p == nullptr)
            {
                plugin_menus::chooseEffect (edit, *b, [this, &edit, &list] (te::Plugin::Ptr np)
                {
                    edit.getUndoManager().beginNewTransaction ("Add Effect");

                    // Insert before the track's volume/meter so effects are pre-fader.
                    int insertIndex = list.size();
                    auto plugins = list.getPlugins();
                    for (int k = 0; k < plugins.size(); ++k)
                        if (plugin_menus::isHiddenPlugin (*plugins[k])) { insertIndex = k; break; }

                    list.insertPlugin (np, insertIndex, nullptr);
                    plugin_menus::showEditor (*np);
                    refresh();
                });
            }
            else if (juce::ModifierKeys::currentModifiers.isPopupMenu())
            {
                juce::PopupMenu m;
                m.addItem ("Open", [p] { plugin_menus::showEditor (*p); });
                m.addItem ("Bypass", true, ! p->isEnabled(), [p] { p->setEnabled (! p->isEnabled()); });
                m.addSeparator();
                m.addItem ("Remove", [this, p] { viewState.edit.getUndoManager().beginNewTransaction ("Remove Effect"); p->deleteFromParent(); refresh(); });
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (b));
            }
            else
            {
                plugin_menus::showEditor (*p);
            }
        };
    }

    lastPluginCount = (track != nullptr ? track->pluginList : viewState.edit.getMasterPluginList()).size();
    resized();
}

void ChannelStrip::refresh()
{
    if (auto* v = getVolumePlugin())
    {
        if (! fader.isMouseButtonDown())
            fader.setValue (v->getSliderPos(), juce::dontSendNotification);
        if (! pan.isMouseButtonDown())
            pan.setValue (v->getPan(), juce::dontSendNotification);

        const float db = v->getVolumeDb();
        dbLabel.setText (db <= -96.0f ? juce::String ("-inf") : juce::String (db, 1), juce::dontSendNotification);
    }

    if (track != nullptr)
    {
        name.setText (track->getName(), juce::dontSendNotification);
        muteButton.setToggleState (track->isMuted (false), juce::dontSendNotification);
        soloButton.setToggleState (track->isSolo (false), juce::dontSendNotification);

        if (getTrackKind (*track) == TrackKind::instrument)
        {
            auto* inst = getInstrument (*track);
            instrumentSlot.setButtonText (inst != nullptr ? inst->getName() : juce::String ("Instrument"));
            styleSlot (instrumentSlot, inst != nullptr);
            instrumentSlot.setColour (juce::TextButton::buttonColourId, colour::trackColour (index).withAlpha (inst != nullptr ? 0.45f : 0.15f));
        }
    }

    const int count = (track != nullptr ? track->pluginList : viewState.edit.getMasterPluginList()).size();
    if (count != lastPluginCount)
        rebuildSlots();
}

void ChannelStrip::timerCallback()
{
    refresh();

    if (track == nullptr)
        if (auto* ctx = viewState.edit.getCurrentPlaybackContext())
            meter.setMeasurer (&ctx->masterLevels);
}

void ChannelStrip::paint (juce::Graphics& g)
{
    const bool selected = track != nullptr && viewState.getSelectedTrack() == track.get();
    auto r = getLocalBounds().toFloat().reduced (2.0f, 0.0f);

    g.setColour (selected ? colour::panelRaised.brighter (0.05f) : colour::panelRaised);
    g.fillRoundedRectangle (r, (float) size::radius);

    // Colour cap at the top identifies the track.
    auto cap = r.removeFromTop (4.0f);
    g.setColour (track != nullptr ? colour::trackColour (index) : colour::textSecondary);
    g.fillRoundedRectangle (cap, 2.0f);

    if (selected)
    {
        g.setColour (colour::accent.withAlpha (0.6f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (2.5f, 0.5f), (float) size::radius, 1.0f);
    }

    // dB scale beside the fader.
    auto f = fader.getBounds();
    g.setFont (font (9.0f));
    g.setColour (colour::textTertiary);

    for (float db : { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f })
    {
        const double pos = te::decibelsToVolumeFaderPosition (db);
        const int y = f.getBottom() - 8 - juce::roundToInt (pos * (f.getHeight() - 16));
        g.drawText (juce::String ((int) db), f.getX() - 22, y - 5, 20, 10, juce::Justification::centredRight);
    }
}

void ChannelStrip::resized()
{
    auto r = getLocalBounds().reduced (6, 0).withTrimmedTop (8).withTrimmedBottom (6);

    name.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (4);

    if (track != nullptr)
    {
        auto ms = r.removeFromBottom (20);
        muteButton.setBounds (ms.removeFromLeft (ms.getWidth() / 2 - 2));
        ms.removeFromLeft (4);
        soloButton.setBounds (ms);
        r.removeFromBottom (6);
    }

    if (instrumentSlot.isVisible())
    {
        instrumentSlot.setBounds (r.removeFromTop (22));
        r.removeFromTop (6);
    }

    for (auto* s : slots)
    {
        if (s->isVisible())
        {
            s->setBounds (r.removeFromTop (20));
            r.removeFromTop (3);
        }
    }

    r.removeFromTop (6);
    pan.setBounds (r.removeFromTop (40).withSizeKeepingCentre (40, 40));
    dbLabel.setBounds (r.removeFromTop (18));

    auto meterArea = r.removeFromRight (14);
    meter.setBounds (meterArea.reduced (0, 4));
    r.removeFromRight (2);
    fader.setBounds (r.withTrimmedLeft (22));
}

void ChannelStrip::mouseDown (const juce::MouseEvent&)
{
    if (track != nullptr)
        viewState.selectTrack (track.get());
}

//==============================================================================
MixerView::MixerView (EditViewState& vs) : viewState (vs)
{
    viewState.edit.state.addListener (this);
    viewState.addChangeListener (this);
    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, true);
    addAndMakeVisible (viewport);
    rebuild();
}

MixerView::~MixerView()
{
    viewState.edit.state.removeListener (this);
    viewState.removeChangeListener (this);
}

void MixerView::handleAsyncUpdate()
{
    auto tracks = te::getAudioTracks (viewState.edit);
    bool same = tracks.size() == strips.size();

    for (int i = 0; same && i < tracks.size(); ++i)
        same = strips[i]->getTrack() == tracks[i];

    if (! same)
        rebuild();
    else
        for (auto* s : strips)
            s->refresh();
}

void MixerView::rebuild()
{
    strips.clear();
    auto tracks = te::getAudioTracks (viewState.edit);

    for (int i = 0; i < tracks.size(); ++i)
        content.addAndMakeVisible (strips.add (new ChannelStrip (viewState, tracks[i], i)));

    if (master == nullptr)
    {
        master = std::make_unique<ChannelStrip> (viewState, nullptr, -1);
        addAndMakeVisible (*master);
    }

    resized();
}

void MixerView::paint (juce::Graphics& g)
{
    g.fillAll (colour::panel);
    g.setColour (colour::separator);
    g.drawVerticalLine (getWidth() - stripWidth - 12, 6.0f, (float) getHeight() - 6.0f);
}

void MixerView::resized()
{
    auto r = getLocalBounds().reduced (6, 6);

    if (master != nullptr)
        master->setBounds (r.removeFromRight (stripWidth));

    r.removeFromRight (12);
    viewport.setBounds (r);

    const int h = r.getHeight() - (viewport.getScrollBarThickness() + 2);
    content.setSize (juce::jmax (r.getWidth(), strips.size() * stripWidth), h);

    for (int i = 0; i < strips.size(); ++i)
        strips[i]->setBounds (i * stripWidth, 0, stripWidth, h);
}

} // namespace tess
