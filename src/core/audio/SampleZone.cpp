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
        float score = 0.0f;
        int semitoneDifference = static_cast<int>(pitch) - static_cast<int>(zone.rootKey);

        // Base distance penalty
        score += std::abs(semitoneDifference);

        if (instrumentType == PlaybackInstrument::Guitar) {
            // 0. EXACT STRING OVERRIDE for Guitar
            if (stringId >= 0 && stringId <= 5) {
                static const uint8_t STANDARD_TUNING[] = {40, 45, 50, 55, 59, 64};
                if (zone.rootKey == STANDARD_TUNING[stringId]) {
                    return &zone;
                }
            }

            // 1. Pitch-Down Penalty (Acoustic strings sound bad when pitched down)
            if (semitoneDifference < 0) {
                score += 2.5f; 
            }

            // 2. The "Plain Steel" Penalty (MIDI 59)
            if (pitch < 59 && zone.rootKey >= 59) {
                score += 5.0f; 
            }
        } else if (instrumentType == PlaybackInstrument::ElectricBass || instrumentType == PlaybackInstrument::MockBass) {
            // Bass prefers to pitch DOWN if choosing between anchors, to preserve low-end weight.
            // Shifting UP too far makes it sound like a synth or a guitar.
            if (semitoneDifference > 0) {
                score += 1.5f; // Add artificial penalty to discourage shifting up
            }
            // Heavily penalize giant shifts
            if (std::abs(semitoneDifference) > 12) {
                score += 10.0f;
            }
        } else if (instrumentType == PlaybackInstrument::AcousticPiano) {
            // Piano tolerates moderate pitch shifts in both directions.
            // Quality degrades noticeably beyond a P5 (7 semitones).
            // Mild preference for shifting down (preserves low-end weight in LH).
            if (semitoneDifference < 0) {
                score += 0.5f; // slight upward-shift preference
            }
            if (std::abs(semitoneDifference) > 7) {
                score += 5.0f; // heavy penalty beyond P5
            }
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
