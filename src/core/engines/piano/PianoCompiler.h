#pragma once

#include "../../midi/IMIRCompiler.h"
#include <memory>

namespace Sonatrix {
namespace Core {
namespace Engines {

// -----------------------------------------------------------------------------
// Piano Engine (Intelligent Voice Leading & Pedal)
// 
// Orchestrates comping patterns by analyzing harmonic density over time.
// It features a voice-leading optimizer to minimize hand movement
// and algorithms to calculate realistic sustain pedaling.
// -----------------------------------------------------------------------------

class PianoCompiler : public MIDI::IMIRCompiler {
public:
    PianoCompiler() = default;
    ~PianoCompiler() override = default;
    
    // Implements IMIRCompiler
    MIDI::MIDIStream CompileClip(
        const EditorClip& clip, 
        const std::vector<ChordTrackEvent>& chordTimeline
    ) const override;
    
private:
    // Models the physical distance equation between old chord and new chord
    std::vector<uint8_t> CalculateSmoothVoicing(
        const std::vector<uint8_t>& previousVoicing,
        const ActiveChordContext& targetChord
    ) const;
    
    // Generates CC64 (Sustain Pedal) messages based on bass note persistence
    void SynthesizePedal(
        MIDI::MIDIStream& outStream,
        MusicalTime startTime,
        MusicalTime endTime,
        const std::vector<ChordTrackEvent>& chordTimeline
    ) const;
};

std::unique_ptr<MIDI::IMIRCompiler> CreatePianoEngine();

} // namespace Engines
} // namespace Core
} // namespace Sonatrix
