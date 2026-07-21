cmake_minimum_required(VERSION 3.25)

project(MilodikFX VERSION 0.10.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static CRT so the shipped exe runs on a machine without the VC++
# redistributable. This must be set before JUCE is added: every static library
# in the link has to agree on the runtime, JUCE's included.
option(MILODIKFX_STATIC_RUNTIME "Link the C runtime statically" ON)

if(MILODIKFX_STATIC_RUNTIME AND MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)
endif()

include(FetchContent)

FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.14
)

FetchContent_MakeAvailable(juce)

# ---------------------------------------------------------------------------
# WebView2 (native window UI)
#
# The UI is the React app rendered by Edge WebView2 inside the app's own
# window. JUCE's FindWebView2 wants the Microsoft.Web.WebView2 NuGet package;
# fetching it here keeps a fresh clone buildable without any manual NuGet step.
# ---------------------------------------------------------------------------
option(MILODIKFX_ENABLE_WEBVIEW "Render the UI in-process with WebView2" ON)

set(MILODIKFX_WEBVIEW2_VERSION "1.0.2903.40" CACHE STRING "Microsoft.Web.WebView2 NuGet package version")

if(MILODIKFX_ENABLE_WEBVIEW AND WIN32)
  FetchContent_Declare(
    webview2
    URL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${MILODIKFX_WEBVIEW2_VERSION}"
    DOWNLOAD_NAME "microsoft.web.webview2-${MILODIKFX_WEBVIEW2_VERSION}.zip"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${CMAKE_BINARY_DIR}/webview2/Microsoft.Web.WebView2.${MILODIKFX_WEBVIEW2_VERSION}"
  )
  FetchContent_MakeAvailable(webview2)

  # FindWebView2 globs this directory for a *Microsoft.Web.WebView2* subfolder.
  set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_BINARY_DIR}/webview2" CACHE PATH "WebView2 NuGet package parent directory" FORCE)
endif()

# ---------------------------------------------------------------------------
# ASIO (optional, needs the Steinberg SDK)
# ---------------------------------------------------------------------------
option(MILODIKFX_ENABLE_ASIO "Enable JUCE ASIO support (requires Steinberg ASIO SDK)" OFF)

set(_MILODIKFX_ASIO_SDK_DEFAULT "$ENV{ASIOSDK_DIR}")
if (NOT _MILODIKFX_ASIO_SDK_DEFAULT)
  set(_MILODIKFX_ASIO_SDK_DEFAULT "$ENV{ASIO_SDK_PATH}")
endif()

set(MILODIKFX_ASIO_SDK_PATH "${_MILODIKFX_ASIO_SDK_DEFAULT}" CACHE PATH "Path to Steinberg ASIO SDK root (contains common/asio.h and host/asiodrivers.h)")

set(_MILODIKFX_ASIO_SDK_VALID OFF)
if (MILODIKFX_ASIO_SDK_PATH AND EXISTS "${MILODIKFX_ASIO_SDK_PATH}/common/asio.h" AND EXISTS "${MILODIKFX_ASIO_SDK_PATH}/host/asiodrivers.h")
  set(_MILODIKFX_ASIO_SDK_VALID ON)
endif()

set(MILODIKFX_ASIO_AUTO_TOGGLED OFF CACHE BOOL "Internal: whether ASIO was auto-enabled")
mark_as_advanced(MILODIKFX_ASIO_AUTO_TOGGLED)

if (_MILODIKFX_ASIO_SDK_VALID AND NOT MILODIKFX_ENABLE_ASIO AND NOT MILODIKFX_ASIO_AUTO_TOGGLED)
  message(STATUS "ASIO SDK detected at '${MILODIKFX_ASIO_SDK_PATH}' - auto-enabling MILODIKFX_ENABLE_ASIO")
  set(MILODIKFX_ENABLE_ASIO ON CACHE BOOL "Enable JUCE ASIO support (requires Steinberg ASIO SDK)" FORCE)
  set(MILODIKFX_ASIO_AUTO_TOGGLED ON CACHE BOOL "Internal: whether ASIO was auto-enabled" FORCE)
endif()

if (MILODIKFX_ENABLE_ASIO)
  message(STATUS "MilodikFX: ASIO ENABLED (SDK: ${MILODIKFX_ASIO_SDK_PATH})")
else()
  message(STATUS "MilodikFX: ASIO disabled - low latency will fall back to WASAPI exclusive mode")
endif()

# ---------------------------------------------------------------------------
# Shared DSP / engine sources, used by both the app and the tests
# ---------------------------------------------------------------------------
set(MILODIKFX_ENGINE_SOURCES
  src/audio/AudioEngine.h
  src/audio/AudioEngine.cpp
  src/dsp/AudioProcessorBase.h
  src/dsp/Biquad.h
  src/dsp/GainProcessor.h
  src/dsp/GainProcessor.cpp
  src/dsp/InputTrimProcessor.h
  src/dsp/InputTrimProcessor.cpp
  src/dsp/DriveVoicing.h
  src/dsp/OverdriveProcessor.h
  src/dsp/OverdriveProcessor.cpp
  src/dsp/EQProcessor.h
  src/dsp/EQProcessor.cpp
  src/dsp/CompressorProcessor.h
  src/dsp/CompressorProcessor.cpp
  src/dsp/ReverbProcessor.h
  src/dsp/ReverbProcessor.cpp
  src/dsp/ToneStackProcessor.h
  src/dsp/ToneStackProcessor.cpp
  src/dsp/NoiseGateProcessor.h
  src/dsp/NoiseGateProcessor.cpp
  src/dsp/CabinetProcessor.h
  src/dsp/CabinetProcessor.cpp
  src/dsp/DelayProcessor.h
  src/dsp/DelayProcessor.cpp
  src/dsp/MasterOutProcessor.h
  src/dsp/MasterOutProcessor.cpp
  src/dsp/MetronomeProcessor.h
  src/dsp/MetronomeProcessor.cpp
  src/dsp/TunerAnalyzer.h
  src/dsp/TunerAnalyzer.cpp
  src/dsp/IrEngine.h
  src/dsp/IrEngine.cpp
  src/dsp/DSPChainManager.h
  src/dsp/DSPChainManager.cpp
  src/dsp/ChainFactory.h
  src/dsp/ChainFactory.cpp
  src/preset/PresetManager.h
  src/preset/PresetManager.cpp
  src/preset/IrLibrary.h
  src/preset/IrLibrary.cpp
  src/preset/SceneManager.h
  src/preset/SceneManager.cpp
  src/api/ParameterRegistry.h
  src/api/ParameterRegistry.cpp
  src/api/UndoHistory.h
  src/api/UndoHistory.cpp
  src/midi/MidiController.h
  src/midi/MidiController.cpp
)

juce_add_gui_app(MilodikFX
  PRODUCT_NAME "MilodikFX"
  COMPANY_NAME "MilodikFX"
  ICON_BIG "${CMAKE_SOURCE_DIR}/resources/icon.png"
  NEEDS_WEBVIEW2 ${MILODIKFX_ENABLE_WEBVIEW}
)

juce_generate_juce_header(MilodikFX)

# ---------------------------------------------------------------------------
# Embedded UI
#
# Baking the built bundle into the exe is what makes it genuinely standalone:
# without this it dies as soon as resources/ui/web is not sitting beside it.
# Embedding is opportunistic so a fresh clone still configures before the
# frontend has ever been built; re-run CMake after the first `npm run build`.
# ---------------------------------------------------------------------------
set(MILODIKFX_UI_DIST "${CMAKE_SOURCE_DIR}/frontend/dist")

if(EXISTS "${MILODIKFX_UI_DIST}/index.html"
   AND EXISTS "${MILODIKFX_UI_DIST}/assets/index.js"
   AND EXISTS "${MILODIKFX_UI_DIST}/assets/index.css")
  message(STATUS "MilodikFX: embedding the UI bundle into the executable")

  juce_add_binary_data(MilodikFX_UiData
    HEADER_NAME MilodikFXUiData.h
    NAMESPACE MilodikFXUiData
    SOURCES
      "${MILODIKFX_UI_DIST}/index.html"
      "${MILODIKFX_UI_DIST}/assets/index.js"
      "${MILODIKFX_UI_DIST}/assets/index.css"
  )

  set(MILODIKFX_HAS_EMBEDDED_UI 1)
else()
  message(WARNING
    "MilodikFX: frontend/dist not found or incomplete - the exe will only serve "
    "the UI from resources/ui/web. Run `cd frontend && npm ci && npm run build`, "
    "then re-run CMake to embed it.")
  set(MILODIKFX_HAS_EMBEDDED_UI 0)
endif()

target_sources(MilodikFX
  PRIVATE
    src/Main.cpp
    src/MainComponent.h
    src/MainComponent.cpp
    src/audio/AudioDeviceController.h
    src/audio/AudioDeviceController.cpp
    src/ui/WebServer.h
    src/ui/WebServer.cpp
    src/api/HttpHandler.h
    src/api/ApiJson.h
    src/api/RestApiDispatcher.h
    src/api/HealthHandler.h
    src/api/DevicesHandler.h
    src/api/DevicesHandler.cpp
    src/api/ParametersHandler.h
    src/api/ParametersHandler.cpp
    src/api/EffectsHandler.h
    src/api/EffectsHandler.cpp
    src/api/IrHandler.h
    src/api/IrHandler.cpp
    src/api/LevelsHandler.h
    src/api/LevelsHandler.cpp
    src/api/TunerHandler.h
    src/api/TunerHandler.cpp
    src/api/MidiHandler.h
    src/api/MidiHandler.cpp
    src/api/ScenesHandler.h
    src/api/ScenesHandler.cpp
    src/api/HistoryHandler.h
    src/api/HistoryHandler.cpp
    src/api/PresetsHandler.h
    src/api/PresetsHandler.cpp
    ${MILODIKFX_ENGINE_SOURCES}
)

target_include_directories(MilodikFX PRIVATE src)

target_compile_definitions(MilodikFX
  PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=$<BOOL:${MILODIKFX_ENABLE_WEBVIEW}>
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=$<BOOL:${MILODIKFX_ENABLE_WEBVIEW}>
    MILODIKFX_ENABLE_WEBVIEW=$<BOOL:${MILODIKFX_ENABLE_WEBVIEW}>
    MILODIKFX_EMBED_UI=${MILODIKFX_HAS_EMBEDDED_UI}
)

if(MILODIKFX_HAS_EMBEDDED_UI)
  target_link_libraries(MilodikFX PRIVATE MilodikFX_UiData)
endif()

if(MILODIKFX_ENABLE_ASIO)
  if(NOT EXISTS "${MILODIKFX_ASIO_SDK_PATH}/common/asio.h")
    message(FATAL_ERROR "MILODIKFX_ASIO_SDK_PATH must point to ASIO SDK root containing common/asio.h")
  endif()
  if(NOT EXISTS "${MILODIKFX_ASIO_SDK_PATH}/host/asiodrivers.h")
    message(FATAL_ERROR "MILODIKFX_ASIO_SDK_PATH must point to ASIO SDK root containing host/asiodrivers.h")
  endif()

  target_compile_definitions(MilodikFX PRIVATE JUCE_ASIO=1)
  target_include_directories(MilodikFX
    PRIVATE
      "${MILODIKFX_ASIO_SDK_PATH}/common"
      "${MILODIKFX_ASIO_SDK_PATH}/host"
  )
endif()

target_link_libraries(MilodikFX
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_data_structures
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Post-build: copy the built frontend next to the exe. The exe also serves an
# embedded fallback page, so a missing dist no longer means a blank window.
add_custom_command(TARGET MilodikFX POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<TARGET_FILE_DIR:MilodikFX>/resources/ui/web/assets"
    COMMENT "Creating resources directory"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/frontend/dist/index.html")
  add_custom_command(TARGET MilodikFX POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy
      "${CMAKE_SOURCE_DIR}/frontend/dist/index.html"
      "$<TARGET_FILE_DIR:MilodikFX>/resources/ui/web/index.html"
      COMMENT "Copying frontend HTML"
  )

  # Clear first so renamed bundles from an earlier build do not pile up.
  add_custom_command(TARGET MilodikFX POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E rm -rf
      "$<TARGET_FILE_DIR:MilodikFX>/resources/ui/web/assets"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_SOURCE_DIR}/frontend/dist/assets"
      "$<TARGET_FILE_DIR:MilodikFX>/resources/ui/web/assets"
      COMMENT "Copying frontend assets"
  )
else()
  message(WARNING "frontend/dist not found - build the frontend (cd frontend && npm ci && npm run build) before running MilodikFX")
endif()

# ---------------------------------------------------------------------------
# Plugin (VST3 + Standalone)
#
# Shares src/dsp with the app through ChainFactory. The standalone wrapper is a
# useful cross-check: it gives JUCE's own device panel, so a latency problem can
# be told apart from a problem in this project's device handling.
# ---------------------------------------------------------------------------
option(MILODIKFX_BUILD_PLUGIN "Build the VST3 / Standalone plugin target" ON)

if(MILODIKFX_BUILD_PLUGIN)
  juce_add_plugin(MilodikFX_Plugin
    PRODUCT_NAME "MilodikFX"
    COMPANY_NAME "MilodikFX"
    PLUGIN_MANUFACTURER_CODE Mldk
    PLUGIN_CODE Mfx1
    BUNDLE_ID "com.milodikfx.plugin"
    ICON_BIG "${CMAKE_SOURCE_DIR}/resources/icon.png"
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
  )

  juce_generate_juce_header(MilodikFX_Plugin)

  target_sources(MilodikFX_Plugin
    PRIVATE
      src/plugin/PluginProcessor.h
      src/plugin/PluginProcessor.cpp
      ${MILODIKFX_ENGINE_SOURCES}
  )

  target_include_directories(MilodikFX_Plugin PRIVATE src)

  target_compile_definitions(MilodikFX_Plugin
    PUBLIC
      JUCE_WEB_BROWSER=0
      JUCE_USE_CURL=0
      JUCE_VST3_CAN_REPLACE_VST2=0
  )

  if(MILODIKFX_ENABLE_ASIO)
    target_compile_definitions(MilodikFX_Plugin PRIVATE JUCE_ASIO=1)
    target_include_directories(MilodikFX_Plugin
      PRIVATE
        "${MILODIKFX_ASIO_SDK_PATH}/common"
        "${MILODIKFX_ASIO_SDK_PATH}/host"
    )
  endif()

  target_link_libraries(MilodikFX_Plugin
    PRIVATE
      juce::juce_audio_utils
      juce::juce_dsp
      juce::juce_data_structures
      juce::juce_recommended_config_flags
      juce::juce_recommended_lto_flags
      juce::juce_recommended_warning_flags
  )
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
juce_add_console_app(MilodikFX_tests
  PRODUCT_NAME "MilodikFX Tests"
)

juce_generate_juce_header(MilodikFX_tests)

target_sources(MilodikFX_tests
  PRIVATE
    tests/UnitTests.cpp
    tests/DspTests.cpp
    tests/RegistryTests.cpp
    tests/IrTests.cpp
    tests/TunerTests.cpp
    tests/MidiTests.cpp
    tests/SceneTests.cpp
    tests/UndoTests.cpp
    tests/InputTrimTests.cpp
    tests/DriveVoicingTests.cpp
    ${MILODIKFX_ENGINE_SOURCES}
)

target_compile_definitions(MilodikFX_tests
  PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(MilodikFX_tests
  PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_formats
    # MidiController is part of the shared engine sources, and juce::MidiInput
    # lives in juce_audio_devices.
    juce::juce_audio_devices
    juce::juce_dsp
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_include_directories(MilodikFX_tests PRIVATE src)

enable_testing()

add_test(NAME MilodikFX_UnitTests COMMAND MilodikFX_tests)

add_test(NAME MilodikFX_Smoke
  COMMAND powershell -ExecutionPolicy Bypass
          -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.ps1"
          -AppPath $<TARGET_FILE:MilodikFX>
)
