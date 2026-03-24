#include "src/core/audio/AssetManager.h"
#include "src/core/audio/PianoVoiceManager.h"
#include "src/core/audio/AudioExporter.h"
#include "src/core/audio/PlaybackInstrument.h"
#include "src/core/engines/piano/PianoCompiler.h"
#include "src/core/engines/piano/PianoVoicingPlanner.h"
#include "src/core/mir/MIRPattern.h"
#include "src/core/mir/PatternLibrary.h"
#include "src/core/arrangement/ChordTrack.h"
#include "src/core/mir/MusicalTime.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>

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

static std::string RoleName(int role) {
    switch (role) {
        case 0: return "LH_Root";
        case 1: return "LH_5th";
        case 2: return "LH_Oct";
        case 3: return "LH_Sh";
        case 4: return "RH_Lo";
        case 5: return "RH_Hi";
        case 6: return "RH_In";
        case 7: return "RH_Tp";
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

// Build a ChordTrackEvent vector from ProgressionEntry list
static std::vector<ChordTrackEvent> BuildTimeline(const std::vector<ProgressionEntry>& entries) {
    std::vector<ChordTrackEvent> timeline;
    for (const auto& e : entries) {
        ChordTrackEvent ev;
        ev.position = MusicalTime(static_cast<int64_t>(e.beat * 960));
        ev.chord.root = e.root;
        ev.chord.quality = e.quality;
        ev.chord.overBass = e.overBass;
        timeline.push_back(ev);
    }
    return timeline;
}

// Run a full compile + trace + render for one style/progression combo
static bool RunRender(const char* title, const char* outputFile,
                      MIDI::PianoStyle style, MIDI::SopranoContour contour,
                      const std::vector<ProgressionEntry>& entries,
                      const std::string& patternId) {

    std::cout << "\n=== " << title << " [" << StyleName(style) << "] ===\n";

    // Build chord timeline
    auto chordTimeline = BuildTimeline(entries);

    // Get pattern
    auto tmpl = PatternLibrary::GetInstance().GetTemplate(patternId);
    if (!tmpl || tmpl->patterns.find(MIRPattern::TargetEngine::Piano) == tmpl->patterns.end()) {
        std::cerr << "  Piano pattern NOT FOUND for " << patternId << "\n";
        return false;
    }
    auto pattern = tmpl->patterns.at(MIRPattern::TargetEngine::Piano);

    // Compile MIDI — tile pattern across each chord position
    MIDI::PianoCompiler compiler(style, contour);
    MIDI::MIDIStream midiStream;
    for (size_t ci = 0; ci < entries.size(); ++ci) {
        EditorClip clip(pattern);
        clip.timelineStart = MusicalTime(static_cast<int64_t>(entries[ci].beat * 960));
        MIDI::MIDIStream chordStream = compiler.CompileClip(clip, chordTimeline);
        for (auto& ev : chordStream.events) {
            midiStream.events.push_back(ev);
        }
    }
    midiStream.SortByTime();

    // Dump resolved voicings per chord
    MIDI::PianoVoicingPlanner dumpPlanner(style, contour);
    auto voicings = dumpPlanner.SolveTimeline(chordTimeline);
    std::cout << "  Voicings:\n";
    for (size_t i = 0; i < entries.size() && i < voicings.size(); ++i) {
        const auto& v = voicings[i];
        uint8_t lh2 = v.GetPitch(MIDI::PianoTargetRole::LH_Fifth);
        if (lh2 == 0) lh2 = v.GetPitch(MIDI::PianoTargetRole::LH_ShellLow);
        std::cout << "    " << std::setw(8) << std::left << entries[i].label
                  << "LH=" << std::setw(5) << PitchToName(v.GetPitch(MIDI::PianoTargetRole::LH_Root))
                  << std::setw(5) << PitchToName(lh2)
                  << " | RH="
                  << std::setw(5) << PitchToName(v.GetPitch(MIDI::PianoTargetRole::RH_GuideLow))
                  << std::setw(5) << PitchToName(v.GetPitch(MIDI::PianoTargetRole::RH_Inner))
                  << std::setw(5) << PitchToName(v.GetPitch(MIDI::PianoTargetRole::RH_GuideHigh))
                  << std::setw(5) << PitchToName(v.GetPitch(MIDI::PianoTargetRole::RH_Top))
                  << " span=" << v.RHSpan() << " den=" << v.RHDensity()
                  << "\n";
    }

    // Trace MIDI events with anchor analysis
    auto& articulation = Audio::AssetManager::GetInstance().GetAcousticPianoArticulation();

    std::cout << "  " << std::left
              << std::setw(8) << "Beat"
              << std::setw(8) << "Chord"
              << std::setw(8) << "Role"
              << std::setw(10) << "Pitch"
              << std::setw(8) << "Anchor"
              << std::setw(6) << "Shift"
              << std::setw(5) << "Vel"
              << "\n";
    std::cout << "  " << std::string(53, '-') << "\n";

    // Match each NoteOn to the chord it falls under
    size_t noteOnCount = 0;
    for (const auto& ev : midiStream.events) {
        if (ev.type != MIDI::MIDIEventType::NoteOn || ev.data2 == 0) continue;

        double beat = static_cast<double>(ev.timelinePosition.ticks) / 960.0;
        uint8_t pitch = ev.data1;
        uint8_t vel = ev.data2;

        // Find chord label
        const char* chordLabel = "?";
        for (size_t i = 0; i < entries.size(); ++i) {
            double entryBeat = entries[i].beat;
            if (beat >= entryBeat) chordLabel = entries[i].label;
        }

        // Find anchor
        const Audio::SampleZone* zone = articulation.FindZone(pitch, vel);
        std::string anchorStr = zone ? PitchToName(zone->rootKey) : "MISS";
        int shift = zone ? (static_cast<int>(pitch) - static_cast<int>(zone->rootKey)) : 0;
        std::string shiftStr = (shift >= 0 ? "+" : "") + std::to_string(shift);

        // Role from pattern (action parameter cycles through pattern events per chord)
        size_t patEvents = pattern->events.size();
        int actionParam = (patEvents > 0) ? pattern->events[noteOnCount % patEvents].actionParameter : -1;
        std::string roleStr = (actionParam >= 0) ? RoleName(actionParam) : "?";

        std::cout << "  " << std::left << std::fixed << std::setprecision(1)
                  << std::setw(8) << beat
                  << std::setw(8) << chordLabel
                  << std::setw(8) << roleStr
                  << std::setw(10) << (PitchToName(pitch) + " (" + std::to_string(pitch) + ")")
                  << std::setw(8) << anchorStr
                  << std::setw(6) << shiftStr
                  << std::setw(5) << static_cast<int>(vel)
                  << "\n";
        noteOnCount++;
    }

    // Render WAV
    std::cout << "  Rendering " << outputFile << "...\n";
    std::vector<float> busVolumes = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    bool success = Audio::AudioExporter::BounceOffline(
        outputFile,
        midiStream.events,
        "assets/Audio/Piano",
        Audio::PlaybackInstrument::AcousticPiano,
        busVolumes,
        44100.0,
        120.0
    );

    if (success) {
        std::cout << "  >> OK: " << outputFile << "\n";
    } else {
        std::cerr << "  >> FAILED: " << outputFile << "\n";
    }
    return success;
}

int main() {
    std::cout << "==========================================\n";
    std::cout << "  PIANO ANCHOR LISTENING TEST v1\n";
    std::cout << "==========================================\n";

    // Load anchors
    std::cout << "1. Loading Piano Anchors...\n";
    if (!Audio::AssetManager::GetInstance().LoadAcousticPianoAnchors("assets/Audio/Piano")) {
        std::cerr << "FATAL: Failed to load piano anchors\n";
        return 1;
    }
    auto& articulation = Audio::AssetManager::GetInstance().GetAcousticPianoArticulation();
    std::cout << "   Kit: " << articulation.name << "\n";
    std::cout << "   Anchors: ";
    for (const auto& zone : articulation.zones) {
        std::cout << PitchToName(zone.rootKey) << " ";
    }
    std::cout << "\n";

    // Load pattern library
    std::cout << "2. Loading patterns...\n";
    PatternLibrary::GetInstance().LoadFromJSON("assets/Patterns/default_library.json");

    // Progressions
    auto iiVI = std::vector<ProgressionEntry>{
        Ch(0,  PitchClass::A,  ChordQuality::Minor7,          "Am7"),
        Ch(4,  PitchClass::D,  ChordQuality::Dominant7,       "D7"),
        Ch(8,  PitchClass::G,  ChordQuality::Major7,          "Gmaj7"),
        Ch(12, PitchClass::C,  ChordQuality::Major7,          "Cmaj7"),
    };

    auto backdoor = std::vector<ProgressionEntry>{
        Ch(0,  PitchClass::C,  ChordQuality::Major7,    "Cmaj7"),
        Ch(4,  PitchClass::A,  ChordQuality::Minor7,    "Am7"),
        Ch(8,  PitchClass::D,  ChordQuality::Minor7,    "Dm7"),
        Ch(12, PitchClass::A_Sharp,  ChordQuality::Dominant7, "Bb7"),
    };

    auto pedalBass = std::vector<ProgressionEntry>{
        Sl(0,  PitchClass::C,  ChordQuality::Major,  "C",    PitchClass::C),
        Sl(4,  PitchClass::D,  ChordQuality::Minor,  "Dm/C", PitchClass::C),
        Sl(8,  PitchClass::E,  ChordQuality::Minor,  "Em/C", PitchClass::C),
        Sl(12, PitchClass::F,  ChordQuality::Major,  "F/C",  PitchClass::C),
    };

    // Render per style
    std::cout << "\n3. Compiling & Rendering...\n";

    int successes = 0, failures = 0;

    // ii-V-I across all styles
    if (RunRender("ii-V-I", "test_piano_pop_iiVI.wav",
                  MIDI::PianoStyle::PopBlock, MIDI::SopranoContour::Hold,
                  iiVI, "piano_pop_block")) successes++; else failures++;

    if (RunRender("ii-V-I", "test_piano_ss_iiVI.wav",
                  MIDI::PianoStyle::SingerSongwriter, MIDI::SopranoContour::Hold,
                  iiVI, "piano_pop_block")) successes++; else failures++;

    if (RunRender("ii-V-I", "test_piano_jazz_iiVI.wav",
                  MIDI::PianoStyle::JazzShell, MIDI::SopranoContour::Hold,
                  iiVI, "piano_pop_block")) successes++; else failures++;

    // Backdoor dominant - Pop
    if (RunRender("Backdoor Dom", "test_piano_pop_backdoor.wav",
                  MIDI::PianoStyle::PopBlock, MIDI::SopranoContour::Hold,
                  backdoor, "piano_pop_block")) successes++; else failures++;

    // Pedal bass - Pop
    if (RunRender("Pedal Bass", "test_piano_pop_pedal.wav",
                  MIDI::PianoStyle::PopBlock, MIDI::SopranoContour::Hold,
                  pedalBass, "piano_pop_block")) successes++; else failures++;

    // Broken syncopation - Pop
    if (RunRender("Broken Syncopation", "test_piano_pop_broken.wav",
                  MIDI::PianoStyle::PopBlock, MIDI::SopranoContour::Hold,
                  iiVI, "piano_pop_broken")) successes++; else failures++;

    std::cout << "\n==========================================\n";
    std::cout << "Results: " << successes << " OK, " << failures << " FAILED\n";
    std::cout << "==========================================\n";
    return (failures > 0) ? 1 : 0;
}
