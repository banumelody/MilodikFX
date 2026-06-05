cmake_minimum_required(VERSION 3.25)

project(MilodikFX VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.0
)

FetchContent_MakeAvailable(juce)

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

juce_add_gui_app(MilodikFX
  PRODUCT_NAME "MilodikFX"
  COMPANY_NAME "MilodikFX"
)

juce_generate_juce_header(MilodikFX)

target_sources(MilodikFX
  PRIVATE
    src/Main.cpp
    src/MainComponent.h
    src/MainComponent.cpp
    src/audio/AudioEngine.h
    src/audio/AudioEngine.cpp
    src/dsp/AudioProcessorBase.h
    src/dsp/GainProcessor.h
    src/dsp/GainProcessor.cpp
    src/dsp/OverdriveProcessor.h
    src/dsp/OverdriveProcessor.cpp
    src/dsp/DSPChainManager.h
    src/dsp/DSPChainManager.cpp
)

target_include_directories(MilodikFX
  PRIVATE
    src
)

target_compile_definitions(MilodikFX
  PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

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
    juce::juce_data_structures
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

juce_add_console_app(MilodikFX_tests
  PRODUCT_NAME "MilodikFX Tests"
)

juce_generate_juce_header(MilodikFX_tests)

target_sources(MilodikFX_tests
  PRIVATE
    tests/UnitTests.cpp
    src/dsp/DSPChainManager.cpp
    src/dsp/GainProcessor.cpp
    src/dsp/OverdriveProcessor.cpp
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
    juce::juce_data_structures
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_include_directories(MilodikFX_tests
  PRIVATE
    src
)

enable_testing()

add_test(NAME MilodikFX_UnitTests COMMAND MilodikFX_tests)
add_test(NAME MilodikFX_Smoke
  COMMAND powershell -ExecutionPolicy Bypass
          -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.ps1"
          -AppPath $<TARGET_FILE:MilodikFX>
)

add_test(NAME MilodikFX_Smoke_ASIO
  COMMAND powershell -ExecutionPolicy Bypass
          -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke-asio.ps1"
          -AppPath $<TARGET_FILE:MilodikFX>
          -BuildDir "${CMAKE_BINARY_DIR}"
)
