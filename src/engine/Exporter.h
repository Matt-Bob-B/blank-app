#pragma once

#include "Tess.h"

namespace tess
{

struct ExportSettings
{
    double sampleRate = 48000.0;
    int bitDepth = 24;
    bool normalise = false;
    te::TimeDuration tail = te::TimeDuration::fromSeconds (2.0); ///< extra time for reverb/release tails
};

/** Renders the whole Edit (all tracks, including master plugins) to a WAV file.
    Runs synchronously on the calling thread. Returns an error message, or empty on success. */
juce::String exportMixdown (te::Edit&, const juce::File& destWav, const ExportSettings& = {});

} // namespace tess
