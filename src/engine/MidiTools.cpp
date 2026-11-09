#include "MidiTools.h"

namespace tess
{

double snapToGrid (double beats, double gridBeats)
{
    if (gridBeats <= 0.0)
        return beats;

    return std::round (beats / gridBeats) * gridBeats;
}

int quantiseNotes (te::MidiClip& clip, double gridBeats, double strength,
                   const juce::Array<te::MidiNote*>& onlySelected)
{
    strength = juce::jlimit (0.0, 1.0, strength);
    auto& seq = clip.getSequence();
    auto* um = &clip.edit.getUndoManager();
    int moved = 0;

    for (auto* note : seq.getNotes())
    {
        if (! onlySelected.isEmpty() && ! onlySelected.contains (note))
            continue;

        const double start = note->getStartBeat().inBeats();
        const double target = snapToGrid (start, gridBeats);
        const double newStart = juce::jmax (0.0, start + (target - start) * strength);

        if (std::abs (newStart - start) > 1.0e-9)
        {
            note->setStartAndLength (te::BeatPosition::fromBeats (newStart), note->getLengthBeats(), um);
            ++moved;
        }
    }

    return moved;
}

te::MidiNote* addNote (te::MidiClip& clip, int pitch, double startBeat, double lengthBeats, int velocity)
{
    return clip.getSequence().addNote (juce::jlimit (0, 127, pitch),
                                       te::BeatPosition::fromBeats (juce::jmax (0.0, startBeat)),
                                       te::BeatDuration::fromBeats (juce::jmax (1.0 / 64.0, lengthBeats)),
                                       juce::jlimit (1, 127, velocity), 0,
                                       &clip.edit.getUndoManager());
}

juce::String noteName (int midiNote)
{
    return juce::MidiMessage::getMidiNoteName (midiNote, true, true, 3);
}

} // namespace tess
