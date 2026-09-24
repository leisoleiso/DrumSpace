#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace midispace {

// A single MIDI note on the quarter-note grid.
struct NoteEvent {
    int pitch = 60;
    float startQuarter = 0.0f;
    float endQuarter = 1.0f;
};

using LatentVector = std::vector<float>;

// Abstract model boundary (C++ mirror of phase1_validate/src/model.py).
// encode: MIDI motif -> latent;  decode: latent -> MIDI motif.
// SpatialEngine and GenerationManager depend only on this interface, never on
// a concrete backend (HTTP Python server now; ONNX/TF2 later).
class ModelInterface {
public:
    virtual ~ModelInterface() = default;

    virtual LatentVector encode(const std::vector<NoteEvent>& notes) = 0;
    virtual std::vector<NoteEvent> decode(const LatentVector& z,
                                          float temperature = 0.5f) = 0;
};

} // namespace midispace
