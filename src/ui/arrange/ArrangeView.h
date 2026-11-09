#pragma once

#include "ui/EditViewState.h"
#include "ui/IconButton.h"

namespace tess
{

class ArrangeView;

/** Left-hand header for one track: colour, name, mute / solo / record-arm. */
class TrackHeader  : public juce::Component
{
public:
    TrackHeader (EditViewState&, te::AudioTrack&, int index);

    te::AudioTrack& getTrack() const  { return *track; }
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    EditViewState& viewState;
    te::AudioTrack::Ptr track;
    int index;

    juce::Label name;
    juce::TextButton muteButton { "M" }, soloButton { "S" }, armButton { "R" };
};

//==============================================================================
/** The main timeline: ruler, track headers and lanes with regions. */
class ArrangeView  : public juce::Component,
                     public juce::FileDragAndDropTarget,
                     public juce::DragAndDropTarget,
                     private juce::ValueTree::Listener,
                     private juce::ChangeListener,
                     private juce::AsyncUpdater,
                     private juce::Timer,
                     private juce::ScrollBar::Listener
{
public:
    explicit ArrangeView (EditViewState&);
    ~ArrangeView() override;

    std::function<void (te::Clip&)> onOpenRegion;   ///< double-click a region (e.g. open piano roll)
    std::function<void()> onAddTrack;

    void zoom (double factor, std::optional<int> anchorX = {});

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scale) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;

    // Drags from the Library panel
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails&) override;
    void itemDropped (const juce::DragAndDropTarget::SourceDetails&) override;

private:
    enum class DragMode { none, move, resizeEnd, resizeStart, scrub, cycle };

    struct Hit
    {
        te::Clip* clip = nullptr;
        DragMode mode = DragMode::none;
    };

    // Geometry
    juce::Rectangle<int> rulerArea() const;
    juce::Rectangle<int> lanesArea() const;
    int trackY (int index) const;
    int trackIndexAt (int y) const;
    te::AudioTrack* trackAt (int y) const;
    double xToBeats (int x) const;
    int beatsToX (double beats) const;
    juce::Rectangle<int> clipBounds (te::Clip&, int trackIndex) const;
    Hit hitTest (juce::Point<int>) const;

    // Drawing
    void drawRuler (juce::Graphics&);
    void drawGrid (juce::Graphics&, juce::Rectangle<int> lanes);
    void drawClip (juce::Graphics&, te::Clip&, juce::Rectangle<int>, juce::Colour);
    void drawMidiPreview (juce::Graphics&, te::MidiClip&, juce::Rectangle<int>, juce::Colour);
    void drawAudioPreview (juce::Graphics&, te::WaveAudioClip&, juce::Rectangle<int>, juce::Colour);

    void rebuildHeaders();
    void updateScrollBars();
    void importAudio (te::AudioTrack&, double beat);

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { triggerAsyncUpdate(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { triggerAsyncUpdate(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { triggerAsyncUpdate(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override                { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override  { repaint(); }
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double) override;

    EditViewState& viewState;
    te::Edit& edit;

    juce::OwnedArray<TrackHeader> headers;
    IconButton addTrackButton { "New Track", icons::Icon::plus };
    juce::ScrollBar hScroll { false }, vScroll { true };
    int scrollY = 0;

    std::map<te::EditItemID, std::unique_ptr<te::SmartThumbnail>> thumbnails;

    // Drag state
    DragMode dragMode = DragMode::none;
    te::Clip::Ptr dragClip;
    double dragStartBeat = 0.0, dragClipStartBeat = 0.0, dragClipEndBeat = 0.0, cycleAnchorBeat = 0.0;
    int dragStartTrack = 0;

    double lastPlayheadX = -1.0;
    std::optional<double> recordStartBeat;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace tess
