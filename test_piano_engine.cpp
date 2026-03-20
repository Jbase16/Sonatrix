#include "src/core/engines/piano/PianoCompiler.h"
#include "src/core/mir/MIRPattern.h"
#include "src/core/arrangement/ChordTrack.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

using namespace Sonatrix::Core;

static std::string PitchToName(uint8_t pitch) {
    if (pitch == 0) return "---";
    const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (pitch / 12) - 1; 
    int noteIndex = pitch % 12;
    return std::string(notes[noteIndex]) + std::to_string(octave) + " (" + std::to_string(pitch) + ")";
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "    HIERARCHICAL PIANO VOICING TRACE\n";
    std::cout << "==========================================\n";

    // Create a complex progression: Am7 -> D7 -> Gmaj7 -> Cmaj7 (vi - ii - V - I type sequence)
    std::vector<ChordTrackEvent> chordTimeline;
    
    // Beat 0: Am7
    ChordTrackEvent e1;
    e1.position = MusicalTime(0);
    e1.chord.root = PitchClass::A;
    e1.chord.quality = ChordQuality::Minor7;
    chordTimeline.push_back(e1);

    // Beat 4: D7
    ChordTrackEvent e2;
    e2.position = BeatsToTime(4.0);
    e2.chord.root = PitchClass::D;
    e2.chord.quality = ChordQuality::Dominant7;
    chordTimeline.push_back(e2);

    // Beat 8: Gmaj7
    ChordTrackEvent e3;
    e3.position = BeatsToTime(8.0);
    e3.chord.root = PitchClass::G;
    e3.chord.quality = ChordQuality::Major7;
    chordTimeline.push_back(e3);

    // Beat 12: Cmaj7
    ChordTrackEvent e4;
    e4.position = BeatsToTime(12.0);
    e4.chord.root = PitchClass::C;
    e4.chord.quality = ChordQuality::Major7;
    chordTimeline.push_back(e4);

    // Beat 16: Bm7b5 (Half Diminished)
    ChordTrackEvent e5;
    e5.position = BeatsToTime(16.0);
    e5.chord.root = PitchClass::B;
    e5.chord.quality = ChordQuality::HalfDiminished7;
    chordTimeline.push_back(e5);

    // Beat 20: Em
    ChordTrackEvent e6;
    e6.position = BeatsToTime(20.0);
    e6.chord.root = PitchClass::E;
    e6.chord.quality = ChordQuality::Minor;
    chordTimeline.push_back(e6);

    std::cout << "1. Resolving Outer & Inner Voices via PianoVoicingPlanner...\n\n";

    // Directly instantiate the planner to trace the structural graph
    MIDI::PianoVoicingPlanner planner;
    planner.SolveTimeline(chordTimeline);

    std::cout << std::left 
              << std::setw(8) << "Beat"
              << std::setw(15) << "Chord"
              << std::setw(20) << "Soprano (RH_Top)"
              << std::setw(20) << "Inner (RH_High)"
              << std::setw(20) << "Inner (RH_Low)"
              << std::setw(20) << "Bass (LH_Root)"
              << "\n";
    std::cout << "--------------------------------------------------------------------------------------------------\n";

    for (size_t i = 0; i < chordTimeline.size(); ++i) {
        double beat = static_cast<double>(chordTimeline[i].position.ticks) / 960.0;
        
        std::string chordName;
        auto root = chordTimeline[i].chord.root;
        const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        chordName += notes[static_cast<int>(root)];
        
        auto q = chordTimeline[i].chord.quality;
        if (q == ChordQuality::Minor) chordName += "m";
        else if (q == ChordQuality::Minor7) chordName += "m7";
        else if (q == ChordQuality::Dominant7) chordName += "7";
        else if (q == ChordQuality::Major7) chordName += "maj7";
        else if (q == ChordQuality::HalfDiminished7) chordName += "m7b5";

        MIDI::PianoVoicing v = planner.GetVoicingForChordIndex(i);

        std::string soprano = PitchToName(v.GetPitch(MIDI::PianoTargetRole::RH_Top));
        std::string innerH = PitchToName(v.GetPitch(MIDI::PianoTargetRole::RH_GuideHigh));
        std::string innerL = PitchToName(v.GetPitch(MIDI::PianoTargetRole::RH_GuideLow));
        std::string bass = PitchToName(v.GetPitch(MIDI::PianoTargetRole::LH_Root));

        std::cout << std::left 
                  << std::setw(8) << std::fixed << std::setprecision(1) << beat
                  << std::setw(15) << chordName
                  << std::setw(20) << soprano
                  << std::setw(20) << innerH
                  << std::setw(20) << innerL
                  << std::setw(20) << bass
                  << "\n";
    }

    std::cout << "==========================================\n";
    return 0;
}
