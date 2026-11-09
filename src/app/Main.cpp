#include "engine/Tess.h"

class TessituraApplication  : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override {}
    void shutdown() override {}
};

START_JUCE_APPLICATION (TessituraApplication)
