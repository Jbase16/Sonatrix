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
  float volumeTrim{1.0f};
  uint8_t lowVelocity{0};
  uint8_t highVelocity{127};
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
    zone.lowVelocity = anchor.lowVelocity;
    zone.highVelocity = anchor.highVelocity;
    zone.volumeTrim = anchor.volumeTrim;
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
          // Octave 2
          {"AT2035 XY Angle Dn PD C2 40 55.wav", 36, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD C2 81 100.wav", 36, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD C2 115 127.wav", 36, 1.0f, 101, 127},
          {"AT2035 XY Angle Dn PD D#2 40 55.wav", 39, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD D#2 81 100.wav", 39, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD D#2 115 127.wav", 39, 1.0f, 101, 127},
          {"AT2035 XY Angle Dn PD F#2 40 55.wav", 42, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD F#2 81 100.wav", 42, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD F#2 115 127.wav", 42, 1.0f, 101, 127},
          {"AT2035 XY Angle Dn PD A2 40 55.wav", 45, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD A2 81 100.wav", 45, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD A2 115 127.wav", 45, 1.0f, 101, 127},

          // Octave 3
          {"AT2035 XY Angle Dn PD C3 40 55.wav", 48, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD C3 81 100.wav", 48, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD C3 115 127.wav", 48, 1.0f, 101, 127},
          {"AT2035 XY Angle Dn PD D#3 40 55.wav", 51, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD D#3 81 100.wav", 51, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD D#3 115 127.wav", 51, 1.0f, 101, 127},
          {"AT2035 XY Angle Dn PD F#3 40 55.wav", 54, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD F#3 81 100.wav", 54, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD F#3 115 127.wav", 54, 1.0f, 101, 127},
          {"AT2035 XY Angle Dn PD A3 40 55.wav", 57, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD A3 81 100.wav", 57, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD A3 115 127.wav", 57, 1.0f, 101, 127},

          // Octave 4
          {"AT2035 XY Angle Dn PD C4 40 55.wav", 60, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD C4 81 100.wav", 60, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD C4 115 127.wav", 60, 1.0f, 101, 127},
          {"AT2035 XY Angle Dn PD D#4 40 55.wav", 63, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD D#4 81 100.wav", 63, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD D#4 115 127.wav", 63, 1.0f, 101, 127},
          {"AT2035 XY Angle Dn PD F#4 40 55.wav", 66, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD F#4 81 100.wav", 66, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD F#4 115 127.wav", 66, 1.0f, 101, 127},
          {"AT2035 XY Angle Dn PD A4 40 55.wav", 69, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD A4 81 100.wav", 69, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD A4 115 127.wav", 69, 1.0f, 101, 127},

          // Octave 5
          {"AT2035 XY Angle Dn PD C5 40 55.wav", 72, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD C5 81 100.wav", 72, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD C5 115 127.wav", 72, 1.0f, 101, 127},
          {"AT2035 XY Angle Dn PD D#5 40 55.wav", 75, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD D#5 81 100.wav", 75, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD D#5 115 127.wav", 75, 1.0f, 101, 127},
          {"AT2035 XY Angle Dn PD F#5 40 55.wav", 78, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD F#5 81 100.wav", 78, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD F#5 115 127.wav", 78, 1.0f, 101, 127},
          {"AT2035 XY Angle Dn PD A5 40 55.wav", 81, 1.0f, 1, 55},
          {"AT2035 XY Angle Dn PD A5 81 100.wav", 81, 1.0f, 56, 100},
          {"AT2035 XY Angle Dn PD A5 115 127.wav", 81, 1.0f, 101, 127},
      },
      1.0f,
      acousticPiano_, "piano anchor");
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
