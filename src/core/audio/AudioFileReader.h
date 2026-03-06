#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Sonatrix {
namespace Core {
namespace Audio {

// -----------------------------------------------------------------------------
// AudioFileReader
//
// Utility for parsing native macOS .wav/aif files into contiguous float memory.
// It also provides a fallback mathematical tone generator if samples are
// missing, guaranteeing the voice-allocation engine can be proven without a
// 10GB disk footprint.
// -----------------------------------------------------------------------------

class AudioFileReader {
public:
  // Reads an interleaved stereo audio file into the destination buffer.
  // Downmixes/upmixes to 2 channels natively if required.
  // Returns true on success.
  static bool LoadFile(const std::string &absolutePath,
                       std::vector<float> &outBuffer, uint32_t &outSampleRate);

  // Generates a pure synthetic sine/triangle blend for Phase 6 structural
  // testing.
  static void GenerateTestTone(float frequencyHz, float durationSeconds,
                               std::vector<float> &outBuffer,
                               uint32_t sampleRate = 44100);
};

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
