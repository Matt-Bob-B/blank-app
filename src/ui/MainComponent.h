#pragma once

#include "engine/EngineHost.h"
#include "EditViewState.h"

namespace tess
{

class ControlBar;
class ArrangeView;
class PianoRoll;
class MixerView;
class LibraryPanel;

/** The single main window: control bar, library, arrange area and the bottom
    editor (piano roll or mixer). Owns menus and keyboard shortcuts. */
class MainComponent  : public juce::Component,
                       public juce::ApplicationCommandTarget,
                       public juce::MenuBarModel,
                       public juce::DragAndDropContainer,
                       private EngineHost::Listener,
                       private juce::Timer
{
public:
    explicit MainComponent (EngineHost&);
    ~MainComponent() override;

    juce::ApplicationCommandManager& getCommandManager()  { return commandManager; }

    /** Asks to save unsaved changes, then calls proceed() unless the user cancels. */
    void confirmDiscardChanges (std::function<void()> proceed);

    std::function<void (const juce::String&)> onTitleChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

    // ApplicationCommandTarget
    juce::ApplicationCommandTarget* getNextCommandTarget() override  { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>&) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int, const juce::String&) override;
    void menuItemSelected (int, int) override {}

private:
    enum class BottomPanel { none, pianoRoll, mixer };

    void editAboutToBeReplaced() override;
    void editReplaced() override  { rebuildForEdit(); }
    void timerCallback() override;
    void rebuildForEdit();
    void setBottomPanel (BottomPanel);
    void updatePanelButtons();

    void newProject();
    void openProject();
    void saveProject (std::function<void (bool)> done = nullptr);
    void saveProjectAs (std::function<void (bool)> done = nullptr);
    void exportMixdown();
    void showAudioSettings();
    void showPluginManager();
    void addTrack (bool instrument);
    void deleteSelectedTrack();
    void placeAudioFile (const juce::File&);

    EngineHost& host;
    juce::ApplicationCommandManager commandManager;

    std::unique_ptr<EditViewState> viewState;
    std::unique_ptr<ControlBar> controlBar;
    std::unique_ptr<ArrangeView> arrange;
    std::unique_ptr<PianoRoll> pianoRoll;
    std::unique_ptr<MixerView> mixer;
    std::unique_ptr<LibraryPanel> library;

    struct Divider;
    std::unique_ptr<Divider> divider;

    BottomPanel bottomPanel = BottomPanel::none;
    bool libraryShown = false;
    int bottomHeight = 320;
    juce::String lastTitle;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace tess
