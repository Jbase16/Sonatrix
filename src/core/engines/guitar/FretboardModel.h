#pragma once

#include "../../arrangement/ChordTrack.h"
#include <array>
#include <cstdint>
#include <vector>

namespace Sonatrix {
namespace Core {
namespace Engines {
namespace Guitar {

// Represents a 6-string guitar fingering configuration.
// Index 0: Low E, Index 5: High E.
// A value of -1 means the string is Muted/Not Played.
// Values 0-24 represent literal fret numbers.
struct GuitarVoicing {
  std::array<int8_t, 6> frets{-1, -1, -1, -1, -1, -1};

  // Calculates the absolute MIDI pitch of a specific string given this
  // voicing's fret. Returns -1 if the string is muted.
  int GetMidiPitch(int stringIndex) const;

  // Physical cost metrics used by the A* heuristic
  float GetAverageFret() const;
  int GetFretSpan() const; // Max fret - Min fret (excluding open strings 0)
  int GetNumFrettedNotes() const; // Count of strings > 0
};

class FretboardModel {
public:
  FretboardModel() = default;

  // Top-level combinatorial generator.
  // Yields every physically possible hand-shape on the 6-string guitar
  // that satisfies the requested harmonic structure.
  std::vector<GuitarVoicing>
  GenerateValidVoicings(const ActiveChordContext &chord) const;

private:
  // Standard Guitar Tuning MIDI Pitches [E2, A2, D3, G3, B3, E4]
  static constexpr std::array<uint8_t, 6> STANDARD_TUNING = {40, 45, 50,
                                                             55, 59, 64};

  // Max physical finger stretch constraint
  static constexpr int MAX_FRET_SPAN = 4;

  // Recursive graph-search to build fretboard states string by string
  void PermuteStrings(int stringIndex, std::array<int8_t, 6> currentVoicing,
                      const std::vector<PitchClass> &targetPitches,
                      std::vector<GuitarVoicing> &outValidVoicings) const;

  // Converts a PitchClass (e.g., C) to an absolute MIDI pitch on a specific
  // string near a target fret area. Returns -1 if physically unreachable.
  int GetFretForPitchClass(PitchClass targetClass, int stringIndex,
                           int anchorFret) const;

  // Validates a completed 6-string permutation against the actual requested
  // chord structure (e.g., ensuring it contains at least a root, third, and
  // fifth, and the bass note is correct).
  bool IsVoicingHarmonicallyValid(const GuitarVoicing &voicing,
                                  const ActiveChordContext &chord) const;

  // Helpers
  std::vector<PitchClass>
  GetRequiredPitches(const ActiveChordContext &chord) const;
};

} // namespace Guitar
} // namespace Engines
} // namespace Core
} // namespace Sonatrix
