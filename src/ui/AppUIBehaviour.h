#pragma once

#include "engine/Tess.h"

namespace tess
{

/** Engine UI hooks for the desktop app: plugin editor windows, warnings and progress. */
class AppUIBehaviour  : public te::UIBehaviour
{
public:
    std::unique_ptr<juce::Component> createPluginWindow (te::PluginWindowState&) override;
    void recreatePluginWindowContentAsync (te::Plugin&) override;
    void showWarningMessage (const juce::String&) override;
    void runTaskWithProgressBar (te::ThreadPoolJobWithProgress&) override;
};

} // namespace tess
