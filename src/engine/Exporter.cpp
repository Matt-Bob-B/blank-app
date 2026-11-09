#include "Exporter.h"

namespace tess
{

juce::String exportMixdown (te::Edit& edit, const juce::File& destWav, const ExportSettings& settings)
{
    const auto length = edit.getLength();

    if (length <= te::TimeDuration())
        return "Nothing to export: the project is empty.";

    if (! destWav.getParentDirectory().createDirectory())
        return "Cannot create folder: " + destWav.getParentDirectory().getFullPathName();

    te::Renderer::Parameters params (edit);
    params.tracksToDo = te::toBitSet (te::getAllTracks (edit));
    params.destFile = destWav.withFileExtension (".wav");
    params.audioFormat = edit.engine.getAudioFileFormatManager().getWavFormat();
    params.bitDepth = settings.bitDepth;
    params.sampleRateForAudio = settings.sampleRate;
    params.time = te::TimeRange (te::TimePosition(), length + settings.tail);
    params.shouldNormalise = settings.normalise;
    params.normaliseToLevelDb = -0.3f;
    params.useMasterPlugins = true;
    params.canRenderInMono = false;
    params.ditheringEnabled = settings.bitDepth < 24;

    params.destFile.deleteFile();

    // Offline renders must not share the graph with live playback.
    auto& transport = edit.getTransport();
    transport.stop (false, true);
    transport.freePlaybackContext();

    const bool ok = te::Renderer::renderToFile ("Export", params) != juce::File();

    transport.ensureContextAllocated();

    if (! ok)
        return "Export failed. Check that the project produces audio.";

    return {};
}

} // namespace tess
