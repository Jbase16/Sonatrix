#pragma once

#include "../../midi/IMIRCompiler.h"
#include "../../ml/DynamicGrooveVector.h"
#include <memory>

namespace Sonatrix {
namespace Core {
namespace Engines {

// -----------------------------------------------------------------------------
// Drum Engine 
// Translates MIR drum hits into MIDI.
// Critically, it acts as the master timing source, populating the global
// DynamicGrooveVector so other engines can phase-lock to its feel.
// -----------------------------------------------------------------------------

class DrumCompiler : public MIDI::IMIRCompiler {
public:
    // Takes a reference to the global vector it must populate
    explicit DrumCompiler(ML::DynamicGrooveVector& globalGrooveVector);
    ~DrumCompiler() override = default;
    
    // Implements IMIRCompiler
    MIDI::MIDIStream CompileClip(
        const EditorClip& clip, 
        const std::vector<ChordTrackEvent>& chordTimeline
    ) const override;
    
private:
    ML::DynamicGrooveVector& grooveVector_;
    
    // Mock algorithm representing PhD-level Latent Groove Extraction
    // In production, this would query a CoreML inference session.
    int64_t ExtractHumanDeviation(MusicalTime absoluteTime, uint8_t baseVelocity) const;
};

std::unique_ptr<MIDI::IMIRCompiler> CreateDrumEngine(ML::DynamicGrooveVector& vector);

} // namespace Engines
} // namespace Core
} // namespace Sonatrix
