# A real, downloadable, open-source VST3/AU/Standalone synth exercising the
# C++ SDK's full activation flow — see README.md for what it demonstrates
# and ../../README.md for the core SDK it depends on.
#
# Configures standalone, with JUCE fetched automatically:
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --target LicensrDemoSynth_Standalone
#
# If you're already vendoring JUCE elsewhere (a larger project embedding
# this example), set LICENSR_DEMO_USE_FETCHCONTENT OFF and
# add_subdirectory(path/to/JUCE) yourself before this directory.
cmake_minimum_required(VERSION 3.21)

if(APPLE)
  project(licensr_demo_synth VERSION 1.0.0 LANGUAGES C CXX OBJC OBJCXX)
else()
  project(licensr_demo_synth VERSION 1.0.0 LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LICENSR_DEMO_USE_FETCHCONTENT "Fetch JUCE via FetchContent instead of expecting it added already" ON)

if(LICENSR_DEMO_USE_FETCHCONTENT)
  include(FetchContent)
  FetchContent_Declare(JUCE
      GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG 8.0.6
      GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(JUCE)
endif()

set(LICENSR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/licensr_sdk)

juce_add_plugin(LicensrDemoSynth
    COMPANY_NAME "Licensr"
    BUNDLE_ID "app.licensr.demosynth"
    PLUGIN_MANUFACTURER_CODE Lcsr
    PLUGIN_CODE Dsyn
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Licensr Demo Synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES "Instrument" "Synth"
    AU_MAIN_TYPE "kAudioUnitType_MusicDevice"
)

target_sources(LicensrDemoSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(LicensrDemoSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(LicensrDemoSynth
    PRIVATE
        licensr::licensr
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
