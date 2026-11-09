#pragma once

#include "ui/EditViewState.h"

namespace tess
{

/** Note editor for the selected MIDI region.
    Click to add a note, drag to move, drag the right edge to resize,
    double-click or Delete to remove; Q quantises. */
class PianoRoll  : public juce::Component,
                   private juce::ChangeListener,
                   private juce::ValueTree::Listener,
                   private juce::AsyncUpdater,
                   private juce::Timer
{
public:
    explicit PianoRoll (EditViewState&);
    ~PianoRoll() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    void deleteSelectedNotes();
    void quantise();

private:
    enum class Drag { none, move, resize, marquee };

    te::MidiClip* getClip() const;
    juce::Rectangle<int> noteArea() const;
    juce::Rectangle<int> keyboardArea() const;
    juce::Rectangle<int> rulerArea() const;

    double clipContentStartBeat() const;   ///< edit beat of note-beat 0
    int beatToX (double editBeat) const;
    double xToBeat (int x) const;
    int pitchToY (int pitch) const;
    int yToPitch (int y) const;
    juce::Rectangle<int> noteBounds (te::MidiNote&) const;
    te::MidiNote* noteAt (juce::Point<int>) const;

    void drawKeyboard (juce::Graphics&);
    void drawRuler (juce::Graphics&);
    void centreOnNotes();
    void applyVelocityToSelection (int velocity);

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override               { triggerAsyncUpdate(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override        { triggerAsyncUpdate(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override  { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override  { repaint(); }
    void timerCallback() override;

    EditViewState& viewState;
    te::Edit& edit;
    te::Clip::Ptr currentClip;

    double pixelsPerBeat = 90.0;
    double scrollBeats = 0.0;
    int noteHeight = 14;
    int topPitch = 84;   ///< pitch at the top edge of the note area

    juce::Array<te::MidiNote*> selection;
    Drag drag = Drag::none;
    te::MidiNote* dragNote = nullptr;
    double dragStartBeat = 0.0;
    int dragStartPitch = 0;
    struct NoteSnapshot { te::MidiNote* note; double start; double length; int pitch; };
    std::vector<NoteSnapshot> dragOriginals;
    juce::Rectangle<int> marquee;
    double lastNoteLength = 1.0;

    juce::ComboBox gridBox;
    juce::TextButton quantiseButton { "Quantise" };
    juce::Slider velocitySlider;
    juce::Label regionLabel, velocityLabel;
    int lastPlayheadX = -1;
};

} // namespace tess
