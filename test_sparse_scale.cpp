#include "src/core/audio/AssetManager.h"
#include "src/core/audio/GranularVoice.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace Sonatrix::Core::Audio;

// -----------------------------------------------------------------------------
// CoreAudio WAV I/O (Copied from test_pecr.cpp for standalone test)
// -----------------------------------------------------------------------------
bool LoadWav(const std::string &path, std::vector<float> &outData,
             double targetSampleRate) {
  outData.clear();
  CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(),
                                              kCFStringEncodingUTF8);
  if (!str)
    return false;
  CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, str,
                                               kCFURLPOSIXPathStyle, false);
  CFRelease(str);
  if (!url)
    return false;

  ExtAudioFileRef audioFile = nullptr;
  if (ExtAudioFileOpenURL(url, &audioFile) != noErr) {
    CFRelease(url);
    return false;
  }
  CFRelease(url);

  AudioStreamBasicDescription sourceFormat = {};
  UInt32 srcSize = sizeof(sourceFormat);
  if (ExtAudioFileGetProperty(audioFile, kExtAudioFileProperty_FileDataFormat,
                              &srcSize, &sourceFormat) != noErr) {
    ExtAudioFileDispose(audioFile);
    return false;
  }

  AudioStreamBasicDescription clientFormat = {};
  clientFormat.mSampleRate = targetSampleRate;
  clientFormat.mFormatID = kAudioFormatLinearPCM;
  clientFormat.mFormatFlags =
      kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  clientFormat.mBytesPerPacket = sizeof(float);
  clientFormat.mFramesPerPacket = 1;
  clientFormat.mBytesPerFrame = sizeof(float);
  clientFormat.mChannelsPerFrame =
      1; // Interleaved Mono for simplicity in this test
  clientFormat.mBitsPerChannel = 32;

  if (ExtAudioFileSetProperty(audioFile, kExtAudioFileProperty_ClientDataFormat,
                              sizeof(clientFormat), &clientFormat) != noErr) {
    ExtAudioFileDispose(audioFile);
    return false;
  }

  SInt64 sourceFrames = 0;
  UInt32 frameSize = sizeof(sourceFrames);
  if (ExtAudioFileGetProperty(audioFile, kExtAudioFileProperty_FileLengthFrames,
                              &frameSize, &sourceFrames) != noErr ||
      sourceFrames <= 0) {
    ExtAudioFileDispose(audioFile);
    return false;
  }

  const double srcRate = (sourceFormat.mSampleRate > 0.0)
                             ? sourceFormat.mSampleRate
                             : targetSampleRate;
  const size_t estimatedTargetFrames = static_cast<size_t>(std::ceil(
      static_cast<double>(sourceFrames) * (targetSampleRate / srcRate)));

  outData.reserve(estimatedTargetFrames + 1024);
  constexpr UInt32 kChunkFrames = 16384;
  std::vector<float> chunk(kChunkFrames);

  while (true) {
    AudioBufferList bufferList = {};
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0].mNumberChannels = 1;
    bufferList.mBuffers[0].mDataByteSize =
        static_cast<UInt32>(chunk.size() * sizeof(float));
    bufferList.mBuffers[0].mData = chunk.data();

    UInt32 ioFrames = kChunkFrames;
    if (ExtAudioFileRead(audioFile, &ioFrames, &bufferList) != noErr) {
      ExtAudioFileDispose(audioFile);
      outData.clear();
      return false;
    }
    if (ioFrames == 0)
      break;
    outData.insert(outData.end(), chunk.begin(), chunk.begin() + ioFrames);
  }

  ExtAudioFileDispose(audioFile);
  return !outData.empty();
}

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
  fileFormat.mChannelsPerFrame = 1;
  fileFormat.mBitsPerChannel = 32;
  fileFormat.mBytesPerPacket = 4;
  fileFormat.mBytesPerFrame = 4;

  ExtAudioFileRef audioFile = nullptr;
  if (ExtAudioFileCreateWithURL(url, kAudioFileWAVEType, &fileFormat, nullptr,
                                kAudioFileFlags_EraseFile,
                                &audioFile) != noErr) {
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
  bufferList.mBuffers[0].mNumberChannels = 1;
  bufferList.mBuffers[0].mDataByteSize =
      static_cast<UInt32>(data.size() * sizeof(float));
  bufferList.mBuffers[0].mData = const_cast<float *>(data.data());

  UInt32 frameCount = static_cast<UInt32>(data.size());
  const bool ok =
      ExtAudioFileWrite(audioFile, frameCount, &bufferList) == noErr;
  ExtAudioFileDispose(audioFile);
  return ok;
}

int main() {
  const double sampleRate = 44100.0;

  std::cout << "Loading 6 Open-String Acoustic Anchors..." << std::endl;
  auto &assets = AssetManager::GetInstance();
  if (!assets.LoadAcousticGuitarAnchors("Assets/Exciters/FS_Guitars")) {
    std::cerr << "Failed to load acoustic anchors." << std::endl;
    return 1;
  }

  const auto &articulation = assets.GetAcousticGuitarArticulation();
  std::vector<float> outputData;

  std::cout << "Rendering Chromatic Scale from MIDI 50 (D3) to 74 (D5)..." << std::endl;

  for (uint8_t pitch = 50; pitch <= 74; pitch++) {
    const SampleZone *bestZone = articulation.FindZone(pitch, 100);
    if (!bestZone) {
      std::cerr << "FindZone returned nullptr for pitch " << (int)pitch << std::endl;
      continue;
    }

    double semitoneShift = static_cast<double>(pitch) - static_cast<double>(bestZone->rootKey);
    double pitchRatio = std::pow(2.0, semitoneShift / 12.0);

    GranularVoice voice;
    voice.Start(bestZone, pitch, pitchRatio, 1.0f);

    std::cout << "Note: " << (int)pitch << " -> Anchor: " << bestZone->filePath 
              << " (Shift: " << semitoneShift << ")" << std::endl;

    // Render 0.5s of sustain
    const size_t sustainFrames = static_cast<size_t>(0.5 * sampleRate);
    std::vector<float> noteData(sustainFrames, 0.0f);
    float *outChannels[1] = {noteData.data()};
    voice.RenderNextBlock(outChannels, sustainFrames, 1);

    // Stop and render 0.1s of release
    voice.Stop();
    const size_t releaseFrames = static_cast<size_t>(0.1 * sampleRate);
    std::vector<float> releaseData(releaseFrames, 0.0f);
    float *relChannels[1] = {releaseData.data()};
    voice.RenderNextBlock(relChannels, releaseFrames, 1);

    outputData.insert(outputData.end(), noteData.begin(), noteData.end());
    outputData.insert(outputData.end(), releaseData.begin(), releaseData.end());
  }

  std::cout << "Saving test_sparse_scale_output.wav..." << std::endl;
  SaveWav("test_sparse_scale_output.wav", outputData, sampleRate);
  std::cout << "Done." << std::endl;
  return 0;
}
