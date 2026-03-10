#include "src/core/audio/AssetManager.h"
#include "src/core/audio/VoiceManager.h"
#include "src/core/midi/GuitarCompiler.h"
#include "src/core/mir/MIRPattern.h"
#include "src/core/mir/DeltaGraph.h"
#include "src/core/arrangement/ChordTrack.h"
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>
#include <vector>

using namespace Sonatrix::Core;

// -----------------------------------------------------------------------------
// CoreAudio WAV Write (Copied for standalone test execution)
// -----------------------------------------------------------------------------
bool SaveWav(const std::string &path, const std::vector<float> &data,
             double sampleRate) {
  CFStringRef str = CFStringCreateWithCString(kCFAllocatorDefault, path.c_str(),
                                              kCFStringEncodingUTF8);
  if (!str) return false;
  CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, str,
                                               kCFURLPOSIXPathStyle, false);
  CFRelease(str);
  if (!url) return false;

  AudioStreamBasicDescription fileFormat = {};
  fileFormat.mSampleRate = sampleRate;
  fileFormat.mFormatID = kAudioFormatLinearPCM;
  fileFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  fileFormat.mFramesPerPacket = 1;
  fileFormat.mChannelsPerFrame = 2; // VoiceManager mixes to Stereo!
  fileFormat.mBitsPerChannel = 32;
  fileFormat.mBytesPerPacket = 8;
  fileFormat.mBytesPerFrame = 8;

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
  bufferList.mBuffers[0].mNumberChannels = 2;
  bufferList.mBuffers[0].mDataByteSize =
      static_cast<UInt32>(data.size() * sizeof(float));
  bufferList.mBuffers[0].mData = const_cast<float *>(data.data());

  UInt32 frameCount = static_cast<UInt32>(data.size() / 2);
  const bool ok = ExtAudioFileWrite(audioFile, frameCount, &bufferList) == noErr;
  ExtAudioFileDispose(audioFile);
  return ok;
}

int main() {
  const double sampleRate = 44100.0;

  std::cout << "1. Initializing Audio Engines..." << std::endl;
  Audio::VoiceManager voiceManager;
  voiceManager.LoadInstrumentKit("Assets/Exciters/FS_Guitars");

  std::cout << "2. Creating Chord Timeline (C Major)..." << std::endl;
  std::vector<ChordTrackEvent> chordTimeline;
  ChordTrackEvent cMajor;
  cMajor.position = MusicalTime(0);
  cMajor.chord.root = PitchClass::C;
  cMajor.chord.quality = ChordQuality::Major;
  cMajor.chord.overBass = PitchClass::C;
  chordTimeline.push_back(cMajor);

  std::cout << "3. Defining MIR Pattern (Guitar Downstroke)..." << std::endl;
  auto pattern = std::make_shared<MIRPattern>();
  pattern->intendedEngine = MIRPattern::TargetEngine::Guitar;
  pattern->totalLength = MusicalTime(1.0); // 1 beat
  
  MIREvent downstroke;
  downstroke.offsetMap = MusicalTime(0);
  downstroke.lengthBeats = 1.0;
  downstroke.type = ArticulationType::GuitarDownstroke;
  downstroke.velocityBase = 110;
  pattern->events.push_back(downstroke);

  EditorClip clip(pattern);
  clip.timelineStart = MusicalTime(0);

  std::cout << "4. Compiling via GuitarCompiler..." << std::endl;
  MIDI::GuitarCompiler compiler;
  MIDI::MIDIStream strumMIDI = compiler.CompileClip(clip, chordTimeline);

  std::cout << "Generated " << strumMIDI.events.size() << " MIDI Events." << std::endl;
  for (const auto& ev : strumMIDI.events) {
      if (ev.type == MIDI::MIDIEventType::NoteOn) {
          std::cout << " - NoteOn: Pitch " << (int)ev.data1 
                    << " (Velocity " << (int)ev.data2 << ")"
                    << " on Channel " << (int)ev.channel << " (String " << ((int)ev.channel - 1) << ")" << std::endl;
      }
  }

  std::cout << "5. Rendering Strum Audio..." << std::endl;
  
  // Send NoteOn events to allocate voices
  std::vector<MIDI::MIDIEvent> onEvents;
  for (const auto& ev : strumMIDI.events) {
      if (ev.type == MIDI::MIDIEventType::NoteOn) onEvents.push_back(ev);
  }
  voiceManager.ProcessMIDI(onEvents, Audio::AssetManager::GetInstance().GetAcousticGuitarArticulation());

  // Render 2 seconds of sustain (VoiceManager pushes out Interleaved Stereo float)
  const size_t numFrames = static_cast<size_t>(2.0 * sampleRate);
  std::vector<float> outputData(numFrames * 2, 0.0f); // 2 channels
  
  // VoiceManager uses planar output for its RenderAudio function
  std::vector<float> leftBuffer(numFrames, 0.0f);
  std::vector<float> rightBuffer(numFrames, 0.0f);
  float* outChannels[2] = { leftBuffer.data(), rightBuffer.data() };

  voiceManager.RenderAudio(outChannels, numFrames, 2);

  // Interleave for WAV export
  for (size_t i = 0; i < numFrames; ++i) {
      outputData[i * 2] = leftBuffer[i];
      outputData[i * 2 + 1] = rightBuffer[i];
  }

  std::cout << "6. Saving test_guitar_strum_output.wav..." << std::endl;
  SaveWav("test_guitar_strum_output.wav", outputData, sampleRate);
  
  std::cout << "Done." << std::endl;
  return 0;
}
