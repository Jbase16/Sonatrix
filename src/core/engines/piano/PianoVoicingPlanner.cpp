#include "PianoVoicingPlanner.h"
#include <iostream>

namespace Sonatrix {
namespace Core {
namespace MIDI {

PianoVoicingPlanner::PianoVoicingPlanner() {
}

bool PianoVoicingPlanner::SolveTimeline(const std::vector<ChordTrackEvent>& chordTimeline) {
    m_candidatesPerChord.clear();
    m_solvedTimeline.clear();

    if (chordTimeline.empty()) return false;

    // Mock pass: just generate basic candidates and select the first one.
    // The Viterbi algorithm will be implemented here safely in isolation.
    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        std::vector<PianoVoicing> candidates;
        GenerateCandidates(chordTimeline[i], candidates);
        m_candidatesPerChord.push_back(candidates);
        
        if (!candidates.empty()) {
            m_solvedTimeline.push_back(candidates[0]); // Naive selection for now
        } else {
            m_solvedTimeline.push_back(PianoVoicing()); // Empty
        }
    }

    return true;
}

PianoVoicing PianoVoicingPlanner::GetVoicingForChordIndex(size_t index) const {
    if (index < m_solvedTimeline.size()) {
        return m_solvedTimeline[index];
    }
    return PianoVoicing();
}

void PianoVoicingPlanner::GenerateCandidates(const ChordTrackEvent& event, std::vector<PianoVoicing>& outVoicings) const {
    // Phase 19: Combinatorial Voice Generation
    // We will generate multiple inversions and spans here.

    // Safe Mock Default
    PianoVoicing v;
    int rootPitch = 36 + static_cast<int>(event.chord.root);
    
    v.pitches[static_cast<int>(PianoTargetRole::LH_Root)] = rootPitch;
    v.pitches[static_cast<int>(PianoTargetRole::LH_Fifth)] = rootPitch + 7;
    v.pitches[static_cast<int>(PianoTargetRole::RH_GuideLow)] = rootPitch + 16;   // 3rd
    v.pitches[static_cast<int>(PianoTargetRole::RH_GuideHigh)] = rootPitch + 19;  // 5th
    v.pitches[static_cast<int>(PianoTargetRole::RH_Top)] = rootPitch + 24;        // Octave

    outVoicings.push_back(v);
}

float PianoVoicingPlanner::CalculateTransitionCost(const PianoVoicing& from, const PianoVoicing& to) const {
    // Phase 19: Transition Energy
    // Evaluate common tones, jump distance, and hand spread.
    return 1.0f;
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
