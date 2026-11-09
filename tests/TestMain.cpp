#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include "engine/Tess.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    doctest::Context context;
    context.applyCommandLine (argc, argv);
    const int result = context.run();

    juce::DeletedAtShutdown::deleteAll();
    return result;
}
