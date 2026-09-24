cmake_minimum_required(VERSION 3.22)
project(DrumSpace VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Reuse the JUCE framework already cloned for the MidiSpace project.
# JUCE 9 bundles a complete VST3 SDK, so no external SDK path is required.
set(JUCE_DIR "C:/Users/26212/midi-space/JUCE")
add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(DrumSpacePlugin
    PRODUCT_NAME            "DrumSpace"
    COMPANY_NAME            "DrumSpace"
    VERSION                 "0.1.0"
    FORMATS                 Standalone VST3
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Juce
    PLUGIN_CODE             DrSp)

target_sources(DrumSpacePlugin PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/CanvasComponent.cpp
    src/HttpModelInterface.cpp
    src/GenerationManager.cpp)

target_include_directories(DrumSpacePlugin PRIVATE src)

# Standalone format needs the audio devices + utils modules for its host wrapper.
target_link_libraries(DrumSpacePlugin PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices)

# Fresh plugin with no legacy VST2 to stay compatible with: skip the VST2/VST3
# parameter-ID migration check.
target_compile_definitions(DrumSpacePlugin PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
