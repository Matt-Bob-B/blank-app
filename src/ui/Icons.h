#pragma once

#include "engine/Tess.h"

namespace tess::icons
{

/** All icons are original vector paths drawn in a 24x24 box (filled unless noted). */
enum class Icon
{
    play, stop, record, toStart, cycle, metronome,
    mixer, pianoRoll, library, plus, scissors
};

juce::Path get (Icon);

/** True for icons drawn as outlines rather than filled shapes. */
bool isStroked (Icon);

} // namespace tess::icons
