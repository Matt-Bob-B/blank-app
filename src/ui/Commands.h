#pragma once

#include "EditViewState.h"

/** Transport and editing actions shared by buttons, menus and keyboard shortcuts. */
namespace tess::commands
{
    void togglePlay (te::Edit&);
    void stop (te::Edit&);
    void goToStart (te::Edit&);

    /** Starts recording. If no track is armed, arms the selected track first
        (MIDI input for instrument tracks, audio input for audio tracks). */
    void toggleRecord (te::Edit&, te::AudioTrack* selectedTrack = nullptr);

    /** Toggles cycle. If no cycle range is set, uses the selected region, else the first 4 bars. */
    void toggleCycle (EditViewState&);
    void toggleMetronome (te::Edit&);

    void deleteSelection (EditViewState&);
    void quantiseSelection (EditViewState&);
    void splitAtPlayhead (EditViewState&);
}
