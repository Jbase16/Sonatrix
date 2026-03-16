#include "AssetManager.h"

#include "AudioFileReader.h"

#include <initializer_list>
#include <iostream>

namespace Sonatrix {
namespace Core {
namespace Audio {

namespace {

struct AnchorDefinition {
  const char *fileName;
  uint8_t rootKey;
};

bool LoadAnchorSet(const std::string &directoryPath,
                   const std::string &articulationName,
                   std::initializer_list<AnchorDefinition> anchors,
                   InstrumentArticulation &articulation,
                   const char *failureLabel) {
  articulation.name = articulationName;
  articulation.zones.clear();

  for (const auto &anchor : anchors) {
    SampleZone zone;
    zone.filePath = directoryPath + "/" + anchor.fileName;
    zone.rootKey = anchor.rootKey;
    zone.lowVelocity = 0;
    zone.highVelocity = 127;
    zone.isLoaded =
        AudioFileReader::LoadFile(zone.filePath, zone.audioData, zone.sampleRate);

    if (!zone.isLoaded) {
      std::cerr << "Sonatrix: Failed to load " << failureLabel << ": "
                << zone.filePath << "\n";
      continue;
    }

    articulation.zones.push_back(std::move(zone));
  }

  return !articulation.zones.empty();
}

} // namespace

bool AssetManager::LoadAcousticGuitarAnchors(const std::string &directoryPath) {
  return LoadAnchorSet(
      directoryPath, "Acoustic_Guitar_Anchors",
      {
          {"E2.wav", 40},
          {"A2.wav", 45},
          {"D3.wav", 50},
          {"G3.wav", 55},
          {"B3.wav", 59},
          {"E4.wav", 64},
      },
      acousticGuitar_, "guitar anchor");
}

bool AssetManager::LoadElectricBassAnchors(const std::string &directoryPath) {
  return LoadAnchorSet(
      directoryPath, "Electric_Bass_Anchors",
      {
          {"B1.wav", 35},
          {"C3.wav", 48},
          {"C4.wav", 60},
      },
      electricBass_, "bass anchor");
}

bool AssetManager::LoadMockBassAnchors(const std::string &directoryPath) {
  return LoadAnchorSet(
      directoryPath, "Bass_Sawtooth_Mock",
      {
          {"C1.wav", 36},
          {"C2.wav", 48},
          {"C3.wav", 60},
      },
      mockBass_, "mock bass anchor");
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
