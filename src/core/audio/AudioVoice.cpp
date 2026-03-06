#include "AudioVoice.h"
#include <cmath>
#include <algorithm>

namespace Sonatrix {
namespace Core {
namespace Audio {

void AudioVoice::Start(const SampleZone* zone, uint8_t pitch, uint8_t velocity) {
    if (!zone || !zone->isLoaded || zone->audioData.empty()) return;
    
    activeZone_ = zone;
    currentPitch_ = pitch;
    currentVelocity_ = velocity;
    
    readPosition_ = 0.0;
    
    // Extremely simplistic pitch shift calculation
    // f(MIDI) = 440 * 2^((d - 69)/12)
    double rootFreq = 440.0 * std::pow(2.0, (activeZone_->rootKey - 69.0) / 12.0);
    double targetFreq = 440.0 * std::pow(2.0, (pitch - 69.0) / 12.0);
    playbackRatio_ = targetFreq / rootFreq;
    
    // Procedural Analysis (Phase 4 ML Timbre Synthesis mock)
    // If the velocity is extremely high, we boost the attack transient mathematically
    if (velocity > 110) {
        proceduralAttackScalar_ = 1.5f; 
    } else {
        proceduralAttackScalar_ = 1.0f;
    }
    
    state_.store(State::Attack, std::memory_order_release);
    envelopeLevel_ = 0.0f; // Begin attack
}

void AudioVoice::Stop() {
    if (state_.load(std::memory_order_acquire) != State::Free) {
        state_.store(State::Release, std::memory_order_release);
    }
}

float AudioVoice::GetStealingPriority() const {
    if (state_.load(std::memory_order_acquire) == State::Free) return 0.0f;
    if (state_.load(std::memory_order_acquire) == State::Release) return 0.1f;
    
    // Lower volume = lower priority to steal
    // In production, Bass root notes = extremely high priority
    return envelopeLevel_ * (static_cast<float>(currentVelocity_) / 127.0f);
}

void AudioVoice::RenderNextBlock(float* outputBuffer, uint32_t numFrames, uint32_t numOutputChannels) {
    auto currentState = state_.load(std::memory_order_acquire);
    if (currentState == State::Free || !activeZone_ || activeZone_->audioData.empty()) {
        return;
    }
    
    const float* rawAudio = activeZone_->audioData.data();
    size_t totalFrames = activeZone_->audioData.size() / activeZone_->numChannels;
    
    // Very basic ADSR envelope constants (mock physical modeling)
    const float attackRate = 0.05f;
    const float releaseRate = 0.01f;
    const float maxVelocityAmp = static_cast<float>(currentVelocity_) / 127.0f;
    
    for (uint32_t i = 0; i < numFrames; ++i) {
        
        // --- 1. Process Envelope State ---
        if (currentState == State::Attack) {
            envelopeLevel_ += attackRate;
            if (envelopeLevel_ >= 1.0f) {
                envelopeLevel_ = 1.0f;
                currentState = State::Sustain;
                state_.store(currentState, std::memory_order_release);
            }
        } else if (currentState == State::Release) {
            envelopeLevel_ -= releaseRate;
            if (envelopeLevel_ <= 0.0f) {
                envelopeLevel_ = 0.0f;
                state_.store(State::Free, std::memory_order_release);
                return; // Voice is dead, stop rendering
            }
        }
        
        // --- 2. Fetch Sample via Linear Interpolation ---
        // Safety bounds
        if (static_cast<size_t>(readPosition_) >= totalFrames - 1) {
            state_.store(State::Free, std::memory_order_release);
            return;
        }
        
        size_t index1 = static_cast<size_t>(readPosition_);
        size_t index2 = index1 + 1;
        float frac = static_cast<float>(readPosition_ - index1);
        
        // --- 3. Render into Output Buffer ---
        // Note: Assumes outputBuffer is pre-zeroed by the DAW/Host or mixing bus.
        // We *add* to the buffer to allow polyphonic mixing.
        
        for (uint32_t chan = 0; chan < numOutputChannels; ++chan) {
            // If sample is mono, read from channel 0. If stereo, read matching channel if available.
            uint32_t readChan = std::min(chan, activeZone_->numChannels - 1);
            
            float val1 = rawAudio[index1 * activeZone_->numChannels + readChan];
            float val2 = rawAudio[index2 * activeZone_->numChannels + readChan];
            
            // Linear interpolation
            float sample = val1 + frac * (val2 - val1);
            
            // Apply Neural Procedural Attack transient (if in early attack phase)
            if (currentState == State::Attack && index1 < 2000) {
                 sample *= proceduralAttackScalar_; // Boost initial transient
            }
            
            // Apply ADSR and MIDI Velocity
            sample *= envelopeLevel_ * maxVelocityAmp;
            
            // Mix into output bus
            outputBuffer[i * numOutputChannels + chan] += sample;
        }
        
        // Advance read pointer by pitch ratio
        readPosition_ += playbackRatio_;
    }
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
