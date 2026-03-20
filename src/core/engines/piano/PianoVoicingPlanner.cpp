#include "PianoVoicingPlanner.h"
#include <vector>
#include <algorithm>
#include <cmath>

namespace Sonatrix {
namespace Core {
namespace MIDI {

// ---------------------------------------------------------
// Harmonic Core Helpers
// ---------------------------------------------------------

// Resolves pitch classes (0-11) required by the chord quality
static std::vector<int> GetPitchClasses(const ActiveChordContext& chord) {
    std::vector<int> pcs;
    int root = static_cast<int>(chord.root);
    pcs.push_back(root);

    int third = -1;
    int fifth = (root + 7) % 12;
    int seventh = -1;

    switch (chord.quality) {
        case ChordQuality::Major:       third = (root + 4) % 12; break;
        case ChordQuality::Minor:       third = (root + 3) % 12; break;
        case ChordQuality::Diminished:  third = (root + 3) % 12; fifth = (root + 6) % 12; break;
        case ChordQuality::Augmented:   third = (root + 4) % 12; fifth = (root + 8) % 12; break;
        case ChordQuality::Dominant7:   third = (root + 4) % 12; seventh = (root + 10) % 12; break;
        case ChordQuality::Major7:      third = (root + 4) % 12; seventh = (root + 11) % 12; break;
        case ChordQuality::Minor7:      third = (root + 3) % 12; seventh = (root + 10) % 12; break;
        case ChordQuality::HalfDiminished7: third = (root + 3) % 12; fifth = (root + 6) % 12; seventh = (root + 10) % 12; break;
        case ChordQuality::Sus2:        third = (root + 2) % 12; break;
        case ChordQuality::Sus4:        third = (root + 5) % 12; break;
        case ChordQuality::Add9:        third = (root + 4) % 12; break;
        case ChordQuality::PowerChord:  break;
        default:                        third = (root + 4) % 12; break;
    }

    if (third != -1) pcs.push_back(third);
    pcs.push_back(fifth);
    if (seventh != -1) pcs.push_back(seventh);

    std::sort(pcs.begin(), pcs.end());
    pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());
    return pcs;
}

// Returns the interval from root to the 3rd for a given chord quality
static int GetThirdInterval(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Minor:
        case ChordQuality::Minor7:
        case ChordQuality::Diminished:
        case ChordQuality::HalfDiminished7:
            return 3; // minor 3rd
        case ChordQuality::Sus2:
            return 2;
        case ChordQuality::Sus4:
            return 5;
        default:
            return 4; // major 3rd
    }
}

// Returns the interval from root to the 7th, or -1 if no 7th
static int GetSeventhInterval(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Dominant7:
        case ChordQuality::Minor7:
        case ChordQuality::HalfDiminished7:
            return 10; // minor 7th
        case ChordQuality::Major7:
            return 11; // major 7th
        default:
            return -1; // no 7th
    }
}

// Emits physical MIDI note numbers within [minPitch, maxPitch]
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

// Returns true if pc is the 3rd or 7th of the chord (guide tones)
static bool IsGuideTone(const ActiveChordContext& chord, int pc) {
    int root = static_cast<int>(chord.root);
    int thirdPc = (root + GetThirdInterval(chord.quality)) % 12;
    int seventhInterval = GetSeventhInterval(chord.quality);
    if (pc == thirdPc) return true;
    if (seventhInterval >= 0 && pc == (root + seventhInterval) % 12) return true;
    return false;
}

// Returns true if pc is the 5th of the chord
static bool IsFifth(const ActiveChordContext& chord, int pc) {
    int root = static_cast<int>(chord.root);
    int fifthPc;
    switch (chord.quality) {
        case ChordQuality::Diminished:
        case ChordQuality::HalfDiminished7:
            fifthPc = (root + 6) % 12; break;
        case ChordQuality::Augmented:
            fifthPc = (root + 8) % 12; break;
        default:
            fifthPc = (root + 7) % 12; break;
    }
    return pc == fifthPc;
}

// ---------------------------------------------------------
// Constructor: Derive constraints from style
// ---------------------------------------------------------
PianoVoicingPlanner::PianoVoicingPlanner(PianoStyle style)
    : m_style(style) {
    switch (style) {
        case PianoStyle::PopBlock:
            m_lhStrategy = LHStrategy::RootFifth;
            m_rhMinPitch = 60;   // C4: no RH below middle C in pop
            m_maxRHSpan = 12;    // octave max
            m_minVoiceSep = 2;   // no minor 2nd clusters
            break;
        case PianoStyle::SingerSongwriter:
            m_lhStrategy = LHStrategy::RootTenth;
            m_rhMinPitch = 60;
            m_maxRHSpan = 14;    // slightly wider for open voicings
            m_minVoiceSep = 3;   // wider spacing for open feel
            break;
        case PianoStyle::JazzShell:
            m_lhStrategy = LHStrategy::Shell;
            m_rhMinPitch = 55;   // G3: jazz allows lower RH for rootless comping
            m_maxRHSpan = 12;
            m_minVoiceSep = 2;
            break;
    }
}

// ---------------------------------------------------------
// Main Entry Point
// ---------------------------------------------------------
std::vector<PianoVoicing> PianoVoicingPlanner::SolveTimeline(const std::vector<ChordTrackEvent>& chordTimeline) const {
    std::vector<PianoVoicing> timeline(chordTimeline.size());
    if (chordTimeline.empty()) return timeline;

    SolveOuterVoices(chordTimeline, timeline);
    SolveInnerVoices(chordTimeline, timeline);

    return timeline;
}

// ---------------------------------------------------------
// Phase A: Macro Pass (Outer Shell — Bass + Soprano)
// ---------------------------------------------------------
void PianoVoicingPlanner::SolveOuterVoices(const std::vector<ChordTrackEvent>& chordTimeline, std::vector<PianoVoicing>& t) const {
    int currentSoprano = 72; // C5 gravity center

    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        const auto& ctx = chordTimeline[i].chord;
        int root = static_cast<int>(ctx.root);

        // --- LEFT HAND ---
        // Anchor bass pitch in C2(36)..E3(52) range
        int bassPitch = 36 + root;
        if (bassPitch > 52) bassPitch -= 12;
        if (bassPitch < 36) bassPitch += 12;

        t[i].pitches[static_cast<int>(PianoTargetRole::LH_Root)] = static_cast<uint8_t>(bassPitch);

        switch (m_lhStrategy) {
            case LHStrategy::RootFifth: {
                int fifthPitch = bassPitch + 7;
                // Mud suppression: if root is below D2(38), skip the 5th
                // because root + P5 in that range creates low-frequency interference
                if (bassPitch >= 38) {
                    t[i].pitches[static_cast<int>(PianoTargetRole::LH_Fifth)] = static_cast<uint8_t>(fifthPitch);
                }
                break;
            }
            case LHStrategy::RootTenth: {
                // Root + 3rd one octave up = 10th interval
                int tenthPitch = bassPitch + 12 + GetThirdInterval(ctx.quality);
                t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] = static_cast<uint8_t>(tenthPitch);
                break;
            }
            case LHStrategy::RootOnly: {
                // Just the root, nothing else
                break;
            }
            case LHStrategy::Shell: {
                // Root + 7th (or 3rd if no 7th)
                int seventhInterval = GetSeventhInterval(ctx.quality);
                if (seventhInterval >= 0) {
                    int shellPitch = bassPitch + seventhInterval;
                    t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] = static_cast<uint8_t>(shellPitch);
                } else {
                    int thirdPitch = bassPitch + GetThirdInterval(ctx.quality);
                    t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] = static_cast<uint8_t>(thirdPitch);
                }
                break;
            }
        }

        // --- SOPRANO (RH_Top) ---
        // Search for the smoothest chord tone in the soprano register
        auto rhTones = GetChordTonesInRange(ctx, 65, 79); // F4 to G5

        int bestTone = currentSoprano;
        float bestCost = 99999.0f;

        for (int tone : rhTones) {
            float distCost = static_cast<float>(std::abs(tone - currentSoprano));
            float gravityCost = static_cast<float>(std::abs(tone - 72)) * 0.1f;
            float totalCost = distCost + gravityCost;

            // Common tone reward: holding a pitch is musically desirable
            if (distCost == 0.0f) {
                totalCost -= 0.5f;
            }

            // Prefer guide tones (3rd/7th) in the soprano for harmonic clarity
            if (IsGuideTone(ctx, tone % 12)) {
                totalCost -= 0.3f;
            }

            if (totalCost < bestCost) {
                bestCost = totalCost;
                bestTone = tone;
            }
        }

        t[i].pitches[static_cast<int>(PianoTargetRole::RH_Top)] = static_cast<uint8_t>(bestTone);
        currentSoprano = bestTone;
    }
}

// ---------------------------------------------------------
// Phase B: Meso Pass (Inner Voice Fill with Constraints)
// ---------------------------------------------------------
void PianoVoicingPlanner::SolveInnerVoices(const std::vector<ChordTrackEvent>& chordTimeline, std::vector<PianoVoicing>& t) const {
    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        int topPitch = t[i].pitches[static_cast<int>(PianoTargetRole::RH_Top)];
        const auto& ctx = chordTimeline[i].chord;
        int topPc = topPitch % 12;

        // Collect all chord tones in the valid RH register, below the soprano
        int searchFloor = std::max(m_rhMinPitch, topPitch - m_maxRHSpan);
        auto candidates = GetChordTonesInRange(ctx, searchFloor, topPitch - 1);

        // Sort candidates by priority:
        // 1. Guide tones (3rd/7th) first
        // 2. Non-fifth chord tones
        // 3. 5th last (most expendable)
        std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
            bool aGuide = IsGuideTone(ctx, a % 12);
            bool bGuide = IsGuideTone(ctx, b % 12);
            if (aGuide != bGuide) return aGuide; // guide tones first

            bool aFifth = IsFifth(ctx, a % 12);
            bool bFifth = IsFifth(ctx, b % 12);
            if (aFifth != bFifth) return !aFifth; // fifths last

            // Among equals, prefer higher pitch (closer to soprano)
            return a > b;
        });

        // Select inner voices with constraint enforcement
        int guideHigh = 0;
        int guideLow = 0;
        int lastPlaced = topPitch;

        for (int candidate : candidates) {
            // Constraint 1: Duplicate suppression — skip if same pitch class as soprano
            if (candidate % 12 == topPc) continue;

            // Constraint 2: Already placed a voice with this pitch class
            if (guideHigh != 0 && candidate % 12 == guideHigh % 12) continue;

            // Constraint 3: Minimum voice separation from last placed voice
            if (lastPlaced - candidate < m_minVoiceSep) continue;

            // Constraint 4: Muddy register gate
            if (candidate < m_rhMinPitch) continue;

            // Place the voice
            if (guideHigh == 0) {
                guideHigh = candidate;
                lastPlaced = candidate;
            } else if (guideLow == 0) {
                guideLow = candidate;
                lastPlaced = candidate;
                break; // Two inner voices are sufficient for v1
            }
        }

        t[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideHigh)] = static_cast<uint8_t>(guideHigh);
        t[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideLow)]  = static_cast<uint8_t>(guideLow);
    }
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
