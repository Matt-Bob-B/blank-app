#include "EngineHost.h"
#include "TrackOps.h"
#include "UIBehaviour.h"

namespace tess
{

EngineHost::EngineHost (std::unique_ptr<te::UIBehaviour> uiBehaviour)
{
    if (uiBehaviour == nullptr)
        uiBehaviour = std::make_unique<HeadlessUIBehaviour>();

    engine = std::make_unique<te::Engine> ("Tessitura", std::move (uiBehaviour), nullptr);

    // Balance-style pan: a centred stereo track plays at unity gain.
    te::setDefaultPanLaw (te::PanLawLinear);

    // Make every MIDI input available for recording and play-through.
    for (auto& midiIn : engine->getDeviceManager().getMidiInDevices())
    {
        midiIn->setMonitorMode (te::InputDevice::MonitorMode::automatic);
        midiIn->setEnabled (true);
    }

    newProject();
}

EngineHost::~EngineHost()
{
    if (edit != nullptr)
        edit->getTransport().stop (false, true);

    edit.reset();
    engine->getTemporaryFileManager().getTempDirectory().deleteRecursively();
}

void EngineHost::newProject()
{
    auto dir = engine->getTemporaryFileManager().getTempDirectory().getChildFile ("Untitled");
    dir.deleteRecursively();
    dir.createDirectory();
    auto file = dir.getChildFile (juce::String ("Untitled") + projectExtension);

    auto newEdit = te::createEmptyEdit (*engine, file);
    // Tracktion's empty Edit includes a default track; start from a clean slate.
    for (auto* t : te::getAudioTracks (*newEdit))
        newEdit->deleteTrack (t);

    newEdit->tempoSequence.getTempo (0)->setBpm (120.0);

    if (auto master = newEdit->getMasterVolumePlugin())
        master->setVolumeDb (0.0f);
    newEdit->tempoSequence.getTimeSig (0)->setStringTimeSig ("4/4");

    addInstrumentTrack (*newEdit, "Instrument 1");
    addAudioTrack (*newEdit, "Audio 1");

    newEdit->getUndoManager().clearUndoHistory();
    newEdit->resetChangedStatus();
    installEdit (std::move (newEdit), file, true);
}

juce::String EngineHost::openProject (const juce::File& file)
{
    if (! file.existsAsFile())
        return "File not found: " + file.getFullPathName();

    auto xml = juce::parseXML (file);

    if (xml == nullptr || ! xml->hasTagName (te::IDs::EDIT.toString()))
        return "Not a Tessitura project: " + file.getFileName();

    auto loaded = te::loadEditFromFile (*engine, file);

    if (loaded == nullptr)
        return "Could not load project: " + file.getFileName();

    installEdit (std::move (loaded), file, false);
    return {};
}

bool EngineHost::saveProject()
{
    if (untitled)
        return false;

    return te::EditFileOperations (*edit).save (false, true, false);
}

bool EngineHost::saveProjectAs (const juce::File& file)
{
    auto target = file.withFileExtension (projectExtension);

    if (! target.getParentDirectory().createDirectory())
        return false;

    if (! te::EditFileOperations (*edit).saveAs (target, true))
        return false;

    projectFile = target;
    untitled = false;
    edit->editFileRetriever = [target] { return target; };
    updateRecordingPaths();
    sendChangeMessage();
    return true;
}

bool EngineHost::hasUnsavedChanges() const
{
    return edit != nullptr && edit->hasChangedSinceSaved();
}

juce::File EngineHost::getAudioFolder() const
{
    return projectFile.getParentDirectory().getChildFile ("Audio");
}

void EngineHost::installEdit (std::unique_ptr<te::Edit> newEdit, const juce::File& file, bool isUntitled)
{
    if (edit != nullptr)
        edit->getTransport().stop (false, true);

    edit = std::move (newEdit);
    projectFile = file;
    untitled = isUntitled;
    edit->editFileRetriever = [file] { return file; };

    updateRecordingPaths();

    edit->getTransport().ensureContextAllocated();
    sendChangeMessage();
}

void EngineHost::updateRecordingPaths()
{
    // Recordings land in "<project folder>/Audio/<project>_<track>_Take_<n>.wav".
    auto audioFolder = getAudioFolder();
    audioFolder.createDirectory();

    auto mask = audioFolder.getFullPathName() + juce::File::getSeparatorString() + "%edit%_%track%_Take_%take%";

    for (auto* waveIn : engine->getDeviceManager().getWaveInputDevices())
        waveIn->setFilenameMask (mask);
}

} // namespace tess
