#include "src/core/engines/piano/PianoVoicingPlanner.h"
#include "src/core/arrangement/ChordTrack.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <set>
#include <cmath>

using namespace Sonatrix::Core;

static std::string PitchToName(uint8_t pitch) {
    if (pitch == 0) return "---";
    const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (pitch / 12) - 1;
    return std::string(notes[pitch % 12]) + std::to_string(octave);
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
    PitchClass overBass;  // same as root = root position
    bool isSlash;
};

static ProgressionEntry Chord(double beat, PitchClass root, ChordQuality q, const char* label) {
    return {beat, root, q, label, root, false};
}

static ProgressionEntry SlashChord(double beat, PitchClass root, ChordQuality q, const char* label, PitchClass bass) {
    return {beat, root, q, label, bass, true};
}

struct QualityMetrics {
    int commonTonesRetained = 0;
    int semitoneResolutions = 0;
    int guideToneContinuity = 0; // guide tone held as common tone across chord change
    int fifthOmissions = 0;
    float avgRHDensity = 0;
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

    std::cout << std::left
              << std::setw(8)  << "Beat"
              << std::setw(10) << "Chord"
              << std::setw(8)  << "LH Rt"
              << std::setw(8)  << "LH 2"
              << std::setw(8)  << "RH Lo"
              << std::setw(8)  << "RH In"
              << std::setw(8)  << "RH Hi"
              << std::setw(8)  << "RH Tp"
              << std::setw(5)  << "Rsp"
              << std::setw(5)  << "Den"
              << std::setw(8)  << "Viol"
              << "\n";
    std::cout << std::string(84, '-') << "\n";

    int violationCount = 0;
    QualityMetrics qm;
    int totalDensity = 0;

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& v = solved[i];

        uint8_t lhRoot = v.GetPitch(MIDI::PianoTargetRole::LH_Root);
        uint8_t lh2 = v.GetPitch(MIDI::PianoTargetRole::LH_Fifth);
        if (lh2 == 0) lh2 = v.GetPitch(MIDI::PianoTargetRole::LH_ShellLow);

        uint8_t rhLow = v.GetPitch(MIDI::PianoTargetRole::RH_GuideLow);
        uint8_t rhInner = v.GetPitch(MIDI::PianoTargetRole::RH_Inner);
        uint8_t rhHigh = v.GetPitch(MIDI::PianoTargetRole::RH_GuideHigh);
        uint8_t rhTop = v.GetPitch(MIDI::PianoTargetRole::RH_Top);

        int rhSpan = v.RHSpan();
        int density = v.RHDensity();
        totalDensity += density;

        // Violations
        std::string violations = "";
        if (rhSpan > 14) { violations += "SPAN "; ++violationCount; }

        // Quality metrics (transitions — compare with previous chord)
        if (i > 0) {
            const auto& prev = solved[i - 1];
            // Common tones: any RH pitch held from previous voicing
            for (int r = 4; r <= 7; ++r) {
                if (v.pitches[r] == 0) continue;
                for (int pr = 4; pr <= 7; ++pr) {
                    if (prev.pitches[pr] == v.pitches[r]) {
                        qm.commonTonesRetained++;
                        break;
                    }
                }
            }
            // Semitone resolutions: any RH voice that moved exactly 1 semitone
            for (int r = 4; r <= 7; ++r) {
                if (v.pitches[r] == 0) continue;
                for (int pr = 4; pr <= 7; ++pr) {
                    if (prev.pitches[pr] == 0) continue;
                    if (std::abs(static_cast<int>(v.pitches[r]) - static_cast<int>(prev.pitches[pr])) == 1) {
                        qm.semitoneResolutions++;
                        break;
                    }
                }
            }
            // Soprano direction
            int sopranoDelta = static_cast<int>(rhTop) - static_cast<int>(prev.GetPitch(MIDI::PianoTargetRole::RH_Top));
            (void)sopranoDelta; // used for future contour tracking
        }

        if (violations.empty()) violations = "OK";

        std::cout << std::left
                  << std::setw(8) << std::fixed << std::setprecision(1) << entries[i].beat
                  << std::setw(10) << entries[i].label
                  << std::setw(8) << PitchToName(lhRoot)
                  << std::setw(8) << PitchToName(lh2)
                  << std::setw(8) << PitchToName(rhLow)
                  << std::setw(8) << PitchToName(rhInner)
                  << std::setw(8) << PitchToName(rhHigh)
                  << std::setw(8) << PitchToName(rhTop)
                  << std::setw(5) << rhSpan
                  << std::setw(5) << density
                  << std::setw(8) << violations
                  << "\n";
    }

    qm.avgRHDensity = static_cast<float>(totalDensity) / static_cast<float>(entries.size());

    std::cout << "--- Quality: "
              << "CommonTones=" << qm.commonTonesRetained
              << "  SemitoneRes=" << qm.semitoneResolutions
              << "  AvgDensity=" << std::fixed << std::setprecision(1) << qm.avgRHDensity
              << "  Violations=" << violationCount
              << "\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "  HIERARCHICAL PIANO VOICING TRACE v3\n";
    std::cout << "==========================================\n";

    // --- Progression 1: ii-V-I ---
    auto prog1 = std::vector<ProgressionEntry>{
        Chord(0,  PitchClass::A,       ChordQuality::Minor7,          "Am7"),
        Chord(4,  PitchClass::D,       ChordQuality::Dominant7,       "D7"),
        Chord(8,  PitchClass::G,       ChordQuality::Major7,          "Gmaj7"),
        Chord(12, PitchClass::C,       ChordQuality::Major7,          "Cmaj7"),
        Chord(16, PitchClass::B,       ChordQuality::HalfDiminished7, "Bm7b5"),
        Chord(20, PitchClass::E,       ChordQuality::Minor,           "Em"),
    };

    // --- Progression 2: Slash Chords ---
    auto prog2 = std::vector<ProgressionEntry>{
        Chord(0,       PitchClass::C,       ChordQuality::Major,     "C"),
        SlashChord(4,  PitchClass::C,       ChordQuality::Major,     "C/E",  PitchClass::E),
        Chord(8,       PitchClass::F,       ChordQuality::Major,     "F"),
        SlashChord(12, PitchClass::G,       ChordQuality::Major,     "G/B",  PitchClass::B),
        Chord(16,      PitchClass::A,       ChordQuality::Minor,     "Am"),
        SlashChord(20, PitchClass::A,       ChordQuality::Minor,     "Am/G", PitchClass::G),
        Chord(24,      PitchClass::F,       ChordQuality::Major,     "F"),
        Chord(28,      PitchClass::G,       ChordQuality::Major,     "G"),
    };

    // --- Progression 3: Chromatic Mediants ---
    auto prog3 = std::vector<ProgressionEntry>{
        Chord(0,  PitchClass::C,       ChordQuality::Major,     "C"),
        Chord(4,  PitchClass::G_Sharp, ChordQuality::Major,     "Ab"),
        Chord(8,  PitchClass::E,       ChordQuality::Major,     "E"),
        Chord(12, PitchClass::C,       ChordQuality::Major,     "C"),
    };

    // Run core style comparison on ii-V-I with Hold contour
    MIDI::PianoStyle styles[] = {MIDI::PianoStyle::PopBlock, MIDI::PianoStyle::SingerSongwriter, MIDI::PianoStyle::JazzShell};
    for (auto style : styles) {
        RunTrace("ii-V-I Chain", style, MIDI::SopranoContour::Hold, prog1);
    }

    // Run slash chord test
    RunTrace("Slash Chords", MIDI::PianoStyle::PopBlock, MIDI::SopranoContour::Hold, prog2);

    // Run contour mode sweep on chromatic mediants
    MIDI::SopranoContour contours[] = {MIDI::SopranoContour::Hold, MIDI::SopranoContour::Rise,
                                        MIDI::SopranoContour::Fall, MIDI::SopranoContour::Arch};
    for (auto c : contours) {
        RunTrace("Chromatic Mediants", MIDI::PianoStyle::PopBlock, c, prog3);
    }

    std::cout << "\n==========================================\n";
    return 0;
}
