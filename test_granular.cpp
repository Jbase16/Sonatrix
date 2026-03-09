#include "src/core/audio/AssetManager.h"
#include "src/core/audio/AudioExporter.h"
#include "src/core/audio/GranularVoice.h"
#include <iostream>
#include <vector>

using namespace Sonatrix::Core::Audio;

int main() {
  bool ok =
      AssetManager::GetInstance().LoadAcousticGuitarAnchors("assets/Guitar");
  if (!ok) {
    std::cout << "MOCKING asset load for the formant test since files don't "
                 "exist yet.\n";
  }

  // We need a 3-second C3 (midi 48 or 60 depending on convention, let's say 48
  // for guitar low C or 60 for middle C). Actually, let's create a dummy
  // SampleZone with a synthetic sine sweep, OR load an actual file if the user
  // has it.
  SampleZone c3Zone;
  c3Zone.rootKey = 60; // C3
  c3Zone.sampleRate = 44100;
  c3Zone.numChannels = 2;

  // We'll run generating a file or loading from raw.wav
  std::cout << "Formant Test: C3 to G4 Granular shift.\n";
  return 0;
}
