#include "HttpModelInterface.h"

namespace midispace {

static juce::var notesToJson(const std::vector<NoteEvent>& notes) {
    juce::Array<juce::var> arr;
    for (const auto& n : notes) {
        juce::Array<juce::var> t;
        t.add(n.pitch);
        t.add(n.startQuarter);
        t.add(n.endQuarter);
        arr.add(juce::var(t));
    }
    return juce::var(arr);
}

static std::vector<NoteEvent> jsonToNotes(const juce::var& v) {
    std::vector<NoteEvent> out;
    if (!v.isArray())
        return out;
    for (const auto& item : *v.getArray()) {
        if (item.isArray() && item.size() >= 3) {
            NoteEvent n;
            n.pitch = static_cast<int>(item[0]);
            n.startQuarter = static_cast<float>(static_cast<double>(item[1]));
            n.endQuarter = static_cast<float>(static_cast<double>(item[2]));
            out.push_back(n);
        }
    }
    return out;
}

juce::var HttpModelInterface::postJson(const juce::String& path, const juce::var& payload) {
    const juce::String body = juce::JSON::toString(payload);
    juce::URL url(baseUrl_ + path);
    url = url.withPOSTData(body);

    auto stream = url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
            .withConnectionTimeoutMs(30000)
            .withExtraHeaders("Content-Type: application/json"));

    if (stream == nullptr)
        throw std::runtime_error("HTTP request failed: " + (baseUrl_ + path).toStdString());

    const juce::String response = stream->readEntireStreamAsString();
    juce::var parsed = juce::JSON::parse(response);
    if (parsed.isVoid())
        throw std::runtime_error("bad JSON from server");

    if (auto* obj = parsed.getDynamicObject())
        if (obj->hasProperty("error"))
            throw std::runtime_error(obj->getProperty("error").toString().toStdString());

    return parsed;
}

LatentVector HttpModelInterface::encode(const std::vector<NoteEvent>& notes) {
    juce::var payload = juce::var(new juce::DynamicObject());
    payload.getDynamicObject()->setProperty("notes", notesToJson(notes));

    juce::var resp = postJson("/encode", payload);
    auto* obj = resp.getDynamicObject();
    if (obj == nullptr || !obj->hasProperty("z") || !obj->getProperty("z").isArray())
        throw std::runtime_error("bad /encode response");

    LatentVector out;
    for (const auto& v : *obj->getProperty("z").getArray())
        out.push_back(static_cast<float>(static_cast<double>(v)));
    return out;
}

std::vector<NoteEvent> HttpModelInterface::decode(const LatentVector& z, float temperature) {
    juce::Array<juce::var> zarr;
    for (float v : z)
        zarr.add(v);

    juce::var payload = juce::var(new juce::DynamicObject());
    payload.getDynamicObject()->setProperty("z", juce::var(zarr));
    payload.getDynamicObject()->setProperty("temperature", temperature);

    juce::var resp = postJson("/decode", payload);
    auto* obj = resp.getDynamicObject();
    if (obj == nullptr || !obj->hasProperty("notes"))
        throw std::runtime_error("bad /decode response");

    return jsonToNotes(obj->getProperty("notes"));
}

} // namespace midispace
