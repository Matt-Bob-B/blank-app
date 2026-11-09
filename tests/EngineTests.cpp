#include <doctest.h>

#include "engine/EngineHost.h"
#include "engine/TrackOps.h"
#include "engine/MidiTools.h"
#include "engine/Exporter.h"

using namespace tess;

namespace
{
    // Tracktion expects a single Engine per process, so the tests share one host.
    EngineHost& host()
    {
        static EngineHost h;
        return h;
    }

    struct TempDir
    {
        TempDir()  { dir.createDirectory(); }
        ~TempDir() { dir.deleteRecursively(); }
        juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getNonexistentChildFile ("tessitura_test", {}, false);
    };

    struct Rendered
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 0;
    };

    Rendered readWav (const juce::File& f)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        Rendered r;

        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (fm.createReaderFor (f)))
        {
            r.buffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&r.buffer, 0, r.buffer.getNumSamples(), 0, true, true);
            r.sampleRate = reader->sampleRate;
        }

        return r;
    }

    juce::File writeSineWav (const juce::File& f, double seconds, float amplitude)
    {
        const double sr = 44100.0;
        juce::AudioBuffer<float> buf (2, (int) (seconds * sr));

        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            auto v = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / sr);
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }

        juce::WavAudioFormat wav;
        f.deleteFile();
        std::unique_ptr<juce::OutputStream> out (f.createOutputStream().release());
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (out.get(), sr, 2, 24, {}, 0));
        REQUIRE (writer != nullptr);
        out.release();
        writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
        return f;
    }

    te::AudioTrack* firstTrackOfKind (te::Edit& edit, TrackKind kind)
    {
        for (auto* t : te::getAudioTracks (edit))
            if (getTrackKind (*t) == kind)
                return t;

        return nullptr;
    }
}

TEST_CASE ("New project has default session layout")
{
    host().newProject();
    auto& edit = host().getEdit();

    auto tracks = te::getAudioTracks (edit);
    REQUIRE (tracks.size() == 2);
    CHECK (getTrackKind (*tracks[0]) == TrackKind::instrument);
    CHECK (getTrackKind (*tracks[1]) == TrackKind::audio);
    CHECK (getInstrument (*tracks[0]) != nullptr);
    CHECK (getInstrument (*tracks[1]) == nullptr);
    CHECK (edit.tempoSequence.getTempo (0)->getBpm() == doctest::Approx (120.0));
    CHECK (host().isUntitled());
}

TEST_CASE ("Exporting an empty project reports an error")
{
    host().newProject();
    TempDir tmp;
    auto err = exportMixdown (host().getEdit(), tmp.dir.getChildFile ("empty.wav"));
    CHECK (err.isNotEmpty());
}

TEST_CASE ("MIDI region through the built-in synth renders audio")
{
    host().newProject();
    auto& edit = host().getEdit();
    auto* track = firstTrackOfKind (edit, TrackKind::instrument);
    REQUIRE (track != nullptr);

    auto* clip = addMidiRegion (*track, 0, 1);
    REQUIRE (clip != nullptr);
    addNote (*clip, 60, 0.0, 2.0, 110);
    addNote (*clip, 64, 0.0, 2.0, 110);

    TempDir tmp;
    auto out = tmp.dir.getChildFile ("midi.wav");
    ExportSettings settings;
    settings.sampleRate = 48000.0;
    settings.tail = te::TimeDuration::fromSeconds (1.0);

    REQUIRE (exportMixdown (edit, out, settings).isEmpty());
    REQUIRE (out.existsAsFile());

    auto r = readWav (out);
    CHECK (r.sampleRate == doctest::Approx (48000.0));
    CHECK (r.buffer.getNumChannels() == 2);

    // One bar at 120 bpm (2 s) plus a 1 s tail.
    CHECK (r.buffer.getNumSamples() / r.sampleRate == doctest::Approx (3.0).epsilon (0.02));

    // Notes sound during the first second...
    CHECK (r.buffer.getMagnitude (0, (int) r.sampleRate) > 0.01f);
    // ...and nothing exceeds full scale.
    CHECK (r.buffer.getMagnitude (0, r.buffer.getNumSamples()) <= 1.0f);
}

TEST_CASE ("Audio file region renders at the expected level")
{
    host().newProject();
    auto& edit = host().getEdit();
    auto* track = firstTrackOfKind (edit, TrackKind::audio);
    REQUIRE (track != nullptr);

    TempDir tmp;
    auto sine = writeSineWav (tmp.dir.getChildFile ("sine.wav"), 1.0, 0.5f);
    auto* clip = addAudioRegion (*track, sine, te::TimePosition::fromSeconds (1.0));
    REQUIRE (clip != nullptr);
    CHECK (clip->getPosition().getStart().inSeconds() == doctest::Approx (1.0));

    auto out = tmp.dir.getChildFile ("audio.wav");
    ExportSettings settings;
    settings.sampleRate = 44100.0;
    settings.tail = te::TimeDuration();
    REQUIRE (exportMixdown (edit, out, settings).isEmpty());

    auto r = readWav (out);
    const int sr = (int) r.sampleRate;
    REQUIRE (r.buffer.getNumSamples() >= 2 * sr - 64);

    // Silence before the region, sine (unity gain, so ~0.5 peak) inside it.
    CHECK (r.buffer.getMagnitude (0, sr / 2) < 0.001f);
    CHECK (r.buffer.getMagnitude (sr + sr / 4, sr / 2) == doctest::Approx (0.5f).epsilon (0.05));
}

TEST_CASE ("Quantise snaps note starts to the grid")
{
    host().newProject();
    auto* track = firstTrackOfKind (host().getEdit(), TrackKind::instrument);
    auto* clip = addMidiRegion (*track, 0, 2);
    REQUIRE (clip != nullptr);

    addNote (*clip, 60, 0.1, 0.5);
    addNote (*clip, 62, 1.07, 0.5);
    addNote (*clip, 64, 2.4, 0.3);

    SUBCASE ("full strength")
    {
        CHECK (quantiseNotes (*clip, Grid::sixteenth) == 3);
        auto notes = clip->getSequence().getNotes();
        CHECK (notes[0]->getStartBeat().inBeats() == doctest::Approx (0.0));
        CHECK (notes[1]->getStartBeat().inBeats() == doctest::Approx (1.0));
        CHECK (notes[2]->getStartBeat().inBeats() == doctest::Approx (2.5));
        CHECK (notes[2]->getLengthBeats().inBeats() == doctest::Approx (0.3)); // lengths untouched
    }

    SUBCASE ("half strength")
    {
        quantiseNotes (*clip, Grid::sixteenth, 0.5);
        auto notes = clip->getSequence().getNotes();
        CHECK (notes[0]->getStartBeat().inBeats() == doctest::Approx (0.05));
        CHECK (notes[2]->getStartBeat().inBeats() == doctest::Approx (2.45));
    }

    SUBCASE ("undo restores the original positions")
    {
        auto& um = host().getEdit().getUndoManager();
        um.beginNewTransaction();
        quantiseNotes (*clip, Grid::quarter);
        um.undo();
        CHECK (clip->getSequence().getNotes()[1]->getStartBeat().inBeats() == doctest::Approx (1.07));
    }
}

TEST_CASE ("Bars follow the time signature")
{
    host().newProject();
    auto& edit = host().getEdit();
    CHECK (barToBeat (edit, 2).inBeats() == doctest::Approx (8.0));

    edit.tempoSequence.getTimeSig (0)->setStringTimeSig ("3/4");
    CHECK (barToBeat (edit, 2).inBeats() == doctest::Approx (6.0));

    auto* clip = addMidiRegion (*firstTrackOfKind (edit, TrackKind::instrument), 1, 1);
    REQUIRE (clip != nullptr);
    // Bar 2 in 3/4 at 120 bpm starts at 1.5 s and lasts 1.5 s.
    CHECK (clip->getPosition().getStart().inSeconds() == doctest::Approx (1.5));
    CHECK (clip->getPosition().getLength().inSeconds() == doctest::Approx (1.5));
}

TEST_CASE ("Project save and reload round-trips tracks, regions and notes")
{
    host().newProject();
    auto& edit = host().getEdit();
    edit.tempoSequence.getTempo (0)->setBpm (96.0);

    auto* track = firstTrackOfKind (edit, TrackKind::instrument);
    track->setName ("Keys");
    auto* clip = addMidiRegion (*track, 0, 1);
    addNote (*clip, 67, 1.0, 1.0, 90);

    TempDir tmp;
    auto file = tmp.dir.getChildFile ("Song").getChildFile ("Song.tessproj");
    REQUIRE (host().saveProjectAs (file));
    CHECK (file.existsAsFile());
    CHECK (! host().isUntitled());
    CHECK (host().getAudioFolder().isDirectory());

    host().newProject();
    REQUIRE (host().openProject (file).isEmpty());

    auto& loaded = host().getEdit();
    CHECK (loaded.tempoSequence.getTempo (0)->getBpm() == doctest::Approx (96.0));

    auto tracks = te::getAudioTracks (loaded);
    REQUIRE (tracks.size() == 2);
    CHECK (tracks[0]->getName() == "Keys");
    CHECK (getTrackKind (*tracks[0]) == TrackKind::instrument);
    CHECK (getInstrument (*tracks[0]) != nullptr);

    auto clips = tracks[0]->getClips();
    REQUIRE (clips.size() == 1);
    auto* loadedClip = dynamic_cast<te::MidiClip*> (clips[0]);
    REQUIRE (loadedClip != nullptr);
    auto notes = loadedClip->getSequence().getNotes();
    REQUIRE (notes.size() == 1);
    CHECK (notes[0]->getNoteNumber() == 67);
    CHECK (notes[0]->getVelocity() == 90);
    CHECK (notes[0]->getStartBeat().inBeats() == doctest::Approx (1.0));
}

TEST_CASE ("Opening a non-project file fails cleanly")
{
    TempDir tmp;
    auto bogus = tmp.dir.getChildFile ("bogus.tessproj");
    bogus.replaceWithText ("not xml");
    CHECK (host().openProject (bogus).isNotEmpty());
    CHECK (host().openProject (tmp.dir.getChildFile ("missing.tessproj")).isNotEmpty());
}

TEST_CASE ("Note names use Middle C = C3")
{
    CHECK (noteName (60) == "C3");
    CHECK (noteName (69) == "A3");
}
