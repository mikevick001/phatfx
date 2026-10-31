cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) unless the caller overrides it.
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(PhatFX VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout if present (./JUCE or -DPHATFX_JUCE_DIR=...),
# otherwise download JUCE 8.0.4.
set(PHATFX_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to a JUCE checkout")
if(EXISTS "${PHATFX_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${PHATFX_JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(PHATFX_COPY_AFTER_BUILD "Install the plug-in into the user plug-in folder after building" ${APPLE})
if(PHATFX_COPY_AFTER_BUILD)
    set(PHATFX_COPY_AFTER_BUILD TRUE)
else()
    set(PHATFX_COPY_AFTER_BUILD FALSE)
endif()

set(PHATFX_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PHATFX_FORMATS AU)
endif()

juce_add_plugin(PhatFX
    COMPANY_NAME "PhatFX"
    BUNDLE_ID "org.phatfx.plugin"
    PLUGIN_MANUFACTURER_CODE Phfx
    PLUGIN_CODE Phfx
    FORMATS ${PHATFX_FORMATS}
    PRODUCT_NAME "PhatFX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Distortion Filter
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD ${PHATFX_COPY_AFTER_BUILD})

target_sources(PhatFX PRIVATE
    Source/Params.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SkinEditor.cpp)

target_compile_definitions(PhatFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PhatFX
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Headless DSP tests (no JUCE needed).
option(PHATFX_BUILD_TESTS "Build DSP tests" ON)
if(PHATFX_BUILD_TESTS)
    add_executable(phatfx_tests tests/dsp_tests.cpp)
    target_include_directories(phatfx_tests PRIVATE Source)
endif()
