#include "PianoVoicingPlanner.h"
#include <iostream>

namespace Sonatrix {
namespace Core {
namespace MIDI {

PianoVoicingPlanner::PianoVoicingPlanner() {
}

bool PianoVoicingPlanner::SolveTimeline(const std::vector<ChordTrackEvent>& chordTimeline) {
    m_solvedTimeline.clear();
    if (chordTimeline.empty()) return false;

    // Pre-allocate the timeline slots
    m_solvedTimeline.resize(chordTimeline.size());

    // Phase A: Structural Skeleton
    SolveOuterVoices(chordTimeline);

    // Phase B: Harmonic Fill
    SolveInnerVoices(chordTimeline);

    return true;
}

PianoVoicing PianoVoicingPlanner::GetVoicingForChordIndex(size_t index) const {
    if (index < m_solvedTimeline.size()) {
        return m_solvedTimeline[index];
    }
    return PianoVoicing();
}

void PianoVoicingPlanner::SolveOuterVoices(const std::vector<ChordTrackEvent>& chordTimeline) {
    // 1. Establish Bass trajectory
    // 2. Establish Soprano trajectory (RH_Top)
    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        int rootPitch = 36 + static_cast<int>(chordTimeline[i].chord.root);
        m_solvedTimeline[i].pitches[static_cast<int>(PianoTargetRole::LH_Root)]   = rootPitch;
        m_solvedTimeline[i].pitches[static_cast<int>(PianoTargetRole::LH_Fifth)]  = rootPitch + 7;
        
        // Mock static soprano line around C5 (72)
        m_solvedTimeline[i].pitches[static_cast<int>(PianoTargetRole::RH_Top)]    = 72;
    }
}

void PianoVoicingPlanner::SolveInnerVoices(const std::vector<ChordTrackEvent>& chordTimeline) {
    // 1. Lock to Guide Tones (3rds/7ths) between Bass and Soprano bounds.
    // 2. Minimize intervallic jumps using Neo-Riemannian heuristics.
    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        int rootPitch = 36 + static_cast<int>(chordTimeline[i].chord.root);
        m_solvedTimeline[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideLow)]  = rootPitch + 16;
        m_solvedTimeline[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideHigh)] = rootPitch + 19;
    }
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
