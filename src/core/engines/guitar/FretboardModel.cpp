#include "FretboardModel.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace Sonatrix {
namespace Core {
namespace Engines {
namespace Guitar {

namespace {

inline int Pc(int midi) {
  int v = midi % 12;
  return (v < 0) ? (v + 12) : v;
}

inline PitchClass TransposePitchClass(PitchClass root, int semitones) {
  return static_cast<PitchClass>((static_cast<int>(root) + semitones) % 12);
}

} // namespace

int GuitarVoicing::GetMidiPitch(int stringIndex) const {
  if (stringIndex < 0 || stringIndex > 5 || frets[stringIndex] == -1)
    return -1;

  constexpr std::array<uint8_t, 6> TUNING = {40, 45, 50, 55, 59, 64};
  return TUNING[stringIndex] + frets[stringIndex];
}

float GuitarVoicing::GetAverageFret() const {
  int sum = 0;
  int count = 0;

  for (int8_t f : frets) {
    if (f > 0) {
      sum += f;
      ++count;
    }
  }

  return (count > 0) ? static_cast<float>(sum) / static_cast<float>(count) : 0.0f;
}

int GuitarVoicing::GetFretSpan() const {
  int minFret = 99;
  int maxFret = -1;

  for (int8_t f : frets) {
    if (f > 0) {
      minFret = std::min(minFret, static_cast<int>(f));
      maxFret = std::max(maxFret, static_cast<int>(f));
    }
  }

  if (maxFret == -1)
    return 0;

  return maxFret - minFret;
}

int GuitarVoicing::GetNumFrettedNotes() const {
  int count = 0;
  for (int8_t f : frets) {
    if (f > 0)
      ++count;
  }
  return count;
}

int GuitarVoicing::GetNumSoundingStrings() const {
  int count = 0;
  for (int i = 0; i < 6; ++i) {
    if (GetMidiPitch(i) != -1)
      ++count;
  }
  return count;
}

int GuitarVoicing::GetLowestSoundingString() const {
  for (int i = 0; i < 6; ++i) {
    if (GetMidiPitch(i) != -1)
      return i;
  }
  return -1;
}

int GuitarVoicing::GetLowestMidiPitch() const {
  const int idx = GetLowestSoundingString();
  return (idx >= 0) ? GetMidiPitch(idx) : -1;
}

std::vector<GuitarVoicing>
FretboardModel::GenerateValidVoicings(const ActiveChordContext &chord) const {
  std::vector<GuitarVoicing> results;
  const std::vector<PitchClass> requiredPitches = GetRequiredPitches(chord);

  std::array<int8_t, 6> initialVoicing = {-1, -1, -1, -1, -1, -1};
  PermuteStrings(0, initialVoicing, chord, requiredPitches, results);

  return results;
}

std::vector<PitchClass>
FretboardModel::GetRequiredPitches(const ActiveChordContext &chord) const {
  std::vector<PitchClass> pitches;
  pitches.push_back(chord.root);

  // Rename enum labels here if your ChordQuality names differ.
  switch (chord.quality) {
  case ChordQuality::Major:
    pitches.push_back(TransposePitchClass(chord.root, 4)); // major 3rd
    pitches.push_back(TransposePitchClass(chord.root, 7)); // perfect 5th
    break;

  case ChordQuality::Minor:
    pitches.push_back(TransposePitchClass(chord.root, 3)); // minor 3rd
    pitches.push_back(TransposePitchClass(chord.root, 7)); // perfect 5th
    break;

  case ChordQuality::Minor7:
    pitches.push_back(TransposePitchClass(chord.root, 3));  // minor 3rd
    pitches.push_back(TransposePitchClass(chord.root, 7));  // perfect 5th
    pitches.push_back(TransposePitchClass(chord.root, 10)); // minor 7th
    break;

  case ChordQuality::Major7:
    pitches.push_back(TransposePitchClass(chord.root, 4));  // major 3rd
    pitches.push_back(TransposePitchClass(chord.root, 7));  // perfect 5th
    pitches.push_back(TransposePitchClass(chord.root, 11)); // major 7th
    break;

  case ChordQuality::Dominant7:
    pitches.push_back(TransposePitchClass(chord.root, 4));  // major 3rd
    pitches.push_back(TransposePitchClass(chord.root, 7));  // perfect 5th
    pitches.push_back(TransposePitchClass(chord.root, 10)); // minor 7th
    break;

  case ChordQuality::Sus2:
    pitches.push_back(TransposePitchClass(chord.root, 2)); // major 2nd
    pitches.push_back(TransposePitchClass(chord.root, 7)); // perfect 5th
    break;

  case ChordQuality::Sus4:
    pitches.push_back(TransposePitchClass(chord.root, 5)); // perfect 4th
    pitches.push_back(TransposePitchClass(chord.root, 7)); // perfect 5th
    break;

  case ChordQuality::Add9:
    pitches.push_back(TransposePitchClass(chord.root, 4)); // major 3rd
    pitches.push_back(TransposePitchClass(chord.root, 7)); // perfect 5th
    pitches.push_back(TransposePitchClass(chord.root, 2)); // add9
    break;

  default:
    // Safe fallback: assume plain major triad.
    pitches.push_back(TransposePitchClass(chord.root, 4));
    pitches.push_back(TransposePitchClass(chord.root, 7));
    break;
  }

  // Deduplicate, because root/add9 etc can collide in weird edge cases.
  std::sort(pitches.begin(), pitches.end(),
            [](PitchClass a, PitchClass b) {
              return static_cast<int>(a) < static_cast<int>(b);
            });
  pitches.erase(std::unique(pitches.begin(), pitches.end(),
                            [](PitchClass a, PitchClass b) {
                              return static_cast<int>(a) == static_cast<int>(b);
                            }),
                pitches.end());

  return pitches;
}

bool FretboardModel::ContainsPitchClass(const GuitarVoicing &voicing,
                                        PitchClass pc) const {
  const int target = static_cast<int>(pc);

  for (int i = 0; i < 6; ++i) {
    const int midi = voicing.GetMidiPitch(i);
    if (midi != -1 && Pc(midi) == target)
      return true;
  }

  return false;
}

int FretboardModel::CountPitchClass(const GuitarVoicing &voicing,
                                    PitchClass pc) const {
  int count = 0;
  const int target = static_cast<int>(pc);

  for (int i = 0; i < 6; ++i) {
    const int midi = voicing.GetMidiPitch(i);
    if (midi != -1 && Pc(midi) == target)
      ++count;
  }

  return count;
}

bool FretboardModel::IsVoicingHarmonicallyValid(
    const GuitarVoicing &voicing,
    const ActiveChordContext &chord,
    const std::vector<PitchClass> &requiredPitches) const {

  const int sounding = voicing.GetNumSoundingStrings();
  if (sounding < 3)
    return false;

  if (voicing.GetFretSpan() > MAX_FRET_SPAN)
    return false;

  // Must contain every required pitch class.
  for (PitchClass pc : requiredPitches) {
    if (!ContainsPitchClass(voicing, pc))
      return false;
  }

  // Root should almost always be present at least once.
  if (!ContainsPitchClass(voicing, chord.root))
    return false;

  // Bass enforcement:
  // - slash/inversion chord: lowest note must equal overBass
  // - root position chord: lowest note must equal root
  const int lowestMidi = voicing.GetLowestMidiPitch();
  if (lowestMidi == -1)
    return false;

  const PitchClass expectedBass =
      chord.isRootPosition() ? chord.root : chord.overBass;

  if (Pc(lowestMidi) != static_cast<int>(expectedBass))
    return false;

  // Avoid pathological voicings that technically contain the chord
  // but are mostly duplicate junk with one token color tone.
  //
  // For 4-note chords, require at least 4 sounding strings.
  if (requiredPitches.size() >= 4 && sounding < 4)
    return false;

  // Optional sanity bias:
  // if this is an Add9 chord, don't allow the 9 to appear only as the lowest note.
  if (chord.quality == ChordQuality::Add9) {
    const PitchClass nine = TransposePitchClass(chord.root, 2);
    if (CountPitchClass(voicing, nine) < 1)
      return false;
  }

  return true;
}

void FretboardModel::PermuteStrings(
    int stringIndex,
    std::array<int8_t, 6> currentVoicing,
    const ActiveChordContext &chord,
    const std::vector<PitchClass> &requiredPitches,
    std::vector<GuitarVoicing> &outValidVoicings) const {

  if (stringIndex == 6) {
    GuitarVoicing v;
    v.frets = currentVoicing;

    if (IsVoicingHarmonicallyValid(v, chord, requiredPitches)) {
      outValidVoicings.push_back(v);
    }
    return;
  }

  // Branch 1: mute this string
  currentVoicing[stringIndex] = -1;
  PermuteStrings(stringIndex + 1, currentVoicing, chord, requiredPitches,
                 outValidVoicings);

  // Branch 2: try every fret on this string that matches a required pitch class
  const int stringBaseMidi = STANDARD_TUNING[stringIndex];

  for (int fret = 0; fret <= MAX_SEARCH_FRET; ++fret) {
    const int midi = stringBaseMidi + fret;
    const int pc = Pc(midi);

    bool matchesRequired = false;
    for (PitchClass target : requiredPitches) {
      if (pc == static_cast<int>(target)) {
        matchesRequired = true;
        break;
      }
    }

    if (!matchesRequired)
      continue;

    currentVoicing[stringIndex] = static_cast<int8_t>(fret);

    // Early physical pruning
    GuitarVoicing temp;
    temp.frets = currentVoicing;
    if (temp.GetFretSpan() <= MAX_FRET_SPAN) {
      PermuteStrings(stringIndex + 1, currentVoicing, chord, requiredPitches,
                     outValidVoicings);
    }
  }
}

} // namespace Guitar
} // namespace Engines
} // namespace Core
} // namespace Sonatrix
