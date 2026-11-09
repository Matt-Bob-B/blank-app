#pragma once

#include "Tess.h"

namespace tess
{

/** Owns the Tracktion Engine and the currently open project (an Edit).

    The UI listens to this object (ChangeBroadcaster) and rebuilds itself
    whenever a different Edit is loaded.
*/
class EngineHost  : public juce::ChangeBroadcaster
{
public:
    static constexpr const char* projectExtension = ".tessproj";

    /** @param uiBehaviour  optional UI hooks; pass nullptr for headless use. */
    explicit EngineHost (std::unique_ptr<te::UIBehaviour> uiBehaviour = nullptr);
    ~EngineHost() override;

    te::Engine& getEngine() noexcept    { return *engine; }
    te::Edit& getEdit() noexcept        { return *edit; }

    /** Replaces the current Edit with a new, untitled session
        (120 bpm, 4/4, one instrument track and one audio track). */
    void newProject();

    /** Opens a .tessproj file. Returns an error message, or an empty string on success. */
    juce::String openProject (const juce::File& projectFile);

    /** Saves to the current file. Fails (returns false) if the project is untitled. */
    bool saveProject();

    /** Saves to a new file and makes it the current project file. */
    bool saveProjectAs (const juce::File& projectFile);

    juce::File getProjectFile() const   { return projectFile; }
    bool isUntitled() const             { return untitled; }
    bool hasUnsavedChanges() const;

    /** Folder that recordings and imported audio are written to for this project. */
    juce::File getAudioFolder() const;

private:
    void updateRecordingPaths();
    void installEdit (std::unique_ptr<te::Edit>, const juce::File&, bool isUntitled);

    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<te::Edit> edit;
    juce::File projectFile;
    bool untitled = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineHost)
};

} // namespace tess
