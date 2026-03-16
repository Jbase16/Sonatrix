#pragma once

#include "SampleZone.h"
#include <string>

namespace Sonatrix {
namespace Core {
namespace Audio {

class AssetManager {
public:
  static AssetManager &GetInstance() {
    static AssetManager instance;
    return instance;
  }

  // Loads sparse acoustic guitar anchors (e.g., E2, A2, D3, G3, B3, E4)
  bool LoadAcousticGuitarAnchors(const std::string &directoryPath);

  // Retrieve the global acoustic guitar articulation
  const InstrumentArticulation &GetAcousticGuitarArticulation() const {
    return acousticGuitar_;
  }

  // Loads sparse electric bass anchors (e.g., B1, C3, C4)
  bool LoadElectricBassAnchors(const std::string &directoryPath);

  // Retrieve the global electric bass articulation
  const InstrumentArticulation &GetElectricBassArticulation() const {
    return electricBass_;
  }

private:
  AssetManager() = default;
  ~AssetManager() = default;

  // Non-copyable
  AssetManager(const AssetManager &) = delete;
  AssetManager &operator=(const AssetManager &) = delete;

  InstrumentArticulation acousticGuitar_;
  InstrumentArticulation electricBass_;
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
