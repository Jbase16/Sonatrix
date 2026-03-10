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

  // 1. Center of Gravity Shift (Macro-distance the hand travels)
  float aCenter = a.GetAverageFret();
  float bCenter = b.GetAverageFret();
  cost += std::abs(aCenter - bCenter) * 2.5f;

  // 2. Individual Finger Movement & Common Tone Discount
  for (int i = 0; i < 6; ++i) {
    int8_t fA = a.frets[i];
    int8_t fB = b.frets[i];

    if (fA == fB && fA > 0) {
      // "Anchor Finger" Discount - The finger remained pressed down.
      // Subtracting cost heavily rewards holding common chord tones.
      cost -= 5.0f;
    } else if (fA != -1 && fB != -1) {
      // Finger slid or moved
      cost += std::abs(fA - fB) * 1.5f;
    } else if (fA != -1 && fB == -1) {
      // String was muted suddenly
      cost += 1.0f;
    } else if (fA == -1 && fB != -1) {
      // String was newly struck
      cost += 0.5f; // Placing a finger takes slight effort
    }
  }

  // 3. Overall Shape Change Penalty
  // Jumping from a tiny compressed shape to a huge 4-fret stretch is penalized
  cost += std::abs(a.GetFretSpan() - b.GetFretSpan()) * 2.0f;

  return std::max(cost, 0.0f); // Never return negative total costs for graphing
}

std::vector<GuitarVoicing> VoicingGraphSolver::SolveVoiceLeading(
    const std::vector<ChordTrackEvent> &chords) const {
  if (chords.empty())
    return {};

  // Viterbi Trellis Setup
  // trellis[time_t][state_idx] = (cumulative_cost, previous_state_idx)
  struct TrellisNode {
    float minCost = std::numeric_limits<float>::max();
    int parentIdx = -1;
  };

  std::vector<std::vector<TrellisNode>> trellis(chords.size());
  std::vector<std::vector<GuitarVoicing>> states(chords.size());

  // 1. Generate all physically valid states for each timestep T
  for (size_t t = 0; t < chords.size(); ++t) {
    states[t] = fretboard_.GenerateValidVoicings(chords[t].chord);
    trellis[t].resize(states[t].size());

    // If a chord has no valid voicings (should be mathematically impossible in
    // standard tuning) just fallback to an empty set to avoid crash.
    if (states[t].empty()) {
      std::cerr << "Solver Error: No valid voicings found for chord at t=" << t
                << "\n";
      return {};
    }
  }

  // 2. Initialize T=0 costs
  // We favor lower/more open voicings naturally as starting positions,
  // but we heavily penalize thin 3-string voicings so it prefers full chords.
  for (size_t i = 0; i < states[0].size(); ++i) {
    float startingCost = states[0][i].GetAverageFret();
    int missingStrings = 6 - states[0][i].GetNumFrettedNotes();
    startingCost += (missingStrings * 10.0f); // Massive penalty for sparse chords
    trellis[0][i].minCost = startingCost;
  }

  // 3. Viterbi Forward Pass
  for (size_t t = 1; t < chords.size(); ++t) {
    for (size_t i = 0; i < states[t].size(); ++i) {

      float bestPathCost = std::numeric_limits<float>::max();
      int bestParent = -1;

      for (size_t j = 0; j < states[t - 1].size(); ++j) {
        float transitionCost =
            EvaluateTransitionCost(states[t - 1][j], states[t][i]);
        float totalCost = trellis[t - 1][j].minCost + transitionCost;

        if (totalCost < bestPathCost) {
          bestPathCost = totalCost;
          bestParent = static_cast<int>(j);
        }
      }

      trellis[t][i].minCost = bestPathCost;
      trellis[t][i].parentIdx = bestParent;
    }
  }

  // 4. Backtrack the Optimal Path
  std::vector<GuitarVoicing> optimalPath(chords.size());

  // Find the terminal state with the lowest total cumulative cost
  float bestFinalCost = std::numeric_limits<float>::max();
  int bestFinalIdx = 0;

  size_t lastT = chords.size() - 1;
  for (size_t i = 0; i < trellis[lastT].size(); ++i) {
    if (trellis[lastT][i].minCost < bestFinalCost) {
      bestFinalCost = trellis[lastT][i].minCost;
      bestFinalIdx = static_cast<int>(i);
    }
  }

  // Trace backward
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
