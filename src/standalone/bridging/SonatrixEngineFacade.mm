//
//  SonatrixEngineFacade.mm
//  Sonatrix
//

#import "SonatrixEngineFacade.h"
#import "../../standalone/StandaloneAudioEngine.h"

#include "../../core/arrangement/ChordTrack.h"
#include "../../core/audio/AudioExporter.h"
#include "../../core/engines/bass/BassCompiler.h"
#include "../../core/engines/drums/DrumCompiler.h"
#include "../../core/midi/GuitarCompiler.h"
#include "../../core/midi/MIDIExporter.h" // Added for MIDI export
#include "../../core/mir/DeltaGraph.h"
#include "../../core/ml/DynamicGrooveVector.h"

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

@interface SonatrixEngineFacade ()
@property(nonatomic, strong) StandaloneAudioEngine *audioEngine;
@property(nonatomic, assign) BOOL internalIsPlaying;
@end

@implementation SonatrixEngineFacade {
  std::vector<Sonatrix::Core::ChordTrackEvent> _chordTrack;
  std::shared_ptr<Sonatrix::Core::ML::DynamicGrooveVector> _grooveVector;

  // Playback Thread handle
  std::unique_ptr<std::thread> _playbackThread;
  BOOL _shouldStopPlayback;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _audioEngine = [[StandaloneAudioEngine alloc] init];
    _grooveVector = std::make_shared<Sonatrix::Core::ML::DynamicGrooveVector>();
    _internalIsPlaying = NO;
    _shouldStopPlayback = NO;
  }
  return self;
}

- (BOOL)isPlaying {
  return _internalIsPlaying;
}

- (void)play {
  [_audioEngine start];
  _internalIsPlaying = YES;
}

- (void)stop {
  _shouldStopPlayback = YES;
  if (_playbackThread && _playbackThread->joinable()) {
    _playbackThread->join();
  }
  [_audioEngine stop];
  _internalIsPlaying = NO;
}

- (void)clearChords {
  _chordTrack.clear();
}

- (void)addChordWithRoot:(uint8_t)rootKey
                 quality:(uint8_t)quality
              tickOffset:(double)offset {
  Sonatrix::Core::ChordTrackEvent ev;
  ev.position = Sonatrix::Core::MusicalTime(static_cast<int64_t>(offset));
  ev.chord.root = static_cast<Sonatrix::Core::PitchClass>(rootKey % 12);
  ev.chord.quality = static_cast<Sonatrix::Core::ChordQuality>(quality);
  _chordTrack.push_back(ev);
}

- (void)compileAndSchedule {
  // 1. Create a mocked MIR sequence to test the compilers
  auto mockPattern = std::make_shared<Sonatrix::Core::MIRPattern>();
  mockPattern->totalLength =
      Sonatrix::Core::MusicalTime(1920); // 1 bar at 480 PPQ

  Sonatrix::Core::MIREvent ev;
  ev.offsetMap = Sonatrix::Core::MusicalTime(0);
  ev.lengthBeats = 0.5;
  ev.type = Sonatrix::Core::ArticulationType::GenericNote;
  mockPattern->events.push_back(ev);

  ev.offsetMap = Sonatrix::Core::MusicalTime(480);
  mockPattern->events.push_back(ev);

  ev.offsetMap = Sonatrix::Core::MusicalTime(960);
  mockPattern->events.push_back(ev);

  ev.offsetMap = Sonatrix::Core::MusicalTime(1440);
  mockPattern->events.push_back(ev);

  Sonatrix::Core::EditorClip clip(mockPattern);
  clip.timelineStart = Sonatrix::Core::MusicalTime(0);

  // 2. Clear old groove vector
  _grooveVector = std::make_shared<Sonatrix::Core::ML::DynamicGrooveVector>();

  // 3. Compile engines
  Sonatrix::Core::MIDI::DrumCompiler drumEngine;
  Sonatrix::Core::MIDI::MIDIStream drumStream =
      drumEngine.CompileClip(clip, _chordTrack, _grooveVector.get());

  Sonatrix::Core::MIDI::BassCompiler bassEngine;
  Sonatrix::Core::MIDI::MIDIStream bassStream =
      bassEngine.CompileClip(clip, _chordTrack, _grooveVector.get());

  // Merge streams
  Sonatrix::Core::MIDI::MIDIStream masterStream;
  masterStream.events.insert(masterStream.events.end(),
                             drumStream.events.begin(),
                             drumStream.events.end());
  masterStream.events.insert(masterStream.events.end(),
                             bassStream.events.begin(),
                             bassStream.events.end());
  masterStream.SortByTime();

  // 4. Fire up background thread to pump MIDI queue
  _shouldStopPlayback = NO;
  if (_playbackThread && _playbackThread->joinable()) {
    _playbackThread->join();
  }

  _playbackThread = std::make_unique<std::thread>([self, masterStream]() {
    int64_t currentTick = 0;

    for (const auto &event : masterStream.events) {
      if (self->_shouldStopPlayback)
        break;

      int64_t sleepTicks = event.timelinePosition.ticks - currentTick;
      if (sleepTicks > 0) {
        // Approximate 120 BPM: 1 beat = 500ms -> 1 tick (480 PPQ) = ~1.04ms
        double msPerTick = (60000.0 / 120.0) / 480.0;
        std::this_thread::sleep_for(std::chrono::milliseconds(
            static_cast<int>(sleepTicks * msPerTick)));
        currentTick = event.timelinePosition.ticks;
      }

      uint8_t status =
          (event.type == Sonatrix::Core::MIDI::MIDIEventType::NoteOn) ? 0x90
                                                                      : 0x80;
      [self.audioEngine pushMIDIEventStatus:status
                                      data1:event.data1
                                      data2:event.data2];
    }
  });
}

- (void)setVolume:(float)volume forBus:(uint8_t)busIndex {
  [_audioEngine setVolume:volume forBus:busIndex];
}

- (BOOL)bounceAudioToPath:(NSString *)path
               assetsPath:(NSString *)assetsPath
                  volumes:(NSArray<NSNumber *> *)volumes {

  // 1. Create a mocked MIR sequence to test the compilers
  auto mockPattern = std::make_shared<Sonatrix::Core::MIRPattern>();
  mockPattern->totalLength =
      Sonatrix::Core::MusicalTime(1920); // 1 bar at 480 PPQ

  Sonatrix::Core::MIREvent ev;
  ev.offsetMap = Sonatrix::Core::MusicalTime(0);
  ev.lengthBeats = 0.5;
  ev.type = Sonatrix::Core::ArticulationType::GenericNote;
  mockPattern->events.push_back(ev);

  ev.offsetMap = Sonatrix::Core::MusicalTime(480);
  mockPattern->events.push_back(ev);

  ev.offsetMap = Sonatrix::Core::MusicalTime(960);
  mockPattern->events.push_back(ev);

  ev.offsetMap = Sonatrix::Core::MusicalTime(1440);
  mockPattern->events.push_back(ev);

  Sonatrix::Core::EditorClip clip(mockPattern);
  clip.timelineStart = Sonatrix::Core::MusicalTime(0);

  // 2. Clear old groove vector
  _grooveVector = std::make_shared<Sonatrix::Core::ML::DynamicGrooveVector>();

  // 3. Compile engines
  Sonatrix::Core::MIDI::DrumCompiler drumEngine;
  Sonatrix::Core::MIDI::MIDIStream drumStream =
      drumEngine.CompileClip(clip, _chordTrack, _grooveVector.get());

  Sonatrix::Core::MIDI::BassCompiler bassEngine;
  Sonatrix::Core::MIDI::MIDIStream bassStream =
      bassEngine.CompileClip(clip, _chordTrack, _grooveVector.get());

  // Merge streams
  Sonatrix::Core::MIDI::MIDIStream masterStream;
  masterStream.events.insert(masterStream.events.end(),
                             drumStream.events.begin(),
                             drumStream.events.end());
  masterStream.events.insert(masterStream.events.end(),
                             bassStream.events.begin(),
                             bassStream.events.end());
  masterStream.SortByTime();

  // 4. Extract Volumes
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
  // 1. Create a mocked MIR sequence to test the compilers
  auto mockPattern = std::make_shared<Sonatrix::Core::MIRPattern>();
  mockPattern->totalLength =
      Sonatrix::Core::MusicalTime(1920); // 1 bar at 480 PPQ

  Sonatrix::Core::MIREvent ev;
  ev.offsetMap = Sonatrix::Core::MusicalTime(0);
  ev.lengthBeats = 0.5;
  ev.type = Sonatrix::Core::ArticulationType::GenericNote;
  mockPattern->events.push_back(ev);

  ev.offsetMap = Sonatrix::Core::MusicalTime(480);
  mockPattern->events.push_back(ev);

  ev.offsetMap = Sonatrix::Core::MusicalTime(960);
  mockPattern->events.push_back(ev);

  ev.offsetMap = Sonatrix::Core::MusicalTime(1440);
  mockPattern->events.push_back(ev);

  Sonatrix::Core::EditorClip clip(mockPattern);
  clip.timelineStart = Sonatrix::Core::MusicalTime(0);

  // 2. Clear old groove vector
  _grooveVector = std::make_shared<Sonatrix::Core::ML::DynamicGrooveVector>();

  // 3. Compile engines
  Sonatrix::Core::MIDI::DrumCompiler drumEngine;
  Sonatrix::Core::MIDI::MIDIStream drumStream =
      drumEngine.CompileClip(clip, _chordTrack, _grooveVector.get());

  Sonatrix::Core::MIDI::BassCompiler bassEngine;
  Sonatrix::Core::MIDI::MIDIStream bassStream =
      bassEngine.CompileClip(clip, _chordTrack, _grooveVector.get());

  // Merge streams
  Sonatrix::Core::MIDI::MIDIStream masterStream;
  masterStream.events.insert(masterStream.events.end(),
                             drumStream.events.begin(),
                             drumStream.events.end());
  masterStream.events.insert(masterStream.events.end(),
                             bassStream.events.begin(),
                             bassStream.events.end());
  masterStream.SortByTime();

  std::string cppOutputPath = [path UTF8String];
  return Sonatrix::Core::MIDI::MIDIExporter::ExportToSMF(cppOutputPath,
                                                         masterStream.events);
}

@end
