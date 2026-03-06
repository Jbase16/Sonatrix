#include "AudioFileReader.h"
#include <AudioToolbox/AudioToolbox.h>
#include <cmath>
#include <iostream>

namespace Sonatrix {
namespace Core {
namespace Audio {

bool AudioFileReader::LoadFile(const std::string &absolutePath,
                               std::vector<float> &outBuffer,
                               uint32_t &outSampleRate) {
  if (absolutePath.empty())
    return false;

  CFURLRef fileURL = CFURLCreateWithFileSystemPath(
      kCFAllocatorDefault, (CFStringRef)absolutePath.c_str(),
      kCFURLPOSIXPathStyle, false);
  if (!fileURL)
    return false;

  ExtAudioFileRef audioFile = nullptr;
  OSStatus err = ExtAudioFileOpenURL(fileURL, &audioFile);
  CFRelease(fileURL);

  if (err != noErr || !audioFile) {
    std::cerr << "Sonatrix: Failed to open ExtAudioFile at path: "
              << absolutePath << std::endl;
    return false;
  }

  // Get original format
  AudioStreamBasicDescription inputFormat;
  UInt32 propSize = sizeof(inputFormat);
  err = ExtAudioFileGetProperty(audioFile, kExtAudioFileProperty_FileDataFormat,
                                &propSize, &inputFormat);
  if (err != noErr) {
    ExtAudioFileDispose(audioFile);
    return false;
  }

  outSampleRate = inputFormat.mSampleRate;
  if (outSampleRate == 0)
    outSampleRate = 44100; // sensible default

  // Set up our desired client format (Interleaved Stereo Float32)
  AudioStreamBasicDescription clientFormat = {};
  clientFormat.mSampleRate = outSampleRate;
  clientFormat.mFormatID = kAudioFormatLinearPCM;
  clientFormat.mFormatFlags =
      kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  clientFormat.mFramesPerPacket = 1;
  clientFormat.mChannelsPerFrame = 2; // Stereo
  clientFormat.mBitsPerChannel = 32;
  clientFormat.mBytesPerPacket = 8;
  clientFormat.mBytesPerFrame = 8;

  err =
      ExtAudioFileSetProperty(audioFile, kExtAudioFileProperty_ClientDataFormat,
                              sizeof(clientFormat), &clientFormat);
  if (err != noErr) {
    ExtAudioFileDispose(audioFile);
    return false;
  }

  // Find absolute length in frames
  SInt64 numFrames = 0;
  propSize = sizeof(numFrames);
  err = ExtAudioFileGetProperty(
      audioFile, kExtAudioFileProperty_FileLengthFrames, &propSize, &numFrames);

  if (numFrames <= 0 || err != noErr) {
    ExtAudioFileDispose(audioFile);
    return false;
  }

  outBuffer.resize(numFrames * 2); // 2 channels

  // Read the data in chunks buffer
  UInt32 framesToRead = (UInt32)numFrames;
  AudioBufferList bufferList;
  bufferList.mNumberBuffers = 1;
  bufferList.mBuffers[0].mNumberChannels = 2;
  bufferList.mBuffers[0].mDataByteSize = framesToRead * 8;
  bufferList.mBuffers[0].mData = outBuffer.data();

  err = ExtAudioFileRead(audioFile, &framesToRead, &bufferList);

  ExtAudioFileDispose(audioFile);

  return (err == noErr && framesToRead > 0);
}

void AudioFileReader::GenerateTestTone(float frequencyHz, float durationSeconds,
                                       std::vector<float> &outBuffer,
                                       uint32_t sampleRate) {
  uint32_t numFrames = static_cast<uint32_t>(durationSeconds * sampleRate);
  outBuffer.resize(numFrames * 2); // Stereo Interleaved

  const float twoPi = 2.0f * M_PI;
  const float phaseIncrement = (frequencyHz * twoPi) / sampleRate;
  float currentPhase = 0.0f;

  for (uint32_t i = 0; i < numFrames; ++i) {
    // Blended Sine / Triangle wave for harmonics
    float sineVal = std::sin(currentPhase);

    // Triangle wave generator (-1.0 to 1.0)
    float normalizedPhase = currentPhase / twoPi;
    float triVal =
        4.0f * std::abs(normalizedPhase - std::floor(normalizedPhase + 0.75f)) -
        1.0f;

    // 70% Sine, 30% Triangle
    float sample = (sineVal * 0.7f) + (triVal * 0.3f);

    // Interleave L and R identically
    outBuffer[i * 2] = sample;
    outBuffer[i * 2 + 1] = sample;

    currentPhase += phaseIncrement;
    if (currentPhase >= twoPi) {
      currentPhase -= twoPi;
    }
  }
}

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
