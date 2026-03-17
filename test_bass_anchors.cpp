#include "src/core/audio/AssetManager.h"
#include "src/core/audio/BassVoiceManager.h"
#include "src/core/engines/bass/BassCompiler.h"
#include "src/core/mir/MIRPattern.h"
#include "src/core/mir/PatternLibrary.h"
#include "src/core/arrangement/ChordTrack.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace Sonatrix::Core;

// -----------------------------------------------------------------------------
// CoreAudio WAV Write
// -----------------------------------------------------------------------------
bool SaveWav(const std::string &path, const std::vector<float> &data,
             double sampleRate) {
  CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(),
                                               kCFStringEncodingUTF8);
  if (!str)
    return false;

  CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, str,
                                               kCFURLPOSIXPathStyle, false);
  CFRelease(str);
  if (!url)
    return false;

  AudioStreamBasicDescription fileFormat = {};
  fileFormat.mSampleRate = sampleRate;
  fileFormat.mFormatID = kAudioFormatLinearPCM;
  fileFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  fileFormat.mFramesPerPacket = 1;
  fileFormat.mChannelsPerFrame = 2; // stereo
  fileFormat.mBitsPerChannel = 32;
  fileFormat.mBytesPerPacket = 8;
  fileFormat.mBytesPerFrame = 8;

  ExtAudioFileRef audioFile = nullptr;
  if (ExtAudioFileCreateWithURL(url, kAudioFileWAVEType, &fileFormat, nullptr,
                                kAudioFileFlags_EraseFile, &audioFile) !=
      noErr) {
    CFRelease(url);
    return false;
  }
  CFRelease(url);

  AudioStreamBasicDescription clientFormat = fileFormat;
  if (ExtAudioFileSetProperty(audioFile, kExtAudioFileProperty_ClientDataFormat,
                              sizeof(clientFormat), &clientFormat) != noErr) {
    ExtAudioFileDispose(audioFile);
    return false;
  }

  AudioBufferList bufferList = {};
  bufferList.mNumberBuffers = 1;
  bufferList.mBuffers[0].mNumberChannels = 2;
  bufferList.mBuffers[0].mDataByteSize =
      static_cast<UInt32>(data.size() * sizeof(float));
  bufferList.mBuffers[0].mData = const_cast<float *>(data.data());

  UInt32 frameCount = static_cast<UInt32>(data.size() / 2);
  const bool ok =
      ExtAudioFileWrite(audioFile, frameCount, &bufferList) == noErr;

  ExtAudioFileDispose(audioFile);
  return ok;
}

static inline int64_t TicksToSample(double ticks, double sampleRate) {
  constexpr double ticksPerSecond = 1920.0;
  return static_cast<int64_t>((ticks / ticksPerSecond) * sampleRate);
}

int main() {
  const std::string patternId = "acoustic_island_soft"; // Known to exist
  const float outputGain = 0.4f;
  const std::string outputPath = "test_bass_render.wav";

  const double sampleRate = 44100.0;
  const size_t blockSize = 256;
  const double totalSeconds = 4.0; 
  const size_t numFrames = static_cast<size_t>(totalSeconds * sampleRate);

  std::cout << "1. Initializing Bass Engine..." << std::endl;

  if (!Audio::AssetManager::GetInstance().LoadElectricBassAnchors("assets/Samples/bass_clean")) {
      std::cerr << "Failed to load Bass Anchors" << std::endl;
      return 1;
  }

  auto &articulation = Audio::AssetManager::GetInstance().GetElectricBassArticulation();
  std::cout << "   Active Articulation: " << articulation.name << " (Zones: " << articulation.zones.size() << ")" << std::endl;

  std::cout << "2. Creating Chord Timeline..." << std::endl;
  std::vector<ChordTrackEvent> chordTimeline;
  ChordTrackEvent e1; e1.position = MusicalTime(0); e1.chord.root = PitchClass::G; chordTimeline.push_back(e1);
  ChordTrackEvent e2; e2.position = BeatsToTime(2.0); e2.chord.root = PitchClass::C; chordTimeline.push_back(e2);

  std::cout << "3. Loading MIR Patterns..." << std::endl;
  PatternLibrary::GetInstance().LoadFromJSON("assets/Patterns/default_library.json");
  
  auto tmpl = PatternLibrary::GetInstance().GetTemplate(patternId);
  if (!tmpl) {
      std::cerr << "Template NOT FOUND: " << patternId << std::endl;
      return 1;
  }
  
  // Guard against missing engine data
  if (tmpl->patterns.find(MIRPattern::TargetEngine::Bass) == tmpl->patterns.end()) {
      std::cerr << "Engine data NOT FOUND for " << (int)MIRPattern::TargetEngine::Bass << std::endl;
      return 1;
  }
  
  // Use the native Bass pattern
  auto pattern = tmpl->patterns.at(MIRPattern::TargetEngine::Bass);
  std::cout << "   Using native timing from " << patternId << " Bass pattern (Events: " << pattern->events.size() << ")" << std::endl;

  Audio::BassVoiceManager voiceManager;
  voiceManager.LoadElectricBassKit("assets/Samples/bass_clean");

  std::cout << "4. Compiling Bass MIDI..." << std::endl;
  auto compiler = MIDI::CreateBassEngine();
  MIDI::MIDIStream bassMIDI;

  EditorClip clip(pattern);
  clip.timelineStart = MusicalTime(0);
  bassMIDI = compiler->CompileClip(clip, chordTimeline);
  bassMIDI.SortByTime();

  std::cout << "5. Rendering (4 beats)..." << std::endl;

  std::vector<float> outputData(numFrames * 2, 0.0f);
  std::vector<float> leftBuffer(blockSize, 0.0f);
  std::vector<float> rightBuffer(blockSize, 0.0f);
  float *outChannels[2] = {leftBuffer.data(), rightBuffer.data()};

  size_t eventIndex = 0;
  std::vector<std::pair<int64_t, MIDI::MIDIEvent>> scheduled;
  for (const auto &ev : bassMIDI.events) {
      scheduled.push_back({TicksToSample(ev.timelinePosition.ticks, sampleRate), ev});
  }

  for (size_t blockStart = 0; blockStart < numFrames; blockStart += blockSize) {
      size_t frames = std::min(blockSize, numFrames - blockStart);
      std::fill(leftBuffer.begin(), leftBuffer.end(), 0.0f);
      std::fill(rightBuffer.begin(), rightBuffer.end(), 0.0f);

      size_t rendered = 0;
      while (eventIndex < scheduled.size() && scheduled[eventIndex].first < (int64_t)(blockStart + frames)) {
          size_t offset = std::max((size_t)0, (size_t)(scheduled[eventIndex].first - blockStart));
          size_t toRender = offset - rendered;
          if (toRender > 0) {
              float *subs[2] = {leftBuffer.data() + rendered, rightBuffer.data() + rendered};
              voiceManager.RenderAudio(subs, toRender, 2);
              rendered += toRender;
          }
          std::vector<MIDI::MIDIEvent> evs = {scheduled[eventIndex].second};
          voiceManager.ProcessMIDI(evs);
          eventIndex++;
      }

      if (rendered < frames) {
          float *subs[2] = {leftBuffer.data() + rendered, rightBuffer.data() + rendered};
          voiceManager.RenderAudio(subs, frames - rendered, 2);
      }

      for (size_t i = 0; i < frames; ++i) {
          outputData[(blockStart + i) * 2] = leftBuffer[i] * outputGain;
          outputData[(blockStart + i) * 2 + 1] = rightBuffer[i] * outputGain;
      }
  }

  std::cout << "6. Saving " << outputPath << "..." << std::endl;
  SaveWav(outputPath, outputData, sampleRate);
  std::cout << "Done." << std::endl;

  return 0;
}
