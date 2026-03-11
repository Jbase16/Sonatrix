#pragma once

#include "FretboardModel.h"
#include "../../mir/MIRPattern.h"
#include <vector>

namespace Sonatrix {
namespace Core {
namespace Engines {
namespace Guitar {

// -----------------------------------------------------------------------------
// VoicingGraphSolver
// Implementation of an A* / Viterbi Pathfinding algorithm that discovers the
// globally optimal sequence of GuitarVoicings across a song timeline.
// -----------------------------------------------------------------------------
class VoicingGraphSolver {
public:
  VoicingGraphSolver() = default;

  // Evaluates a sequence of harmonic chords and returns the optimal
  // physical fretboard sequence.
  std::vector<GuitarVoicing>
  SolveVoiceLeading(
      const std::vector<ChordTrackEvent> &chords,
      Sonatrix::Core::GuitarVoicingMode voicingMode =
          Sonatrix::Core::GuitarVoicingMode::Default) const;

private:
  FretboardModel fretboard_;

  // Evaluates the heuristic physical cost of moving the hand from Voicing A to
  // Voicing B. Lower means less physical strain / faster transition.
  float EvaluateTransitionCost(const GuitarVoicing &a,
                               const GuitarVoicing &b,
                               Sonatrix::Core::GuitarVoicingMode voicingMode) const;
  float EvaluateVoicingPreferenceCost(
      const GuitarVoicing &voicing, const ActiveChordContext &chord,
      Sonatrix::Core::GuitarVoicingMode voicingMode) const;
};

} // namespace Guitar
} // namespace Engines
} // namespace Core
} // namespace Sonatrix
