#include "plugin/PluginEditor.h"

#include "plugin/PluginBackend.h"
#include "plugin/PluginProcessor.h"

#if MILODIKFX_PLUGIN_EMBED_UI
 #include "MilodikFXUiData.h"
#endif

namespace milodikfx::plugin
{
namespace
{
constexpr int kDefaultWidth = 1180;
constexpr int kDefaultHeight = 760;

#if MILODIKFX_PLUGIN_WEBVIEW
/** The name the frontend's transport layer invokes; see services/transport.ts. */
const juce::Identifier kApiFunction { "milodikfxApi" };

juce::String mimeTypeFor (const juce::String& path)
{
    if (path.endsWithIgnoreCase (".html")) return "text/html";
    if (path.endsWithIgnoreCase (".js"))   return "text/javascript";
    if (path.endsWithIgnoreCase (".css"))  return "text/css";
    if (path.endsWithIgnoreCase (".svg"))  return "image/svg+xml";
    if (path.endsWithIgnoreCase (".json")) return "application/json";
    if (path.endsWithIgnoreCase (".png"))  return "image/png";

    return "application/octet-stream";
}

#if MILODIKFX_PLUGIN_EMBED_UI
/**
 * Looks a bundle file up by its original name rather than by JUCE's mangled
 * symbol, so renaming one cannot silently stop it being found -- the same rule
 * the app's server follows.
 */
std::optional<juce::WebBrowserComponent::Resource> embeddedResource (const juce::String& fileName)
{
    for (int i = 0; i < MilodikFXUiData::namedResourceListSize; ++i)
    {
        if (fileName != MilodikFXUiData::originalFilenames[i])
            continue;

        int size = 0;
        const auto* data = MilodikFXUiData::getNamedResource (MilodikFXUiData::namedResourceList[i], size);

        if (data == nullptr || size <= 0)
            return std::nullopt;

        const auto* bytes = reinterpret_cast<const std::byte*> (data);

        return juce::WebBrowserComponent::Resource {
            std::vector<std::byte> (bytes, bytes + size),
            mimeTypeFor (fileName)
        };
    }

    return std::nullopt;
}
#endif

/**
 * Answers a page request out of the embedded bundle.
 *
 * Vite emits stable filenames (assets/index.js, assets/index.css), which both
 * the embedding step and this depend on. Anything unrecognised falls back to
 * index.html so the single-page app keeps working if the WebView ever navigates
 * somewhere it invented.
 */
std::optional<juce::WebBrowserComponent::Resource> provideResource (const juce::String& url)
{
   #if MILODIKFX_PLUGIN_EMBED_UI
    auto path = url.upToFirstOccurrenceOf ("?", false, false);

    while (path.startsWithChar ('/'))
        path = path.substring (1);

    // The bundle is flat in the binary: assets/index.js is stored as index.js.
    const auto fileName = path.isEmpty() ? juce::String ("index.html")
                                         : path.fromLastOccurrenceOf ("/", false, false);

    if (auto resource = embeddedResource (fileName))
        return resource;

    return embeddedResource ("index.html");
   #else
    juce::ignoreUnused (url);
    return std::nullopt;
   #endif
}
#endif // MILODIKFX_PLUGIN_WEBVIEW
} // namespace

MilodikFXAudioProcessorEditor::MilodikFXAudioProcessorEditor (MilodikFXAudioProcessor& processorToUse)
    : juce::AudioProcessorEditor (&processorToUse), audioProcessor (processorToUse)
{
   #if MILODIKFX_PLUGIN_WEBVIEW
    auto& backend = audioProcessor.getBackend();

    auto browser = std::make_unique<juce::WebBrowserComponent> (
        juce::WebBrowserComponent::Options {}
            .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options (
                juce::WebBrowserComponent::Options::WinWebView2 {}
                    .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)))
            .withNativeIntegrationEnabled()
            .withResourceProvider (provideResource)
            .withNativeFunction (kApiFunction,
                                 [&backend] (const juce::Array<juce::var>& args,
                                             juce::WebBrowserComponent::NativeFunctionCompletion completion)
                                 {
                                     // (method, path, query, body) -- the same four
                                     // things an HTTP request carries, so the whole
                                     // REST layer below is reused unchanged.
                                     const auto arg = [&args] (int index)
                                     {
                                         return index < args.size() ? args[index].toString() : juce::String();
                                     };

                                     const auto response = backend.call (arg (0), arg (1), arg (2), arg (3));

                                     auto* result = new juce::DynamicObject();
                                     result->setProperty ("status", response.statusCode);
                                     result->setProperty ("body", juce::String (response.body));

                                     completion (juce::var (result));
                                 }));

    // Any origin works; it only has to be one the resource provider owns.
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());

    view = std::move (browser);
   #else
    view = std::make_unique<juce::GenericAudioProcessorEditor> (audioProcessor);
   #endif

    addAndMakeVisible (*view);

    setResizable (true, true);
    setResizeLimits (720, 480, 3840, 2160);
    setSize (kDefaultWidth, kDefaultHeight);
}

MilodikFXAudioProcessorEditor::~MilodikFXAudioProcessorEditor() = default;

void MilodikFXAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Matches the UI's own background, so a slow first paint is not a white flash.
    g.fillAll (juce::Colour (0xff0d1017));
}

void MilodikFXAudioProcessorEditor::resized()
{
    if (view != nullptr)
        view->setBounds (getLocalBounds());
}
} // namespace milodikfx::plugin
