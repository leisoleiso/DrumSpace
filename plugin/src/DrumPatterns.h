#pragma once

#include "ModelInterface.h"
#include <vector>

namespace midispace {

// General MIDI drum pitches (matching magenta's 9-class DrumsConverter).
enum : int {
    kKick       = 36,
    kSnare      = 38,
    kClosedHH   = 42,
    kOpenHH     = 46,
    kLowTom     = 45,
    kMidTom     = 48,
    kHighTom    = 50,
    kCrash      = 49,
    kRide       = 51,
};

// One drum hit = one 16th-note step (0.25 quarter), matching the model's
// steps_per_quarter=4 grid.
inline NoteEvent d(int pitch, float startQuarter) {
    return { pitch, startQuarter, startQuarter + 0.25f };
}

// 8th-note closed hi-hat (every half quarter).
inline void addHH8(std::vector<NoteEvent>& out) {
    for (int i = 0; i < 16; ++i)
        out.push_back(d(kClosedHH, i * 0.5f));
}

// 16th-note closed hi-hat (every quarter step).
inline void addHH16(std::vector<NoteEvent>& out) {
    for (int i = 0; i < 32; ++i)
        out.push_back(d(kClosedHH, i * 0.25f));
}

// --- Presets ---------------------------------------------------------------

struct DrumPreset {
    const char* name;
    std::vector<NoteEvent> pattern;
};

inline std::vector<DrumPreset> drumPresets() {
    std::vector<DrumPreset> presets;

    // Four-on-the-floor (dance/electronic).
    {
        std::vector<NoteEvent> p;
        for (int q = 0; q < 8; ++q)      // kick every quarter
            p.push_back(d(kKick, static_cast<float>(q)));
        p.push_back(d(kSnare, 2.0f));    // snare on beats 2 and 4
        p.push_back(d(kSnare, 6.0f));
        addHH8(p);
        presets.push_back({ "Four on the Floor", std::move(p) });
    }

    // Rock.
    {
        std::vector<NoteEvent> p;
        p.push_back(d(kKick, 0.0f));
        p.push_back(d(kKick, 1.5f));
        p.push_back(d(kKick, 4.0f));
        p.push_back(d(kKick, 5.5f));
        p.push_back(d(kSnare, 2.0f));
        p.push_back(d(kSnare, 6.0f));
        addHH8(p);
        presets.push_back({ "Rock", std::move(p) });
    }

    // Funk.
    {
        std::vector<NoteEvent> p;
        p.push_back(d(kKick, 0.0f));
        p.push_back(d(kKick, 1.75f));
        p.push_back(d(kKick, 3.5f));
        p.push_back(d(kKick, 4.0f));
        p.push_back(d(kKick, 5.75f));
        p.push_back(d(kKick, 7.0f));
        p.push_back(d(kSnare, 2.0f));
        p.push_back(d(kSnare, 6.0f));
        addHH16(p);
        p.push_back(d(kOpenHH, 2.5f));
        p.push_back(d(kOpenHH, 6.5f));
        presets.push_back({ "Funk", std::move(p) });
    }

    // Hip-hop.
    {
        std::vector<NoteEvent> p;
        p.push_back(d(kKick, 0.0f));
        p.push_back(d(kKick, 1.5f));
        p.push_back(d(kKick, 3.0f));
        p.push_back(d(kKick, 4.5f));
        p.push_back(d(kKick, 6.0f));
        p.push_back(d(kSnare, 2.0f));
        p.push_back(d(kSnare, 6.0f));
        addHH16(p);
        presets.push_back({ "Hip-Hop", std::move(p) });
    }

    // Breakbeat.
    {
        std::vector<NoteEvent> p;
        p.push_back(d(kKick, 0.0f));
        p.push_back(d(kKick, 1.75f));
        p.push_back(d(kSnare, 2.0f));
        p.push_back(d(kKick, 3.5f));
        p.push_back(d(kSnare, 4.0f));
        p.push_back(d(kKick, 5.75f));
        p.push_back(d(kSnare, 6.0f));
        addHH16(p);
        presets.push_back({ "Breakbeat", std::move(p) });
    }

    return presets;
}

// Default anchor patterns (node 1/2/3): three distinct presets.
inline std::vector<std::vector<NoteEvent>> allMotifs() {
    const auto presets = drumPresets();
    return { presets[0].pattern, presets[1].pattern, presets[2].pattern };
}

} // namespace midispace
