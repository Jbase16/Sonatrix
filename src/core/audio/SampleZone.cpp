#include "SampleZone.h"
#include <cmath>

namespace Sonatrix {
namespace Core {
namespace Audio {

const SampleZone* InstrumentArticulation::FindZone(uint8_t pitch, uint8_t velocity, int stringId, uint8_t anchorOverride) const {
    if (zones.empty()) return nullptr;

    const SampleZone* bestZone = nullptr;
    float bestScore = 999999.0f; // Lower score is better

    // If an explicit anchor is requested (e.g. for piano texturing), use it directly
    if (anchorOverride > 0) {
        for (const auto& zone : zones) {
            if (zone.rootKey == anchorOverride && velocity >= zone.lowVelocity && velocity <= zone.highVelocity) {
                return &zone;
            }
        }
    }

    for (const auto& zone : zones) {
        if (velocity < zone.lowVelocity || velocity > zone.highVelocity) continue;

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
            // Piano fallback: Use pure nearest-anchor.
            // (Real selection now happens via cluster logic in PianoCompiler, which provides anchorOverride).
            if (std::abs(semitoneDifference) > 7) {
                score += 5.0f;
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
