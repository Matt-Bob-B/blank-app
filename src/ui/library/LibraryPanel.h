#pragma once

#include "engine/Tess.h"

namespace tess
{

/** Browser for audio files on disk. Double-click a file, or drag it onto the
    arrange area, to place it on an audio track. */
class LibraryPanel  : public juce::Component,
                      private juce::FileBrowserListener
{
public:
    explicit LibraryPanel (te::Engine&);
    ~LibraryPanel() override;

    std::function<void (const juce::File&)> onFileChosen;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setRoot (const juce::File&);
    void selectionChanged() override {}
    void fileClicked (const juce::File&, const juce::MouseEvent&) override {}
    void fileDoubleClicked (const juce::File&) override;
    void browserRootChanged (const juce::File&) override {}

    te::Engine& engine;
    juce::TimeSliceThread thread { "Library scanner" };
    std::unique_ptr<juce::WildcardFileFilter> filter;
    std::unique_ptr<juce::DirectoryContentsList> contents;
    std::unique_ptr<juce::FileTreeComponent> tree;
    juce::ComboBox location;
    juce::Array<juce::File> roots;
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace tess
