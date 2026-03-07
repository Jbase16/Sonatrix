#include "AudioMixer.h"
#include <cstring>
#include <algorithm>

namespace Sonatrix {
namespace Core {
namespace Audio {

AudioMixer::AudioMixer() {
    for (size_t i = 0; i < static_cast<size_t>(MixerBus::Count); ++i) {
        buses_[i].volume.store(0.8f, std::memory_order_relaxed);
        buses_[i].pan.store(0.0f, std::memory_order_relaxed);
    }
}

void AudioMixer::SetBusVolume(MixerBus bus, float volume) {
    if (bus >= MixerBus::Count) return;
    
    // Clamp to [0.0, 1.0]
    volume = std::clamp(volume, 0.0f, 1.0f);
    buses_[static_cast<size_t>(bus)].volume.store(volume, std::memory_order_relaxed);
}

void AudioMixer::SetBusPan(MixerBus bus, float pan) {
    if (bus >= MixerBus::Count) return;
    
    // Clamp to [-1.0, 1.0]
    pan = std::clamp(pan, -1.0f, 1.0f);
    buses_[static_cast<size_t>(bus)].pan.store(pan, std::memory_order_relaxed);
}

void AudioMixer::ClearBuffers(float** outputChannels, uint32_t numFrames, uint32_t numChannels) {
    for (uint32_t ch = 0; ch < numChannels; ++ch) {
        if (outputChannels[ch]) {
            std::memset(outputChannels[ch], 0, numFrames * sizeof(float));
        }
    }
}

void AudioMixer::MixBusToOutput(MixerBus bus, const float* const* busChannels, float** outputChannels, uint32_t numFrames, uint32_t numChannels) {
    if (bus >= MixerBus::Count || numChannels < 1) return;
    
    // Load current volume and pan values once per block (avoids atomics in loop)
    // Note: A true production mixer might interpolate the gain envelope across the block 
    // to prevent zipper noise, but this is acceptable for Phase 12 MVP.
    float vol = buses_[static_cast<size_t>(bus)].volume.load(std::memory_order_relaxed);
    float pan = buses_[static_cast<size_t>(bus)].pan.load(std::memory_order_relaxed);
    
    if (vol < 0.001f) return; // Muted, save CPU
    
    // Simple Constant-Power Panning (Approximation for stereo out)
    float leftGain = vol;
    float rightGain = vol;
    
    if (numChannels >= 2) {
        leftGain = vol * (pan <= 0.0f ? 1.0f : (1.0f - pan));    // Pan -1.0 -> Left 1.0. Pan 1.0 -> Left 0.0
        rightGain = vol * (pan >= 0.0f ? 1.0f : (1.0f + pan));   // Pan 1.0 -> Right 1.0. Pan -1.0 -> Right 0.0
    }
    
    // Process Mono Source (Bus 0 -> Outputs 0...N)
    if (busChannels[0] && busChannels[1] == nullptr) {
        for (uint32_t i = 0; i < numFrames; ++i) {
            float monoSample = busChannels[0][i];
            
            if (numChannels == 1) {
                outputChannels[0][i] += monoSample * vol;
            } else if (numChannels >= 2) {
                outputChannels[0][i] += monoSample * leftGain;
                outputChannels[1][i] += monoSample * rightGain;
            }
        }
    } 
    // Process Stereo Source (Bus L/R -> Outputs 0...N)
    else if (busChannels[0] && busChannels[1]) {
        for (uint32_t i = 0; i < numFrames; ++i) {
            float leftSample = busChannels[0][i];
            float rightSample = busChannels[1][i];
            
            if (numChannels == 1) {
                outputChannels[0][i] += (leftSample + rightSample) * 0.5f * vol;
            } else if (numChannels >= 2) {
                outputChannels[0][i] += leftSample * leftGain;
                outputChannels[1][i] += rightSample * rightGain;
            }
        }
    }
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
