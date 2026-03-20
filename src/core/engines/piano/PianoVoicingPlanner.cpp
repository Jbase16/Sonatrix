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

// Returns the 9th pitch class for color-tone usage
static int GetNinthPc(const ActiveChordContext& chord) {
    int root = static_cast<int>(chord.root);
    return (root + 2) % 12; // major 9th by default
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
            m_allowRHInner = true;  // 5th as color
            break;
        case PianoStyle::SingerSongwriter:
            m_lhStrategy = LHStrategy::RootTenth;
            m_rhMinPitch = 60;
            m_maxRHSpan = 14;
            m_minVoiceSep = 3;
            m_allowRHInner = true;  // 9th as color
            break;
        case PianoStyle::JazzShell:
            m_lhStrategy = LHStrategy::Shell;
            m_rhMinPitch = 55;
            m_maxRHSpan = 12;
            m_minVoiceSep = 2;
            m_allowRHInner = true;  // tension as color
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
    size_t phraseLen = chordTimeline.size();

    for (size_t i = 0; i < phraseLen; ++i) {
        const auto& ctx = chordTimeline[i].chord;
        int root = static_cast<int>(ctx.root);

        // --- LEFT HAND ---
        // Slash chord support: if overBass differs from root, use overBass as LH note
        int bassNote;
        if (!ctx.isRootPosition()) {
            bassNote = 36 + static_cast<int>(ctx.overBass);
        } else {
            bassNote = 36 + root;
        }
        // Clamp to C2(36)..E3(52)
        if (bassNote > 52) bassNote -= 12;
        if (bassNote < 36) bassNote += 12;

        t[i].pitches[static_cast<int>(PianoTargetRole::LH_Root)] = static_cast<uint8_t>(bassNote);

        switch (m_lhStrategy) {
            case LHStrategy::RootFifth: {
                int fifthPitch = bassNote + 7;
                if (bassNote >= 38) { // mud suppression
                    t[i].pitches[static_cast<int>(PianoTargetRole::LH_Fifth)] = static_cast<uint8_t>(fifthPitch);
                }
                break;
            }
            case LHStrategy::RootTenth: {
                int tenthPitch = bassNote + 12 + GetThirdInterval(ctx.quality);
                t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] = static_cast<uint8_t>(tenthPitch);
                break;
            }
            case LHStrategy::RootOnly:
                break;
            case LHStrategy::Shell: {
                int seventhInterval = GetSeventhInterval(ctx.quality);
                if (seventhInterval >= 0) {
                    t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] = static_cast<uint8_t>(bassNote + seventhInterval);
                } else {
                    t[i].pitches[static_cast<int>(PianoTargetRole::LH_ShellLow)] = static_cast<uint8_t>(bassNote + GetThirdInterval(ctx.quality));
                }
                break;
            }
        }

        // --- SOPRANO (RH_Top) with Contour Intent ---
        auto rhTones = GetChordTonesInRange(ctx, 65, 79); // F4 to G5

        // Calculate phrase position as normalized 0.0..1.0
        float phrasePos = (phraseLen > 1) ? static_cast<float>(i) / static_cast<float>(phraseLen - 1) : 0.5f;

        // Contour bias: shifts the gravity center based on phrase position
        float gravityCenter = 72.0f; // default C5
        switch (m_contour) {
            case SopranoContour::Hold:
                // No bias shift — pure smoothness
                break;
            case SopranoContour::Rise:
                // Gravity center rises from F4(65) to G5(79) across the phrase
                gravityCenter = 65.0f + phrasePos * 14.0f;
                break;
            case SopranoContour::Fall:
                // Gravity center falls from G5(79) to F4(65)
                gravityCenter = 79.0f - phrasePos * 14.0f;
                break;
            case SopranoContour::Arch:
                // Rise to apex at midpoint, then fall
                // Parabolic: peaks at 0.5
                gravityCenter = 65.0f + (1.0f - 4.0f * (phrasePos - 0.5f) * (phrasePos - 0.5f)) * 14.0f;
                break;
        }

        int bestTone = currentSoprano;
        float bestCost = 99999.0f;

        for (int tone : rhTones) {
            float distCost = static_cast<float>(std::abs(tone - currentSoprano));
            float gravityCost = std::abs(static_cast<float>(tone) - gravityCenter) * 0.15f;
            float totalCost = distCost + gravityCost;

            // Common tone reward
            if (distCost == 0.0f) {
                totalCost -= 0.5f;
            }

            // Guide tone preference
            if (IsGuideTone(ctx, tone % 12)) {
                totalCost -= 0.3f;
            }

            // Contour direction momentum
            if (m_contour == SopranoContour::Rise && tone > currentSoprano) {
                totalCost -= 0.2f; // reward upward motion
            } else if (m_contour == SopranoContour::Fall && tone < currentSoprano) {
                totalCost -= 0.2f; // reward downward motion
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
// Phase B: Meso Pass (Inner Voice Fill + RH_Inner Color)
// ---------------------------------------------------------
void PianoVoicingPlanner::SolveInnerVoices(const std::vector<ChordTrackEvent>& chordTimeline, std::vector<PianoVoicing>& t) const {
    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        int topPitch = t[i].pitches[static_cast<int>(PianoTargetRole::RH_Top)];
        const auto& ctx = chordTimeline[i].chord;
        int topPc = topPitch % 12;

        int searchFloor = std::max(m_rhMinPitch, topPitch - m_maxRHSpan);
        auto candidates = GetChordTonesInRange(ctx, searchFloor, topPitch - 1);

        // Priority sort: guide tones first, root second, 5th last
        std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
            bool aGuide = IsGuideTone(ctx, a % 12);
            bool bGuide = IsGuideTone(ctx, b % 12);
            if (aGuide != bGuide) return aGuide;

            bool aFifth = IsFifth(ctx, a % 12);
            bool bFifth = IsFifth(ctx, b % 12);
            if (aFifth != bFifth) return !aFifth;

            return a > b; // higher = closer to soprano
        });

        // Place guide voices (RH_GuideHigh, RH_GuideLow)
        int guideHigh = 0;
        int guideLow = 0;
        int lastPlaced = topPitch;
        std::vector<int> usedPcs;
        usedPcs.push_back(topPc);

        for (int candidate : candidates) {
            int cpc = candidate % 12;

            // Duplicate suppression
            if (std::find(usedPcs.begin(), usedPcs.end(), cpc) != usedPcs.end()) continue;

            // Minimum voice separation
            if (lastPlaced - candidate < m_minVoiceSep) continue;

            // Muddy register gate
            if (candidate < m_rhMinPitch) continue;

            if (guideHigh == 0) {
                guideHigh = candidate;
                lastPlaced = candidate;
                usedPcs.push_back(cpc);
            } else if (guideLow == 0) {
                guideLow = candidate;
                lastPlaced = candidate;
                usedPcs.push_back(cpc);
                break;
            }
        }

        t[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideHigh)] = static_cast<uint8_t>(guideHigh);
        t[i].pitches[static_cast<int>(PianoTargetRole::RH_GuideLow)]  = static_cast<uint8_t>(guideLow);

        // --- RH_Inner: Color Tone ---
        // Only populate if style allows and there is acoustic space
        if (!m_allowRHInner) continue;

        // Determine the target color pitch class based on style
        int colorPc = -1;
        switch (m_style) {
            case PianoStyle::PopBlock:
                // 5th as mild thickening (only if not already present)
                if (IsFifth(ctx, topPc) || (guideHigh != 0 && IsFifth(ctx, guideHigh % 12)) ||
                    (guideLow != 0 && IsFifth(ctx, guideLow % 12))) {
                    // 5th already present somewhere, skip
                    continue;
                }
                colorPc = (static_cast<int>(ctx.root) + 7) % 12;
                break;
            case PianoStyle::SingerSongwriter:
                // 9th for open shimmer
                colorPc = GetNinthPc(ctx);
                break;
            case PianoStyle::JazzShell: {
                // If chord has a 7th, add the 9th. Otherwise add the 7th as a tension.
                int seventhInt = GetSeventhInterval(ctx.quality);
                if (seventhInt >= 0) {
                    colorPc = GetNinthPc(ctx);
                } else {
                    // Add a dominant 7th as color on triads
                    colorPc = (static_cast<int>(ctx.root) + 10) % 12;
                }
                break;
            }
        }

        if (colorPc < 0) continue;

        // Skip if this pitch class is already in the voicing
        if (std::find(usedPcs.begin(), usedPcs.end(), colorPc) != usedPcs.end()) continue;

        // Find the best physical placement for this color tone between guideLow and guideHigh
        // (or between guideHigh and top if guideLow is empty)
        int colorFloor = (guideLow != 0) ? guideLow : (guideHigh != 0 ? guideHigh : searchFloor);
        int colorCeil = topPitch - 1;

        int bestColor = 0;
        int bestDist = 999;
        for (int p = colorFloor; p <= colorCeil; ++p) {
            if (p % 12 != colorPc) continue;
            if (p < m_rhMinPitch) continue;

            // Check separation from all placed voices
            bool tooClose = false;
            if (std::abs(p - topPitch) < m_minVoiceSep) tooClose = true;
            if (guideHigh != 0 && std::abs(p - guideHigh) < m_minVoiceSep) tooClose = true;
            if (guideLow != 0 && std::abs(p - guideLow) < m_minVoiceSep) tooClose = true;
            if (tooClose) continue;

            // Prefer placement close to the center of the voicing
            int voiceCenter = (topPitch + (guideLow != 0 ? guideLow : guideHigh)) / 2;
            int dist = std::abs(p - voiceCenter);
            if (dist < bestDist) {
                bestDist = dist;
                bestColor = p;
            }
        }

        if (bestColor > 0) {
            t[i].pitches[static_cast<int>(PianoTargetRole::RH_Inner)] = static_cast<uint8_t>(bestColor);
        }
    }
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
