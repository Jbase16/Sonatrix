#pragma once

#include "src/core/midi/IMIRCompiler.h"
#include "PianoVoicingPlanner.h"

#include <memory>

namespace Sonatrix {
namespace Core {
namespace MIDI {

// -----------------------------------------------------------------------------
// Piano Engine (Intelligent Voice Leading & Pedal)
//
// Orchestrates comping patterns by analyzing harmonic density over time.
// It uses PianoVoicingPlanner to resolve an energy-based voice-leading sequence
// before compiling the semantic MIR.
// -----------------------------------------------------------------------------

class PianoCompiler : public IMIRCompiler {
public:
  PianoCompiler();
  ~PianoCompiler() override = default;

  // Implements IMIRCompiler
  MIDIStream CompileClip(
      const EditorClip &clip,
      const std::vector<ChordTrackEvent> &chordTimeline,
      Sonatrix::Core::ML::DynamicGrooveVector *grooveVectorContext = nullptr) const override;
};

std::unique_ptr<IMIRCompiler> CreatePianoEngine();

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
