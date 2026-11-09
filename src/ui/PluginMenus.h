#pragma once

#include "engine/Tess.h"

namespace tess::plugin_menus
{

/** Shows a menu of instruments (built-in + scanned VST3) and calls back with the created plugin. */
void chooseInstrument (te::Edit&, juce::Component& target, std::function<void (te::Plugin::Ptr)> onChosen);

/** Shows a menu of audio effects (built-in + scanned VST3) and calls back with the created plugin. */
void chooseEffect (te::Edit&, juce::Component& target, std::function<void (te::Plugin::Ptr)> onChosen);

/** True for the engine-internal plugins every track carries (volume/pan, meter), which the UI hides. */
bool isHiddenPlugin (te::Plugin&);

/** Opens the plugin's editor window (VST3 GUI or a generic editor for built-ins). */
void showEditor (te::Plugin&);

} // namespace tess::plugin_menus
