#include "AssetManager.h"
#include "AudioFileReader.h"
#include <iostream>

namespace Sonatrix {
namespace Core {
namespace Audio {

bool AssetManager::LoadAcousticGuitarAnchors(const std::string &directoryPath) {
  acousticGuitar_.name = "Acoustic_Guitar_Granular";
  acousticGuitar_.zones.clear();

  // We load a sparse set of anchor samples. For a 6-string guitar, ideally
  // one per open string, or one per octave. We'll set up a generic mapping
  // here. For now, we mock loading a few anchors (similar to test_pecr.cpp
  // logic)

  // Example mappings: E2 (40), A2 (45), D3 (50), G3 (55), B3 (59), E4 (64)
  const int rootKeys[] = {40, 45, 50, 55, 59, 64};
  const std::string fileNames[] = {"/E2.wav", "/A2.wav", "/D3.wav",
                                   "/G3.wav", "/B3.wav", "/E4.wav"};

  for (int i = 0; i < 6; ++i) {
    SampleZone zone;
    zone.filePath = directoryPath + fileNames[i];
    zone.rootKey = rootKeys[i];
    zone.lowVelocity = 0;
    zone.highVelocity = 127;
    // For sparse matrix, let's span the keys between anchors
    // Actually, InstrumentArticulation::FindZone handles closest rootKey logic
    // if implemented properly.

    zone.isLoaded = AudioFileReader::LoadFile(zone.filePath, zone.audioData,
                                              zone.sampleRate);
    if (!zone.isLoaded) {
      std::cerr << "Sonatrix: Failed to load Granular Anchor: " << zone.filePath
                << "\n";
      // Don't fail completely, keep trying others
    } else {
      std::cout << "Sonatrix: Loaded Granular Anchor: " << zone.filePath << " ("
                << zone.audioData.size() / 2 << " frames)\n";
      acousticGuitar_.zones.push_back(zone);
    }
  }

  return !acousticGuitar_.zones.empty();
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
