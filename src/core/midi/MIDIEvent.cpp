#include "MIDIEvent.h"
#include <algorithm>

namespace Sonatrix {
namespace Core {
namespace MIDI {

void MIDIStream::SortByTime() {
    std::sort(events.begin(), events.end(), [](const MIDIEvent& a, const MIDIEvent& b) {
        if (a.timelinePosition != b.timelinePosition)
            return a.timelinePosition < b.timelinePosition;
        // At same tick: NoteOff before NoteOn for clean retrigger of common tones
        if (a.type != b.type)
            return a.type == MIDIEventType::NoteOff;
        // Stable tiebreaker by pitch
        return a.data1 < b.data1;
    });
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
