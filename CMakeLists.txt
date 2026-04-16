cmake_minimum_required(VERSION 3.22)
project(FartBlaster VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

file(GLOB FART_SAMPLES "${CMAKE_SOURCE_DIR}/Resources/*.wav")

juce_add_binary_data(FartSampleData
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        ${FART_SAMPLES}
        "${CMAKE_SOURCE_DIR}/Resources/logo-truckpacker.png"
)

juce_add_plugin(FartBlaster
    COMPANY_NAME "FartCo"
    PLUGIN_MANUFACTURER_CODE Fart
    PLUGIN_CODE Fb3k
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Fart Blaster 3000"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(FartBlaster)

target_sources(FartBlaster PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(FartBlaster PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(FartBlaster
    PRIVATE
        FartSampleData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
