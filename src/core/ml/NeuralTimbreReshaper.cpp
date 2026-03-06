#include "NeuralTimbreReshaper.h"
#include <cmath>

namespace Sonatrix {
namespace Core {
namespace ML {

void NeuralTimbreReshaper::ApplyDynamicHarmonicExciter(
    float* audioBuffer, 
    uint32_t numFrames, 
    uint8_t originalSampleVelocity, 
    uint8_t targetMIDIVelocity
) {
    // If the target velocity is less than or equal to the sample's inherent recording velocity,
    // we do not need to synthesize a "harder" attack. We only apply standard ADSR volume lowering.
    if (targetMIDIVelocity <= originalSampleVelocity) {
        return; 
    }
    
    // The "Excitation Factor" models how much harder the strings were struck.
    // e.g. We have an 80 velocity sample, but we want a 127 velocity sound.
    float excitationFactor = static_cast<float>(targetMIDIVelocity - originalSampleVelocity) / 127.0f;
    
    // -------------------------------------------------------------------------
    // DSP Mock for CoreML Neural Output:
    // A true ML model would map a latent harmonic vector to a non-linear 
    // waveshaper. Here, we apply a simplistic dynamic waveshaping algorithm 
    // (soft clipping + second-order harmonic generation) to simulate the 
    // physical "bite" of a harder plectrum strike without adding multi-gigabyte 
    // sample layers.
    // -------------------------------------------------------------------------

    const float drive = 1.0f + (excitationFactor * 2.0f); // Boost gain into shaper
    
    for (uint32_t i = 0; i < numFrames; ++i) {
        float x = audioBuffer[i] * drive;
        
        // Asymmetrical soft clipping (generates even & odd harmonics like a tube/console)
        float shaped;
        if (x > 0.0f) {
            // Soft compress positive peaks
            shaped = x / (1.0f + x);
        } else {
            // Harder compression on negative peaks (asymmetrical)
            shaped = x / (1.0f - x * 0.5f);
        }
        
        // Blend the shaped output back based on the excitation factor
        audioBuffer[i] = (audioBuffer[i] * (1.0f - excitationFactor)) + (shaped * excitationFactor);
    }
}

} // namespace ML
} // namespace Core
} // namespace Sonatrix
