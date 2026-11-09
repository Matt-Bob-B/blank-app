#include "MainComponent.h"
#include "Commands.h"
#include "transport/ControlBar.h"
#include "arrange/ArrangeView.h"
#include "pianoroll/PianoRoll.h"
#include "mixer/MixerView.h"
#include "library/LibraryPanel.h"
#include "theme/Theme.h"
#include "engine/TrackOps.h"
#include "engine/Exporter.h"

namespace tess
{

using namespace theme;

namespace cmd
{
    enum : juce::CommandID
    {
        newProject = 1, openProject, saveProject, saveProjectAs, exportMixdown,
        undo, redo, deleteSelection, splitRegion, quantise,
        newInstrumentTrack, newAudioTrack, deleteTrack,
        playStop, record, cycle, metronome, goToStart,
        togglePianoRoll, toggleMixer, toggleLibrary, zoomIn, zoomOut,
        audioSettings, pluginManager
    };
}

//==============================================================================
/** Drag handle between the arrange area and the bottom editor. */
struct MainComponent::Divider  : public juce::Component
{
    std::function<void (int)> onDrag;
    int startY = 0;

    Divider() { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colour::separator);
        g.setColour (colour::textTertiary);
        g.fillRoundedRectangle (getLocalBounds().toFloat().withSizeKeepingCentre (36.0f, 3.0f), 1.5f);
    }

    void mouseDown (const juce::MouseEvent& e) override  { startY = e.getScreenY(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (onDrag) onDrag (e.getScreenY() - startY);
        startY = e.getScreenY();
    }
};

//==============================================================================
MainComponent::MainComponent (EngineHost& h) : host (h)
{
    setLookAndFeel (nullptr);
    divider = std::make_unique<Divider>();
    divider->onDrag = [this] (int dy)
    {
        bottomHeight = juce::jlimit (160, getHeight() - 220, bottomHeight - dy);
        resized();
    };
    addChildComponent (*divider);

    library = std::make_unique<LibraryPanel> (host.getEngine());
    library->onFileChosen = [this] (const juce::File& f) { placeAudioFile (f); };
    addChildComponent (*library);

    commandManager.registerAllCommandsForTarget (this);
    commandManager.setFirstCommandTarget (this);
    addKeyListener (commandManager.getKeyMappings());
    setWantsKeyboardFocus (true);

    host.addListener (this);
    rebuildForEdit();
    startTimerHz (4);

    setSize (1440, 900);
}

MainComponent::~MainComponent()
{
    host.removeListener (this);
    editAboutToBeReplaced();
}

void MainComponent::editAboutToBeReplaced()
{
    // Every view holds references into the Edit; drop them before it is deleted.
    pianoRoll.reset();
    mixer.reset();
    arrange.reset();
    controlBar.reset();
    viewState.reset();
}

void MainComponent::rebuildForEdit()
{
    editAboutToBeReplaced();

    auto& edit = host.getEdit();
    viewState = std::make_unique<EditViewState> (edit);

    controlBar = std::make_unique<ControlBar> (*viewState);
    controlBar->onToggleLibrary   = [this] { commandManager.invokeDirectly (cmd::toggleLibrary, true); };
    controlBar->onTogglePianoRoll = [this] { commandManager.invokeDirectly (cmd::togglePianoRoll, true); };
    controlBar->onToggleMixer     = [this] { commandManager.invokeDirectly (cmd::toggleMixer, true); };
    addAndMakeVisible (*controlBar);

    arrange = std::make_unique<ArrangeView> (*viewState);
    arrange->onOpenRegion = [this] (te::Clip& clip)
    {
        viewState->selectClip (&clip);

        if (dynamic_cast<te::MidiClip*> (&clip) != nullptr)
            setBottomPanel (BottomPanel::pianoRoll);
    };
    arrange->onAddTrack = [this]
    {
        juce::PopupMenu m;
        m.addItem ("New Instrument Track", [this] { addTrack (true); });
        m.addItem ("New Audio Track", [this] { addTrack (false); });
        m.showMenuAsync ({});
    };
    addAndMakeVisible (*arrange);

    pianoRoll = std::make_unique<PianoRoll> (*viewState);
    addChildComponent (*pianoRoll);

    mixer = std::make_unique<MixerView> (*viewState);
    addChildComponent (*mixer);

    // Select the first track so MIDI input plays it straight away.
    if (auto* first = te::getAudioTracks (edit)[0])
    {
        viewState->selectTrack (first);

        if (getTrackKind (*first) == TrackKind::instrument)
            armTrackForMidi (*first, true);
    }

    setBottomPanel (bottomPanel);
    resized();
}

void MainComponent::timerCallback()
{
    auto name = host.isUntitled() ? juce::String ("Untitled") : host.getProjectFile().getFileNameWithoutExtension();
    auto title = name + (host.hasUnsavedChanges() ? juce::String (" (edited)") : juce::String()) + juce::String (" \xe2\x80\x94 Tessitura");

    if (title != lastTitle)
    {
        lastTitle = title;
        if (onTitleChanged)
            onTitleChanged (title);
    }

    // MIDI follows the selected instrument track.
    if (viewState != nullptr && controlBar != nullptr)
        if (auto* t = viewState->getSelectedTrack())
            if (getTrackKind (*t) == TrackKind::instrument && ! isTrackArmed (*t) && ! host.getEdit().getTransport().isRecording())
                armTrackForMidi (*t, true);
}

void MainComponent::setBottomPanel (BottomPanel p)
{
    bottomPanel = p;
    pianoRoll->setVisible (p == BottomPanel::pianoRoll);
    mixer->setVisible (p == BottomPanel::mixer);
    divider->setVisible (p != BottomPanel::none);
    updatePanelButtons();
    resized();
}

void MainComponent::updatePanelButtons()
{
    if (controlBar != nullptr)
        controlBar->setPanelStates (libraryShown, bottomPanel == BottomPanel::pianoRoll, bottomPanel == BottomPanel::mixer);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colour::window);
}

void MainComponent::resized()
{
    if (controlBar == nullptr)
        return;

    auto r = getLocalBounds();
    controlBar->setBounds (r.removeFromTop (size::controlBarHeight));

    library->setVisible (libraryShown);
    if (libraryShown)
        library->setBounds (r.removeFromLeft (260));

    if (bottomPanel != BottomPanel::none)
    {
        bottomHeight = juce::jlimit (160, juce::jmax (160, r.getHeight() - 200), bottomHeight);
        auto bottom = r.removeFromBottom (bottomHeight);
        divider->setBounds (r.removeFromBottom (6));
        pianoRoll->setBounds (bottom);
        mixer->setBounds (bottom);
    }

    arrange->setBounds (r);
}

//==============================================================================
void MainComponent::getAllCommands (juce::Array<juce::CommandID>& c)
{
    c.addArray ({ cmd::newProject, cmd::openProject, cmd::saveProject, cmd::saveProjectAs, cmd::exportMixdown,
                  cmd::undo, cmd::redo, cmd::deleteSelection, cmd::splitRegion, cmd::quantise,
                  cmd::newInstrumentTrack, cmd::newAudioTrack, cmd::deleteTrack,
                  cmd::playStop, cmd::record, cmd::cycle, cmd::metronome, cmd::goToStart,
                  cmd::togglePianoRoll, cmd::toggleMixer, cmd::toggleLibrary, cmd::zoomIn, cmd::zoomOut,
                  cmd::audioSettings, cmd::pluginManager });
}

void MainComponent::getCommandInfo (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    using KP = juce::KeyPress;
    const auto ctrl = juce::ModifierKeys::commandModifier;
    const auto shift = juce::ModifierKeys::shiftModifier;
    auto& um = host.getEdit().getUndoManager();

    switch (id)
    {
        case cmd::newProject:       info.setInfo ("New Project", {}, "File", 0);              info.addDefaultKeypress ('n', ctrl); break;
        case cmd::openProject:      info.setInfo ("Open...", {}, "File", 0);                  info.addDefaultKeypress ('o', ctrl); break;
        case cmd::saveProject:      info.setInfo ("Save", {}, "File", 0);                     info.addDefaultKeypress ('s', ctrl); break;
        case cmd::saveProjectAs:    info.setInfo ("Save As...", {}, "File", 0);               info.addDefaultKeypress ('s', ctrl | shift); break;
        case cmd::exportMixdown:    info.setInfo ("Export Mixdown (WAV)...", {}, "File", 0);  info.addDefaultKeypress ('e', ctrl); break;

        case cmd::undo:
            info.setInfo ("Undo " + um.getUndoDescription(), {}, "Edit", 0);
            info.setActive (um.canUndo());
            info.addDefaultKeypress ('z', ctrl);
            break;
        case cmd::redo:
            info.setInfo ("Redo " + um.getRedoDescription(), {}, "Edit", 0);
            info.setActive (um.canRedo());
            info.addDefaultKeypress ('z', ctrl | shift);
            info.addDefaultKeypress ('y', ctrl);
            break;
        case cmd::deleteSelection:  info.setInfo ("Delete Region", {}, "Edit", 0);           info.addDefaultKeypress (KP::deleteKey, 0); info.addDefaultKeypress (KP::backspaceKey, 0); break;
        case cmd::splitRegion:      info.setInfo ("Split Region at Playhead", {}, "Edit", 0); info.addDefaultKeypress ('t', ctrl); break;
        case cmd::quantise:         info.setInfo ("Quantise Notes", {}, "Edit", 0);          info.addDefaultKeypress ('q', 0); break;

        case cmd::newInstrumentTrack: info.setInfo ("New Instrument Track", {}, "Track", 0); info.addDefaultKeypress ('n', ctrl | juce::ModifierKeys::altModifier); break;
        case cmd::newAudioTrack:      info.setInfo ("New Audio Track", {}, "Track", 0);      info.addDefaultKeypress ('a', ctrl | juce::ModifierKeys::altModifier); break;
        case cmd::deleteTrack:        info.setInfo ("Delete Track", {}, "Track", 0);         info.addDefaultKeypress (KP::deleteKey, ctrl); break;

        case cmd::playStop:   info.setInfo ("Play / Stop", {}, "Transport", 0);   info.addDefaultKeypress (KP::spaceKey, 0); break;
        case cmd::record:     info.setInfo ("Record", {}, "Transport", 0);        info.addDefaultKeypress ('r', 0); break;
        case cmd::cycle:      info.setInfo ("Cycle", {}, "Transport", 0);         info.addDefaultKeypress ('c', 0);
                              info.setTicked (host.getEdit().getTransport().looping.get()); break;
        case cmd::metronome:  info.setInfo ("Metronome", {}, "Transport", 0);     info.addDefaultKeypress ('k', 0);
                              info.setTicked (host.getEdit().clickTrackEnabled.get()); break;
        case cmd::goToStart:  info.setInfo ("Go to Beginning", {}, "Transport", 0); info.addDefaultKeypress (KP::homeKey, 0); info.addDefaultKeypress (KP::returnKey, 0); break;

        case cmd::togglePianoRoll: info.setInfo ("Piano Roll", {}, "View", 0); info.addDefaultKeypress ('p', 0); info.setTicked (bottomPanel == BottomPanel::pianoRoll); break;
        case cmd::toggleMixer:     info.setInfo ("Mixer", {}, "View", 0);      info.addDefaultKeypress ('x', 0); info.setTicked (bottomPanel == BottomPanel::mixer); break;
        case cmd::toggleLibrary:   info.setInfo ("Library", {}, "View", 0);    info.addDefaultKeypress ('y', 0); info.setTicked (libraryShown); break;
        case cmd::zoomIn:          info.setInfo ("Zoom In", {}, "View", 0);    info.addDefaultKeypress ('=', ctrl); break;
        case cmd::zoomOut:         info.setInfo ("Zoom Out", {}, "View", 0);   info.addDefaultKeypress ('-', ctrl); break;

        case cmd::audioSettings:  info.setInfo ("Audio & MIDI Devices...", {}, "Settings", 0); info.addDefaultKeypress (',', ctrl); break;
        case cmd::pluginManager:  info.setInfo ("Plug-in Manager...", {}, "Settings", 0); break;
        default: break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    auto& edit = host.getEdit();

    switch (info.commandID)
    {
        case cmd::newProject:       newProject(); break;
        case cmd::openProject:      openProject(); break;
        case cmd::saveProject:      saveProject(); break;
        case cmd::saveProjectAs:    saveProjectAs(); break;
        case cmd::exportMixdown:    exportMixdown(); break;

        case cmd::undo:             edit.undo(); break;
        case cmd::redo:             edit.redo(); break;
        case cmd::deleteSelection:  commands::deleteSelection (*viewState); break;
        case cmd::splitRegion:      commands::splitAtPlayhead (*viewState); break;
        case cmd::quantise:         commands::quantiseSelection (*viewState); break;

        case cmd::newInstrumentTrack: addTrack (true); break;
        case cmd::newAudioTrack:      addTrack (false); break;
        case cmd::deleteTrack:        deleteSelectedTrack(); break;

        case cmd::playStop:   commands::togglePlay (edit); break;
        case cmd::record:     commands::toggleRecord (edit, viewState->getSelectedTrack()); break;
        case cmd::cycle:      commands::toggleCycle (*viewState); break;
        case cmd::metronome:  commands::toggleMetronome (edit); break;
        case cmd::goToStart:  commands::goToStart (edit); break;

        case cmd::togglePianoRoll: setBottomPanel (bottomPanel == BottomPanel::pianoRoll ? BottomPanel::none : BottomPanel::pianoRoll); break;
        case cmd::toggleMixer:     setBottomPanel (bottomPanel == BottomPanel::mixer ? BottomPanel::none : BottomPanel::mixer); break;
        case cmd::toggleLibrary:   libraryShown = ! libraryShown; updatePanelButtons(); resized(); break;
        case cmd::zoomIn:          arrange->zoom (1.25); break;
        case cmd::zoomOut:         arrange->zoom (0.8); break;

        case cmd::audioSettings:   showAudioSettings(); break;
        case cmd::pluginManager:   showPluginManager(); break;
        default: return false;
    }

    commandManager.commandStatusChanged();
    return true;
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Track", "Transport", "View", "Settings" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu m;
    auto add = [&] (juce::CommandID id) { m.addCommandItem (&commandManager, id); };

    switch (index)
    {
        case 0: add (cmd::newProject); add (cmd::openProject); m.addSeparator();
                add (cmd::saveProject); add (cmd::saveProjectAs); m.addSeparator();
                add (cmd::exportMixdown); m.addSeparator();
                m.addCommandItem (&commandManager, juce::StandardApplicationCommandIDs::quit);
                break;
        case 1: add (cmd::undo); add (cmd::redo); m.addSeparator();
                add (cmd::deleteSelection); add (cmd::splitRegion); add (cmd::quantise);
                break;
        case 2: add (cmd::newInstrumentTrack); add (cmd::newAudioTrack); m.addSeparator(); add (cmd::deleteTrack); break;
        case 3: add (cmd::playStop); add (cmd::record); add (cmd::goToStart); m.addSeparator(); add (cmd::cycle); add (cmd::metronome); break;
        case 4: add (cmd::toggleLibrary); add (cmd::togglePianoRoll); add (cmd::toggleMixer); m.addSeparator(); add (cmd::zoomIn); add (cmd::zoomOut); break;
        case 5: add (cmd::audioSettings); add (cmd::pluginManager); break;
        default: break;
    }

    return m;
}

//==============================================================================
void MainComponent::confirmDiscardChanges (std::function<void()> proceed)
{
    if (! host.hasUnsavedChanges())
    {
        proceed();
        return;
    }

    auto options = juce::MessageBoxOptions::makeOptionsYesNoCancel (juce::MessageBoxIconType::QuestionIcon,
                                                                    "Save changes?",
                                                                    "Do you want to save the changes to this project before continuing?",
                                                                    "Save", "Don't Save", "Cancel", this);

    juce::AlertWindow::showAsync (options, [this, proceed] (int result)
    {
        if (result == 1)       saveProject ([proceed] (bool ok) { if (ok) proceed(); });
        else if (result == 2)  proceed();
    });
}

void MainComponent::newProject()
{
    confirmDiscardChanges ([this] { host.newProject(); });
}

void MainComponent::openProject()
{
    confirmDiscardChanges ([this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Open Project",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       juce::String ("*") + EngineHost::projectExtension);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (! f.existsAsFile())
                                      return;

                                  auto error = host.openProject (f);
                                  if (error.isNotEmpty())
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open project", error);
                              });
    });
}

void MainComponent::saveProject (std::function<void (bool)> done)
{
    if (host.isUntitled())
    {
        saveProjectAs (done);
        return;
    }

    const bool ok = host.saveProject();

    if (! ok)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save failed",
                                                "Couldn't write " + host.getProjectFile().getFullPathName());
    if (done)
        done (ok);
}

void MainComponent::saveProjectAs (std::function<void (bool)> done)
{
    auto start = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Tessitura").getChildFile ("Untitled");
    start.getParentDirectory().createDirectory();

    chooser = std::make_unique<juce::FileChooser> ("Save Project", start.withFileExtension (EngineHost::projectExtension),
                                                   juce::String ("*") + EngineHost::projectExtension);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this, done] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();

                              if (f == juce::File())
                              {
                                  if (done) done (false);
                                  return;
                              }

                              // Each project lives in its own folder (project file + Audio folder).
                              auto name = f.getFileNameWithoutExtension();
                              auto folder = f.getParentDirectory().getFileName() == name ? f.getParentDirectory()
                                                                                          : f.getParentDirectory().getChildFile (name);
                              auto target = folder.getChildFile (name + EngineHost::projectExtension);
                              const bool ok = host.saveProjectAs (target);

                              if (! ok)
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save failed",
                                                                          "Couldn't write " + target.getFullPathName());
                              if (done)
                                  done (ok);
                          });
}

void MainComponent::exportMixdown()
{
    auto name = host.isUntitled() ? juce::String ("Untitled") : host.getProjectFile().getFileNameWithoutExtension();
    auto dir = host.isUntitled() ? juce::File::getSpecialLocation (juce::File::userMusicDirectory) : host.getProjectFile().getParentDirectory();

    chooser = std::make_unique<juce::FileChooser> ("Export Mixdown", dir.getChildFile (name + ".wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File())
                                  return;

                              auto error = tess::exportMixdown (host.getEdit(), f);

                              if (error.isEmpty())
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Export complete",
                                                                          "Saved " + f.withFileExtension (".wav").getFullPathName());
                              else
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Export failed", error);
                          });
}

void MainComponent::showAudioSettings()
{
    auto& dm = host.getEngine().getDeviceManager().deviceManager;
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent> (dm, 0, 256, 1, 256, true, false, true, false);
    selector->setSize (520, 560);

    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Audio & MIDI Devices";
    o.dialogBackgroundColour = colour::panel;
    o.content.setOwned (selector.release());
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

void MainComponent::showPluginManager()
{
    auto& pm = host.getEngine().getPluginManager();
    auto deadMansPedal = host.getEngine().getPropertyStorage().getAppPrefsFolder().getChildFile ("PluginScanCrashList.txt");
    auto list = std::make_unique<juce::PluginListComponent> (pm.pluginFormatManager, pm.knownPluginList, deadMansPedal, nullptr, true);
    list->setSize (760, 560);

    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Plug-in Manager \xe2\x80\x94 click Options > Scan for new or updated VST3 plug-ins";
    o.dialogBackgroundColour = colour::panel;
    o.content.setOwned (list.release());
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

void MainComponent::addTrack (bool instrument)
{
    auto& edit = host.getEdit();
    edit.getUndoManager().beginNewTransaction (instrument ? "New Instrument Track" : "New Audio Track");

    int count = 1;
    for (auto* t : te::getAudioTracks (edit))
        if ((getTrackKind (*t) == TrackKind::instrument) == instrument)
            ++count;

    auto* track = instrument ? addInstrumentTrack (edit, "Instrument " + juce::String (count))
                             : addAudioTrack (edit, "Audio " + juce::String (count));

    if (track != nullptr)
        viewState->selectTrack (track);
}

void MainComponent::deleteSelectedTrack()
{
    if (auto* t = viewState->getSelectedTrack())
    {
        auto& edit = host.getEdit();
        edit.getUndoManager().beginNewTransaction ("Delete Track");
        viewState->selectClip (nullptr);
        viewState->selectTrack (nullptr);
        edit.deleteTrack (t);
    }
}

void MainComponent::placeAudioFile (const juce::File& f)
{
    auto& edit = host.getEdit();
    auto* track = viewState->getSelectedTrack();

    edit.getUndoManager().beginNewTransaction ("Add Audio");

    if (track == nullptr || getTrackKind (*track) != TrackKind::audio)
        track = addAudioTrack (edit, f.getFileNameWithoutExtension());

    if (track != nullptr)
        if (auto* clip = addAudioRegion (*track, f, edit.getTransport().getPosition()))
            viewState->selectClip (clip);
}

} // namespace tess
