#pragma once

// Single include point for the audio engine. Everything in Tessitura talks to
// Tracktion Engine (and, through it, JUCE) via this header.
#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion;
using namespace std::literals;
