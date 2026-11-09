#pragma once

#include "engine/Tess.h"

namespace tess
{

/** View state shared by the arrange window, piano roll, mixer and inspector:
    selection, zoom and grid. Components listen for changes and repaint. */
class EditViewState  : public juce::ChangeBroadcaster
{
public:
    explicit EditViewState (te::Edit& e) : edit (e) {}

    te::Edit& edit;

    // Arrange zoom and scroll (horizontal unit = beats).
    double pixelsPerBeat = 28.0;
    double scrollBeats = 0.0;
    double gridBeats = 1.0;     ///< arrange snap; 0 = off
    bool snapEnabled = true;

    // Piano roll grid.
    double pianoRollGrid = 0.25;

    te::AudioTrack* getSelectedTrack() const    { return dynamic_cast<te::AudioTrack*> (selectedTrack.get()); }
    te::Clip* getSelectedClip() const           { return selectedClip.get(); }

    void selectTrack (te::AudioTrack* t)
    {
        if (selectedTrack.get() != t)
        {
            selectedTrack = t;
            sendChangeMessage();
        }
    }

    void selectClip (te::Clip* c)
    {
        if (selectedClip.get() != c)
        {
            selectedClip = c;

            if (c != nullptr)
                if (auto* t = dynamic_cast<te::AudioTrack*> (c->getTrack()))
                    selectedTrack = t;

            sendChangeMessage();
        }
    }

    double timeToBeats (te::TimePosition t) const   { return edit.tempoSequence.toBeats (t).inBeats(); }
    te::TimePosition beatsToTime (double b) const   { return edit.tempoSequence.toTime (te::BeatPosition::fromBeats (b)); }

    double snap (double beats, bool bypass = false) const
    {
        if (! snapEnabled || bypass || gridBeats <= 0.0)
            return beats;

        return std::round (beats / gridBeats) * gridBeats;
    }

    double beatsPerBar() const
    {
        auto& sig = *edit.tempoSequence.getTimeSig (0);
        return sig.numerator.get() * 4.0 / sig.denominator.get();
    }

private:
    te::Track::Ptr selectedTrack;
    te::Clip::Ptr selectedClip;
};

} // namespace tess
