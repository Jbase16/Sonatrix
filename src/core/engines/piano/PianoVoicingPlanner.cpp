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

static int GetThirdInterval(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Minor:
        case ChordQuality::Minor7:
        case ChordQuality::Diminished:
        case ChordQuality::HalfDiminished7:
            return 3;
        case ChordQuality::Sus2: return 2;
        case ChordQuality::Sus4: return 5;
        default: return 4;
    }
}

static int GetSeventhInterval(ChordQuality quality) {
    switch (quality) {
        case ChordQuality::Dominant7:
        case ChordQuality::Minor7:
        case ChordQuality::HalfDiminished7:
            return 10;
        case ChordQuality::Major7:
            return 11;
        default: return -1;
    }
}

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

static bool IsGuideTone(const ActiveChordContext& chord, int pc) {
    int root = static_cast<int>(chord.root);
    int thirdPc = (root + GetThirdInterval(chord.quality)) % 12;
    int seventhInterval = GetSeventhInterval(chord.quality);
    if (pc == thirdPc) return true;
    if (seventhInterval >= 0 && pc == (root + seventhInterval) % 12) return true;
    return false;
}

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

static int GetNinthPc(const ActiveChordContext& chord) {
    return (static_cast<int>(chord.root) + 2) % 12;
}

// ---------------------------------------------------------
// Avoid-Tone Logic
// ---------------------------------------------------------
// Certain pitch classes create harsh dissonances in specific
// chord contexts. These are not "wrong notes" — they are
// context-dependent clashes that a competent pianist would avoid.
static bool IsAvoidTone(const ActiveChordContext& chord, int pc) {
    int root = static_cast<int>(chord.root);

    switch (chord.quality) {
        case ChordQuality::Major:
        case ChordQuality::Major7:
            // b9 (minor 2nd from root) clashes with major 3rd
            if (pc == (root + 1) % 12) return true;
            // #11 (tritone from root) clashes in pop context unless Lydian
            if (pc == (root + 6) % 12) return true;
            break;
        case ChordQuality::Dominant7:
            // b9 is sometimes used but generally avoided in pop
            if (pc == (root + 1) % 12) return true;
            break;
        case ChordQuality::Minor:
        case ChordQuality::Minor7:
            // b2 (minor 9th from root) clashes with minor 3rd (Phrygian sound)
            if (pc == (root + 1) % 12) return true;
            // b6 (minor 6th) is context-dependent; avoid in pop, fine in Dorian
            if (pc == (root + 8) % 12) return true;
            break;
        default:
            break;
    }
    return false;
}

// ---------------------------------------------------------
// Constructor
// ---------------------------------------------------------
PianoVoicingPlanner::PianoVoicingPlanner(PianoStyle style, SopranoContour contour)
    : m_style(style), m_contour(contour) {
    switch (style) {
        case PianoStyle::PopBlock:
            m_lhStrategy = LHStrategy::RootFifth;
            m_rhMinPitch = 60;
            m_maxRHSpan = 12;
            m_minVoiceSep = 2;
            m_allowRHInner = true;
            break;
        case PianoStyle::SingerSongwriter:
            m_lhStrategy = LHStrategy::RootTenth;
            m_rhMinPitch = 60;
            m_maxRHSpan = 14;
            m_minVoiceSep = 3;
            m_allowRHInner = true;
            break;
        case PianoStyle::JazzShell:
            m_lhStrategy = LHStrategy::Shell;
            m_rhMinPitch = 55;
            m_maxRHSpan = 12;
            m_minVoiceSep = 2;
            m_allowRHInner = true;
            break;
    }
}

// ---------------------------------------------------------
// Main Entry Point (3-pass)
// ---------------------------------------------------------
std::vector<PianoVoicing> PianoVoicingPlanner::SolveTimeline(const std::vector<ChordTrackEvent>& chordTimeline) const {
    std::vector<PianoVoicing> timeline(chordTimeline.size());
    if (chordTimeline.empty()) return timeline;

    // Pass 1: Plan the soprano trajectory independent of harmony
    auto sopranoPath = PlanSopranoTrajectory(chordTimeline.size());

    // Pass 2: Solve outer voices (snap trajectory to chord tones + LH)
    SolveOuterVoices(chordTimeline, sopranoPath, timeline);

    // Pass 3: Inner voice fill + color tone
    SolveInnerVoices(chordTimeline, timeline);

    return timeline;
}

// ---------------------------------------------------------
// Pass 1: Pre-compute the ideal Soprano trajectory
// ---------------------------------------------------------
// This is the key structural difference from Phase 19.2.
// Instead of choosing chord tones locally, we first define what
// the top-line SHOULD do across the phrase, then constrain
// chord-tone selection to serve that shape.
//
// The trajectory emits a concrete MIDI pitch target per position.
// It does NOT yet know what chord tones are available — that
// happens in Pass 2 when we snap to the nearest valid tone.
std::vector<int> PianoVoicingPlanner::PlanSopranoTrajectory(size_t phraseLen) const {
    std::vector<int> trajectory(phraseLen);

    // Define soprano register endpoints
    constexpr int kSopFloor = 65; // F4
    constexpr int kSopCeil  = 79; // G5
    constexpr int kSopMid   = 72; // C5

    for (size_t i = 0; i < phraseLen; ++i) {
        float t = (phraseLen > 1) ? static_cast<float>(i) / static_cast<float>(phraseLen - 1) : 0.5f;

        float target;
        switch (m_contour) {
            case SopranoContour::Hold:
                target = static_cast<float>(kSopMid);
                break;
            case SopranoContour::Rise:
                // Linear ascent from floor to ceiling
                target = static_cast<float>(kSopFloor) + t * static_cast<float>(kSopCeil - kSopFloor);
                break;
            case SopranoContour::Fall:
                // Linear descent from ceiling to floor
                target = static_cast<float>(kSopCeil) - t * static_cast<float>(kSopCeil - kSopFloor);
                break;
            case SopranoContour::Arch:
                // Parabolic: peaks at midpoint
                // f(t) = floor + (1 - 4*(t-0.5)^2) * range
                target = static_cast<float>(kSopFloor) +
                         (1.0f - 4.0f * (t - 0.5f) * (t - 0.5f)) *
                         static_cast<float>(kSopCeil - kSopFloor);
                break;
        }

        trajectory[i] = static_cast<int>(std::round(target));
    }

    return trajectory;
}

// ---------------------------------------------------------
// Pass 2: Solve Outer Voices (Bass + Soprano → Trajectory)
// ---------------------------------------------------------
void PianoVoicingPlanner::SolveOuterVoices(const std::vector<ChordTrackEvent>& chordTimeline,
                                            const std::vector<int>& sopranoPath,
                                            std::vector<PianoVoicing>& t) const {
    int currentSoprano = sopranoPath[0]; // seed from trajectory

    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        const auto& ctx = chordTimeline[i].chord;
        int root = static_cast<int>(ctx.root);

        // --- LEFT HAND ---
        int bassNote;
        if (!ctx.isRootPosition()) {
            bassNote = 36 + static_cast<int>(ctx.overBass);
        } else {
            bassNote = 36 + root;
        }
        if (bassNote > 52) bassNote -= 12;
        if (bassNote < 36) bassNote += 12;

        t[i].pitches[static_cast<int>(PianoTargetRole::LH_Root)] = static_cast<uint8_t>(bassNote);

        switch (m_lhStrategy) {
            case LHStrategy::RootFifth: {
                if (bassNote >= 38) {
                    t[i].pitches[static_cast<int>(PianoTargetRole::LH_Fifth)] = static_cast<uint8_t>(bassNote + 7);
                }
                break;
            }
            case LHStrategy::RootTenth: {
                t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] =
                    static_cast<uint8_t>(bassNote + 12 + GetThirdInterval(ctx.quality));
                break;
            }
            case LHStrategy::RootOnly:
                break;
            case LHStrategy::Shell: {
                int seventhInterval = GetSeventhInterval(ctx.quality);
                if (seventhInterval >= 0) {
                    t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] =
                        static_cast<uint8_t>(bassNote + seventhInterval);
                } else {
                    t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] =
                        static_cast<uint8_t>(bassNote + GetThirdInterval(ctx.quality));
                }
                break;
            }
        }

        // --- SOPRANO: Two competing forces ---
        // 1. The planned trajectory (where the contour wants us to be)
        // 2. Local smoothness (distance from current position)
        //
        // The trajectory is the INTENT. Smoothness is the CONSTRAINT.
        // We score each candidate against both, with the trajectory
        // weighted more heavily than in Phase 19.2's pure-gravity approach.

        auto rhTones = GetChordTonesInRange(ctx, 65, 79);

        int trajectoryTarget = sopranoPath[i];
        int bestTone = currentSoprano;
        float bestCost = 99999.0f;

        for (int tone : rhTones) {
            // Cost 1: Distance from planned trajectory target
            // This is the primary shaping force. Weight = 0.6
            float trajCost = std::abs(static_cast<float>(tone) - static_cast<float>(trajectoryTarget)) * 0.6f;

            // Cost 2: Distance from current position (smoothness)
            // This prevents wild jumps. Weight = 1.0
            float smoothCost = static_cast<float>(std::abs(tone - currentSoprano));

            float totalCost = trajCost + smoothCost;

            // Common tone reward (holding a pitch is desirable unless the
            // trajectory strongly wants us to move)
            if (tone == currentSoprano) {
                totalCost -= 0.5f;
            }

            // Guide tone preference
            if (IsGuideTone(ctx, tone % 12)) {
                totalCost -= 0.3f;
            }

            // Directional momentum: if the trajectory is asking for movement,
            // reward motion in the right direction
            int trajDelta = trajectoryTarget - currentSoprano;
            int toneDelta = tone - currentSoprano;
            if (trajDelta != 0 && toneDelta != 0) {
                // Same sign = moving in trajectory direction
                bool sameDir = (trajDelta > 0 && toneDelta > 0) || (trajDelta < 0 && toneDelta < 0);
                if (sameDir) {
                    totalCost -= 0.25f;
                }
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
// Pass 3: Inner Voice Fill with Assignment Continuity
// ---------------------------------------------------------
// The key difference from Phase 19.3: inner voices now carry
// their previous assignments forward as soft targets. Instead
// of blindly picking the first valid candidate, we score each
// candidate against both harmonic priority AND proximity to
// the previous chord's same-role pitch.

// Harmonic extension table: maps (style, quality) -> preferred
// color pitch class interval from root. Returns -1 if no
// color tone is appropriate for this context.
static int GetColorToneInterval(PianoStyle style, ChordQuality quality) {
    switch (style) {
        case PianoStyle::PopBlock:
            switch (quality) {
                case ChordQuality::Major:
                case ChordQuality::Major7:
                case ChordQuality::Minor:
                case ChordQuality::Minor7:
                    return 7; // 5th as thickening
                case ChordQuality::Dominant7:
                    return 2; // 9th adds brightness
                case ChordQuality::Sus2:
                case ChordQuality::Sus4:
                case ChordQuality::Diminished:
                case ChordQuality::HalfDiminished7:
                    return -1; // already colorful or too dense
                default:
                    return 7;
            }
        case PianoStyle::SingerSongwriter:
            switch (quality) {
                case ChordQuality::Major:
                case ChordQuality::Major7:
                case ChordQuality::Minor:
                case ChordQuality::Minor7:
                case ChordQuality::Dominant7:
                    return 2; // 9th for shimmer
                case ChordQuality::Sus4:
                    return 2; // 9th for open color
                case ChordQuality::Sus2:
                    return -1; // already has the 2nd
                default:
                    return -1;
            }
        case PianoStyle::JazzShell:
            switch (quality) {
                case ChordQuality::Major7:
                case ChordQuality::Minor:
                case ChordQuality::Major:
                    return 2; // 9th
                case ChordQuality::Minor7:
                case ChordQuality::HalfDiminished7:
                    return 5; // 11th
                case ChordQuality::Dominant7:
                    return 9; // 13th
                default:
                    return -1;
            }
    }
    return -1;
}

void PianoVoicingPlanner::SolveInnerVoices(const std::vector<ChordTrackEvent>& chordTimeline, std::vector<PianoVoicing>& t) const {
    // Previous voice assignments — chord 0 has no history
    int prevGuideHigh = 0;
    int prevGuideLow = 0;
    int prevInner = 0;

    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        int topPitch = t[i].pitches[static_cast<int>(PianoTargetRole::RH_Top)];
        const auto& ctx = chordTimeline[i].chord;

        int searchFloor = std::max(m_rhMinPitch, topPitch - m_maxRHSpan);
        auto candidates = GetChordTonesInRange(ctx, searchFloor, topPitch - 1);

        std::vector<int> usedPcs;
        usedPcs.push_back(topPitch % 12);

        // --- GuideHigh: scored selection ---
        int guideHigh = 0;
        {
            float bestScore = -99999.0f;
            for (int c : candidates) {
                int cpc = c % 12;
                if (std::find(usedPcs.begin(), usedPcs.end(), cpc) != usedPcs.end()) continue;
                if (topPitch - c < m_minVoiceSep) continue;
                if (c < m_rhMinPitch) continue;

                float score = 0.0f;

                // Harmonic priority
                if (IsGuideTone(ctx, cpc)) score += 3.0f;
                else if (IsFifth(ctx, cpc)) score -= 1.0f;
                else score += 1.0f;

                // Proximity to soprano
                score -= static_cast<float>(topPitch - c) * 0.1f;

                // Continuity from previous GuideHigh
                if (prevGuideHigh > 0) {
                    int dist = std::abs(c - prevGuideHigh);
                    if (dist == 0) score += 4.0f;
                    else if (dist == 1) score += 2.5f;
                    else if (dist == 2) score += 1.5f;
                    else score -= static_cast<float>(dist) * 0.3f;
                }

                if (score > bestScore) {
                    bestScore = score;
                    guideHigh = c;
                }
            }
        }

        if (guideHigh > 0) usedPcs.push_back(guideHigh % 12);
        t[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideHigh)] = static_cast<uint8_t>(guideHigh);

        // --- GuideLow: scored selection ---
        int guideLow = 0;
        {
            float bestScore = -99999.0f;
            int lastPlaced = (guideHigh > 0) ? guideHigh : topPitch;

            for (int c : candidates) {
                int cpc = c % 12;
                if (std::find(usedPcs.begin(), usedPcs.end(), cpc) != usedPcs.end()) continue;
                if (lastPlaced - c < m_minVoiceSep) continue;
                if (c < m_rhMinPitch) continue;

                float score = 0.0f;

                if (IsGuideTone(ctx, cpc)) score += 3.0f;
                else if (IsFifth(ctx, cpc)) score -= 1.0f;
                else score += 1.0f;

                score -= static_cast<float>(topPitch - c) * 0.05f;

                // Continuity from previous GuideLow
                if (prevGuideLow > 0) {
                    int dist = std::abs(c - prevGuideLow);
                    if (dist == 0) score += 4.0f;
                    else if (dist == 1) score += 2.5f;
                    else if (dist == 2) score += 1.5f;
                    else score -= static_cast<float>(dist) * 0.3f;
                }

                if (score > bestScore) {
                    bestScore = score;
                    guideLow = c;
                }
            }
        }

        if (guideLow > 0) usedPcs.push_back(guideLow % 12);
        t[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideLow)] = static_cast<uint8_t>(guideLow);

        // --- RH_Inner: Color Tone (extension-table driven) ---
        int innerPitch = 0;

        if (m_allowRHInner) {
            int colorInterval = GetColorToneInterval(m_style, ctx.quality);
            if (colorInterval >= 0) {
                int colorPc = (static_cast<int>(ctx.root) + colorInterval) % 12;

                if (!IsAvoidTone(ctx, colorPc) &&
                    std::find(usedPcs.begin(), usedPcs.end(), colorPc) == usedPcs.end()) {

                    int bestColor = 0;
                    float bestScore = -99999.0f;

                    for (int p = searchFloor; p < topPitch; ++p) {
                        if (p % 12 != colorPc) continue;
                        if (p < m_rhMinPitch) continue;

                        bool tooClose = false;
                        if (std::abs(p - topPitch) < m_minVoiceSep) tooClose = true;
                        if (guideHigh > 0 && std::abs(p - guideHigh) < m_minVoiceSep) tooClose = true;
                        if (guideLow > 0 && std::abs(p - guideLow) < m_minVoiceSep) tooClose = true;
                        if (tooClose) continue;

                        float score = 0.0f;

                        // Center placement
                        int voiceCenter = (topPitch + (guideLow > 0 ? guideLow : guideHigh)) / 2;
                        score -= static_cast<float>(std::abs(p - voiceCenter)) * 0.2f;

                        // Continuity from previous Inner
                        if (prevInner > 0) {
                            int dist = std::abs(p - prevInner);
                            if (dist == 0) score += 4.0f;
                            else if (dist == 1) score += 2.5f;
                            else if (dist == 2) score += 1.5f;
                            else score -= static_cast<float>(dist) * 0.3f;
                        }

                        if (score > bestScore) {
                            bestScore = score;
                            bestColor = p;
                        }
                    }

                    innerPitch = bestColor;
                }
            }
        }

        t[i].pitches[static_cast<int>(PianoTargetRole::RH_Inner)] = static_cast<uint8_t>(innerPitch);

        // Carry forward for next chord's continuity
        prevGuideHigh = guideHigh;
        prevGuideLow = guideLow;
        prevInner = innerPitch;
    }
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix

