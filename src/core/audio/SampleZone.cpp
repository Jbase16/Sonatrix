#include "SampleZone.h"
#include <cmath>

namespace Sonatrix {
namespace Core {
namespace Audio {

const SampleZone* InstrumentArticulation::FindZone(uint8_t pitch, uint8_t velocity, int stringId) const {
    if (zones.empty()) return nullptr;

    const SampleZone* bestZone = nullptr;
    float bestScore = 999999.0f; // Lower score is better

    for (const auto& zone : zones) {
        // 0. EXACT STRING OVERRIDE
        // If the calling engine physically dictates which string to play on,
        // we override all penalty logic and map directly to that open string's root pitch.
        if (stringId >= 0 && stringId <= 5) {
            static const uint8_t STANDARD_TUNING[] = {40, 45, 50, 55, 59, 64};
            if (zone.rootKey == STANDARD_TUNING[stringId]) {
                return &zone;
            }
        }

        float score = 0.0f;
        int semitoneDifference = static_cast<int>(pitch) - static_cast<int>(zone.rootKey);

        // 1. Base distance penalty (1 point per semitone)
        score += std::abs(semitoneDifference);

        // 2. Pitch-Down Penalty (Acoustic strings sound bad when pitched down)
        if (semitoneDifference < 0) {
            score += 2.5f; // Add artificial distance to discourage shifting down
        }

        // 3. The "Plain Steel" Penalty (MIDI 59)
        // If we are below the B3 string (59), heavily penalize using the B3 or E4 anchors
        // so we don't accidentally pull bright steel sounds into the warm wound-string range.
        if (pitch < 59 && zone.rootKey >= 59) {
            score += 5.0f; 
        }

        // Keep the zone with the lowest total penalty
        if (score < bestScore) {
            bestScore = score;
            bestZone = &zone;
        }
    }

    return bestZone;
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
