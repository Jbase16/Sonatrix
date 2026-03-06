#include "MIDIEvent.h"
#include <algorithm>

namespace Sonatrix {
namespace Core {
namespace MIDI {

void MIDIStream::SortByTime() {
    std::sort(events.begin(), events.end(), [](const MIDIEvent& a, const MIDIEvent& b) {
        return a.timelinePosition < b.timelinePosition;
    });
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
