#pragma once

#include <vector>
#include <atomic>
#include <cstdint>

namespace Sonatrix {
namespace Core {
namespace Audio {

// Standard defined bus indices for our 5 Core Instruments
enum class MixerBus : uint8_t {
    Drums = 0,
    Bass = 1,
    Guitar = 2,
    Piano = 3,
    Strings = 4,
    Count = 5
};

class AudioMixer {
public:
    AudioMixer();
    
    // Setters for Volume (0.0 to 1.0) and Pan (-1.0 to 1.0)
    // Safe to call from GUI thread (atomic under the hood)
    void SetBusVolume(MixerBus bus, float volume);
    void SetBusPan(MixerBus bus, float pan);
    
    // Process an individual bus block into the main output accumulator
    // Should be called from the Audio Realtime Thread
    void MixBusToOutput(MixerBus bus, const float* const* busChannels, float** outputChannels, uint32_t numFrames, uint32_t numChannels);

    // Clear the output buffers with silence before a render pass
    static void ClearBuffers(float** outputChannels, uint32_t numFrames, uint32_t numChannels);

private:
    struct BusState {
        std::atomic<float> volume{0.8f}; // Default volume
        std::atomic<float> pan{0.0f};    // Center pan
    };
    
    BusState buses_[static_cast<size_t>(MixerBus::Count)];
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
