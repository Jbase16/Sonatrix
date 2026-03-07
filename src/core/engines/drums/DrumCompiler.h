#pragma once

#include "../../midi/IMIRCompiler.h"

namespace Sonatrix {
namespace Core {
namespace MIDI {

// -----------------------------------------------------------------------------
// Drum Engine
// Compiles abstract beats into General MIDI standard mappings.
// Critically, it records its stochastic (or ML-derived) micro-timings
// into the global DynamicGrooveVector for other engines to follow.
// -----------------------------------------------------------------------------

class DrumCompiler : public IMIRCompiler {
public:
  DrumCompiler() = default;
  ~DrumCompiler() override = default;

  // Implements IMIRCompiler
  MIDIStream CompileClip(const EditorClip &clip,
                         const std::vector<ChordTrackEvent> &chordTimeline,
                         Sonatrix::Core::ML::DynamicGrooveVector
                             *grooveVectorContext = nullptr) const override;
};

// Provides instantiation without exposing the class externally
std::unique_ptr<IMIRCompiler> CreateDrumEngine();

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
