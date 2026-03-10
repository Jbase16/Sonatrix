#include "src/core/audio/AssetManager.h"
#include "src/core/audio/VoiceManager.h"
#include "src/core/midi/GuitarCompiler.h"
#include "src/core/mir/MIRPattern.h"
#include "src/core/mir/DeltaGraph.h"
#include "src/core/arrangement/ChordTrack.h"

#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <iostream>
#include <memory>
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

// -----------------------------------------------------------------------------
// Helper: convert MusicalTime ticks to absolute sample position
// Assumption: 120 BPM and 960 PPQN => 1920 ticks/second
// -----------------------------------------------------------------------------
static inline int64_t TicksToSample(double ticks, double sampleRate) {
  constexpr double ticksPerSecond = 1920.0;
  return static_cast<int64_t>((ticks / ticksPerSecond) * sampleRate);
}

int main() {
  const double sampleRate = 44100.0;
  const size_t blockSize = 256;
  const double totalSeconds = 8.0; // 16 beats @ 120 BPM
  const size_t numFrames = static_cast<size_t>(totalSeconds * sampleRate);

  std::cout << "1. Initializing Audio Engines..." << std::endl;
  Audio::VoiceManager voiceManager;
  Audio::AssetManager::GetInstance().LoadAcousticGuitarAnchors(
      "Assets/Exciters/FS_Guitars");

  auto &articulation =
      Audio::AssetManager::GetInstance().GetAcousticGuitarArticulation();

  std::cout << "2. Creating Chord Timeline (C -> G -> Am -> F)..." << std::endl;
  std::vector<ChordTrackEvent> chordTimeline;

  // C Major (Beats 0-2)
  ChordTrackEvent cMajor;
  cMajor.position = MusicalTime(0);
  cMajor.chord.root = PitchClass::C;
  cMajor.chord.quality = ChordQuality::Major;
  cMajor.chord.overBass = PitchClass::C;
  chordTimeline.push_back(cMajor);

  // G Major (Beats 4-8)
  ChordTrackEvent gMajor;
  gMajor.position = BeatsToTime(4.0);
  gMajor.chord.root = PitchClass::G;
  gMajor.chord.quality = ChordQuality::Major;
  gMajor.chord.overBass = PitchClass::G;
  chordTimeline.push_back(gMajor);

  // A Minor (Beats 8-12)
  ChordTrackEvent aMinor;
  aMinor.position = BeatsToTime(8.0);
  aMinor.chord.root = PitchClass::A;
  aMinor.chord.quality = ChordQuality::Minor;
  aMinor.chord.overBass = PitchClass::A;
  chordTimeline.push_back(aMinor);

  // F Major (Beats 12-16)
  ChordTrackEvent fMajor;
  fMajor.position = BeatsToTime(12.0);
  fMajor.chord.root = PitchClass::F;
  fMajor.chord.quality = ChordQuality::Major;
  fMajor.chord.overBass = PitchClass::F;
  chordTimeline.push_back(fMajor);

  std::cout << "3. Defining MIR Pattern (D D U U D U) over 4 beats..." << std::endl;
  auto pattern = std::make_shared<MIRPattern>();
  pattern->intendedEngine = MIRPattern::TargetEngine::Guitar;
  pattern->totalLength = BeatsToTime(4.0); // Full 4 beat measure
  
  // Downstroke on beat 1
  MIREvent stroke1;
  stroke1.offsetMap = MusicalTime(0);
  stroke1.lengthBeats = 1.0;
  stroke1.type = ArticulationType::GuitarDownstroke;
  stroke1.velocityBase = 110;
  pattern->events.push_back(stroke1);

  // Downstroke on beat 2
  MIREvent stroke2;
  stroke2.offsetMap = BeatsToTime(1.0);
  stroke2.lengthBeats = 0.5;
  stroke2.type = ArticulationType::GuitarDownstroke;
  stroke2.velocityBase = 90;
  pattern->events.push_back(stroke2);

  // Upstroke on beat 2.5
  MIREvent stroke3;
  stroke3.offsetMap = BeatsToTime(1.5);
  stroke3.lengthBeats = 1.0;
  stroke3.type = ArticulationType::GuitarUpstroke;
  stroke3.velocityBase = 100;
  pattern->events.push_back(stroke3);

  // Upstroke on beat 3.5
  MIREvent stroke4;
  stroke4.offsetMap = BeatsToTime(2.5);
  stroke4.lengthBeats = 0.5;
  stroke4.type = ArticulationType::GuitarUpstroke;
  stroke4.velocityBase = 90;
  pattern->events.push_back(stroke4);

  // Downstroke on beat 4
  MIREvent stroke5;
  stroke5.offsetMap = BeatsToTime(3.0);
  stroke5.lengthBeats = 0.5;
  stroke5.type = ArticulationType::GuitarDownstroke;
  stroke5.velocityBase = 90;
  pattern->events.push_back(stroke5);

  // Upstroke on beat 4.5
  MIREvent stroke6;
  stroke6.offsetMap = BeatsToTime(3.5);
  stroke6.lengthBeats = 0.5;
  stroke6.type = ArticulationType::GuitarUpstroke;
  stroke6.velocityBase = 80;
  pattern->events.push_back(stroke6);

  std::cout << "4. Compiling via GuitarCompiler..." << std::endl;
  MIDI::GuitarCompiler compiler;
  MIDI::MIDIStream allStrums;

  // Repeat 4-beat clip 4 times => 16 beats total
  for (int i = 0; i < 4; ++i) {
    EditorClip clip(pattern);
    clip.timelineStart = BeatsToTime(i * 4.0);

    MIDI::MIDIStream strumMIDI = compiler.CompileClip(clip, chordTimeline);
    allStrums.events.insert(allStrums.events.end(), strumMIDI.events.begin(),
                            strumMIDI.events.end());
  }

  std::stable_sort(allStrums.events.begin(), allStrums.events.end(),
            [](const MIDI::MIDIEvent &a, const MIDI::MIDIEvent &b) {
              return a.timelinePosition < b.timelinePosition;
            });

  std::cout << "Generated " << allStrums.events.size()
            << " total MIDI Events over 8 beats." << std::endl;

  std::cout << "5. Rendering Progression Audio..." << std::endl;

  for (const auto& ev : allStrums.events) {
    std::cout << "tick=" << ev.timelinePosition.ticks
              << " type=" << static_cast<int>(ev.type)
              << " pitch=" << static_cast<int>(ev.data1)
              << " vel=" << static_cast<int>(ev.data2)
              << std::endl;
  }

  std::vector<float> outputData(numFrames * 2, 0.0f);

  std::vector<float> leftBuffer(blockSize, 0.0f);
  std::vector<float> rightBuffer(blockSize, 0.0f);
  float *outChannels[2] = {leftBuffer.data(), rightBuffer.data()};

  size_t eventIndex = 0;

  // Precompute absolute sample positions for all MIDI events
  struct ScheduledEvent {
    int64_t samplePosition = 0;
    MIDI::MIDIEvent event;
  };

  std::vector<ScheduledEvent> scheduledEvents;
  scheduledEvents.reserve(allStrums.events.size());

  for (const auto &ev : allStrums.events) {
    ScheduledEvent s;
    s.samplePosition = TicksToSample(static_cast<double>(ev.timelinePosition.ticks),
                                     sampleRate);
    s.event = ev;
    scheduledEvents.push_back(s);
  }

  for (size_t blockStartFrame = 0; blockStartFrame < numFrames;
       blockStartFrame += blockSize) {
    const size_t framesInBlock =
        std::min(blockSize, numFrames - blockStartFrame);

    std::fill(leftBuffer.begin(), leftBuffer.end(), 0.0f);
    std::fill(rightBuffer.begin(), rightBuffer.end(), 0.0f);

    size_t renderedSoFar = 0;
    const int64_t blockStartSample = static_cast<int64_t>(blockStartFrame);
    const int64_t blockEndSample =
        static_cast<int64_t>(blockStartFrame + framesInBlock);

    // Process all events that fall inside this block at sample-accurate offsets
    while (eventIndex < scheduledEvents.size() &&
           scheduledEvents[eventIndex].samplePosition < blockEndSample) {
      const int64_t eventSample = scheduledEvents[eventIndex].samplePosition;

      // Clamp just in case of rounding weirdness
      const int64_t clampedEventSample =
          std::max(blockStartSample, std::min(eventSample, blockEndSample));

      const size_t framesUntilEvent =
          static_cast<size_t>(clampedEventSample - blockStartSample) - renderedSoFar;

      // Render audio up to the event
      if (framesUntilEvent > 0) {
        float *subChannels[2] = {
            leftBuffer.data() + renderedSoFar,
            rightBuffer.data() + renderedSoFar};

        voiceManager.RenderAudio(subChannels,
                                 static_cast<uint32_t>(framesUntilEvent), 2);
        renderedSoFar += framesUntilEvent;
      }

      // Inject exactly this event at this sample point
      std::vector<MIDI::MIDIEvent> singleEvent;
      singleEvent.push_back(scheduledEvents[eventIndex].event);
      voiceManager.ProcessMIDI(singleEvent, articulation);

      ++eventIndex;
    }

    // Render the remainder of the block after the last event
    const size_t remainingFrames = framesInBlock - renderedSoFar;
    if (remainingFrames > 0) {
      float *subChannels[2] = {
          leftBuffer.data() + renderedSoFar,
          rightBuffer.data() + renderedSoFar};

      voiceManager.RenderAudio(subChannels,
                               static_cast<uint32_t>(remainingFrames), 2);
    }

    // Interleave into final output buffer with -16dB Gain Reduction!
    // This prevents the 6 overlapping strings from clipping the WAV file into a square wave.
    for (size_t i = 0; i < framesInBlock; ++i) {
      outputData[(blockStartFrame + i) * 2] = leftBuffer[i] * 0.15f;
      outputData[(blockStartFrame + i) * 2 + 1] = rightBuffer[i] * 0.15f;
    }
  }

  std::cout << "6. Saving test_guitar_strum_output.wav..." << std::endl;
  if (!SaveWav("test_guitar_strum_output.wav", outputData, sampleRate)) {
    std::cerr << "Failed to save WAV." << std::endl;
    return 1;
  }

  std::cout << "Done." << std::endl;
  return 0;
}