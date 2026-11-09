#pragma once

#include "Tess.h"

namespace tess
{

/** Engine UI hooks used when no window is involved (tests, command-line export).
    Long-running engine jobs such as renders run to completion on the calling thread,
    and warnings are kept so callers can report them. */
class HeadlessUIBehaviour  : public te::UIBehaviour
{
public:
    void runTaskWithProgressBar (te::ThreadPoolJobWithProgress& task) override
    {
        while (task.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
        {}
    }

    void showWarningMessage (const juce::String& message) override
    {
        lastWarning = message;
        DBG ("Engine warning: " << message);
    }

    juce::String lastWarning;
};

} // namespace tess
