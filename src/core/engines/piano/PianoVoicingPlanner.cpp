#include "PianoVoicingPlanner.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

namespace Sonatrix {
namespace Core {
namespace MIDI {

// ---------------------------------------------------------
// Harmonic Core Helpers
// ---------------------------------------------------------

// Resolves absolute pitch classes (0-11) required by the chord
static std::vector<int> GetPitchClasses(const ActiveChordContext& chord) {
    std::vector<int> pcs;
    int root = static_cast<int>(chord.root);
    pcs.push_back(root);

    int third = -1;
    int fifth = (root + 7) % 12;
    int seventh = -1;

    switch (chord.quality) {
        case ChordQuality::Major: third = (root + 4) % 12; break;
        case ChordQuality::Minor: third = (root + 3) % 12; break;
        case ChordQuality::Diminished: third = (root + 3) % 12; fifth = (root + 6) % 12; break;
        case ChordQuality::Augmented: third = (root + 4) % 12; fifth = (root + 8) % 12; break;
        case ChordQuality::Dominant7: third = (root + 4) % 12; seventh = (root + 10) % 12; break;
        case ChordQuality::Major7: third = (root + 4) % 12; seventh = (root + 11) % 12; break;
        case ChordQuality::Minor7: third = (root + 3) % 12; seventh = (root + 10) % 12; break;
        case ChordQuality::HalfDiminished7: third = (root + 3) % 12; fifth = (root + 6) % 12; seventh = (root + 10) % 12; break;
        case ChordQuality::Sus2: third = (root + 2) % 12; break;
        case ChordQuality::Sus4: third = (root + 5) % 12; break;
        case ChordQuality::Add9: third = (root + 4) % 12; break; // simplistic for now
        case ChordQuality::PowerChord: break;
        default: third = (root + 4) % 12; break; 
    }

    if (third != -1) pcs.push_back(third);
    pcs.push_back(fifth);
    if (seventh != -1) pcs.push_back(seventh);

    std::sort(pcs.begin(), pcs.end());
    pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());
    return pcs;
}

// Emits physical MIDI values (0-127) within the specified register boundaries
static std::vector<int> GetChordTonesInRange(const ActiveChordContext& chord, int minPitch, int maxPitch) {
    auto pcs = GetPitchClasses(chord);
    std::vector<int> tones;
    for (int p = minPitch; p <= maxPitch; ++p) {
        if (std::find(pcs.begin(), pcs.end(), p % 12) != pcs.end()) {
            tones.push_back(p);
        }
    }
    return tones;
}

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
    int currentSoprano = 72; // Default starting gravity center (C5)

    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        const auto& ctx = chordTimeline[i].chord;
        
        // 1. Establish Bass Trajectory (Macro Bottom)
        int rootPitch = 36 + static_cast<int>(ctx.root); // Start C2
        if (rootPitch > 45) rootPitch -= 12; // Keep LH bass tight between E1(40) to A2(45) roughly

        m_solvedTimeline[i].pitches[static_cast<int>(PianoTargetRole::LH_Root)]   = rootPitch;
        m_solvedTimeline[i].pitches[static_cast<int>(PianoTargetRole::LH_Fifth)]  = rootPitch + 7;
        
        // 2. Establish Soprano Trajectory (Macro Top / RH_Top)
        // Bounding pop melody range between F4 (65) and G5 (79)
        auto rhTones = GetChordTonesInRange(ctx, 65, 79); 
        
        int bestTone = currentSoprano;
        float bestCost = 99999.0f;
        
        for (int t : rhTones) {
            // Evaluates smoothness and phrasing. 
            // Avoids jumping unless pulled by gravity to the register center (72).
            float distCost = std::abs(t - currentSoprano);
            float gravityCost = std::abs(t - 72) * 0.1f;
            float totalCost = distCost + gravityCost;
            
            // Explicitly reward common tones (zero distance) to build sustained counter-lines
            if (distCost == 0.0f) {
                totalCost -= 0.5f; 
            }
            
            if (totalCost < bestCost) {
                bestCost = totalCost;
                bestTone = t;
            }
        }
        
        m_solvedTimeline[i].pitches[static_cast<int>(PianoTargetRole::RH_Top)] = static_cast<uint8_t>(bestTone);
        currentSoprano = bestTone; // Track momentum for next chord boundary
    }
}

void PianoVoicingPlanner::SolveInnerVoices(const std::vector<ChordTrackEvent>& chordTimeline) {
    // Meso Pass: Structurally "hang" the inner guide tones below the solved Soprano boundary.
    // Because the Soprano boundary moves smoothly, the inner voices will automatically 
    // exhibit Neo-Riemannian voice-leading without complex matrix searching.
    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        int topPitch = m_solvedTimeline[i].pitches[static_cast<int>(PianoTargetRole::RH_Top)];
        const auto& ctx = chordTimeline[i].chord;
        
        // Search downwards from the note immediately beneath the Soprano.
        auto allTones = GetChordTonesInRange(ctx, 48, topPitch - 1); 
        
        int guideHigh = 0;
        int guideLow = 0;
        
        if (allTones.size() >= 2) {
            guideHigh = allTones[allTones.size() - 1]; // First chord tone below top
            guideLow = allTones[allTones.size() - 2];  // Second chord tone below top
        } else if (allTones.size() == 1) {
            guideHigh = allTones[0];
        }
        
        m_solvedTimeline[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideHigh)] = static_cast<uint8_t>(guideHigh);
        m_solvedTimeline[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideLow)]  = static_cast<uint8_t>(guideLow);
    }
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
