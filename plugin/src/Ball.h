#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

namespace midispace {

// The draggable playhead whose position defines the current musical state.
struct Ball {
    juce::Point<float> position;
    juce::Colour colour = juce::Colours::dodgerblue;
    bool dragging = false;
};

} // namespace midispace
