#pragma once

#include <cstdint>
#include <compare>

namespace Sonatrix {
namespace Core {

// -----------------------------------------------------------------------------
// MusicalTime represents exact position in a tempo-independent timeline.
// Used for all arrangement and pattern scheduling.
// Expressed in Pulses Per Quarter Note (PPQN), typically 960 for high res.
// -----------------------------------------------------------------------------
struct MusicalTime {
    int64_t ticks{0};
    
    constexpr MusicalTime() = default;
    constexpr explicit MusicalTime(int64_t t) : ticks(t) {}
    
    constexpr auto operator<=>(const MusicalTime&) const = default;
    
    constexpr MusicalTime operator+(const MusicalTime& other) const {
        return MusicalTime(ticks + other.ticks);
    }
    
    constexpr MusicalTime operator-(const MusicalTime& other) const {
        return MusicalTime(ticks - other.ticks);
    }
};

static constexpr int32_t STANDARD_PPQN = 960;

// Converts a beat offset (e.g. 1.5 = beat 2, halfway through) to ticks
constexpr MusicalTime BeatsToTime(double beats) {
    return MusicalTime(static_cast<int64_t>(beats * STANDARD_PPQN));
}

} // namespace Core
} // namespace Sonatrix
