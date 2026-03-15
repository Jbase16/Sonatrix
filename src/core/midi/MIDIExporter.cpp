#include "MIDIExporter.h"
#include <fstream>

namespace Sonatrix {
namespace Core {
namespace MIDI {

static void Write32Bit(std::vector<uint8_t> &buf, uint32_t val) {
  buf.push_back((val >> 24) & 0xFF);
  buf.push_back((val >> 16) & 0xFF);
  buf.push_back((val >> 8) & 0xFF);
  buf.push_back(val & 0xFF);
}

static void Write16Bit(std::vector<uint8_t> &buf, uint16_t val) {
  buf.push_back((val >> 8) & 0xFF);
  buf.push_back(val & 0xFF);
}

static void WriteVLQ(std::vector<uint8_t> &buf, uint32_t val) {
  uint32_t buffer = val & 0x7F;
  while ((val >>= 7)) {
    buffer <<= 8;
    buffer |= ((val & 0x7F) | 0x80);
  }
  while (true) {
    buf.push_back(buffer & 0xFF);
    if (buffer & 0x80)
      buffer >>= 8;
    else
      break;
  }
}

bool MIDIExporter::ExportToSMF(const std::string &outputPath,
                               const std::vector<MIDIEvent> &midiStream,
                               uint16_t ppq,
                               double tempoBPM) {
  std::vector<uint8_t> data;

  // 1. Header Chunk
  data.push_back('M');
  data.push_back('T');
  data.push_back('h');
  data.push_back('d');
  Write32Bit(data, 6);   // Header size
  Write16Bit(data, 0);   // Format 0 (single track)
  Write16Bit(data, 1);   // 1 track
  Write16Bit(data, ppq); // PPQ resolution

  // 2. Track Chunk
  std::vector<uint8_t> trackData;

  // Set Tempo Meta Event.
  const double safeTempoBPM = (tempoBPM > 0.0) ? tempoBPM : 120.0;
  const uint32_t microsPerQuarter =
      static_cast<uint32_t>(60000000.0 / safeTempoBPM);
  WriteVLQ(trackData, 0);
  trackData.push_back(0xFF);
  trackData.push_back(0x51);
  trackData.push_back(0x03);
  trackData.push_back((microsPerQuarter >> 16) & 0xFF);
  trackData.push_back((microsPerQuarter >> 8) & 0xFF);
  trackData.push_back(microsPerQuarter & 0xFF);

  uint32_t lastTick = 0;
  for (const auto &ev : midiStream) {
    // Only trigger positive deltas (should be sorted already)
    uint32_t currentTick = static_cast<uint32_t>(ev.timelinePosition.ticks);
    uint32_t delta = (currentTick >= lastTick) ? (currentTick - lastTick) : 0;
    lastTick = currentTick;

    WriteVLQ(trackData, delta);

    uint8_t status = static_cast<uint8_t>(ev.type) | (ev.channel & 0x0F);
    trackData.push_back(status);
    trackData.push_back(ev.data1);
    trackData.push_back(ev.data2);
  }

  // End of Track Meta Event
  WriteVLQ(trackData, 0);
  trackData.push_back(0xFF);
  trackData.push_back(0x2F);
  trackData.push_back(0x00);

  // Write Track Header
  data.push_back('M');
  data.push_back('T');
  data.push_back('r');
  data.push_back('k');
  Write32Bit(data, static_cast<uint32_t>(trackData.size()));
  data.insert(data.end(), trackData.begin(), trackData.end());

  // 3. Write to file
  std::ofstream file(outputPath, std::ios::out | std::ios::binary);
  if (!file)
    return false;
  file.write(reinterpret_cast<const char *>(data.data()), data.size());
  file.close();

  return true;
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
