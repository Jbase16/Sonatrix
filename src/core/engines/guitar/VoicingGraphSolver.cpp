#include "VoicingGraphSolver.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace Sonatrix {
namespace Core {
namespace Engines {
namespace Guitar {

namespace {

struct KnownAcousticShape {
  PitchClass root;
  ChordQuality quality;
  std::array<int8_t, 6> frets;
};

constexpr std::array<KnownAcousticShape, 11> kKnownAcousticShapes = {{
    {PitchClass::G, ChordQuality::Major, {3, 2, 0, 0, 3, 3}},
    {PitchClass::G, ChordQuality::Major, {3, 2, 0, 0, 0, 3}},
    {PitchClass::E, ChordQuality::Minor, {0, 2, 2, 0, 0, 0}},
    {PitchClass::E, ChordQuality::Minor7, {0, 2, 2, 0, 3, 3}},
    {PitchClass::E, ChordQuality::Minor7, {0, 2, 2, 0, 3, 0}},
    {PitchClass::C, ChordQuality::Major, {-1, 3, 2, 0, 1, 0}},
    {PitchClass::C, ChordQuality::Add9, {-1, 3, 2, 0, 3, 3}},
    {PitchClass::D, ChordQuality::Major, {-1, -1, 0, 2, 3, 2}},
    {PitchClass::A, ChordQuality::Minor, {-1, 0, 2, 2, 1, 0}},
    {PitchClass::D, ChordQuality::Minor, {-1, -1, 0, 2, 3, 1}},
    {PitchClass::F, ChordQuality::Major, {1, 3, 3, 2, 1, 1}},
}};

bool IsBasicAcousticChordQuality(ChordQuality quality) {
  switch (quality) {
  case ChordQuality::Major:
  case ChordQuality::Minor:
  case ChordQuality::Minor7:
  case ChordQuality::Add9:
  case ChordQuality::Sus2:
  case ChordQuality::Sus4:
    return true;
  default:
    return false;
  }
}

bool MatchesKnownAcousticShape(const GuitarVoicing &voicing,
                               const ActiveChordContext &chord) {
  if (!chord.isRootPosition()) {
    return false;
  }

  return std::any_of(
      kKnownAcousticShapes.begin(), kKnownAcousticShapes.end(),
      [&](const KnownAcousticShape &shape) {
        return shape.root == chord.root && shape.quality == chord.quality &&
               shape.frets == voicing.frets;
      });
}

bool HasKnownAcousticEquivalent(const ActiveChordContext &chord) {
  if (!chord.isRootPosition()) {
    return false;
  }

  return std::any_of(
      kKnownAcousticShapes.begin(), kKnownAcousticShapes.end(),
      [&](const KnownAcousticShape &shape) {
        return shape.root == chord.root && shape.quality == chord.quality;
      });
}

std::vector<std::array<int8_t, 6>>
GetAcousticShapeFamilyVariants(const ActiveChordContext &chord) {
  std::vector<std::array<int8_t, 6>> variants;
  if (!chord.isRootPosition()) {
    return variants;
  }

  for (const auto &shape : kKnownAcousticShapes) {
    if (shape.root == chord.root && shape.quality == chord.quality) {
      variants.push_back(shape.frets);
    }
  }

  return variants;
}

int ExpectedBassString(const ActiveChordContext &chord) {
  const PitchClass bass = chord.isRootPosition() ? chord.root : chord.overBass;
  switch (bass) {
  case PitchClass::E:
  case PitchClass::F:
  case PitchClass::F_Sharp:
  case PitchClass::G:
  case PitchClass::G_Sharp:
    return 0;
  case PitchClass::A:
  case PitchClass::A_Sharp:
  case PitchClass::B:
  case PitchClass::C:
  case PitchClass::C_Sharp:
    return 1;
  case PitchClass::D:
  case PitchClass::D_Sharp:
  default:
    return 2;
  }
}

int CountRepeatedUpperMidiPitches(const GuitarVoicing &voicing) {
  std::vector<int> pitches;
  pitches.reserve(6);

  for (int stringIndex = 0; stringIndex < 6; ++stringIndex) {
    const int midi = voicing.GetMidiPitch(stringIndex);
    if (midi != -1) {
      pitches.push_back(midi);
    }
  }

  if (pitches.size() < 3) {
    return 0;
  }

  std::sort(pitches.begin(), pitches.end());

  int duplicates = 0;
  for (size_t i = 2; i < pitches.size(); ++i) {
    if (pitches[i] == pitches[i - 1]) {
      ++duplicates;
    }
  }

  return duplicates;
}

bool LooksLikeFullBarre(const GuitarVoicing &voicing) {
  if (voicing.GetNumOpenStrings() > 0) {
    return false;
  }

  std::array<int, 25> fretCounts{};
  for (int8_t fret : voicing.frets) {
    if (fret > 0 && fret < static_cast<int8_t>(fretCounts.size())) {
      ++fretCounts[static_cast<size_t>(fret)];
    }
  }

  return std::any_of(fretCounts.begin() + 2, fretCounts.end(),
                     [](int count) { return count >= 4; });
}

} // namespace

float VoicingGraphSolver::EvaluateTransitionCost(
    const GuitarVoicing &a, const GuitarVoicing &b,
    Sonatrix::Core::GuitarVoicingMode voicingMode) const {
  float cost = 0.0f;

  // 1. Center of Gravity Shift (macro hand movement)
  const float aCenter = a.GetAverageFret();
  const float bCenter = b.GetAverageFret();
  cost += std::abs(aCenter - bCenter) * 2.5f;

  // 2. Individual finger movement / common-tone reward
  for (int i = 0; i < 6; ++i) {
    const int8_t fA = a.frets[i];
    const int8_t fB = b.frets[i];

    if (fA == fB && fA > 0) {
      // Reward held fretted common tones
      cost -= 5.0f;
    } else if (voicingMode == Sonatrix::Core::GuitarVoicingMode::AcousticOpen &&
               fA == 0 && fB == 0) {
      // Open-string common tones are especially valuable in acoustic patterns.
      cost -= 3.0f;
    } else if (fA != -1 && fB != -1) {
      // Finger moved on a sounding string
      cost += std::abs(fA - fB) * 1.5f;
    } else if (fA != -1 && fB == -1) {
      // A previously sounding string became muted
      cost += 1.0f;
    } else if (fA == -1 && fB != -1) {
      // A new sounding string was added
      cost += 0.5f;
    }
  }

  // 3. Overall shape change penalty
  cost += std::abs(a.GetFretSpan() - b.GetFretSpan()) * 2.0f;

  // 4. Sparsity penalty
  // IMPORTANT: penalize missing SOUNDING STRINGS, not missing fretted notes.
  // Open strings count as full, legitimate guitar voicing content.
  const int missingStrings = 6 - b.GetNumSoundingStrings();
  const float missingStringPenalty =
      (voicingMode == Sonatrix::Core::GuitarVoicingMode::AcousticOpen) ? 2.0f
                                                                        : 10.0f;
  cost += static_cast<float>(missingStrings) * missingStringPenalty;

  return std::max(cost, 0.0f);
}

float VoicingGraphSolver::EvaluateVoicingPreferenceCost(
    const GuitarVoicing &voicing, const ActiveChordContext &chord,
    Sonatrix::Core::GuitarVoicingMode voicingMode) const {
  if (voicingMode != Sonatrix::Core::GuitarVoicingMode::AcousticOpen) {
    return 0.0f;
  }

  const float averageFret = voicing.GetAverageFret();
  const int maxFret = voicing.GetMaxFret();
  const int openStrings = voicing.GetNumOpenStrings();
  const int soundingStrings = voicing.GetNumSoundingStrings();
  const bool hasKnownOpenEquivalent = HasKnownAcousticEquivalent(chord);

  float cost = 0.0f;
  cost += averageFret * 1.5f;
  cost += static_cast<float>(std::max(0, maxFret - 5)) * 6.0f;
  cost += static_cast<float>(std::max(0, maxFret - 7)) * 12.0f;
  cost += std::max(0.0f, averageFret - 5.0f) * 10.0f;

  cost -= static_cast<float>(openStrings) * 5.0f;
  if (openStrings == 0) {
    cost += hasKnownOpenEquivalent ? 18.0f : 8.0f;
  }
  if (openStrings >= 2) {
    cost -= 6.0f;
  }

  if (maxFret <= 5) {
    cost -= 10.0f;
  }
  if (averageFret <= 3.0f) {
    cost -= 6.0f;
  }

  if (IsBasicAcousticChordQuality(chord.quality) && averageFret > 5.0f) {
    cost += 12.0f;
  }
  if (IsBasicAcousticChordQuality(chord.quality) && maxFret > 7) {
    cost += 14.0f;
  }

  cost += static_cast<float>(CountRepeatedUpperMidiPitches(voicing)) * 6.0f;

  if (LooksLikeFullBarre(voicing) && hasKnownOpenEquivalent) {
    cost += 10.0f;
  }

  const int expectedBassString = ExpectedBassString(chord);
  const int actualBassString = voicing.GetLowestSoundingString();
  if (actualBassString == expectedBassString) {
    cost -= 4.0f;
  }

  if (MatchesKnownAcousticShape(voicing, chord)) {
    cost -= 30.0f;
  }

  if (soundingStrings < 4) {
    cost += 6.0f;
  }

  return cost;
}

std::vector<GuitarVoicing> VoicingGraphSolver::ResolveAcousticShapeFamilyCandidates(
    const ActiveChordContext &chord,
    const std::vector<GuitarVoicing> &candidates) const {
  const auto familyVariants = GetAcousticShapeFamilyVariants(chord);
  if (familyVariants.empty()) {
    return {};
  }

  std::vector<GuitarVoicing> familyCandidates;
  familyCandidates.reserve(candidates.size());

  for (const auto &candidate : candidates) {
    const bool inFamily = std::any_of(
        familyVariants.begin(), familyVariants.end(),
        [&](const std::array<int8_t, 6> &variantFrets) {
          return candidate.frets == variantFrets;
        });
    if (inFamily) {
      familyCandidates.push_back(candidate);
    }
  }

  return familyCandidates;
}

std::vector<GuitarVoicing> VoicingGraphSolver::SolveVoiceLeading(
    const std::vector<ChordTrackEvent> &chords,
    Sonatrix::Core::GuitarVoicingMode voicingMode) const {
  if (chords.empty()) {
    return {};
  }

  struct TrellisNode {
    float minCost = std::numeric_limits<float>::max();
    int parentIdx = -1;
  };

  std::vector<std::vector<TrellisNode>> trellis(chords.size());
  std::vector<std::vector<GuitarVoicing>> states(chords.size());
  std::vector<bool> usedFamilyRestrictedCandidates(chords.size(), false);

  // 1. Generate valid voicing states for each chord
  for (size_t t = 0; t < chords.size(); ++t) {
    auto generatedCandidates = fretboard_.GenerateValidVoicings(chords[t].chord);
    if (voicingMode == Sonatrix::Core::GuitarVoicingMode::AcousticOpen) {
      auto familyCandidates =
          ResolveAcousticShapeFamilyCandidates(chords[t].chord, generatedCandidates);
      if (!familyCandidates.empty()) {
        states[t] = std::move(familyCandidates);
        usedFamilyRestrictedCandidates[t] = true;
      } else {
        states[t] = std::move(generatedCandidates);
      }
    } else {
      states[t] = std::move(generatedCandidates);
    }
    trellis[t].resize(states[t].size());

    if (states[t].empty()) {
      std::cerr << "Solver Error: No valid voicings found for chord at t=" << t
                << "\n";
      return {};
    }
  }

  // 2. Initialize starting costs
  for (size_t i = 0; i < states[0].size(); ++i) {
    float startingCost = 0.0f;
    if (!usedFamilyRestrictedCandidates[0]) {
      startingCost = states[0][i].GetAverageFret();

      // Penalize sparse voicings based on sounding strings, not fretted notes
      const int missingStrings = 6 - states[0][i].GetNumSoundingStrings();
      const float missingStringPenalty =
          (voicingMode == Sonatrix::Core::GuitarVoicingMode::AcousticOpen) ? 2.0f
                                                                            : 10.0f;
      startingCost += static_cast<float>(missingStrings) * missingStringPenalty;
      startingCost +=
          EvaluateVoicingPreferenceCost(states[0][i], chords[0].chord, voicingMode);
    }

    trellis[0][i].minCost = startingCost;
  }

  // 3. Forward pass
  for (size_t t = 1; t < chords.size(); ++t) {
    for (size_t i = 0; i < states[t].size(); ++i) {
      float bestPathCost = std::numeric_limits<float>::max();
      int bestParent = -1;

      for (size_t j = 0; j < states[t - 1].size(); ++j) {
        const float transitionCost =
            EvaluateTransitionCost(states[t - 1][j], states[t][i], voicingMode);
        const float voicingPreferenceCost = usedFamilyRestrictedCandidates[t]
                                                ? 0.0f
                                                : EvaluateVoicingPreferenceCost(
                                                      states[t][i], chords[t].chord,
                                                      voicingMode);
        const float totalCost =
            trellis[t - 1][j].minCost + transitionCost + voicingPreferenceCost;

        if (totalCost < bestPathCost) {
          bestPathCost = totalCost;
          bestParent = static_cast<int>(j);
        }
      }

      trellis[t][i].minCost = bestPathCost;
      trellis[t][i].parentIdx = bestParent;
    }
  }

  // 4. Backtrack
  std::vector<GuitarVoicing> optimalPath(chords.size());

  float bestFinalCost = std::numeric_limits<float>::max();
  int bestFinalIdx = 0;
  const size_t lastT = chords.size() - 1;

  for (size_t i = 0; i < trellis[lastT].size(); ++i) {
    if (trellis[lastT][i].minCost < bestFinalCost) {
      bestFinalCost = trellis[lastT][i].minCost;
      bestFinalIdx = static_cast<int>(i);
    }
  }

  int currentIdx = bestFinalIdx;
  for (int t = static_cast<int>(lastT); t >= 0; --t) {
    optimalPath[t] = states[t][currentIdx];
    currentIdx = trellis[t][currentIdx].parentIdx;
  }

  return optimalPath;
}

} // namespace Guitar
} // namespace Engines
} // namespace Core
} // namespace Sonatrix
