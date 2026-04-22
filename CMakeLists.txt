cmake_minimum_required(VERSION 3.22)
project(LM1894_DNR VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE ---
# Set JUCE_DIR to the path of your JUCE installation, or pass via -DJUCE_DIR=...
if(NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "${CMAKE_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE framework")
endif()

if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR
        "JUCE not found at ${JUCE_DIR}. "
        "Clone JUCE there or set -DJUCE_DIR=<path>.")
endif()

# --- Plugin target ---
juce_add_plugin(LM1894_DNR
    COMPANY_NAME       "LM1894Project"
    PLUGIN_NAME        "LM1894 DNR"
    PLUGIN_CODE        Lm18
    FORMATS            VST3 Standalone
    PRODUCT_NAME       "LM1894 DNR"
    IS_SYNTH           FALSE
    NEEDS_MIDI_INPUT   FALSE
    NEEDS_MIDI_OUTPUT  FALSE
    IS_MIDI_EFFECT     FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# --- Sources ---
target_sources(LM1894_DNR PRIVATE
    plugin/juce/Source/PluginProcessor.cpp
    plugin/juce/Source/PluginProcessor.h
    plugin/juce/Source/PluginEditor.cpp
    plugin/juce/Source/PluginEditor.h
)

target_include_directories(LM1894_DNR PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(LM1894_DNR PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(LM1894_DNR
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
