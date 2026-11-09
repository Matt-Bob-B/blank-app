#include "engine/EngineHost.h"
#include "ui/AppUIBehaviour.h"
#include "ui/MainComponent.h"
#include "ui/theme/Theme.h"

namespace tess
{

class MainWindow  : public juce::DocumentWindow
{
public:
    MainWindow (EngineHost& host, std::function<void()> onClose)
        : juce::DocumentWindow ("Tessitura", theme::colour::window, juce::DocumentWindow::allButtons),
          closeCallback (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        content = new MainComponent (host);
        content->onTitleChanged = [this] (const juce::String& t) { setName (t); };
        setContentOwned (content, true);
        setMenuBar (content);
        setResizable (true, false);
        setResizeLimits (960, 600, 10000, 10000);
        centreWithSize (juce::jmin (1600, getParentMonitorArea().getWidth() - 80),
                        juce::jmin (1000, getParentMonitorArea().getHeight() - 80));
        setVisible (true);
        content->grabKeyboardFocus();
    }

    ~MainWindow() override  { setMenuBar (nullptr); }

    MainComponent& getContent()  { return *content; }

    void closeButtonPressed() override  { closeCallback(); }

private:
    MainComponent* content = nullptr;
    std::function<void()> closeCallback;
};

//==============================================================================
class Application  : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        // Plug-ins are scanned in a child copy of this app so a crashing plug-in can't take Tessitura down.
        if (te::PluginManager::startChildProcessPluginScan (commandLine))
            return;

        lookAndFeel = std::make_unique<theme::LookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());

        host = std::make_unique<EngineHost> (std::make_unique<AppUIBehaviour>());
        host->getEngine().getPluginManager().setUsesSeparateProcessForScanning (true);

        window = std::make_unique<MainWindow> (*host, [this] { systemRequestedQuit(); });

        auto file = juce::File::getCurrentWorkingDirectory().getChildFile (commandLine.unquoted().trim());
        if (commandLine.isNotEmpty() && file.hasFileExtension (EngineHost::projectExtension))
            host->openProject (file);
    }

    void shutdown() override
    {
        window.reset();
        host.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        lookAndFeel.reset();
    }

    void systemRequestedQuit() override
    {
        if (window == nullptr)
        {
            quit();
            return;
        }

        window->getContent().confirmDiscardChanges ([] { juce::JUCEApplication::getInstance()->quit(); });
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        auto file = juce::File (commandLine.unquoted().trim());

        if (window != nullptr && file.hasFileExtension (EngineHost::projectExtension))
            window->getContent().confirmDiscardChanges ([this, file] { host->openProject (file); });
    }

private:
    std::unique_ptr<theme::LookAndFeel> lookAndFeel;
    std::unique_ptr<EngineHost> host;
    std::unique_ptr<MainWindow> window;
};

} // namespace tess

START_JUCE_APPLICATION (tess::Application)
