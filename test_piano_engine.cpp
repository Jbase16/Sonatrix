#include "src/core/engines/piano/PianoVoicingPlanner.h"
#include "src/core/arrangement/ChordTrack.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>

using namespace Sonatrix::Core;

static std::string PitchToName(uint8_t pitch) {
    if (pitch == 0) return "---";
    const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return std::string(notes[pitch % 12]) + std::to_string((pitch / 12) - 1);
}

static std::string StyleName(MIDI::PianoStyle s) {
    switch (s) {
        case MIDI::PianoStyle::PopBlock: return "Pop";
        case MIDI::PianoStyle::SingerSongwriter: return "SS";
        case MIDI::PianoStyle::JazzShell: return "Jazz";
    }
    return "?";
}

static std::string ContourName(MIDI::SopranoContour c) {
    switch (c) {
        case MIDI::SopranoContour::Hold: return "Hold";
        case MIDI::SopranoContour::Rise: return "Rise";
        case MIDI::SopranoContour::Fall: return "Fall";
        case MIDI::SopranoContour::Arch: return "Arch";
    }
    return "?";
}

struct ProgressionEntry {
    double beat;
    PitchClass root;
    ChordQuality quality;
    const char* label;
    PitchClass overBass;
    bool isSlash;
};

static ProgressionEntry Ch(double beat, PitchClass root, ChordQuality q, const char* label) {
    return {beat, root, q, label, root, false};
}

static ProgressionEntry Sl(double beat, PitchClass root, ChordQuality q, const char* label, PitchClass bass) {
    return {beat, root, q, label, bass, true};
}

// ---------------------------------------------------------
// Voice Identity Metrics
// ---------------------------------------------------------
// Tracks whether the SAME ROLE retains the same pitch or
// resolves by semitone across chord changes. This is not
// set-overlap. This is structural identity.
struct VoiceIdentityMetrics {
    int roleRetained[4] = {0}; // per RH role (GuideLow=0, Inner=1, GuideHigh=2, Top=3)
    int roleResolved[4] = {0}; // semitone motion in same role
    int totalTransitions = 0;

    int TotalRetained() const { int s = 0; for (int i = 0; i < 4; ++i) s += roleRetained[i]; return s; }
    int TotalResolved() const { int s = 0; for (int i = 0; i < 4; ++i) s += roleResolved[i]; return s; }
};

static void RunTrace(const char* title, MIDI::PianoStyle style, MIDI::SopranoContour contour,
                     const std::vector<ProgressionEntry>& entries) {
    std::cout << "\n=== " << title << " [" << StyleName(style) << "/" << ContourName(contour) << "] ===\n";

    std::vector<ChordTrackEvent> chordTimeline;
    for (const auto& e : entries) {
        ChordTrackEvent ev;
        ev.position = MusicalTime(static_cast<int64_t>(e.beat * 960));
        ev.chord.root = e.root;
        ev.chord.quality = e.quality;
        ev.chord.overBass = e.overBass;
        chordTimeline.push_back(ev);
    }

    MIDI::PianoVoicingPlanner planner(style, contour);
    auto solved = planner.SolveTimeline(chordTimeline);

    // Header
    std::cout << std::left
              << std::setw(8) << "Beat"
              << std::setw(10) << "Chord"
              << std::setw(7) << "LH Rt"
              << std::setw(7) << "LH 2"
              << std::setw(7) << "RH Lo"
              << std::setw(7) << "RH In"
              << std::setw(7) << "RH Hi"
              << std::setw(7) << "RH Tp"
              << std::setw(5) << "Rsp"
              << std::setw(5) << "Den"
              << "\n";
    std::cout << std::string(72, '-') << "\n";

    VoiceIdentityMetrics vim;
    constexpr int rhRoles[] = {
        static_cast<int>(MIDI::PianoTargetRole::RH_GuideLow),
        static_cast<int>(MIDI::PianoTargetRole::RH_Inner),
        static_cast<int>(MIDI::PianoTargetRole::RH_GuideHigh),
        static_cast<int>(MIDI::PianoTargetRole::RH_Top)
    };

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& v = solved[i];

        uint8_t lhRoot = v.GetPitch(MIDI::PianoTargetRole::LH_Root);
        uint8_t lh2 = v.GetPitch(MIDI::PianoTargetRole::LH_Fifth);
        if (lh2 == 0) lh2 = v.GetPitch(MIDI::PianoTargetRole::LH_ShellLow);

        uint8_t rhLow = v.GetPitch(MIDI::PianoTargetRole::RH_GuideLow);
        uint8_t rhInner = v.GetPitch(MIDI::PianoTargetRole::RH_Inner);
        uint8_t rhHigh = v.GetPitch(MIDI::PianoTargetRole::RH_GuideHigh);
        uint8_t rhTop = v.GetPitch(MIDI::PianoTargetRole::RH_Top);

        // Voice identity tracking
        if (i > 0) {
            vim.totalTransitions++;
            const auto& prev = solved[i - 1];
            for (int r = 0; r < 4; ++r) {
                int roleIdx = rhRoles[r];
                uint8_t curr = v.pitches[roleIdx];
                uint8_t prv = prev.pitches[roleIdx];
                if (curr == 0 || prv == 0) continue;
                if (curr == prv) {
                    vim.roleRetained[r]++;
                } else if (std::abs(static_cast<int>(curr) - static_cast<int>(prv)) == 1) {
                    vim.roleResolved[r]++;
                }
            }
        }

        std::cout << std::left
                  << std::setw(8) << std::fixed << std::setprecision(1) << entries[i].beat
                  << std::setw(10) << entries[i].label
                  << std::setw(7) << PitchToName(lhRoot)
                  << std::setw(7) << PitchToName(lh2)
                  << std::setw(7) << PitchToName(rhLow)
                  << std::setw(7) << PitchToName(rhInner)
                  << std::setw(7) << PitchToName(rhHigh)
                  << std::setw(7) << PitchToName(rhTop)
                  << std::setw(5) << v.RHSpan()
                  << std::setw(5) << v.RHDensity()
                  << "\n";
    }

    // Print voice identity report
    const char* roleNames[] = {"GuidLo", "Inner", "GuidHi", "Top"};
    std::cout << "--- Voice Identity (role-to-role): ";
    for (int r = 0; r < 4; ++r) {
        if (vim.roleRetained[r] > 0 || vim.roleResolved[r] > 0) {
            std::cout << roleNames[r] << "[hold=" << vim.roleRetained[r]
                      << " res=" << vim.roleResolved[r] << "] ";
        }
    }
    std::cout << " | Transitions=" << vim.totalTransitions
              << " TotalHeld=" << vim.TotalRetained()
              << " TotalRes=" << vim.TotalResolved() << "\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "  HIERARCHICAL PIANO VOICING TRACE v4\n";
    std::cout << "==========================================\n";

    // --- Prog 1: ii-V-I (standard test) ---
    auto iiVI = std::vector<ProgressionEntry>{
        Ch(0,  PitchClass::A,      ChordQuality::Minor7,          "Am7"),
        Ch(4,  PitchClass::D,      ChordQuality::Dominant7,       "D7"),
        Ch(8,  PitchClass::G,      ChordQuality::Major7,          "Gmaj7"),
        Ch(12, PitchClass::C,      ChordQuality::Major7,          "Cmaj7"),
        Ch(16, PitchClass::B,      ChordQuality::HalfDiminished7, "Bm7b5"),
        Ch(20, PitchClass::E,      ChordQuality::Minor,           "Em"),
    };

    // --- Prog 2: 8-bar pop for contour divergence test ---
    auto pop8 = std::vector<ProgressionEntry>{
        Ch(0,  PitchClass::C,      ChordQuality::Major,    "C"),
        Ch(4,  PitchClass::G,      ChordQuality::Major,    "G"),
        Ch(8,  PitchClass::A,      ChordQuality::Minor,    "Am"),
        Ch(12, PitchClass::F,      ChordQuality::Major,    "F"),
        Ch(16, PitchClass::C,      ChordQuality::Major,    "C"),
        Ch(20, PitchClass::G,      ChordQuality::Major,    "G"),
        Ch(24, PitchClass::F,      ChordQuality::Major,    "F"),
        Ch(28, PitchClass::G,      ChordQuality::Major,    "G"),
    };

    // --- Prog 3: Slash chords + pedal bass ---
    auto slashPedal = std::vector<ProgressionEntry>{
        Ch(0,       PitchClass::C, ChordQuality::Major,    "C"),
        Sl(4,       PitchClass::C, ChordQuality::Major,    "C/E",  PitchClass::E),
        Ch(8,       PitchClass::F, ChordQuality::Major,    "F"),
        Sl(12,      PitchClass::G, ChordQuality::Major,    "G/B",  PitchClass::B),
        Ch(16,      PitchClass::A, ChordQuality::Minor,    "Am"),
        Sl(20,      PitchClass::A, ChordQuality::Minor,    "Am/G", PitchClass::G),
        Sl(24,      PitchClass::F, ChordQuality::Major,    "F/A",  PitchClass::A),
        Ch(28,      PitchClass::G, ChordQuality::Major,    "G"),
    };

    // --- Prog 4: Deceptive cadence + sus resolution + major/minor toggle ---
    auto hardProg = std::vector<ProgressionEntry>{
        Ch(0,  PitchClass::C,      ChordQuality::Major,    "C"),
        Ch(4,  PitchClass::G,      ChordQuality::Dominant7, "G7"),
        Ch(8,  PitchClass::A,      ChordQuality::Minor,    "Am"),    // deceptive
        Ch(12, PitchClass::D,      ChordQuality::Sus4,     "Dsus4"),
        Ch(16, PitchClass::D,      ChordQuality::Major,    "D"),     // sus resolution
        Ch(20, PitchClass::G,      ChordQuality::Minor,    "Gm"),    // major->minor toggle
        Ch(24, PitchClass::G,      ChordQuality::Major,    "G"),     // minor->major toggle
        Ch(28, PitchClass::C,      ChordQuality::Major,    "C"),
    };

    // 1. Style comparison on ii-V-I (Hold contour)
    MIDI::PianoStyle styles[] = {MIDI::PianoStyle::PopBlock, MIDI::PianoStyle::SingerSongwriter, MIDI::PianoStyle::JazzShell};
    for (auto s : styles) {
        RunTrace("ii-V-I Chain", s, MIDI::SopranoContour::Hold, iiVI);
    }

    // 2. Contour divergence on 8-bar pop (same style, all 4 contours)
    MIDI::SopranoContour contours[] = {MIDI::SopranoContour::Hold, MIDI::SopranoContour::Rise,
                                        MIDI::SopranoContour::Fall, MIDI::SopranoContour::Arch};
    for (auto c : contours) {
        RunTrace("8-Bar Pop", MIDI::PianoStyle::PopBlock, c, pop8);
    }

    // 3. Slash chords + pedal bass
    RunTrace("Slash + Pedal", MIDI::PianoStyle::PopBlock, MIDI::SopranoContour::Hold, slashPedal);

    // 4. Hard progression
    for (auto s : styles) {
        RunTrace("Deceptive/Sus/Toggle", s, MIDI::SopranoContour::Hold, hardProg);
    }

    std::cout << "\n==========================================\n";
    return 0;
}
