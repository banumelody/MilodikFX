#pragma once

#include <JuceHeader.h>

#include <memory>

namespace milodikfx::plugin
{
class MilodikFXAudioProcessor;

/**
 * The plugin's window: the same React UI the standalone app runs.
 *
 * JUCE 8's WebBrowserComponent can serve a page out of memory through a resource
 * provider, which is what makes this possible without the plugin becoming a web
 * server. The bundle is the identical one embedded in the app's exe, so there is
 * one UI to build, test and keep honest rather than two that drift.
 *
 * Writes go the other way, through a native function rather than the resource
 * provider, because a provider is handed only a URL -- no method, no body. The
 * bridge is shaped like an HTTP call regardless (method, path, query, body ->
 * status, body), so `RestApiDispatcher` and every handler below it are reused
 * verbatim. See PluginBackend.
 *
 * When WebView2 is unavailable -- a machine without the Edge runtime, or a build
 * with the WebView switched off -- this falls back to JUCE's generic parameter
 * editor rather than showing an empty window.
 */
class MilodikFXAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit MilodikFXAudioProcessorEditor (MilodikFXAudioProcessor& processor);
    ~MilodikFXAudioProcessorEditor() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    MilodikFXAudioProcessor& audioProcessor;

    std::unique_ptr<juce::Component> view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MilodikFXAudioProcessorEditor)
};
} // namespace milodikfx::plugin
