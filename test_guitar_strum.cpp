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

  std::cout << "2. Creating Chord Timeline (C -> G -> Am -> F)..." << std::endl;
  std::vector<ChordTrackEvent> chordTimeline;
  
  // C Major (Beats 0-2)
  ChordTrackEvent cMajor;
  cMajor.position = MusicalTime(0);
  cMajor.chord.root = PitchClass::C;
  cMajor.chord.quality = ChordQuality::Major;
  cMajor.chord.overBass = PitchClass::C;
  chordTimeline.push_back(cMajor);

  // G Major (Beats 2-4)
  ChordTrackEvent gMajor;
  gMajor.position = BeatsToTime(2.0);
  gMajor.chord.root = PitchClass::G;
  gMajor.chord.quality = ChordQuality::Major;
  gMajor.chord.overBass = PitchClass::G;
  chordTimeline.push_back(gMajor);

  // A Minor (Beats 4-6)
  ChordTrackEvent aMinor;
  aMinor.position = BeatsToTime(4.0);
  aMinor.chord.root = PitchClass::A;
  aMinor.chord.quality = ChordQuality::Minor;
  aMinor.chord.overBass = PitchClass::A;
  chordTimeline.push_back(aMinor);

  // F Major (Beats 6-8)
  ChordTrackEvent fMajor;
  fMajor.position = BeatsToTime(6.0);
  fMajor.chord.root = PitchClass::F;
  fMajor.chord.quality = ChordQuality::Major;
  fMajor.chord.overBass = PitchClass::F;
  chordTimeline.push_back(fMajor);

  std::cout << "3. Defining MIR Pattern (D D U U D)..." << std::endl;
  auto pattern = std::make_shared<MIRPattern>();
  pattern->intendedEngine = MIRPattern::TargetEngine::Guitar;
  pattern->totalLength = BeatsToTime(2.0); // 2 beats long, repeats for each chord
  
  // Downstroke on beat 1
  MIREvent stroke1;
  stroke1.offsetMap = MusicalTime(0);
  stroke1.lengthBeats = 0.5;
  stroke1.type = ArticulationType::GuitarDownstroke;
  stroke1.velocityBase = 110;
  pattern->events.push_back(stroke1);

  // Downstroke on beat 1.5
  MIREvent stroke2;
  stroke2.offsetMap = BeatsToTime(0.5);
  stroke2.lengthBeats = 0.5;
  stroke2.type = ArticulationType::GuitarDownstroke;
  stroke2.velocityBase = 90;
  pattern->events.push_back(stroke2);

  // Upstroke on beat 2
  MIREvent stroke3;
  stroke3.offsetMap = BeatsToTime(1.0);
  stroke3.lengthBeats = 0.5;
  stroke3.type = ArticulationType::GuitarUpstroke;
  stroke3.velocityBase = 100;
  pattern->events.push_back(stroke3);

  // Upstroke on beat 2.5
  MIREvent stroke4;
  stroke4.offsetMap = BeatsToTime(1.5);
  stroke4.lengthBeats = 0.5;
  stroke4.type = ArticulationType::GuitarUpstroke;
  stroke4.velocityBase = 90;
  pattern->events.push_back(stroke4);

  // Create empty MIDI stream to accumulate the 4 loops
  MIDI::MIDIStream allStrums;
  std::cout << "4. Compiling via GuitarCompiler..." << std::endl;
  MIDI::GuitarCompiler compiler;

  // We loop the 2-beat pattern 4 times, advancing the clip's timeline start
  for (int i = 0; i < 4; ++i) {
      EditorClip clip(pattern);
      clip.timelineStart = BeatsToTime(i * 2.0);
      
      MIDI::MIDIStream strumMIDI = compiler.CompileClip(clip, chordTimeline);
      allStrums.events.insert(allStrums.events.end(), strumMIDI.events.begin(), strumMIDI.events.end());
  }

  std::cout << "Generated " << allStrums.events.size() << " total MIDI Events over 8 beats." << std::endl;
  
  std::cout << "5. Rendering Progression Audio..." << std::endl;
  
  // Render 4 seconds of sustain in small blocks (8 beats at 120bpm = 4 seconds)
  const size_t numFrames = static_cast<size_t>(4.0 * sampleRate);
  std::vector<float> outputData(numFrames * 2, 0.0f); // 2 channels
  
  const size_t blockSize = 256;
  std::vector<float> leftBuffer(blockSize, 0.0f);
  std::vector<float> rightBuffer(blockSize, 0.0f);
  float* outChannels[2] = { leftBuffer.data(), rightBuffer.data() };

  // Sort MIDI events by time so we can pop them as the playhead advances
  std::sort(allStrums.events.begin(), allStrums.events.end(),
            [](const MIDI::MIDIEvent& a, const MIDI::MIDIEvent& b) {
                return a.timelinePosition < b.timelinePosition;
            });

  auto& articulation = Audio::AssetManager::GetInstance().GetAcousticGuitarArticulation();
   MusicalTime currentPlayhead(0);
  size_t eventIndex = 0;

  for (size_t nextFrame = 0; nextFrame < numFrames; nextFrame += blockSize) {
      size_t framesToProcess = std::min(blockSize, numFrames - nextFrame);
      
      // Advance playhead time (assume 120 BPM -> 1 Beat = 0.5s)
      // 960 PPQN * 2 beats per second = 1920 ticks per second
      double ticksPerSample = 1920.0 / sampleRate;
      MusicalTime blockEndTime(currentPlayhead.ticks + static_cast<int64_t>(framesToProcess * ticksPerSample));

      // Dispatch events that fall in this block
      std::vector<MIDI::MIDIEvent> blockEvents;
      while (eventIndex < allStrums.events.size() && allStrums.events[eventIndex].timelinePosition < blockEndTime) {
          blockEvents.push_back(allStrums.events[eventIndex]);
          eventIndex++;
      }

      if (!blockEvents.empty()) {
          voiceManager.ProcessMIDI(blockEvents, articulation);
      }

      // Render the audio block
      voiceManager.RenderAudio(outChannels, framesToProcess, 2);

      // Interleave
      for (size_t i = 0; i < framesToProcess; ++i) {
          outputData[(nextFrame + i) * 2] = leftBuffer[i];
          outputData[(nextFrame + i) * 2 + 1] = rightBuffer[i];
      }
      
      currentPlayhead = blockEndTime;
  }

  std::cout << "6. Saving test_guitar_strum_output.wav..." << std::endl;
  SaveWav("test_guitar_strum_output.wav", outputData, sampleRate);
  
  std::cout << "Done." << std::endl;
  return 0;
}
