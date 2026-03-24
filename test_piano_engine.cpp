#include "src/core/engines/piano/PianoVoicingPlanner.h"
#include "src/core/arrangement/ChordTrack.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>
#include <set>

using namespace Sonatrix::Core;

static std::string PitchToName(uint8_t pitch) {
    if (pitch == 0) return "---";
    const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return std::string(notes[pitch % 12]) + std::to_string((pitch / 12) - 1);
}

static std::string PcToName(int pc) {
    const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return notes[pc % 12];
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

static std::string FunctionName(MIDI::HarmonicFunction f) {
    switch (f) {
        case MIDI::HarmonicFunction::Default: return "---";
        case MIDI::HarmonicFunction::Pedal: return "PED";
        case MIDI::HarmonicFunction::DominantResolution: return "DOM";
        case MIDI::HarmonicFunction::ReinterpretiveHold: return "RHD";
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
// Per-Transition Diagnostic
// ---------------------------------------------------------
struct TransitionEvent {
    std::string description;
};

static void AnalyzeTransition(size_t idx, const MIDI::PianoVoicing& prev, const MIDI::PianoVoicing& curr,
                               const char* prevLabel, const char* currLabel,
                               std::vector<TransitionEvent>& events) {
    constexpr int rhRoles[] = {
        static_cast<int>(MIDI::PianoTargetRole::RH_GuideLow),
        static_cast<int>(MIDI::PianoTargetRole::RH_Inner),
        static_cast<int>(MIDI::PianoTargetRole::RH_GuideHigh),
        static_cast<int>(MIDI::PianoTargetRole::RH_Top)
    };
    const char* roleNames[] = {"Lo", "In", "Hi", "Tp"};

    std::vector<std::pair<uint8_t, int>> prevPitches, currPitches;
    for (int r = 0; r < 4; ++r) {
        uint8_t pp = prev.pitches[rhRoles[r]];
        uint8_t cp = curr.pitches[rhRoles[r]];
        if (pp > 0) prevPitches.push_back({pp, r});
        if (cp > 0) currPitches.push_back({cp, r});
    }

    std::string line = "  " + std::string(prevLabel) + " -> " + std::string(currLabel) + ": ";

    std::set<uint8_t> matchedCurr;
    int holds = 0, resolves = 0, migrates = 0, drops = 0;

    for (auto& [pp, pr] : prevPitches) {
        uint8_t sameRoleCurr = curr.pitches[rhRoles[pr]];
        if (sameRoleCurr == pp) {
            line += PitchToName(pp) + "[" + roleNames[pr] + " hold] ";
            matchedCurr.insert(sameRoleCurr);
            holds++;
            continue;
        }

        if (sameRoleCurr > 0 && std::abs(static_cast<int>(sameRoleCurr) - static_cast<int>(pp)) == 1) {
            line += PitchToName(pp) + "->" + PitchToName(sameRoleCurr) + "[" + roleNames[pr] + " res] ";
            matchedCurr.insert(sameRoleCurr);
            resolves++;
            continue;
        }

        bool found = false;
        for (auto& [cp, cr] : currPitches) {
            if (cp == pp && cr != pr && matchedCurr.find(cp) == matchedCurr.end()) {
                line += PitchToName(pp) + "[" + roleNames[pr] + "->" + roleNames[cr] + " migrate] ";
                matchedCurr.insert(cp);
                migrates++;
                found = true;
                break;
            }
        }
        if (found) continue;

        for (auto& [cp, cr] : currPitches) {
            if (std::abs(static_cast<int>(cp) - static_cast<int>(pp)) == 1 &&
                matchedCurr.find(cp) == matchedCurr.end()) {
                line += PitchToName(pp) + "->" + PitchToName(cp) + "[" + roleNames[pr] + "->" + roleNames[cr] + " xres] ";
                matchedCurr.insert(cp);
                resolves++;
                found = true;
                break;
            }
        }
        if (found) continue;

        line += PitchToName(pp) + "[" + roleNames[pr] + " drop] ";
        drops++;
    }

    line += " | h=" + std::to_string(holds) + " r=" + std::to_string(resolves) +
            " m=" + std::to_string(migrates) + " d=" + std::to_string(drops);
    events.push_back({line});
}

// ---------------------------------------------------------
// Aggregate Metrics
// ---------------------------------------------------------
struct AggregateMetrics {
    int crossHeld = 0, crossResolved = 0;
    int perRoleHeld = 0, perRoleResolved = 0;
    int transitions = 0;
};

static void RunTrace(const char* title, MIDI::PianoStyle style, MIDI::SopranoContour contour,
                     const std::vector<ProgressionEntry>& entries, bool showDiagnostics = false) {
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

    // Use VoicingExplanation from planner if diagnostics requested
    std::vector<MIDI::VoicingExplanation> expVec;
    auto solved = planner.SolveTimeline(chordTimeline, showDiagnostics ? &expVec : nullptr);

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
              << std::setw(5) << "Fn"
              << "\n";
    std::cout << std::string(77, '-') << "\n";

    constexpr int rhRoles[] = {
        static_cast<int>(MIDI::PianoTargetRole::RH_GuideLow),
        static_cast<int>(MIDI::PianoTargetRole::RH_Inner),
        static_cast<int>(MIDI::PianoTargetRole::RH_GuideHigh),
        static_cast<int>(MIDI::PianoTargetRole::RH_Top)
    };

    AggregateMetrics agg;
    std::vector<TransitionEvent> diagEvents;

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& v = solved[i];

        uint8_t lhRoot = v.GetPitch(MIDI::PianoTargetRole::LH_Root);
        uint8_t lh2 = v.GetPitch(MIDI::PianoTargetRole::LH_Fifth);
        if (lh2 == 0) lh2 = v.GetPitch(MIDI::PianoTargetRole::LH_ShellLow);

        std::string fnLabel = "---";
        if (showDiagnostics && i < expVec.size()) {
            fnLabel = FunctionName(expVec[i].transitionFunction);
        }

        std::cout << std::left
                  << std::setw(8) << std::fixed << std::setprecision(1) << entries[i].beat
                  << std::setw(10) << entries[i].label
                  << std::setw(7) << PitchToName(lhRoot)
                  << std::setw(7) << PitchToName(lh2)
                  << std::setw(7) << PitchToName(v.GetPitch(MIDI::PianoTargetRole::RH_GuideLow))
                  << std::setw(7) << PitchToName(v.GetPitch(MIDI::PianoTargetRole::RH_Inner))
                  << std::setw(7) << PitchToName(v.GetPitch(MIDI::PianoTargetRole::RH_GuideHigh))
                  << std::setw(7) << PitchToName(v.GetPitch(MIDI::PianoTargetRole::RH_Top))
                  << std::setw(5) << v.RHSpan()
                  << std::setw(5) << v.RHDensity()
                  << std::setw(5) << fnLabel
                  << "\n";

        if (i > 0) {
            agg.transitions++;
            const auto& prev = solved[i - 1];

            for (int r = 0; r < 4; ++r) {
                uint8_t curr = v.pitches[rhRoles[r]];
                uint8_t prv = prev.pitches[rhRoles[r]];
                if (curr == 0 || prv == 0) continue;
                if (curr == prv) agg.perRoleHeld++;
                else if (std::abs(static_cast<int>(curr) - static_cast<int>(prv)) == 1) agg.perRoleResolved++;
            }

            std::vector<uint8_t> prevP, currP;
            for (int r = 0; r < 4; ++r) {
                uint8_t pp = prev.pitches[rhRoles[r]];
                uint8_t cp = v.pitches[rhRoles[r]];
                if (pp > 0) prevP.push_back(pp);
                if (cp > 0) currP.push_back(cp);
            }
            for (uint8_t pp : prevP) {
                bool matched = false;
                for (uint8_t cp : currP) {
                    if (cp == pp) { agg.crossHeld++; matched = true; break; }
                }
                if (!matched) {
                    for (uint8_t cp : currP) {
                        if (std::abs(static_cast<int>(cp) - static_cast<int>(pp)) == 1) {
                            agg.crossResolved++; break;
                        }
                    }
                }
            }

            if (showDiagnostics) {
                AnalyzeTransition(i, prev, v, entries[i-1].label, entries[i].label, diagEvents);
            }
        }
    }

    std::cout << "--- Per-Role: held=" << agg.perRoleHeld << " res=" << agg.perRoleResolved
              << " total=" << (agg.perRoleHeld + agg.perRoleResolved) << "\n";
    std::cout << "--- Cross-Role: held=" << agg.crossHeld << " res=" << agg.crossResolved
              << " total=" << (agg.crossHeld + agg.crossResolved)
              << " | Transitions=" << agg.transitions << "\n";

    if (showDiagnostics && !expVec.empty()) {
        // Sufficiency from VoicingExplanation (no reverse-engineering)
        std::cout << "--- Sufficiency:\n";
        for (size_t i = 0; i < entries.size() && i < expVec.size(); ++i) {
            const auto& ex = expVec[i];
            std::cout << "  " << std::setw(10) << std::left << entries[i].label
                      << (ex.sufficient ? " OK " : "MISS")
                      << " req=" << ex.totalRequired
                      << " sop=" << ex.coveredBySoprano
                      << " bas=" << ex.coveredByBass
                      << " lh2=" << ex.coveredByLh2
                      << " rh=" << ex.suppliedByTuple
                      << " fn=" << FunctionName(ex.transitionFunction)
                      << " gate=" << ex.sufficiencyGateUsed
                      << " cw=" << std::fixed << std::setprecision(1) << ex.continuityWeightUsed
                      << " eval=" << ex.candidatesEvaluated
                      << "\n";
        }

        if (!diagEvents.empty()) {
            std::cout << "--- Transitions:\n";
            for (const auto& ev : diagEvents) {
                std::cout << ev.description << "\n";
            }
        }
    }
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "  HIERARCHICAL PIANO VOICING TRACE v7\n";
    std::cout << "==========================================\n";

    auto iiVI = std::vector<ProgressionEntry>{
        Ch(0,  PitchClass::A,      ChordQuality::Minor7,          "Am7"),
        Ch(4,  PitchClass::D,      ChordQuality::Dominant7,       "D7"),
        Ch(8,  PitchClass::G,      ChordQuality::Major7,          "Gmaj7"),
        Ch(12, PitchClass::C,      ChordQuality::Major7,          "Cmaj7"),
        Ch(16, PitchClass::B,      ChordQuality::HalfDiminished7, "Bm7b5"),
        Ch(20, PitchClass::E,      ChordQuality::Minor,           "Em"),
    };

    auto hardProg = std::vector<ProgressionEntry>{
        Ch(0,  PitchClass::C,      ChordQuality::Major,     "C"),
        Ch(4,  PitchClass::G,      ChordQuality::Dominant7, "G7"),
        Ch(8,  PitchClass::A,      ChordQuality::Minor,     "Am"),
        Ch(12, PitchClass::D,      ChordQuality::Sus4,      "Dsus4"),
        Ch(16, PitchClass::D,      ChordQuality::Major,     "D"),
        Ch(20, PitchClass::G,      ChordQuality::Minor,     "Gm"),
        Ch(24, PitchClass::G,      ChordQuality::Major,     "G"),
        Ch(28, PitchClass::C,      ChordQuality::Major,     "C"),
    };

    auto backdoor = std::vector<ProgressionEntry>{
        Ch(0,  PitchClass::C,      ChordQuality::Major7,    "Cmaj7"),
        Ch(4,  PitchClass::A,      ChordQuality::Minor7,    "Am7"),
        Ch(8,  PitchClass::D,      ChordQuality::Minor7,    "Dm7"),
        Ch(12, PitchClass::A_Sharp, ChordQuality::Dominant7, "Bb7"),
        Ch(16, PitchClass::C,      ChordQuality::Major7,    "Cmaj7"),
        Ch(20, PitchClass::F,      ChordQuality::Major7,    "Fmaj7"),
        Ch(24, PitchClass::A_Sharp, ChordQuality::Dominant7, "Bb7"),
        Ch(28, PitchClass::C,      ChordQuality::Major7,    "Cmaj7"),
    };

    auto pedalBass = std::vector<ProgressionEntry>{
        Sl(0,  PitchClass::C,      ChordQuality::Major,     "C",      PitchClass::C),
        Sl(4,  PitchClass::D,      ChordQuality::Minor,     "Dm/C",   PitchClass::C),
        Sl(8,  PitchClass::E,      ChordQuality::Minor,     "Em/C",   PitchClass::C),
        Sl(12, PitchClass::F,      ChordQuality::Major,     "F/C",    PitchClass::C),
        Sl(16, PitchClass::G,      ChordQuality::Major,     "G/C",    PitchClass::C),
        Sl(20, PitchClass::A,      ChordQuality::Minor,     "Am/C",   PitchClass::C),
        Sl(24, PitchClass::F,      ChordQuality::Major,     "F/C",    PitchClass::C),
        Sl(28, PitchClass::C,      ChordQuality::Major,     "C",      PitchClass::C),
    };

    MIDI::PianoStyle allStyles[] = {
        MIDI::PianoStyle::PopBlock,
        MIDI::PianoStyle::SingerSongwriter,
        MIDI::PianoStyle::JazzShell
    };

    // ii-V-I across all styles
    for (auto s : allStyles) {
        RunTrace("ii-V-I Chain", s, MIDI::SopranoContour::Hold, iiVI, true);
    }

    // Backdoor dominant across all styles
    for (auto s : allStyles) {
        RunTrace("Backdoor Dom", s, MIDI::SopranoContour::Hold, backdoor, true);
    }

    // Hard prog — Pop only
    RunTrace("Deceptive/Sus/Toggle", MIDI::PianoStyle::PopBlock, MIDI::SopranoContour::Hold, hardProg, true);

    // Pedal bass — Pop vs Jazz
    RunTrace("Pedal Bass", MIDI::PianoStyle::PopBlock, MIDI::SopranoContour::Hold, pedalBass, true);
    RunTrace("Pedal Bass", MIDI::PianoStyle::JazzShell, MIDI::SopranoContour::Hold, pedalBass, true);

    std::cout << "\n==========================================\n";
    return 0;
}
