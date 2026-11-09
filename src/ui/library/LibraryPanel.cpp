#include "LibraryPanel.h"
#include "ui/theme/Theme.h"

namespace tess
{

using namespace theme;

LibraryPanel::LibraryPanel (te::Engine& e) : engine (e)
{
    auto wildcard = engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats();
    filter = std::make_unique<juce::WildcardFileFilter> (wildcard, "*", "Audio files");
    contents = std::make_unique<juce::DirectoryContentsList> (filter.get(), thread);
    tree = std::make_unique<juce::FileTreeComponent> (*contents);
    tree->addListener (this);
    tree->setDragAndDropDescription ("tessitura-library-file");
    tree->setColour (juce::TreeView::backgroundColourId, colour::panel);
    tree->setColour (juce::FileTreeComponent::highlightColourId, colour::accent.withAlpha (0.5f));
    tree->setColour (juce::FileTreeComponent::textColourId, colour::text);
    tree->setColour (juce::TreeView::linesColourId, colour::textTertiary);
    tree->setItemHeight (24);
    addAndMakeVisible (*tree);

    roots = { juce::File::getSpecialLocation (juce::File::userMusicDirectory),
              juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
              juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
              juce::File::getSpecialLocation (juce::File::userHomeDirectory) };

    location.addItem ("Music", 1);
    location.addItem ("Desktop", 2);
    location.addItem ("Documents", 3);
    location.addItem ("Home", 4);
    location.addSeparator();
    location.addItem ("Choose Folder...", 99);
    location.onChange = [this]
    {
        const int id = location.getSelectedId();

        if (id >= 1 && id <= roots.size())
        {
            setRoot (roots[id - 1]);
        }
        else if (id == 99)
        {
            chooser = std::make_unique<juce::FileChooser> ("Choose a sample folder", roots[0]);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      auto dir = fc.getResult();

                                      if (dir.isDirectory())
                                      {
                                          roots.add (dir);
                                          location.addItem (dir.getFileName(), roots.size());
                                          location.setSelectedId (roots.size());
                                      }
                                      else
                                      {
                                          location.setSelectedId (1);
                                      }
                                  });
        }
    };
    addAndMakeVisible (location);

    thread.startThread (juce::Thread::Priority::low);
    location.setSelectedId (1);
}

LibraryPanel::~LibraryPanel()
{
    tree->removeListener (this);
    tree.reset();
    contents.reset();
    thread.stopThread (2000);
}

void LibraryPanel::setRoot (const juce::File& dir)
{
    contents->setDirectory (dir, true, true);
}

void LibraryPanel::fileDoubleClicked (const juce::File& f)
{
    if (f.existsAsFile() && onFileChosen)
        onFileChosen (f);
}

void LibraryPanel::paint (juce::Graphics& g)
{
    g.fillAll (colour::panel);
    g.setColour (colour::text);
    g.setFont (font (13.0f, true));
    g.drawText ("Library", 12, 8, getWidth() - 24, 20, juce::Justification::centredLeft);
    g.setColour (colour::separator);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

    g.setColour (colour::textTertiary);
    g.setFont (font (11.0f));
    g.drawFittedText ("Double-click or drag audio files onto the tracks.",
                      getLocalBounds().removeFromBottom (36).reduced (12, 4), juce::Justification::centredLeft, 2);
}

void LibraryPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 0).withTrimmedRight (1);
    r.removeFromTop (32);
    location.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);
    r.removeFromBottom (36);
    tree->setBounds (r);
}

} // namespace tess
