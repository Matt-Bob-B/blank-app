#pragma once

#include "ui/EditViewState.h"
#include "ui/IconButton.h"

namespace tess
{

/** The LCD-style position / tempo / signature display in the centre of the control bar.
    Drag the tempo up or down to change it; double-click tempo or signature to type a value. */
class TransportDisplay  : public juce::Component,
                          private juce::Timer
{
public:
    explicit TransportDisplay (te::Edit&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<int> positionArea() const;
    juce::Rectangle<int> tempoArea() const;
    juce::Rectangle<int> signatureArea() const;
    void editValue (juce::Rectangle<int> area, const juce::String& current, std::function<void (const juce::String&)> apply);

    te::Edit& edit;
    double tempoAtDragStart = 0.0;
    bool draggingTempo = false;
    std::unique_ptr<juce::TextEditor> inlineEditor;
};

//==============================================================================
/** Top toolbar: view toggles, transport, LCD, metronome. */
class ControlBar  : public juce::Component,
                    private juce::Timer
{
public:
    ControlBar (EditViewState&);

    std::function<void()> onToggleLibrary, onTogglePianoRoll, onToggleMixer;

    void setPanelStates (bool libraryShown, bool pianoRollShown, bool mixerShown);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    EditViewState& viewState;
    te::Edit& edit;

    IconButton libraryButton   { "Library (Y)", icons::Icon::library };
    IconButton toStartButton   { "Go to Beginning (Home)", icons::Icon::toStart };
    IconButton stopButton      { "Stop", icons::Icon::stop };
    IconButton playButton      { "Play / Stop (Space)", icons::Icon::play, theme::colour::play };
    IconButton recordButton    { "Record (R)", icons::Icon::record, theme::colour::record };
    IconButton cycleButton     { "Cycle (C)", icons::Icon::cycle, theme::colour::cycle };
    IconButton metronomeButton { "Metronome (K)", icons::Icon::metronome, theme::colour::accent };
    IconButton pianoRollButton { "Piano Roll (P)", icons::Icon::pianoRoll };
    IconButton mixerButton     { "Mixer (X)", icons::Icon::mixer };

    TransportDisplay display;
};

} // namespace tess
