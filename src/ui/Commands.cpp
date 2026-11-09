#include "Commands.h"
#include "engine/TrackOps.h"
#include "engine/MidiTools.h"

namespace tess::commands
{

void togglePlay (te::Edit& edit)
{
    auto& t = edit.getTransport();

    if (t.isPlaying())
        t.stop (false, false);
    else
        t.play (false);
}

void stop (te::Edit& edit)
{
    auto& t = edit.getTransport();

    if (t.isPlaying() || t.isRecording())
        t.stop (false, false);
    else
        t.setPosition (te::TimePosition());   // second press of stop returns to start
}

void goToStart (te::Edit& edit)
{
    edit.getTransport().setPosition (te::TimePosition());
}

void toggleRecord (te::Edit& edit, te::AudioTrack* selected)
{
    auto& t = edit.getTransport();

    if (t.isRecording())
    {
        t.stop (false, false);
        return;
    }

    bool anyArmed = false;

    for (auto* track : te::getAudioTracks (edit))
        anyArmed = anyArmed || isTrackArmed (*track);

    if (! anyArmed && selected != nullptr)
    {
        if (getTrackKind (*selected) == TrackKind::instrument)
            armTrackForMidi (*selected, true);
        else
            armTrackForAudio (*selected, true);
    }

    t.record (false);
}

void toggleCycle (EditViewState& vs)
{
    auto& t = vs.edit.getTransport();

    if (! t.looping.get() && t.getLoopRange().isEmpty())
    {
        if (auto* clip = vs.getSelectedClip())
            t.setLoopRange (clip->getEditTimeRange());
        else
            t.setLoopRange ({ te::TimePosition(), vs.beatsToTime (vs.beatsPerBar() * 4.0) });
    }

    t.looping = ! t.looping.get();
}

void toggleMetronome (te::Edit& edit)
{
    edit.clickTrackEnabled = ! edit.clickTrackEnabled.get();
}

void deleteSelection (EditViewState& vs)
{
    if (auto* clip = vs.getSelectedClip())
    {
        vs.edit.getUndoManager().beginNewTransaction ("Delete Region");
        vs.selectClip (nullptr);
        clip->removeFromParent();
    }
}

void quantiseSelection (EditViewState& vs)
{
    if (auto* midi = dynamic_cast<te::MidiClip*> (vs.getSelectedClip()))
    {
        vs.edit.getUndoManager().beginNewTransaction ("Quantise");
        quantiseNotes (*midi, vs.pianoRollGrid);
    }
}

void splitAtPlayhead (EditViewState& vs)
{
    auto pos = vs.edit.getTransport().getPosition();

    if (auto* clip = vs.getSelectedClip())
    {
        if (clip->getEditTimeRange().containsInclusive (pos) && clip->getPosition().getStart() < pos)
        {
            if (auto* owner = dynamic_cast<te::ClipTrack*> (clip->getTrack()))
            {
                vs.edit.getUndoManager().beginNewTransaction ("Split Region");
                owner->splitClip (*clip, pos);
            }
        }
    }
}

} // namespace tess::commands
