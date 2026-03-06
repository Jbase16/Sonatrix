#include "DynamicGrooveVector.h"
#include <algorithm>
#include <cstdlib>

namespace Sonatrix {
namespace Core {
namespace ML {

void DynamicGrooveVector::EmplaceOffset(const GrooveOffset& offset) {
    offsets_.push_back(offset);
    
    // Maintain chronological order for fast searching
    std::sort(offsets_.begin(), offsets_.end(), 
        [](const GrooveOffset& a, const GrooveOffset& b) {
            return a.anchorTime < b.anchorTime;
        }
    );
}

void DynamicGrooveVector::Clear() {
    offsets_.clear();
}

std::optional<GrooveOffset> DynamicGrooveVector::GetOffsetForSubbeat(const MusicalTime& targetTime) const {
    if (offsets_.empty()) {
        return std::nullopt;
    }
    
    // Find the closest anchor point within a reasonable threshold (e.g., within a 16th note).
    // If a bass note falls on a 16th note, it wants the groove offset of the drummer's hi-hat on that 16th note.
    const int64_t THRESHOLD_TICKS = STANDARD_PPQN / 4; // 16th note
    
    auto it = std::lower_bound(offsets_.begin(), offsets_.end(), targetTime,
        [](const GrooveOffset& offset, const MusicalTime& target) {
            return offset.anchorTime < target;
        }
    );
    
    // Check closest match (either 'it' or 'it - 1')
    std::optional<GrooveOffset> closestMatch = std::nullopt;
    int64_t minDistance = THRESHOLD_TICKS;

    if (it != offsets_.end()) {
        int64_t dist = std::abs((it->anchorTime - targetTime).ticks);
        if (dist < minDistance) {
            minDistance = dist;
            closestMatch = *it;
        }
    }
    
    if (it != offsets_.begin()) {
        auto prevIt = std::prev(it);
        int64_t dist = std::abs((prevIt->anchorTime - targetTime).ticks);
        if (dist < minDistance) {
            closestMatch = *prevIt;
        }
    }
    
    return closestMatch;
}

} // namespace ML
} // namespace Core
} // namespace Sonatrix
