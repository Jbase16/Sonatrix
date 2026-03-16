#include "AudioExporter.h"

#include "BassVoiceManager.h"
#include "GuitarVoiceManager.h"

#include <AudioToolbox/AudioToolbox.h>
#include <algorithm>
#include <iostream>
#include <memory>

namespace Sonatrix {
namespace Core {
namespace Audio {

namespace {

std::unique_ptr<VoiceManager>
CreateVoiceManagerForBounce(const std::string &assetsPath,
                            PlaybackInstrument instrument) {
  switch (instrument) {
  case PlaybackInstrument::Guitar: {
    auto manager = std::make_unique<GuitarVoiceManager>();
    if (!manager->LoadAcousticGuitarKit(assetsPath)) {
      return nullptr;
    }
    return manager;
  }
  case PlaybackInstrument::ElectricBass: {
    auto manager = std::make_unique<BassVoiceManager>();
    if (!manager->LoadElectricBassKit(assetsPath)) {
      return nullptr;
    }
    return manager;
  }
  case PlaybackInstrument::MockBass: {
    auto manager = std::make_unique<BassVoiceManager>();
    if (!manager->LoadMockBassKit(assetsPath)) {
      return nullptr;
    }
    return manager;
  }
  }

  return nullptr;
}

} // namespace

bool AudioExporter::BounceOffline(
    const std::string &outputPath,
    const std::vector<Sonatrix::Core::MIDI::MIDIEvent> &midiStream,
    const std::string &assetsPath, PlaybackInstrument instrument,
    const std::vector<float> &busVolumes, double sampleRate,
    double tempoBPM) {
  const double safeTempoBPM = (tempoBPM > 0.0) ? tempoBPM : 120.0;

  std::unique_ptr<VoiceManager> voiceManager =
      CreateVoiceManagerForBounce(assetsPath, instrument);
  if (!voiceManager) {
    std::cerr << "AudioExporter: Failed to create instrument voice manager for "
              << assetsPath << std::endl;
    return false;
  }

  auto &mixer = voiceManager->GetMixer();
  for (size_t i = 0; i < busVolumes.size(); ++i) {
    mixer.SetBusVolume(static_cast<MixerBus>(i), busVolumes[i]);
  }

  CFStringRef pathStr = CFStringCreateWithCString(
      kCFAllocatorDefault, outputPath.c_str(), kCFStringEncodingUTF8);
  CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, pathStr,
                                               kCFURLPOSIXPathStyle, false);
  CFRelease(pathStr);

  AudioStreamBasicDescription outputFormat = {};
  outputFormat.mSampleRate = sampleRate;
  outputFormat.mFormatID = kAudioFormatLinearPCM;
  outputFormat.mFormatFlags =
      kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
  outputFormat.mBytesPerPacket = 4;
  outputFormat.mFramesPerPacket = 1;
  outputFormat.mBytesPerFrame = 4;
  outputFormat.mChannelsPerFrame = 2;
  outputFormat.mBitsPerChannel = 16;

  ExtAudioFileRef audioFile;
  OSStatus err =
      ExtAudioFileCreateWithURL(url, kAudioFileWAVEType, &outputFormat, nullptr,
                                kAudioFileFlags_EraseFile, &audioFile);
  CFRelease(url);

  if (err != noErr) {
    std::cerr << "Failed to create ExtAudioFile: " << err << std::endl;
    return false;
  }

  AudioStreamBasicDescription clientFormat = {};
  clientFormat.mSampleRate = sampleRate;
  clientFormat.mFormatID = kAudioFormatLinearPCM;
  clientFormat.mFormatFlags =
      kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  clientFormat.mBytesPerPacket = 8;
  clientFormat.mFramesPerPacket = 1;
  clientFormat.mBytesPerFrame = 8;
  clientFormat.mChannelsPerFrame = 2;
  clientFormat.mBitsPerChannel = 32;

  err = ExtAudioFileSetProperty(
      audioFile, kExtAudioFileProperty_ClientDataFormat,
      sizeof(AudioStreamBasicDescription), &clientFormat);
  if (err != noErr) {
    ExtAudioFileDispose(audioFile);
    return false;
  }

  uint32_t framesPerBuffer = 512;
  std::vector<float> leftBuffer(framesPerBuffer, 0.0f);
  std::vector<float> rightBuffer(framesPerBuffer, 0.0f);
  float *channelPointers[2] = {leftBuffer.data(), rightBuffer.data()};

  AudioBufferList bufferList;
  bufferList.mNumberBuffers = 1;
  bufferList.mBuffers[0].mNumberChannels = 2;
  bufferList.mBuffers[0].mDataByteSize = framesPerBuffer * sizeof(float) * 2;
  std::vector<float> interleavedBuffer(framesPerBuffer * 2, 0.0f);
  bufferList.mBuffers[0].mData = interleavedBuffer.data();

  int64_t currentFrame = 0;
  int64_t totalFrames = 0;

  if (!midiStream.empty()) {
    const double totalTicks =
        static_cast<double>(midiStream.back().timelinePosition.ticks);
    const double msPerTick =
        (60000.0 / safeTempoBPM) /
        static_cast<double>(Sonatrix::Core::STANDARD_PPQN);
    const double totalSeconds = (totalTicks * msPerTick) / 1000.0;
    totalFrames = static_cast<int64_t>(totalSeconds * sampleRate) +
                  static_cast<int64_t>(2.0 * sampleRate);
  } else {
    totalFrames = static_cast<int64_t>(2.0 * sampleRate);
  }

  size_t midiEventIndex = 0;

  while (currentFrame < totalFrames) {
    uint32_t framesToRender = static_cast<uint32_t>(
        std::min<int64_t>(framesPerBuffer, totalFrames - currentFrame));

    const double blockEndSecs =
        static_cast<double>(currentFrame + framesToRender) / sampleRate;

    std::vector<MIDI::MIDIEvent> blockEvents;
    while (midiEventIndex < midiStream.size()) {
      const auto &event = midiStream[midiEventIndex];
      const double msPerTick =
          (60000.0 / safeTempoBPM) /
          static_cast<double>(Sonatrix::Core::STANDARD_PPQN);
      const double eventSecs = (event.timelinePosition.ticks * msPerTick) / 1000.0;
      if (eventSecs < blockEndSecs) {
        blockEvents.push_back(event);
        ++midiEventIndex;
      } else {
        break;
      }
    }

    voiceManager->ProcessMIDI(blockEvents);

    std::fill(leftBuffer.begin(), leftBuffer.end(), 0.0f);
    std::fill(rightBuffer.begin(), rightBuffer.end(), 0.0f);
    voiceManager->RenderAudio(channelPointers, framesToRender, 2);

    for (uint32_t i = 0; i < framesToRender; ++i) {
      interleavedBuffer[i * 2] = leftBuffer[i];
      interleavedBuffer[i * 2 + 1] = rightBuffer[i];
    }

    bufferList.mBuffers[0].mDataByteSize = framesToRender * sizeof(float) * 2;
    ExtAudioFileWrite(audioFile, framesToRender, &bufferList);

    currentFrame += framesToRender;
  }

  ExtAudioFileDispose(audioFile);
  return true;
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
