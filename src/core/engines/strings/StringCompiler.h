#pragma once

#include "../../midi/IMIRCompiler.h"
#include <memory>

namespace Sonatrix {
namespace Core {
namespace Engines {

// -----------------------------------------------------------------------------
// String Engine (Dynamic Divisi Allocation)
//
// Allocates voices across orchestral string section lines.
// It clamps ranges explicitly (e.g. Violins top notes, Cellos bottom).
// It features predictive dynamic swells.
// -----------------------------------------------------------------------------

class StringCompiler : public MIDI::IMIRCompiler {
public:
  StringCompiler() = default;
  ~StringCompiler() override = default;

  MIDI::MIDIStream
  CompileClip(const EditorClip &clip,
              const std::vector<ChordTrackEvent> &chordTimeline,
              Sonatrix::Core::ML::DynamicGrooveVector *grooveVectorContext =
                  nullptr) const override;

private:
  // Maps a dense block chord into sectionally-appropriate divisi (Vln1, Vln2,
  // Vla, Vc, Cb)
  std::vector<std::pair<uint8_t, uint8_t>>
  AllocateDivisi(const ActiveChordContext &targetChord) const;

  // Pre-calculates CC11 (Expression) swells reading ahead to the next chord
  // transition
  void CalculatePredictiveExpressionCurve(MIDI::MIDIStream &outStream,
                                          MusicalTime chordStartTime,
                                          MusicalTime chordEndTime) const;
};

std::unique_ptr<MIDI::IMIRCompiler> CreateStringEngine();

} // namespace Engines
} // namespace Core
} // namespace Sonatrix
