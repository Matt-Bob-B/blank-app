#pragma once

#include "Tess.h"

namespace tess
{

/** Grid sizes offered in the piano roll, expressed in beats (quarter notes). */
struct Grid
{
    static constexpr double bar       = 4.0;
    static constexpr double quarter   = 1.0;
    static constexpr double eighth    = 0.5;
    static constexpr double sixteenth = 0.25;
    static constexpr double thirtySecond = 0.125;
};

/** Moves note starts toward the nearest grid line.
    @param gridBeats  grid size in beats (e.g. Grid::sixteenth)
    @param strength   0..1 — 1 snaps fully, 0.5 moves halfway (a "humanised" quantise)
    @param onlySelected  if non-empty, only these notes are quantised
    Note lengths are preserved. Returns the number of notes moved. */
int quantiseNotes (te::MidiClip&, double gridBeats, double strength = 1.0,
                   const juce::Array<te::MidiNote*>& onlySelected = {});

/** Snaps a beat value to the nearest grid line. */
double snapToGrid (double beats, double gridBeats);

/** Adds a note to the clip. Positions are in beats relative to the clip start. */
te::MidiNote* addNote (te::MidiClip&, int pitch, double startBeat, double lengthBeats, int velocity = 100);

/** Returns a display name for a MIDI note number, e.g. 60 -> "C3" (Middle C = C3). */
juce::String noteName (int midiNote);

} // namespace tess
