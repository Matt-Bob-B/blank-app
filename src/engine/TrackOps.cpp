#include "TrackOps.h"

namespace tess
{

namespace
{
    const juce::Identifier kindProperty ("tessKind");

    te::AudioTrack* appendTrack (te::Edit& edit, const juce::String& name, TrackKind kind)
    {
        auto track = edit.insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (edit), nullptr);

        if (track == nullptr)
            return nullptr;

        track->setName (name);
        track->state.setProperty (kindProperty, kind == TrackKind::instrument ? "instrument" : "audio", nullptr);
        return track.get();
    }

    bool isInstrumentPlugin (te::Plugin& p)
    {
        return p.isSynth() || dynamic_cast<te::FourOscPlugin*> (&p) != nullptr
                || dynamic_cast<te::SamplerPlugin*> (&p) != nullptr;
    }

    void setArmed (te::AudioTrack& track, bool arm, te::InputDevice::DeviceType wantedType)
    {
        auto& edit = track.edit;
        edit.getTransport().ensureContextAllocated();

        for (auto* instance : edit.getAllInputDevices())
        {
            if (instance->getInputDevice().getDeviceType() != wantedType)
                continue;

            if (arm)
            {
                [[maybe_unused]] auto res = instance->setTarget (track.itemID, true, &edit.getUndoManager(), 0);
                instance->setRecordingEnabled (track.itemID, true);
            }
            else if (te::isOnTargetTrack (*instance, track, 0))
            {
                instance->setRecordingEnabled (track.itemID, false);
            }

            if (wantedType == te::InputDevice::waveDevice)
                break; // only route the first (default) audio input
        }

        edit.restartPlayback();
    }
}

TrackKind getTrackKind (const te::AudioTrack& track)
{
    return track.state[kindProperty].toString() == "instrument" ? TrackKind::instrument : TrackKind::audio;
}

te::AudioTrack* addInstrumentTrack (te::Edit& edit, const juce::String& name)
{
    auto* track = appendTrack (edit, name, TrackKind::instrument);

    if (track != nullptr)
        if (auto synth = edit.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}))
            track->pluginList.insertPlugin (synth, 0, nullptr);

    return track;
}

te::AudioTrack* addAudioTrack (te::Edit& edit, const juce::String& name)
{
    return appendTrack (edit, name, TrackKind::audio);
}

te::Plugin* getInstrument (te::AudioTrack& track)
{
    for (auto* p : track.pluginList.getPlugins())
        if (isInstrumentPlugin (*p))
            return p;

    return nullptr;
}

void setInstrument (te::AudioTrack& track, te::Plugin::Ptr newInstrument)
{
    if (auto* existing = getInstrument (track))
        existing->deleteFromParent();

    if (newInstrument != nullptr)
        track.pluginList.insertPlugin (newInstrument, 0, nullptr);

    track.state.setProperty (kindProperty, "instrument", nullptr);
}

te::BeatPosition barToBeat (te::Edit& edit, int bar)
{
    // Walk the tempo map so bars stay correct across time-signature changes.
    auto& ts = edit.tempoSequence;
    return ts.toBeats (te::tempo::BarsAndBeats { bar, {} });
}

te::MidiClip* addMidiRegion (te::AudioTrack& track, int startBar, int numBars)
{
    auto& edit = track.edit;
    auto& ts = edit.tempoSequence;
    auto start = ts.toTime (barToBeat (edit, startBar));
    auto end   = ts.toTime (barToBeat (edit, startBar + juce::jmax (1, numBars)));

    auto clip = track.insertMIDIClip (te::TimeRange (start, end), nullptr);
    return clip.get();
}

te::WaveAudioClip* addAudioRegion (te::AudioTrack& track, const juce::File& file, te::TimePosition start)
{
    te::AudioFile audioFile (track.edit.engine, file);

    if (! audioFile.isValid())
        return nullptr;

    auto length = te::TimeDuration::fromSeconds (audioFile.getLength());
    auto clip = track.insertWaveClip (file.getFileNameWithoutExtension(), file,
                                      { { start, start + length }, {} }, false);
    return clip.get();
}

void armTrackForMidi (te::AudioTrack& track, bool shouldBeArmed)
{
    setArmed (track, shouldBeArmed, te::InputDevice::physicalMidiDevice);
}

void armTrackForAudio (te::AudioTrack& track, bool shouldBeArmed)
{
    setArmed (track, shouldBeArmed, te::InputDevice::waveDevice);
}

bool isTrackArmed (te::AudioTrack& track)
{
    for (auto* instance : track.edit.getAllInputDevices())
        if (te::isOnTargetTrack (*instance, track, 0) && instance->isRecordingEnabled (track.itemID))
            return true;

    return false;
}

} // namespace tess
