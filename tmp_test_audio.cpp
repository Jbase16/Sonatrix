#include "src/core/audio/VoiceManager.h"
#include "src/core/midi/MIDIEvent.h"
#include <cmath>
#include <iostream>
#include <vector>

using namespace Sonatrix::Core;

int main() {
  Audio::VoiceManager manager;
  manager.InitializeTestTones();

  std::cout << "Test Tones Initialized. Zone count: "
            << manager.GetTestArticulation().zones.size() << "\n";
  if (!manager.GetTestArticulation().zones.empty()) {
    std::cout << "Zone 0 Loaded: "
              << manager.GetTestArticulation().zones[0].isLoaded << "\n";
    std::cout << "Zone 0 Data Size: "
              << manager.GetTestArticulation().zones[0].audioData.size()
              << "\n";
  }

  std::vector<MIDI::MIDIEvent> events;
  MIDI::MIDIEvent ev;
  ev.type = MIDI::MIDIEventType::NoteOn;
  ev.data1 = 60;  // Middle C
  ev.data2 = 100; // Velocity
  events.push_back(ev);

  manager.ProcessMIDI(events, manager.GetTestArticulation());

  float chan1[512] = {0};
  float chan2[512] = {0};
  float *channels[2] = {chan1, chan2};

  manager.RenderAudio(channels, 512, 2);

  float sum = 0.0f;
  for (int i = 0; i < 512; ++i) {
    sum += std::abs(chan1[i]);
  }
  std::cout << "Sum of audio after RenderAudio (Attack): " << sum << "\n";

  // Simulate some time passing to hit ADSR sustain
  for (int b = 0; b < 10; ++b) {
    manager.RenderAudio(channels, 512, 2);
  }

  sum = 0.0f;
  for (int i = 0; i < 512; ++i) {
    sum += std::abs(chan1[i]);
  }
  std::cout << "Sum of audio after 10 blocks (Sustain): " << sum << "\n";

  return 0;
}
