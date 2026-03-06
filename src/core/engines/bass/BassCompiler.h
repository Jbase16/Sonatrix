#pragma once

#include "../../midi/IMIRCompiler.h"
#include "../../ml/DynamicGrooveVector.h"
#include <memory>

namespace Sonatrix {
namespace Core {
namespace Engines {

// -----------------------------------------------------------------------------
// Bass Engine 
// Responsible for generating bass guitar MIDI from MIR patterns.
// Critically, it reads the Dynamic Groove Vector populated by the Drums
// to phase-lock its timing.
// -----------------------------------------------------------------------------

class BassCompiler : public MIDI::IMIRCompiler {
public:
    // Takes a const reference to the global vector to READ timing deviations
    explicit BassCompiler(const ML::DynamicGrooveVector& globalGrooveVector);
    ~BassCompiler() override = default;
    
    // Implements IMIRCompiler
    MIDI::MIDIStream CompileClip(
        const EditorClip& clip, 
        const std::vector<ChordTrackEvent>& chordTimeline
    ) const override;
    
private:
    const ML::DynamicGrooveVector& grooveVector_;
    
    // Simplistic heuristic to find the active chord root
    uint8_t GetBassPitchForTime(MusicalTime time, const std::vector<ChordTrackEvent>& chordTimeline) const;
};

std::unique_ptr<MIDI::IMIRCompiler> CreateBassEngine(const ML::DynamicGrooveVector& vector);

} // namespace Engines
} // namespace Core
} // namespace Sonatrix
