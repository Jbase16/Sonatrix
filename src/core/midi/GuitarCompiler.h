#pragma once

#include "../engines/guitar/FretboardModel.h"
#include "IMIRCompiler.h"

namespace Sonatrix {
namespace Core {
namespace MIDI {

// -----------------------------------------------------------------------------
// Guitar Engine (Constraint-Based Solver)
// Translates MIR strum commands into exact 6-string realizations.
// -----------------------------------------------------------------------------

class GuitarCompiler : public IMIRCompiler {
public:
  GuitarCompiler() = default;
  ~GuitarCompiler() override = default;

  MIDIStream CompileClip(const EditorClip &clip,
                         const std::vector<ChordTrackEvent> &chordTimeline,
                         Sonatrix::Core::ML::DynamicGrooveVector
                             *grooveVectorContext = nullptr) const override;

private:
  // We removed the old EvaluateVoiceLeadingCost function as the Viterbi Graph
  // Solver handles this natively.

  // Helper that adds strings sequentially based on Strum direction
  // (micro-timing dispersion)
  void EmitStrum(
      MIDIStream &outStream, MusicalTime baseTime, ArticulationType direction,
      uint8_t baseVelocity,
      const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing) const;
};

// Provides instantiation without exposing the class externally
std::unique_ptr<IMIRCompiler> CreateGuitarEngine();

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
