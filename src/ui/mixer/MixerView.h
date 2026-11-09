#pragma once

#include "ui/EditViewState.h"

namespace tess
{

/** Stereo peak meter fed by a LevelMeasurer, with smooth decay and peak hold. */
class LevelMeter  : public juce::Component,
                    private juce::Timer
{
public:
    LevelMeter() { startTimerHz (30); }
    ~LevelMeter() override { setMeasurer (nullptr); }

    void setMeasurer (te::LevelMeasurer*);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    juce::WeakReference<te::LevelMeasurer> measurer;
    te::LevelMeasurer::Client client;
    float levels[2] { -100.0f, -100.0f }, peaks[2] { -100.0f, -100.0f };
    int peakHold[2] {};
};

//==============================================================================
/** One mixer channel: inserts, pan, fader, meter, mute / solo. Pass nullptr for the master strip. */
class ChannelStrip  : public juce::Component,
                      private juce::Timer
{
public:
    ChannelStrip (EditViewState&, te::AudioTrack* track, int index);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();

    te::AudioTrack* getTrack() const  { return track.get(); }

private:
    void timerCallback() override;
    te::VolumeAndPanPlugin* getVolumePlugin() const;
    juce::Array<te::Plugin*> getInserts() const;
    void rebuildSlots();

    EditViewState& viewState;
    te::AudioTrack::Ptr track;   // nullptr = master
    int index;

    juce::Label name;
    juce::TextButton instrumentSlot;
    juce::OwnedArray<juce::TextButton> slots;
    juce::Slider pan, fader;
    juce::Label dbLabel;
    juce::TextButton muteButton { "M" }, soloButton { "S" };
    LevelMeter meter;
    int lastPluginCount = -1;
};

//==============================================================================
class MixerView  : public juce::Component,
                   private juce::ValueTree::Listener,
                   private juce::AsyncUpdater,
                   private juce::ChangeListener
{
public:
    explicit MixerView (EditViewState&);
    ~MixerView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuild();
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { triggerAsyncUpdate(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { triggerAsyncUpdate(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                { triggerAsyncUpdate(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override  { repaint(); for (auto* s : strips) s->repaint(); }

    EditViewState& viewState;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<ChannelStrip> strips;
    std::unique_ptr<ChannelStrip> master;
};

} // namespace tess
