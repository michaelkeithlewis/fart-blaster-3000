cmake_minimum_required(VERSION 3.22)
project(FartBlaster VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Code-signing options (macOS). Leave APPLE_DEV_ID empty for ad-hoc.
# For a real Developer ID build:
#   cmake .. -DAPPLE_DEV_ID="Developer ID Application: NAME (TEAMID)" -DAPPLE_TEAM_ID=TEAMID
set(APPLE_DEV_ID "" CACHE STRING "Developer ID Application identity for codesign")
set(APPLE_TEAM_ID "" CACHE STRING "Apple Developer Team ID")

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
    NEEDS_MIDI_INPUT TRUE
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

# -----------------------------------------------------------------------------
# Code signing (macOS, Developer ID + hardened runtime)
# -----------------------------------------------------------------------------
if(APPLE AND APPLE_DEV_ID)
    set(ENTITLEMENTS "${CMAKE_SOURCE_DIR}/Resources/entitlements.plist")
    set(ARTEFACT_DIR "${CMAKE_CURRENT_BINARY_DIR}/FartBlaster_artefacts/$<CONFIG>")
    set(VST3_BUNDLE  "${ARTEFACT_DIR}/VST3/Fart Blaster 3000.vst3")
    set(AU_BUNDLE    "${ARTEFACT_DIR}/AU/Fart Blaster 3000.component")
    set(APP_BUNDLE   "${ARTEFACT_DIR}/Standalone/Fart Blaster 3000.app")
    set(VST3_INSTALL "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/Fart Blaster 3000.vst3")
    set(AU_INSTALL   "$ENV{HOME}/Library/Audio/Plug-Ins/Components/Fart Blaster 3000.component")

    # Sign a bundle with hardened runtime, timestamp, and entitlements.
    # Runs as a POST_BUILD step *after* JUCE's own copy-to-system-folders step,
    # so we explicitly sign both the build artifact and the installed copy.
    function(fartblaster_sign target bundle)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND codesign --force --sign "${APPLE_DEV_ID}"
                    --options runtime --timestamp
                    --entitlements "${ENTITLEMENTS}"
                    "${bundle}"
            COMMAND codesign --verify --strict "${bundle}"
            COMMENT "Codesigning ${bundle}"
            VERBATIM)
    endfunction()

    fartblaster_sign(FartBlaster_VST3       "${VST3_BUNDLE}")
    fartblaster_sign(FartBlaster_VST3       "${VST3_INSTALL}")
    fartblaster_sign(FartBlaster_AU         "${AU_BUNDLE}")
    fartblaster_sign(FartBlaster_AU         "${AU_INSTALL}")
    fartblaster_sign(FartBlaster_Standalone "${APP_BUNDLE}")

    message(STATUS "Code signing enabled with: ${APPLE_DEV_ID}")
else()
    message(STATUS "Code signing disabled (ad-hoc) — pass -DAPPLE_DEV_ID=... to enable")
endif()
