#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "ModelInterface.h"

namespace midispace {

// Minimal, self-contained MIDI file writer (format 0, 480 PPQ) for saving a
// monophonic 2-bar motif.  Avoids juce::MidiFile's second/ticks ambiguity.
namespace detail {

inline void pushVlq(std::vector<uint8_t>& out, uint32_t v) {
    uint32_t buf = v & 0x7F;
    while ((v >>= 7) > 0) {
        buf <<= 8;
        buf |= 0x80;
        buf |= (v & 0x7F);
    }
    for (;;) {
        out.push_back(static_cast<uint8_t>(buf & 0xFF));
        if (buf & 0x80) buf >>= 8;
        else break;
    }
}

inline void pushU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

inline void pushU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v >> 24));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

} // namespace detail

inline std::vector<uint8_t> buildMidiFile(const std::vector<NoteEvent>& notes, double bpm = 120.0) {
    const uint16_t ppq = 480;
    const uint32_t usPerQuarter = static_cast<uint32_t>(std::llround(60000000.0 / bpm));

    std::vector<uint8_t> track;

    // Tempo meta event at tick 0.
    track.push_back(0x00);
    track.push_back(0xFF); track.push_back(0x51); track.push_back(0x03);
    track.push_back(static_cast<uint8_t>((usPerQuarter >> 16) & 0xFF));
    track.push_back(static_cast<uint8_t>((usPerQuarter >> 8) & 0xFF));
    track.push_back(static_cast<uint8_t>(usPerQuarter & 0xFF));

    // Note on/off events.
    struct Ev { uint32_t tick; uint8_t status; uint8_t pitch; uint8_t vel; };
    std::vector<Ev> evs;
    for (const auto& n : notes) {
        const uint32_t st = static_cast<uint32_t>(std::llround(n.startQuarter * ppq));
        const uint32_t en = static_cast<uint32_t>(std::llround(n.endQuarter * ppq));
        evs.push_back({ st, 0x90, static_cast<uint8_t>(n.pitch), 100 });
        evs.push_back({ en, 0x80, static_cast<uint8_t>(n.pitch), 0 });
    }
    std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { return a.tick < b.tick; });

    uint32_t lastTick = 0;
    for (const auto& e : evs) {
        detail::pushVlq(track, e.tick - lastTick);
        track.push_back(e.status);
        track.push_back(e.pitch);
        track.push_back(e.vel);
        lastTick = e.tick;
    }

    // End of track.
    detail::pushVlq(track, 0);
    track.push_back(0xFF); track.push_back(0x2F); track.push_back(0x00);

    // Assemble file: MThd + MTrk.
    std::vector<uint8_t> out;
    const char* hdr = "MThd";
    out.insert(out.end(), hdr, hdr + 4);
    detail::pushU32(out, 6);
    detail::pushU16(out, 0);      // format 0
    detail::pushU16(out, 1);      // one track
    detail::pushU16(out, ppq);

    const char* trk = "MTrk";
    out.insert(out.end(), trk, trk + 4);
    detail::pushU32(out, static_cast<uint32_t>(track.size()));
    out.insert(out.end(), track.begin(), track.end());
    return out;
}

inline bool writeMidiFile(const std::vector<NoteEvent>& notes, const juce::File& file, double bpm = 120.0) {
    const auto bytes = buildMidiFile(notes, bpm);
    return file.replaceWithData(bytes.data(), bytes.size());
}

} // namespace midispace
