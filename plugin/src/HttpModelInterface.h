#pragma once

#include "ModelInterface.h"
#include <juce_core/juce_core.h>

namespace midispace {

// Concrete ModelInterface that talks to the local Python MusicVAE server
// (server/server.py) over HTTP/JSON.
class HttpModelInterface : public ModelInterface {
public:
    explicit HttpModelInterface(juce::String baseUrl) : baseUrl_(std::move(baseUrl)) {}

    LatentVector encode(const std::vector<NoteEvent>& notes) override;
    std::vector<NoteEvent> decode(const LatentVector& z, float temperature) override;

private:
    // POST a JSON payload to `path` and return the parsed JSON response.
    juce::var postJson(const juce::String& path, const juce::var& payload);

    juce::String baseUrl_;
};

} // namespace midispace
