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
                   PlaybackInstrument instrumentType,
                   std::initializer_list<AnchorDefinition> anchors,
                   float outputGain,
                   InstrumentArticulation &articulation,
                   const char *failureLabel) {
  articulation.name = articulationName;
  articulation.instrumentType = instrumentType;
  articulation.outputGain = outputGain;
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
      directoryPath, "Acoustic_Guitar_Anchors", PlaybackInstrument::Guitar,
      {
          {"E2.wav", 40},
          {"A2.wav", 45},
          {"D3.wav", 50},
          {"G3.wav", 55},
          {"B3.wav", 59},
          {"E4.wav", 64},
      },
      // Full guitar voicings stack several near-full-scale anchors at once, so
      // keep some built-in headroom before user mixer gain is applied.
      0.35f,
      acousticGuitar_, "guitar anchor");
}

bool AssetManager::LoadElectricBassAnchors(const std::string &directoryPath) {
  return LoadAnchorSet(
      directoryPath, "Electric_Bass_Anchors", PlaybackInstrument::ElectricBass,
      {
          {"B1.wav", 35},
          {"C3.wav", 48},
          {"C4.wav", 60},
      },
      1.0f,
      electricBass_, "bass anchor");
}

bool AssetManager::LoadMockBassAnchors(const std::string &directoryPath) {
  return LoadAnchorSet(
      directoryPath, "Bass_Sawtooth_Mock", PlaybackInstrument::MockBass,
      {
          {"C1.wav", 36},
          {"C2.wav", 48},
          {"C3.wav", 60},
      },
      1.0f,
      mockBass_, "mock bass anchor");
}

bool AssetManager::LoadAcousticPianoAnchors(const std::string &directoryPath) {
  return LoadAnchorSet(
      directoryPath, "Acoustic_Piano_Anchors", PlaybackInstrument::AcousticPiano,
      {
          {"16.wav", 36},
          {"028.wav", 48},
          {"40.wav", 60},
          {"052.wav", 72},
      },
      0.6f,
      acousticPiano_, "piano anchor");
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
