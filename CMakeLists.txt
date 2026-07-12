cmake_minimum_required(VERSION 3.22)
project(NoiseReductionPlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point to local JUCE installation
add_subdirectory("C:/Users/leona/Documents/JUCE" ${CMAKE_BINARY_DIR}/JUCE)

# Create the plugin target
juce_add_plugin(NoiseReductionPlugin
    PRODUCT_NAME "Noise Reduction"
    COMPANY_NAME "NoiseReduction"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Nsrd
    PLUGIN_CODE NsRd
    FORMATS VST3 Standalone
    BUNDLE_ID com.noisereduction.noisereductionplugin
)

# Source files
target_sources(NoiseReductionPlugin
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/SpectrumAnalyzer.cpp
)

# Link against JUCE modules
target_link_libraries(NoiseReductionPlugin
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
)

# Compile definitions
target_compile_definitions(NoiseReductionPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)