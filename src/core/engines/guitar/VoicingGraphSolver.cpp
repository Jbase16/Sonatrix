#include "VoicingGraphSolver.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace Sonatrix {
namespace Core {
namespace Engines {
namespace Guitar {

float VoicingGraphSolver::EvaluateTransitionCost(const GuitarVoicing &a,
                                                 const GuitarVoicing &b) const {
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
  cost += static_cast<float>(missingStrings) * 10.0f;

  return std::max(cost, 0.0f);
}

std::vector<GuitarVoicing> VoicingGraphSolver::SolveVoiceLeading(
    const std::vector<ChordTrackEvent> &chords) const {
  if (chords.empty()) {
    return {};
  }

  struct TrellisNode {
    float minCost = std::numeric_limits<float>::max();
    int parentIdx = -1;
  };

  std::vector<std::vector<TrellisNode>> trellis(chords.size());
  std::vector<std::vector<GuitarVoicing>> states(chords.size());

  // 1. Generate valid voicing states for each chord
  for (size_t t = 0; t < chords.size(); ++t) {
    states[t] = fretboard_.GenerateValidVoicings(chords[t].chord);
    trellis[t].resize(states[t].size());

    if (states[t].empty()) {
      std::cerr << "Solver Error: No valid voicings found for chord at t=" << t
                << "\n";
      return {};
    }
  }

  // 2. Initialize starting costs
  for (size_t i = 0; i < states[0].size(); ++i) {
    float startingCost = states[0][i].GetAverageFret();

    // Penalize sparse voicings based on sounding strings, not fretted notes
    const int missingStrings = 6 - states[0][i].GetNumSoundingStrings();
    startingCost += static_cast<float>(missingStrings) * 10.0f;

    trellis[0][i].minCost = startingCost;
  }

  // 3. Forward pass
  for (size_t t = 1; t < chords.size(); ++t) {
    for (size_t i = 0; i < states[t].size(); ++i) {
      float bestPathCost = std::numeric_limits<float>::max();
      int bestParent = -1;

      for (size_t j = 0; j < states[t - 1].size(); ++j) {
        const float transitionCost =
            EvaluateTransitionCost(states[t - 1][j], states[t][i]);
        const float totalCost = trellis[t - 1][j].minCost + transitionCost;

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