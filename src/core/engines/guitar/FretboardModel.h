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

  int GetMidiPitch(int stringIndex) const;
  float GetAverageFret() const;
  int GetMaxFret() const;
  int GetFretSpan() const;
  int GetNumFrettedNotes() const;
  int GetNumOpenStrings() const;
  int GetNumSoundingStrings() const;

  int GetLowestSoundingString() const;
  int GetLowestMidiPitch() const;
};

class FretboardModel {
public:
  FretboardModel() = default;

  std::vector<GuitarVoicing>
  GenerateValidVoicings(const ActiveChordContext &chord) const;

private:
  static constexpr std::array<uint8_t, 6> STANDARD_TUNING = {40, 45, 50, 55, 59, 64};
  static constexpr int MAX_FRET_SPAN = 4;
  static constexpr int MAX_SEARCH_FRET = 15;

  void PermuteStrings(int stringIndex,
                      std::array<int8_t, 6> currentVoicing,
                      const ActiveChordContext &chord,
                      const std::vector<PitchClass> &requiredPitches,
                      std::vector<GuitarVoicing> &outValidVoicings) const;

  bool IsVoicingHarmonicallyValid(const GuitarVoicing &voicing,
                                  const ActiveChordContext &chord,
                                  const std::vector<PitchClass> &requiredPitches) const;

  std::vector<PitchClass>
  GetRequiredPitches(const ActiveChordContext &chord) const;

  bool ContainsPitchClass(const GuitarVoicing &voicing, PitchClass pc) const;
  int CountPitchClass(const GuitarVoicing &voicing, PitchClass pc) const;
};

} // namespace Guitar
} // namespace Engines
} // namespace Core
} // namespace Sonatrix
