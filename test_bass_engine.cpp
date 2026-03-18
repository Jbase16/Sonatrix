#include "src/core/audio/AssetManager.h"
#include "src/core/audio/BassVoiceManager.h"
#include "src/core/engines/bass/BassCompiler.h"
#include "src/core/mir/MIRPattern.h"
#include "src/core/mir/PatternLibrary.h"
#include "src/core/arrangement/ChordTrack.h"
#include "src/core/audio/AudioExporter.h"
#include "src/core/audio/PlaybackInstrument.h"
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
    if (pitch == 0) return "N/A";
    const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int octave = (pitch / 12) - 1; // MIDI Note 60 is C4. 60/12 = 5. 5-1 = 4.
    int noteIndex = pitch % 12;
    std::string name = notes[noteIndex] + std::to_string(octave);
    return name;
}

int main() {
  const std::string patternId = "acoustic_folk_drive";
  
  std::cout << "==========================================\n";
  std::cout << "      BASS ENGINE COMPILATION TRACE\n";
  std::cout << "==========================================\n";

  std::cout << "1. Base Initialization...\n";
  if (!Audio::AssetManager::GetInstance().LoadElectricBassAnchors("assets/Samples/bass_clean")) {
      std::cerr << "Failed to load Bass Anchors\n";
      return 1;
  }
  auto &articulation = Audio::AssetManager::GetInstance().GetElectricBassArticulation();
  std::cout << "   Active Kit:     " << articulation.name << "\n";
  std::cout << "   Loaded Anchors: ";
  for (const auto& zone : articulation.zones) {
      std::cout << PitchToName(zone.rootKey) << " ";
  }
  std::cout << "\n\n";

  std::cout << "2. Harmonic Timeline (Progression)....\n";
  std::vector<ChordTrackEvent> chordTimeline;
  
  // Am7 -> F -> C -> G
  ChordTrackEvent e1; e1.position = MusicalTime(0); e1.chord.root = PitchClass::A; chordTimeline.push_back(e1);
  ChordTrackEvent e2; e2.position = BeatsToTime(4.0); e2.chord.root = PitchClass::F; chordTimeline.push_back(e2);
  ChordTrackEvent e3; e3.position = BeatsToTime(8.0); e3.chord.root = PitchClass::C; chordTimeline.push_back(e3);
  ChordTrackEvent e4; e4.position = BeatsToTime(12.0); e4.chord.root = PitchClass::G; chordTimeline.push_back(e4);

  for (const auto& ev : chordTimeline) {
      double beat = static_cast<double>(ev.position.ticks) / 960.0;
      std::cout << "   Beat " << beat << ": Root = " << (int)ev.chord.root << "\n";
  }
  std::cout << "\n";

  std::cout << "3. MIR Pattern Extraction...\n";
  PatternLibrary::GetInstance().LoadFromJSON("assets/Patterns/default_library.json");
  auto tmpl = PatternLibrary::GetInstance().GetTemplate(patternId);
  if (!tmpl || tmpl->patterns.find(MIRPattern::TargetEngine::Bass) == tmpl->patterns.end()) {
      std::cerr << "Bass pattern NOT FOUND for " << patternId << "\n";
      return 1;
  }
  auto pattern = tmpl->patterns.at(MIRPattern::TargetEngine::Bass);
  std::cout << "   " << patternId << " Bass (Events: " << pattern->events.size() << ")\n\n";

  std::cout << "4. BassCompiler Translation...\n";
  auto compiler = MIDI::CreateBassEngine();
  EditorClip clip(pattern);
  clip.timelineStart = MusicalTime(0);
  MIDI::MIDIStream bassMIDI = compiler->CompileClip(clip, chordTimeline);
  bassMIDI.SortByTime();

  // Print the trace
  std::cout << "   " << std::left 
            << std::setw(8) << "Beat"
            << std::setw(12) << "Function"
            << std::setw(15) << "Target Pitch"
            << std::setw(15) << "Chosen Anchor"
            << std::setw(10) << "Shift"
            << "\n";
  std::cout << "   --------------------------------------------------------------\n";

  size_t eventIndex = 0;
  for (const auto &ev : bassMIDI.events) {
      if (ev.type != MIDI::MIDIEventType::NoteOn) continue;
      
      double beat = static_cast<double>(ev.timelinePosition.ticks) / 960.0;
      uint8_t targetPitch = ev.data1;
      
      // We look back into the MIR pattern to find the ActionParameter mapping
      int actionParam = 0;
      if (eventIndex < pattern->events.size()) {
          actionParam = pattern->events[eventIndex].actionParameter;
      }
      
      // Determine what FindZone() decides
      const Audio::SampleZone *zone = articulation.FindZone(targetPitch, ev.data2);
      std::string anchorStr = zone ? PitchToName(zone->rootKey) : "MISS";
      int shift = zone ? (static_cast<int>(targetPitch) - static_cast<int>(zone->rootKey)) : 0;
      std::string shiftMode = (shift > 0) ? "+" : "";

      std::string functionStr = "Root";
      if (actionParam == 1) functionStr = "Fifth";
      else if (actionParam == 2) functionStr = "Octave";
      else if (actionParam == 3 || actionParam == 4) {
          // Find next chord root for hint
          int nextRoot = -1;
          for (const auto& cEv : chordTimeline) {
              if (cEv.position > ev.timelinePosition) {
                  nextRoot = (int)cEv.chord.root;
                  break;
              }
          }
          functionStr = (actionParam == 3 ? "Appr-Lo" : "Appr-Hi");
          if (nextRoot != -1) {
              const char* notes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
              functionStr += " -> " + std::string(notes[nextRoot % 12]);
          }
      }

      std::cout << "   " << std::left 
                << std::setw(8) << std::fixed << std::setprecision(2) << beat
                << std::setw(12) << functionStr
                << std::setw(15) << (PitchToName(targetPitch) + " (" + std::to_string(targetPitch) + ")")
                << std::setw(15) << anchorStr
                << std::setw(10) << (shiftMode + std::to_string(shift))
                << "\n";
                
      eventIndex++;
  }

  std::cout << "==========================================\n";
  
  std::cout << "\n5. Audio Export...\n";
  std::vector<float> busVolumes = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f}; // 5 buses
  bool success = Audio::AudioExporter::BounceOffline(
      "test_bass_engine.wav",
      bassMIDI.events,
      "assets/Samples/bass_clean",
      Audio::PlaybackInstrument::ElectricBass,
      busVolumes,
      44100.0,
      120.0
  );

  if (success) {
      std::cout << "   Successfully exported to test_bass_engine.wav\n";
  } else {
      std::cerr << "   Export FAILED\n";
  }

  return 0;
}
