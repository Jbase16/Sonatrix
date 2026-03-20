#include "src/core/engines/piano/PianoVoicingPlanner.h"
#include "src/core/arrangement/ChordTrack.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <set>

using namespace Sonatrix::Core;

static std::string PitchToName(uint8_t pitch) {
    if (pitch == 0) return "---";
    const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (pitch / 12) - 1;
    int noteIndex = pitch % 12;
    return std::string(notes[noteIndex]) + std::to_string(octave);
}

static std::string StyleName(MIDI::PianoStyle s) {
    switch (s) {
        case MIDI::PianoStyle::PopBlock: return "PopBlock";
        case MIDI::PianoStyle::SingerSongwriter: return "SingerSongwriter";
        case MIDI::PianoStyle::JazzShell: return "JazzShell";
    }
    return "Unknown";
}

struct ProgressionEntry {
    double beat;
    PitchClass root;
    ChordQuality quality;
    const char* label;
};

static void RunTrace(const char* title, MIDI::PianoStyle style, const std::vector<ProgressionEntry>& entries) {
    std::cout << "\n=== " << title << " [" << StyleName(style) << "] ===\n";

    std::vector<ChordTrackEvent> chordTimeline;
    for (const auto& e : entries) {
        ChordTrackEvent ev;
        ev.position = MusicalTime(static_cast<int64_t>(e.beat * 960));
        ev.chord.root = e.root;
        ev.chord.quality = e.quality;
        chordTimeline.push_back(ev);
    }

    MIDI::PianoVoicingPlanner planner(style);
    auto solved = planner.SolveTimeline(chordTimeline);

    std::cout << std::left
              << std::setw(8) << "Beat"
              << std::setw(10) << "Chord"
              << std::setw(10) << "LH Root"
              << std::setw(10) << "LH 2nd"
              << std::setw(10) << "RH Low"
              << std::setw(10) << "RH High"
              << std::setw(10) << "RH Top"
              << std::setw(6) << "RHsp"
              << std::setw(6) << "LHsp"
              << std::setw(6) << "Dupes"
              << std::setw(12) << "Violations"
              << "\n";
    std::cout << std::string(96, '-') << "\n";

    int violationCount = 0;

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& v = solved[i];

        uint8_t lhRoot = v.GetPitch(MIDI::PianoTargetRole::LH_Root);
        uint8_t lh2 = v.GetPitch(MIDI::PianoTargetRole::LH_Fifth);
        if (lh2 == 0) lh2 = v.GetPitch(MIDI::PianoTargetRole::LH_ShellLow);

        uint8_t rhLow = v.GetPitch(MIDI::PianoTargetRole::RH_GuideLow);
        uint8_t rhHigh = v.GetPitch(MIDI::PianoTargetRole::RH_GuideHigh);
        uint8_t rhTop = v.GetPitch(MIDI::PianoTargetRole::RH_Top);

        int rhSpan = v.RHSpan();
        int lhSpan = v.LHSpan();

        // Check for duplicate pitch classes across RH
        std::set<int> rhPcs;
        std::string dupes = "";
        auto checkDupe = [&](uint8_t p) {
            if (p == 0) return;
            if (!rhPcs.insert(p % 12).second) {
                dupes += PitchToName(p) + " ";
            }
        };
        checkDupe(rhLow);
        checkDupe(rhHigh);
        checkDupe(rhTop);

        // Check violations
        std::string violations = "";
        if (rhSpan > 12) { violations += "SPAN! "; ++violationCount; }
        if (rhHigh != 0 && rhTop != 0 && (rhTop - rhHigh) < 2) { violations += "SEP! "; ++violationCount; }
        if (rhLow != 0 && rhHigh != 0 && (rhHigh - rhLow) < 2) { violations += "SEP! "; ++violationCount; }
        if (rhLow != 0 && rhLow < 60 && style != MIDI::PianoStyle::JazzShell) { violations += "MUD! "; ++violationCount; }

        if (dupes.empty()) dupes = "-";
        if (violations.empty()) violations = "OK";

        std::cout << std::left
                  << std::setw(8) << std::fixed << std::setprecision(1) << entries[i].beat
                  << std::setw(10) << entries[i].label
                  << std::setw(10) << PitchToName(lhRoot)
                  << std::setw(10) << PitchToName(lh2)
                  << std::setw(10) << PitchToName(rhLow)
                  << std::setw(10) << PitchToName(rhHigh)
                  << std::setw(10) << PitchToName(rhTop)
                  << std::setw(6) << rhSpan
                  << std::setw(6) << lhSpan
                  << std::setw(6) << dupes
                  << std::setw(12) << violations
                  << "\n";
    }

    std::cout << "Total violations: " << violationCount << "\n";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "  HIERARCHICAL PIANO VOICING TRACE v2\n";
    std::cout << "==========================================\n";

    // --- Progression 1: Standard ii-V-I ---
    std::vector<ProgressionEntry> prog1 = {
        {0,  PitchClass::A,      ChordQuality::Minor7,          "Am7"},
        {4,  PitchClass::D,      ChordQuality::Dominant7,       "D7"},
        {8,  PitchClass::G,      ChordQuality::Major7,          "Gmaj7"},
        {12, PitchClass::C,      ChordQuality::Major7,          "Cmaj7"},
        {16, PitchClass::B,      ChordQuality::HalfDiminished7, "Bm7b5"},
        {20, PitchClass::E,      ChordQuality::Minor,           "Em"},
    };

    // --- Progression 2: Secondary Dominants ---
    std::vector<ProgressionEntry> prog2 = {
        {0,  PitchClass::C,      ChordQuality::Major,           "C"},
        {4,  PitchClass::E,      ChordQuality::Dominant7,       "E7"},
        {8,  PitchClass::A,      ChordQuality::Minor,           "Am"},
        {12, PitchClass::D,      ChordQuality::Dominant7,       "D7"},
        {16, PitchClass::G,      ChordQuality::Major,           "G"},
    };

    // --- Progression 3: Chromatic Mediants ---
    std::vector<ProgressionEntry> prog3 = {
        {0,  PitchClass::C,      ChordQuality::Major,           "C"},
        {4,  PitchClass::G_Sharp, ChordQuality::Major,          "Ab"},
        {8,  PitchClass::E,      ChordQuality::Major,           "E"},
        {12, PitchClass::C,      ChordQuality::Major,           "C"},
    };

    // Run all three progressions across all three styles
    MIDI::PianoStyle styles[] = {MIDI::PianoStyle::PopBlock, MIDI::PianoStyle::SingerSongwriter, MIDI::PianoStyle::JazzShell};

    for (auto style : styles) {
        RunTrace("ii-V-I Chain", style, prog1);
        RunTrace("Secondary Dominants", style, prog2);
        RunTrace("Chromatic Mediants", style, prog3);
    }

    std::cout << "\n==========================================\n";
    return 0;
}
