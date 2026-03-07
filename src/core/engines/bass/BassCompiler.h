#pragma once

#include "../../midi/IMIRCompiler.h"

namespace Sonatrix {
namespace Core {
namespace MIDI {

// -----------------------------------------------------------------------------
// Bass Engine
// Translates MIR rhythmic templates into harmonic basslines based on the
// ChordTrack. Crucially, it queries the DynamicGrooveVector to phase-lock its
// generated notes squarely into the Drummer's microscopic pocket.
// -----------------------------------------------------------------------------

class BassCompiler : public IMIRCompiler {
public:
  BassCompiler() = default;
  ~BassCompiler() override = default;

  // Implements IMIRCompiler
  MIDIStream CompileClip(const EditorClip &clip,
                         const std::vector<ChordTrackEvent> &chordTimeline,
                         Sonatrix::Core::ML::DynamicGrooveVector
                             *grooveVectorContext = nullptr) const override;
};

// Provides instantiation without exposing the class externally
std::unique_ptr<IMIRCompiler> CreateBassEngine();

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
