#pragma once

#include <cstdint>
#include <vector>
#include "../mir/MusicalTime.h"

namespace Sonatrix {
namespace Core {
namespace MIDI {

// -----------------------------------------------------------------------------
// Standard MIDI Event Representation
// Represents literal output going to a DAW (via drag & drop) or internal voices.
// Unlike MIR, this has no concept of "Strum" or "Divisi". It is raw note data.
// -----------------------------------------------------------------------------

enum class MIDIEventType : uint8_t {
    NoteOn = 0x90,
    NoteOff = 0x80,
    ControlChange = 0xB0,
    PitchBend = 0xE0
};

struct MIDIEvent {
    MusicalTime timelinePosition; // Absolute time
    MIDIEventType type;
    uint8_t channel{0};           // 0-15
    uint8_t data1{0};             // Note Number or CC Number
    uint8_t data2{0};             // Velocity or CC Value
};

// Represents a sequence of MIDI events, usually the result of an engine compiling MIR
struct MIDIStream {
    std::vector<MIDIEvent> events;
    
    // Sorts events by timeline position (required for valid MIDI files)
    void SortByTime();
};

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
