#pragma once

#include "Tess.h"

namespace tess
{

/** Tessitura distinguishes two kinds of track. In Tracktion both are AudioTracks;
    the kind is stored on the track's state so it survives save/load. */
enum class TrackKind { audio, instrument };

TrackKind getTrackKind (const te::AudioTrack&);

/** Appends an instrument track with the built-in synth loaded. */
te::AudioTrack* addInstrumentTrack (te::Edit&, const juce::String& name);

/** Appends an audio track (records from / plays audio files). */
te::AudioTrack* addAudioTrack (te::Edit&, const juce::String& name);

/** The track's instrument plugin (first synth in the plugin list), or nullptr. */
te::Plugin* getInstrument (te::AudioTrack&);

/** Replaces the instrument on a track (built-in or hosted VST3). */
void setInstrument (te::AudioTrack&, te::Plugin::Ptr newInstrument);

/** Creates an empty MIDI region on the track covering the given bar range (bars are 0-based). */
te::MidiClip* addMidiRegion (te::AudioTrack&, int startBar, int numBars);

/** Places an audio file on the track at the given time. */
te::WaveAudioClip* addAudioRegion (te::AudioTrack&, const juce::File& audioFile, te::TimePosition start);

/** Routes all MIDI inputs to this track and arms (or disarms) it for recording. */
void armTrackForMidi (te::AudioTrack&, bool shouldBeArmed);

/** Routes the default audio input to this track and arms (or disarms) it for recording. */
void armTrackForAudio (te::AudioTrack&, bool shouldBeArmed);

bool isTrackArmed (te::AudioTrack&);

/** Returns the beat position at the start of the given bar (0-based) using the Edit's tempo map. */
te::BeatPosition barToBeat (te::Edit&, int bar);

} // namespace tess
