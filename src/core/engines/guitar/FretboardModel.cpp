#include "FretboardModel.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace Sonatrix {
namespace Core {
namespace Engines {
namespace Guitar {

int GuitarVoicing::GetMidiPitch(int stringIndex) const {
  if (stringIndex < 0 || stringIndex > 5 || frets[stringIndex] == -1)
    return -1;
  constexpr std::array<uint8_t, 6> TUNING = {40, 45, 50,
                                             55, 59, 64}; // E A D G B e
  return TUNING[stringIndex] + frets[stringIndex];
}

float GuitarVoicing::GetAverageFret() const {
  int sum = 0;
  int count = 0;
  for (int8_t f : frets) {
    if (f > 0) { // Don't average open strings or muted
      sum += f;
      count++;
    }
  }
  return count > 0 ? static_cast<float>(sum) / count : 0.0f;
}

int GuitarVoicing::GetFretSpan() const {
  int minFret = 99;
  int maxFret = -1;
  for (int8_t f : frets) {
    if (f > 0) {
      if (f < minFret)
        minFret = f;
      if (f > maxFret)
        maxFret = f;
    }
  }
  if (maxFret == -1)
    return 0; // All open or muted
  return maxFret - minFret;
}

int GuitarVoicing::GetNumFrettedNotes() const {
  int count = 0;
  for (int8_t f : frets) {
    if (f > 0)
      count++;
  }
  return count;
}

int GuitarVoicing::GetNumSoundingStrings() const {
  int count = 0;
  for (int i = 0; i < 6; ++i) {
    if (GetMidiPitch(i) != -1) {
      ++count;
    }
  }
  return count;
}

std::vector<GuitarVoicing>
FretboardModel::GenerateValidVoicings(const ActiveChordContext &chord) const {
  std::vector<GuitarVoicing> results;
  std::vector<PitchClass> requiredPitches = GetRequiredPitches(chord);

  // 1. Determine anchor points.
  // To avoid searching a massive 24^6 combinatorial space, we constrain the
  // search to "Positions" on the neck. A guitarist usually anchors their index
  // finger at a fret (0-15).

  for (int anchorFret = 0; anchorFret <= 15; ++anchorFret) {
    std::array<int8_t, 6> initialVoicing = {-1, -1, -1, -1, -1, -1};
    PermuteStrings(0, initialVoicing, requiredPitches, results);
  }

  // Note: The above anchor loop simplifies the true solver.
  // Instead of looping anchors here, we can simply let PermuteStrings iterate
  // strictly bounded frets per string, then throw out ones with Span >
  // MAX_FRET_SPAN. The implementation below uses the direct pruning method.

  results.clear();
  std::array<int8_t, 6> initialVoicing = {-1, -1, -1, -1, -1, -1};
  PermuteStrings(0, initialVoicing, requiredPitches, results);

  return results;
}

// Generates the core triads/sevenths requested by the Chord Quality
std::vector<PitchClass>
FretboardModel::GetRequiredPitches(const ActiveChordContext &chord) const {
  std::vector<PitchClass> pitches;

  int rootNode = static_cast<int>(chord.root);
  pitches.push_back(chord.root);

  switch (chord.quality) {
  case ChordQuality::Major:
    pitches.push_back(
        static_cast<PitchClass>((rootNode + 4) % 12)); // Major 3rd
    pitches.push_back(
        static_cast<PitchClass>((rootNode + 7) % 12)); // Perfect 5th
    break;
  case ChordQuality::Minor:
    pitches.push_back(
        static_cast<PitchClass>((rootNode + 3) % 12)); // Minor 3rd
    pitches.push_back(
        static_cast<PitchClass>((rootNode + 7) % 12)); // Perfect 5th
    break;
  default:
    // Mocking other qualities for now.
    pitches.push_back(static_cast<PitchClass>((rootNode + 4) % 12));
    pitches.push_back(static_cast<PitchClass>((rootNode + 7) % 12));
    break;
  }

  // Enforce inversion bass note if required (e.g. C/E)
  if (!chord.isRootPosition()) {
    pitches.push_back(chord.overBass);
  }

  return pitches;
}

void FretboardModel::PermuteStrings(
    int stringIndex, std::array<int8_t, 6> currentVoicing,
    const std::vector<PitchClass> &targetPitches,
    std::vector<GuitarVoicing> &outValidVoicings) const {
  // Base Case: All 6 strings evaluated
  if (stringIndex == 6) {
    GuitarVoicing v;
    v.frets = currentVoicing;

    // Final Physical Pruning (Is my hand big enough?)
    if (v.GetFretSpan() <= MAX_FRET_SPAN && v.GetNumFrettedNotes() <= 6) {

      // Final Harmonic Pruning: Enforce all required pitches are present
      std::vector<int> presentPCs;
      for (int i = 0; i < 6; ++i) {
          if (v.frets[i] != -1) {
              presentPCs.push_back(v.GetMidiPitch(i) % 12);
          }
      }
      
      bool containsAll = true;
      for (PitchClass pc : targetPitches) {
          if (std::find(presentPCs.begin(), presentPCs.end(), static_cast<int>(pc)) == presentPCs.end()) {
              containsAll = false;
              break;
          }
      }

      if (containsAll) {
        outValidVoicings.push_back(v);
      }
    }
    return;
  }

  // Recursive Branch 1: Mute this string and move to the next
  currentVoicing[stringIndex] = -1;
  PermuteStrings(stringIndex + 1, currentVoicing, targetPitches,
                 outValidVoicings);

  // Recursive Branch 2...N: Try playing valid pitches on this string
  for (PitchClass target : targetPitches) {
    // Find every instance of this pitch class on this specific string
    // Guitars have repeating pitch classes across the neck (e.g., E is fret 0,
    // 12, 24)
    int stringBaseMidi = STANDARD_TUNING[stringIndex];
    int targetPcOffset = static_cast<int>(target);

    for (int fret = 0; fret <= 15;
         ++fret) { // Limit search to 15th fret for practicality
      int currentMidi = stringBaseMidi + fret;
      if ((currentMidi % 12) == targetPcOffset) {
        currentVoicing[stringIndex] = fret;

        // Early Physical Pruning: If adding this fret exceeds span relative to
        // existing frets, abort branch
        GuitarVoicing tempV;
        tempV.frets = currentVoicing;
        if (tempV.GetFretSpan() <= MAX_FRET_SPAN) {
          PermuteStrings(stringIndex + 1, currentVoicing, targetPitches,
                         outValidVoicings);
        }
      }
    }
  }
}

} // namespace Guitar
} // namespace Engines
} // namespace Core
} // namespace Sonatrix
