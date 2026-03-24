#pragma once

#include <cstdint>
#include <vector>
#include "PlaybackInstrument.h"
#include <string>

namespace Sonatrix {
namespace Core {
namespace Audio {

// -----------------------------------------------------------------------------
// SampleZone
// 
// Represents a single contiguous audio buffer loaded into memory (RAM).
// In Sonatrix, we use a Sparse Matrix approach (fewer round-robins)
// to save memory, relying on Procedural Timbre Enhancement later.
// -----------------------------------------------------------------------------

struct SampleZone {
    std::string filePath;
    uint8_t rootKey{60};       // The natural MIDI pitch of the recording
    uint8_t lowVelocity{0};    // The lowest velocity this sample covers
    uint8_t highVelocity{127}; // The highest velocity this sample covers
    float volumeTrim{1.0f};    // Linear gain trim for this specific file
    
    // The actual raw interleaved or planar floating-point audio data.
    // In production, this would be populated by libsndfile or CoreAudio ExtAudioFileRead.
    std::vector<float> audioData;
    uint32_t sampleRate{44100};
    uint32_t numChannels{2};
    
    // Is the sample fully loaded into RAM?
    bool isLoaded{false};
};

// -----------------------------------------------------------------------------
// InstrumentArticulations
// 
// Represents a collection of Zones for a specific playing style.
// E.g., "Guitar Muted Downstrokes" vs "Guitar Open Sustains"
// -----------------------------------------------------------------------------

struct InstrumentArticulation {
    std::string name;
    float outputGain{1.0f};
    PlaybackInstrument instrumentType{PlaybackInstrument::Guitar};
    std::vector<SampleZone> zones;
    
    // Real-time lookup: Find the best zone for a given requested pitch, velocity, and optional explicit physical string
    const SampleZone* FindZone(uint8_t pitch, uint8_t velocity, int stringId = -1, uint8_t anchorOverride = 0) const;
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
