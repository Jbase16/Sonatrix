#pragma once

#include "../engines/guitar/FretboardModel.h"
#include "IMIRCompiler.h"
#include <vector>

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

  struct NoteTarget {
    int pitch{-1};
    int stringIndex{-1};
    GuitarTargetRole role{GuitarTargetRole::None};
  };

  struct SoundingString {
    int stringIndex{-1};
    int pitch{-1};
  };

  // Helper that adds strings sequentially based on Strum direction
  // (micro-timing dispersion).
  void EmitStrum(MIDIStream &outStream, MusicalTime baseTime,
                 ArticulationType direction, uint8_t baseVelocity,
                 MusicalTime duration,
                 const std::vector<NoteTarget> &stringTargets) const;

  std::vector<NoteTarget>
  ResolveTargetsForEvent(const MIREvent &event,
                         const Sonatrix::Core::Engines::Guitar::GuitarVoicing
                             &voicing,
                         const std::vector<int> &usedFigurePitches,
                         const std::vector<int> &usedFigureStrings) const;

  std::vector<SoundingString> GetSoundingStringsByLane(
      const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing) const;
  int GetLaneString(const std::vector<SoundingString> &soundingStrings,
                    int lane) const;
  int ResolveDefaultLaneString(
      GuitarTargetRole role,
      const std::vector<SoundingString> &soundingStrings) const;
  std::vector<int> GetRoleCandidateStrings(
      GuitarTargetRole role,
      const std::vector<SoundingString> &soundingStrings) const;

  int ResolveBassString(
      const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
      const std::vector<int> &usedFigurePitches = {},
      const std::vector<int> &usedFigureStrings = {}) const;
  int ResolveAltBassString(
      const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
      const std::vector<int> &usedFigurePitches = {},
      const std::vector<int> &usedFigureStrings = {}) const;
  int ResolveInnerLowString(
      const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
      const std::vector<int> &usedFigurePitches = {},
      const std::vector<int> &usedFigureStrings = {}) const;
  int ResolveInnerHighString(
      const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
      const std::vector<int> &usedFigurePitches = {},
      const std::vector<int> &usedFigureStrings = {}) const;
  int ResolveTrebleString(
      const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
      const std::vector<int> &usedFigurePitches = {},
      const std::vector<int> &usedFigureStrings = {}) const;
  int ResolveTopString(
      const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
      const std::vector<int> &usedFigurePitches = {},
      const std::vector<int> &usedFigureStrings = {}) const;
  int ResolveFlexibleRoleString(
      GuitarTargetRole role,
      const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
      const std::vector<int> &usedFigurePitches = {},
      const std::vector<int> &usedFigureStrings = {}) const;

  int ResolveRoleString(
      GuitarTargetRole role,
      const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
      const std::vector<int> &usedFigurePitches = {},
      const std::vector<int> &usedFigureStrings = {}) const;
  const char *GetRoleName(GuitarTargetRole role) const;
  void DebugPrintResolvedEvent(
      MusicalTime absoluteTime, int chordIndex, const MIREvent &event,
      const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
      const std::vector<NoteTarget> &resolvedTargets) const;
};

// Provides instantiation without exposing the class externally
std::unique_ptr<IMIRCompiler> CreateGuitarEngine();

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
