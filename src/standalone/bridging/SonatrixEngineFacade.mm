//
//  SonatrixEngineFacade.mm
//  Sonatrix
//

#import "SonatrixEngineFacade.h"
#import "../../standalone/StandaloneAudioEngine.h"

#include "../../core/arrangement/ChordTrack.h"
#include "../../core/audio/AudioExporter.h"
#include "../../core/midi/GuitarCompiler.h"
#include "../../core/midi/MIDIExporter.h"
#include "../../core/mir/DeltaGraph.h"
#include "../../core/mir/MIRPattern.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace {

constexpr double kDefaultTempoBPM = 120.0;

double MillisecondsPerTick() {
  return (60000.0 / kDefaultTempoBPM) /
         static_cast<double>(Sonatrix::Core::STANDARD_PPQN);
}

void AddInteractiveGuitarEvent(
    Sonatrix::Core::MIRPattern &pattern, double offsetBeats, double lengthBeats,
    uint8_t velocity, Sonatrix::Core::ArticulationType type,
    Sonatrix::Core::GuitarTargetRole targetRole =
        Sonatrix::Core::GuitarTargetRole::None,
    Sonatrix::Core::GuitarTargetRole secondaryTargetRole =
        Sonatrix::Core::GuitarTargetRole::None) {
  Sonatrix::Core::MIREvent event;
  event.offsetMap = Sonatrix::Core::BeatsToTime(offsetBeats);
  event.lengthBeats = lengthBeats;
  event.velocityBase = velocity;
  event.type = type;
  event.guitarTargetRole = targetRole;
  event.guitarSecondaryTargetRole = secondaryTargetRole;
  pattern.events.push_back(event);
}

std::shared_ptr<Sonatrix::Core::MIRPattern> BuildInteractiveChordPattern() {
  auto pattern = std::make_shared<Sonatrix::Core::MIRPattern>();
  pattern->intendedEngine = Sonatrix::Core::MIRPattern::TargetEngine::Guitar;
  pattern->guitarVoicingMode = Sonatrix::Core::GuitarVoicingMode::AcousticOpen;
  pattern->totalLength = Sonatrix::Core::BeatsToTime(4.0);

  AddInteractiveGuitarEvent(*pattern, 0.0, 0.5, 110,
                            Sonatrix::Core::ArticulationType::GuitarPinch,
                            Sonatrix::Core::GuitarTargetRole::Bass,
                            Sonatrix::Core::GuitarTargetRole::Top);
  AddInteractiveGuitarEvent(*pattern, 1.0 / 3.0, 0.33, 72,
                            Sonatrix::Core::ArticulationType::GuitarPluck,
                            Sonatrix::Core::GuitarTargetRole::InnerLow);
  AddInteractiveGuitarEvent(*pattern, 2.0 / 3.0, 0.33, 78,
                            Sonatrix::Core::ArticulationType::GuitarPluck,
                            Sonatrix::Core::GuitarTargetRole::InnerHigh);
  AddInteractiveGuitarEvent(*pattern, 1.0, 0.5, 105,
                            Sonatrix::Core::ArticulationType::GuitarPinch,
                            Sonatrix::Core::GuitarTargetRole::Bass,
                            Sonatrix::Core::GuitarTargetRole::Top);
  AddInteractiveGuitarEvent(*pattern, 4.0 / 3.0, 0.33, 75,
                            Sonatrix::Core::ArticulationType::GuitarPluck,
                            Sonatrix::Core::GuitarTargetRole::InnerHigh);
  AddInteractiveGuitarEvent(*pattern, 5.0 / 3.0, 0.33, 80,
                            Sonatrix::Core::ArticulationType::GuitarPluck,
                            Sonatrix::Core::GuitarTargetRole::Treble);
  AddInteractiveGuitarEvent(*pattern, 2.0, 0.5, 115,
                            Sonatrix::Core::ArticulationType::GuitarPinch,
                            Sonatrix::Core::GuitarTargetRole::Bass,
                            Sonatrix::Core::GuitarTargetRole::Top);
  AddInteractiveGuitarEvent(*pattern, 7.0 / 3.0, 0.33, 72,
                            Sonatrix::Core::ArticulationType::GuitarPluck,
                            Sonatrix::Core::GuitarTargetRole::InnerLow);
  AddInteractiveGuitarEvent(*pattern, 8.0 / 3.0, 0.33, 78,
                            Sonatrix::Core::ArticulationType::GuitarPluck,
                            Sonatrix::Core::GuitarTargetRole::InnerHigh);
  AddInteractiveGuitarEvent(*pattern, 3.0, 0.5, 100,
                            Sonatrix::Core::ArticulationType::GuitarPinch,
                            Sonatrix::Core::GuitarTargetRole::Bass,
                            Sonatrix::Core::GuitarTargetRole::Top);
  AddInteractiveGuitarEvent(*pattern, 10.0 / 3.0, 0.33, 75,
                            Sonatrix::Core::ArticulationType::GuitarPluck,
                            Sonatrix::Core::GuitarTargetRole::InnerHigh);
  AddInteractiveGuitarEvent(*pattern, 11.0 / 3.0, 0.33, 80,
                            Sonatrix::Core::ArticulationType::GuitarPluck,
                            Sonatrix::Core::GuitarTargetRole::Treble);

  return pattern;
}

Sonatrix::Core::MIDI::MIDIStream CompileInteractiveArrangement(
    const std::vector<Sonatrix::Core::ChordTrackEvent> &chordTrack) {
  Sonatrix::Core::MIDI::MIDIStream arrangementStream;
  if (chordTrack.empty()) {
    return arrangementStream;
  }

  Sonatrix::Core::MIDI::GuitarCompiler guitarEngine;
  auto pattern = BuildInteractiveChordPattern();

  for (const auto &chordEvent : chordTrack) {
    Sonatrix::Core::EditorClip clip(pattern);
    clip.timelineStart = chordEvent.position;

    auto clipStream = guitarEngine.CompileClip(clip, chordTrack, nullptr);
    arrangementStream.events.insert(arrangementStream.events.end(),
                                    clipStream.events.begin(),
                                    clipStream.events.end());
  }

  arrangementStream.SortByTime();
  return arrangementStream;
}

} // namespace

@interface SonatrixEngineFacade ()
@property(nonatomic, strong) StandaloneAudioEngine *audioEngine;
@property(nonatomic, assign) BOOL internalIsPlaying;
- (void)restartPlaybackThread;
- (void)stopPlaybackThread;
@end

@implementation SonatrixEngineFacade {
  std::vector<Sonatrix::Core::ChordTrackEvent> _chordTrack;
  Sonatrix::Core::MIDI::MIDIStream _scheduledStream;
  std::unique_ptr<std::thread> _playbackThread;
  BOOL _shouldStopPlayback;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _audioEngine = [[StandaloneAudioEngine alloc] init];
    _internalIsPlaying = NO;
    _shouldStopPlayback = NO;
  }
  return self;
}

- (void)dealloc {
  [self stop];
  [super dealloc];
}

- (BOOL)isPlaying {
  return _internalIsPlaying;
}

- (void)play {
  if (_internalIsPlaying) {
    return;
  }

  [_audioEngine start];
  _internalIsPlaying = YES;
  [self restartPlaybackThread];
}

- (void)stop {
  [self stopPlaybackThread];
  [_audioEngine stop];
  _internalIsPlaying = NO;
}

- (void)clearChords {
  _chordTrack.clear();
  _scheduledStream.events.clear();
  [self stopPlaybackThread];
}

- (void)addChordWithRoot:(uint8_t)rootKey
                 quality:(uint8_t)quality
              tickOffset:(double)offset {
  Sonatrix::Core::ChordTrackEvent ev;
  ev.position = Sonatrix::Core::MusicalTime(static_cast<int64_t>(offset));
  ev.chord.root = static_cast<Sonatrix::Core::PitchClass>(rootKey % 12);
  ev.chord.quality = static_cast<Sonatrix::Core::ChordQuality>(quality);
  _chordTrack.push_back(ev);

  std::stable_sort(
      _chordTrack.begin(), _chordTrack.end(),
      [](const Sonatrix::Core::ChordTrackEvent &lhs,
         const Sonatrix::Core::ChordTrackEvent &rhs) {
        return lhs.position.ticks < rhs.position.ticks;
      });
}

- (void)compileAndSchedule {
  _scheduledStream = CompileInteractiveArrangement(_chordTrack);

  if (_internalIsPlaying) {
    [self restartPlaybackThread];
  } else {
    [self stopPlaybackThread];
  }
}

- (void)restartPlaybackThread {
  [self stopPlaybackThread];

  if (_scheduledStream.events.empty()) {
    return;
  }

  const Sonatrix::Core::MIDI::MIDIStream stream = _scheduledStream;
  _shouldStopPlayback = NO;

  _playbackThread = std::make_unique<std::thread>([self, stream]() {
    int64_t currentTick = 0;

    for (const auto &event : stream.events) {
      if (self->_shouldStopPlayback) {
        break;
      }

      const int64_t sleepTicks = event.timelinePosition.ticks - currentTick;
      if (sleepTicks > 0) {
        std::this_thread::sleep_for(
            std::chrono::duration<double, std::milli>(sleepTicks *
                                                      MillisecondsPerTick()));
      }
      currentTick = event.timelinePosition.ticks;

      uint8_t status = 0;
      switch (event.type) {
      case Sonatrix::Core::MIDI::MIDIEventType::NoteOn:
        status = 0x90;
        break;
      case Sonatrix::Core::MIDI::MIDIEventType::NoteOff:
        status = 0x80;
        break;
      case Sonatrix::Core::MIDI::MIDIEventType::ControlChange:
        status = 0xB0;
        break;
      case Sonatrix::Core::MIDI::MIDIEventType::PitchBend:
        status = 0xE0;
        break;
      }

      [self.audioEngine pushMIDIEventStatus:status
                                      data1:event.data1
                                      data2:event.data2
                                    channel:event.channel];
    }
  });
}

- (void)stopPlaybackThread {
  _shouldStopPlayback = YES;
  if (_playbackThread && _playbackThread->joinable()) {
    _playbackThread->join();
  }
  _playbackThread.reset();
}

- (void)setVolume:(float)volume forBus:(uint8_t)busIndex {
  [_audioEngine setVolume:volume forBus:busIndex];
}

- (BOOL)bounceAudioToPath:(NSString *)path
               assetsPath:(NSString *)assetsPath
                  volumes:(NSArray<NSNumber *> *)volumes {
  const Sonatrix::Core::MIDI::MIDIStream masterStream =
      CompileInteractiveArrangement(_chordTrack);

  std::vector<float> cVols;
  for (NSNumber *vol in volumes) {
    cVols.push_back([vol floatValue]);
  }

  std::string cppOutputPath = [path UTF8String];
  std::string cppAssetsPath = [assetsPath UTF8String];

  return Sonatrix::Core::Audio::AudioExporter::BounceOffline(
      cppOutputPath, masterStream.events, cppAssetsPath, cVols);
}

- (BOOL)exportMIDIToPath:(NSString *)path {
  const Sonatrix::Core::MIDI::MIDIStream masterStream =
      CompileInteractiveArrangement(_chordTrack);

  std::string cppOutputPath = [path UTF8String];
  return Sonatrix::Core::MIDI::MIDIExporter::ExportToSMF(cppOutputPath,
                                                         masterStream.events);
}

@end
