#include "SampleZone.h"
#include <cmath>

namespace Sonatrix {
namespace Core {
namespace Audio {

const SampleZone* InstrumentArticulation::FindZone(uint8_t pitch, uint8_t velocity) const {
    if (zones.empty()) return nullptr;
    
    const SampleZone* bestMatch = nullptr;
    int minPitchDist = 127;
    
    for (const auto& zone : zones) {
        // Must match velocity criteria (if any exists in our sparse matrix)
        if (velocity >= zone.lowVelocity && velocity <= zone.highVelocity) {
            
            int dist = std::abs(static_cast<int>(pitch) - static_cast<int>(zone.rootKey));
            
            // Prefer the closest pitch logically to minimize DSP repitching artifacts
            if (dist < minPitchDist) {
                minPitchDist = dist;
                bestMatch = &zone;
            }
        }
    }
    
    // If no strict velocity match exists, fallback to closest pitch regardless
    if (!bestMatch) {
         for (const auto& zone : zones) {
             int dist = std::abs(static_cast<int>(pitch) - static_cast<int>(zone.rootKey));
             if (dist < minPitchDist) {
                 minPitchDist = dist;
                 bestMatch = &zone;
             }
         }
    }
    
    return bestMatch;
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
