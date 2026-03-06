#pragma once

#include "SampleZone.h"
#include <atomic>

namespace Sonatrix {
namespace Core {
namespace Audio {

// -----------------------------------------------------------------------------
// AudioVoice
// 
// A single, lock-free synthesis node. It maintains its own playback pointer
// through a SampleZone and applies ADSR enveloping.
// -----------------------------------------------------------------------------

class AudioVoice {
public:
    AudioVoice() = default;
    ~AudioVoice() = default;
    
    // Called exactly when a MIDI NoteOn is received.
    // Must NOT allocate memory or block.
    void Start(const SampleZone* zone, uint8_t pitch, uint8_t velocity);
    
    // Triggers the Release phase of the ADSR envelope.
    void Stop();
    
    // Forwards the playback head and fills the real-time buffer.
    void RenderNextBlock(float* outputBuffer, uint32_t numFrames, uint32_t numChannels);
    
    // Is the voice freely available to be hijacked by the VoiceManager?
    bool IsFree() const { return state_ == State::Free; }
    
    // Priority metric for Note-Stealing (lowest priority gets stolen first)
    float GetStealingPriority() const;

private:
    enum class State { Free, Attack, Decay, Sustain, Release };
    
    std::atomic<State> state_{State::Free};
    const SampleZone* activeZone_{nullptr};
    
    uint8_t currentPitch_{0};
    uint8_t currentVelocity_{0};
    
    // Playback head (floating point to allow for pitch shifting / interpolation)
    double readPosition_{0.0};
    
    // Pitch shift ratio (e.g., Target Pitch 62 / Root Key 60 -> ratio > 1.0)
    double playbackRatio_{1.0};
    
    // Current amplitude scalar based on ADSR logic
    float envelopeLevel_{0.0f};
    
    // Neural Procedural Timbre state (Mocked)
    float proceduralAttackScalar_{1.0f}; 
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
