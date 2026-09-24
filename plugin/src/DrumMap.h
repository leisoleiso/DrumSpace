#pragma once

#include <juce_core/juce_core.h>

namespace midispace {

// Canonical 9 drum classes (fixed GM pitches).  Index order matches
// drumPresets()/allMotifs() and the model's 9 output classes.
inline constexpr int kDrumGmPitch[9] = { 36, 38, 42, 46, 45, 48, 50, 49, 51 };

inline const char* drumClassName(int i) {
    static const char* names[9] = {
        "Kick", "Snare", "Closed HH", "Open HH",
        "Low Tom", "Mid Tom", "High Tom", "Crash", "Ride"
    };
    return (i >= 0 && i < 9) ? names[i] : "?";
}

// Maps the 9 canonical GM drum pitches to user-defined output pitches.
// Internal data (motifs, latents, encode/decode) always uses GM pitches;
// only the MIDI sent to the host is remapped (mapOut), and incoming captured
// MIDI is mapped back to GM (mapIn).
struct DrumMap {
    int out[9] = { 36, 38, 42, 46, 45, 48, 50, 49, 51 };   // per class

    // GM pitch -> user's output pitch.
    int mapOut(int pitch) const {
        for (int i = 0; i < 9; ++i)
            if (kDrumGmPitch[i] == pitch)
                return out[i];
        return pitch;   // unknown pitch: pass through unchanged
    }

    // User's output pitch -> GM pitch (reverse lookup; first match wins).
    int mapIn(int pitch) const {
        for (int i = 0; i < 9; ++i)
            if (out[i] == pitch)
                return kDrumGmPitch[i];
        return pitch;
    }

    // Serialise as "36,38,42,..." (9 comma-separated pitches).
    juce::String toString() const {
        juce::StringArray parts;
        for (int i = 0; i < 9; ++i)
            parts.add(juce::String(out[i]));
        return parts.joinIntoString(",");
    }

    static DrumMap fromString(const juce::String& s) {
        DrumMap m;
        const auto parts = juce::StringArray::fromTokens(s, ",", "");
        for (int i = 0; i < 9 && i < parts.size(); ++i)
            m.out[i] = parts[i].getIntValue();
        return m;
    }
};

// Directory where named drum-key maps are stored.
inline juce::File drumMapDirectory() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("DrumSpace")
        .getChildFile("maps");
}

} // namespace midispace
