#pragma once

#include "ModelInterface.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace midispace {

// Post-processing for generated drum patterns.  Drums have fixed General-MIDI
// pitches (no transposition / octave folding) but a density control is still
// useful for thinning a busy pattern.
struct MusicalControls {
    float density = 1.0f;          // 0.1..1.0 hit density

    std::vector<NoteEvent> apply(std::vector<NoteEvent> notes) const {
        // Density: keep the first/last hit and every keepEvery-th hit.
        if (density < 0.999f && notes.size() > 2) {
            const int keepEvery = std::max(1, (int) std::lround(1.0f / std::max(density, 0.1f)));
            std::vector<NoteEvent> kept;
            kept.reserve(notes.size());
            for (size_t i = 0; i < notes.size(); ++i)
                if (i == 0 || i + 1 == notes.size() || static_cast<int>(i) % keepEvery == 0)
                    kept.push_back(notes[i]);
            notes = std::move(kept);
        }
        return notes;
    }
};

} // namespace midispace
