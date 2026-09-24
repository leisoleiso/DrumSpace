#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "ModelInterface.h"

namespace midispace {

// A user-placed musical anchor in the 2D control space.  Carries its MIDI
// motif and its cached latent vector (encoded once), so the canvas is fully
// self-contained and nodes can be added/removed dynamically.
struct MidiNode {
    juce::String id;
    juce::String name;               // display label, e.g. "A"
    juce::Point<float> position;     // canvas-space centre (pixels)
    juce::Colour colour = juce::Colours::tomato;
    bool selected = false;

    std::vector<NoteEvent> motif;    // the 2-bar MIDI motif (for preview/re-encode)
    LatentVector latent;             // cached posterior mean (for interpolation)
};

// Stable per-node palette so colours stay distinct and survive save/load.
inline juce::Colour paletteColour(int index) {
    static const juce::Colour p[] = {
        juce::Colours::tomato, juce::Colours::forestgreen, juce::Colours::orange,
        juce::Colours::mediumpurple, juce::Colours::steelblue, juce::Colours::gold,
    };
    const int n = static_cast<int>(sizeof(p) / sizeof(p[0]));
    return p[((index % n) + n) % n];
}

// Drum-piece short name for a General MIDI drum pitch (e.g. "Kick", "Snare").
inline juce::String drumPitchName(int pitch) {
    switch (pitch) {
        case 36: return "Kick";
        case 38: return "Snare";
        case 42: return "HH";
        case 46: return "OHH";
        case 45: return "LoTom";
        case 48: return "MidTom";
        case 50: return "HiTom";
        case 49: return "Crash";
        case 51: return "Ride";
        default:  return {};
    }
}

// First drum's type short-name (e.g. "Kick") for the node label.
inline juce::String firstNoteName(const MidiNode& node) {
    if (node.motif.empty())
        return {};
    return drumPitchName(node.motif[0].pitch);
}

} // namespace midispace
