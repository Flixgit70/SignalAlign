cmake_minimum_required(VERSION 3.22)

project(SignalAlign VERSION 0.2.28)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

# JUCE checkout can always be supplied with -DJUCE_PATH=...
# Defaults match the shared project workflow on macOS and Windows.
if (NOT DEFINED JUCE_PATH OR JUCE_PATH STREQUAL "")
    if (WIN32)
        set(JUCE_PATH "C:/Users/fkrue/Nextcloud2/Projekte/Development/JUCE/Framework"
            CACHE PATH "Path to the JUCE repository")
    else()
        set(JUCE_PATH "/Users/felixkruckels/JUCE"
            CACHE PATH "Path to the JUCE repository")
    endif()
endif()

if (EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${JUCE_PATH}" JUCE)
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

if (WIN32)
    # The Windows installer script copies the VST3 after a successful build.
    # Avoid JUCE trying to copy into Program Files during the build itself.
    set(SIGNALALIGN_COPY_PLUGIN_AFTER_BUILD FALSE)
else()
    set(SIGNALALIGN_COPY_PLUGIN_AFTER_BUILD TRUE)
endif()

juce_add_plugin(SignalAlign
    VERSION 0.2.28
    COMPANY_NAME "SignalAlign"
    BUNDLE_ID "de.felixkruckels.signalalign"
    PLUGIN_MANUFACTURER_CODE Fkrk
    PLUGIN_CODE Sgal
    FORMATS VST3 Standalone
    PRODUCT_NAME "SignalAlign"
    PLUGIN_NAME "SignalAlign"
    DESCRIPTION "Robust dual-channel coherence, phase and time-alignment analyser"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${SIGNALALIGN_COPY_PLUGIN_AFTER_BUILD}
    VST3_CATEGORIES Analyzer Tools Stereo
)


juce_add_binary_data(SignalAlignAssets
    SOURCES
        Assets/K2_logo.png
)

juce_generate_juce_header(SignalAlign)

target_sources(SignalAlign PRIVATE
    Source/AnalysisEngine.cpp
    Source/AnalysisEngine.h
    Source/AnalysisTypes.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(SignalAlign PUBLIC
    SIGNALALIGN_16_INPUT=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    # SignalAlign has never had a VST2 release. Disable JUCE's VST2->VST3
    # replacement compatibility path so current JUCE uses native VST3 parameter IDs.
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SignalAlign PRIVATE
    SignalAlignAssets
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# SignalAlign16: 16-input variant
# Input 1 is always REFERENCE. Exactly one of Inputs 2-16 is selected as COMPARE.
# The original stereo SignalAlign target remains unchanged and can coexist with it.
# -----------------------------------------------------------------------------
juce_add_plugin(SignalAlign16
    VERSION 0.2.28
    COMPANY_NAME "SignalAlign"
    BUNDLE_ID "de.felixkruckels.signalalign16"
    PLUGIN_MANUFACTURER_CODE Fkrk
    PLUGIN_CODE Sg16
    FORMATS VST3
    PRODUCT_NAME "SignalAlign 16"
    PLUGIN_NAME "SignalAlign 16"
    DESCRIPTION "16-input selectable compare coherence, phase and time-alignment analyser"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${SIGNALALIGN_COPY_PLUGIN_AFTER_BUILD}
    VST3_CATEGORIES Analyzer Tools
)

juce_generate_juce_header(SignalAlign16)

target_sources(SignalAlign16 PRIVATE
    Source/AnalysisEngine.cpp
    Source/AnalysisEngine.h
    Source/AnalysisTypes.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(SignalAlign16 PUBLIC
    SIGNALALIGN_16_INPUT=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SignalAlign16 PRIVATE
    SignalAlignAssets
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

