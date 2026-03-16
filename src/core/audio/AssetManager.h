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

  bool LoadAcousticGuitarAnchors(const std::string &directoryPath);
  const InstrumentArticulation &GetAcousticGuitarArticulation() const {
    return acousticGuitar_;
  }

  bool LoadElectricBassAnchors(const std::string &directoryPath);
  const InstrumentArticulation &GetElectricBassArticulation() const {
    return electricBass_;
  }

  bool LoadMockBassAnchors(const std::string &directoryPath);
  const InstrumentArticulation &GetMockBassArticulation() const {
    return mockBass_;
  }

private:
  AssetManager() = default;
  ~AssetManager() = default;

  AssetManager(const AssetManager &) = delete;
  AssetManager &operator=(const AssetManager &) = delete;

  InstrumentArticulation acousticGuitar_;
  InstrumentArticulation electricBass_;
  InstrumentArticulation mockBass_;
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
