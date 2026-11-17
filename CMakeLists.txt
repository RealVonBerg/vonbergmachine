cmake_minimum_required(VERSION 3.22)

project(VONBERGMACHINE VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VONBERGMACHINE
    COMPANY_NAME                "VonBerg"
    BUNDLE_ID                   "com.vonberg.vonbergmachine"
    PLUGIN_MANUFACTURER_CODE    Vnbg
    PLUGIN_CODE                 Vbmc
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "VONBERGMACHINE"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Drum)

juce_generate_juce_header(VONBERGMACHINE)

target_sources(VONBERGMACHINE PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/engine/VbmEngine.cpp
    Source/engine/VbmGenerator.cpp
    Source/engine/VbmKit.cpp)

target_compile_definitions(VONBERGMACHINE PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1)

target_link_libraries(VONBERGMACHINE
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
