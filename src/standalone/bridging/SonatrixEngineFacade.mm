//
//  SonatrixEngineFacade.mm
//  Sonatrix
//

#import "SonatrixEngineFacade.h"
#import "../../standalone/StandaloneAudioEngine.h"

#include "../../core/arrangement/ChordTrack.h"
#include "../../core/audio/AudioExporter.h"
#include "../../core/engines/guitar/VoicingGraphSolver.h"
#include "../../core/midi/GuitarCompiler.h"
#include "../../core/midi/MIDIExporter.h"
#include "../../core/mir/DeltaGraph.h"
#include "../../core/mir/MIRPattern.h"
#include "../../core/mir/PatternLibrary.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace {

constexpr double kDefaultTempoBPM = 120.0;
constexpr const char *kDefaultPatternTemplateId = "acoustic_12_8_arpeggiated";

double MillisecondsPerTick(double tempoBPM) {
  const double safeTempoBPM = tempoBPM > 0.0 ? tempoBPM : kDefaultTempoBPM;
  return (60000.0 / safeTempoBPM) /
         static_cast<double>(Sonatrix::Core::STANDARD_PPQN);
}

Sonatrix::Core::ChordTrackEvent MakeChordEvent(uint8_t rootKey, uint8_t quality,
                                               double offset) {
  Sonatrix::Core::ChordTrackEvent ev;
  ev.position = Sonatrix::Core::MusicalTime(static_cast<int64_t>(offset));
  ev.chord.root = static_cast<Sonatrix::Core::PitchClass>(rootKey % 12);
  ev.chord.quality = static_cast<Sonatrix::Core::ChordQuality>(quality);
  ev.chord.overBass = ev.chord.root;
  return ev;
}

std::array<int8_t, 6> SuggestAcousticVoicingFrets(
    const Sonatrix::Core::ActiveChordContext &chord) {
  Sonatrix::Core::ChordTrackEvent event;
  event.position = Sonatrix::Core::MusicalTime(0);
  event.chord = chord;

  Sonatrix::Core::Engines::Guitar::VoicingGraphSolver solver;
  const auto voicings = solver.SolveVoiceLeading(
      {event}, Sonatrix::Core::GuitarVoicingMode::AcousticOpen);
  if (!voicings.empty()) {
    return voicings.front().frets;
  }

  return {-1, -1, -1, -1, -1, -1};
}

std::string ResolvePatternLibraryPath(NSBundle *bundle) {
  NSFileManager *fileManager = [NSFileManager defaultManager];

  if (bundle != nil) {
    NSString *resourcePath = [bundle resourcePath];
    if (resourcePath != nil) {
      NSString *bundledPath =
          [resourcePath stringByAppendingPathComponent:@"Assets/Patterns/default_library.json"];
      if ([fileManager fileExistsAtPath:bundledPath]) {
        return std::string([bundledPath UTF8String]);
      }
    }
  }

  NSString *repoPath =
      @"/Users/jason/Developer/Sonatrix/assets/Patterns/default_library.json";
  if ([fileManager fileExistsAtPath:repoPath]) {
    return std::string([repoPath UTF8String]);
  }

  return {};
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

std::shared_ptr<const Sonatrix::Core::MIRPattern> ResolveSelectedPattern(
    const std::string &selectedTemplateId) {
  auto &library = Sonatrix::Core::PatternLibrary::GetInstance();

  auto resolveFromTemplate = [&](const std::string &templateId)
      -> std::shared_ptr<const Sonatrix::Core::MIRPattern> {
    if (templateId.empty()) {
      return nullptr;
    }

    const auto tmpl = library.GetTemplate(templateId);
    if (!tmpl) {
      return nullptr;
    }

    const auto it =
        tmpl->patterns.find(Sonatrix::Core::MIRPattern::TargetEngine::Guitar);
    if (it == tmpl->patterns.end()) {
      return nullptr;
    }

    return it->second;
  };

  if (auto selectedPattern = resolveFromTemplate(selectedTemplateId)) {
    return selectedPattern;
  }

  if (auto defaultPattern = resolveFromTemplate(kDefaultPatternTemplateId)) {
    return defaultPattern;
  }

  return BuildInteractiveChordPattern();
}

Sonatrix::Core::MIDI::MIDIStream CompileInteractiveArrangement(
    const std::vector<Sonatrix::Core::ChordTrackEvent> &chordTrack,
    const std::string &selectedTemplateId) {
  Sonatrix::Core::MIDI::MIDIStream arrangementStream;
  if (chordTrack.empty()) {
    return arrangementStream;
  }

  Sonatrix::Core::MIDI::GuitarCompiler guitarEngine;
  auto pattern = ResolveSelectedPattern(selectedTemplateId);
  if (!pattern) {
    return arrangementStream;
  }

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
- (void)restartPlaybackThreadFromTick:(int64_t)startTick;
- (void)stopPlaybackThread;
@end

@implementation SonatrixEngineFacade {
  std::vector<Sonatrix::Core::ChordTrackEvent> _chordTrack;
  Sonatrix::Core::MIDI::MIDIStream _scheduledStream;
  std::unique_ptr<std::thread> _playbackThread;
  std::string _selectedPatternTemplateId;
  std::atomic<bool> _shouldStopPlayback;
  std::atomic<int64_t> _currentPlayheadTick;
  double _tempoBPM;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _audioEngine = [[StandaloneAudioEngine alloc] init];
    _selectedPatternTemplateId = kDefaultPatternTemplateId;
    _internalIsPlaying = NO;
    _shouldStopPlayback.store(false);
    _currentPlayheadTick.store(0);
    _tempoBPM = kDefaultTempoBPM;

    const std::string patternLibraryPath =
        ResolvePatternLibraryPath([NSBundle mainBundle]);
    if (!patternLibraryPath.empty()) {
      Sonatrix::Core::PatternLibrary::GetInstance().LoadFromJSON(
          patternLibraryPath);
    }
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

- (double)currentPlayheadTick {
  return static_cast<double>(_currentPlayheadTick.load());
}

- (double)tempoBPM {
  return _tempoBPM;
}

- (void)play {
  [self playFromTick:[self currentPlayheadTick]];
}

- (void)playFromTick:(double)tickOffset {
  if (_internalIsPlaying) {
    return;
  }

  _currentPlayheadTick.store(
      std::max<int64_t>(0, static_cast<int64_t>(tickOffset)));
  [_audioEngine start];
  _internalIsPlaying = YES;
  [self restartPlaybackThreadFromTick:_currentPlayheadTick.load()];
}

- (void)stop {
  [self stopPlaybackThread];
  [_audioEngine stop];
  _internalIsPlaying = NO;
}

- (void)seekToTick:(double)tickOffset {
  const int64_t targetTick =
      std::max<int64_t>(0, static_cast<int64_t>(tickOffset));
  _currentPlayheadTick.store(targetTick);

  if (_internalIsPlaying) {
    [self restartPlaybackThreadFromTick:targetTick];
  }
}

- (void)setTempoBPM:(double)tempoBPM {
  _tempoBPM = std::clamp(tempoBPM, 40.0, 240.0);

  if (_internalIsPlaying) {
    [self restartPlaybackThreadFromTick:_currentPlayheadTick.load()];
  }
}

- (void)clearChords {
  _chordTrack.clear();
  _scheduledStream.events.clear();
  _currentPlayheadTick.store(0);
  [self stopPlaybackThread];
}

- (void)addChordWithRoot:(uint8_t)rootKey
                 quality:(uint8_t)quality
              tickOffset:(double)offset {
  [self addChordWithRoot:rootKey
                 quality:quality
              tickOffset:offset
             guitarFrets:nil
               noteOrder:nil
          noteVelocities:nil];
}

- (void)addChordWithRoot:(uint8_t)rootKey
                 quality:(uint8_t)quality
              tickOffset:(double)offset
             guitarFrets:(NSArray<NSNumber *> *)guitarFrets
               noteOrder:(NSArray<NSNumber *> *)noteOrder
          noteVelocities:(NSArray<NSNumber *> *)noteVelocities {
  Sonatrix::Core::ChordTrackEvent ev = MakeChordEvent(rootKey, quality, offset);

  auto applyEditArray = [&](NSArray<NSNumber *> *source,
                            auto &targetArray,
                            auto converter) {
    if (source == nil || [source count] != 6) {
      return false;
    }

    for (NSUInteger index = 0; index < 6; ++index) {
      targetArray[index] = converter(source[index]);
    }
    return true;
  };

  const bool hasFrets = applyEditArray(
      guitarFrets, ev.guitarEditData.frets,
      [](NSNumber *value) { return static_cast<int8_t>([value intValue]); });
  if (hasFrets) {
    ev.guitarEditData.hasCustomVoicing = true;

    applyEditArray(noteOrder, ev.guitarEditData.noteOrder,
                   [](NSNumber *value) {
                     return static_cast<int8_t>([value intValue]);
                   });
    applyEditArray(noteVelocities, ev.guitarEditData.noteVelocity,
                   [](NSNumber *value) {
                     return static_cast<uint8_t>(
                         std::clamp([value intValue], 1, 127));
                   });
  }

  _chordTrack.push_back(ev);

  std::stable_sort(
      _chordTrack.begin(), _chordTrack.end(),
      [](const Sonatrix::Core::ChordTrackEvent &lhs,
         const Sonatrix::Core::ChordTrackEvent &rhs) {
        return lhs.position.ticks < rhs.position.ticks;
      });
}

- (void)compileAndSchedule {
  _scheduledStream =
      CompileInteractiveArrangement(_chordTrack, _selectedPatternTemplateId);

  if (_internalIsPlaying) {
    [self restartPlaybackThreadFromTick:_currentPlayheadTick.load()];
  } else {
    [self stopPlaybackThread];
  }
}

- (void)restartPlaybackThread {
  [self restartPlaybackThreadFromTick:_currentPlayheadTick.load()];
}

- (void)restartPlaybackThreadFromTick:(int64_t)startTick {
  [self stopPlaybackThread];

  if (_scheduledStream.events.empty()) {
    return;
  }

  const Sonatrix::Core::MIDI::MIDIStream stream = _scheduledStream;
  const int64_t clampedStartTick = std::max<int64_t>(0, startTick);
  const double tempoBPM = _tempoBPM;
  _shouldStopPlayback.store(false);
  _currentPlayheadTick.store(clampedStartTick);

  _playbackThread =
      std::make_unique<std::thread>([self, stream, clampedStartTick, tempoBPM]() {
        int64_t currentTick = clampedStartTick;

        for (const auto &event : stream.events) {
          if (self->_shouldStopPlayback.load()) {
            break;
          }

          if (event.timelinePosition.ticks < clampedStartTick) {
            continue;
          }

          const int64_t sleepTicks = event.timelinePosition.ticks - currentTick;
          if (sleepTicks > 0) {
            std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(
                sleepTicks * MillisecondsPerTick(tempoBPM)));
          }
          currentTick = event.timelinePosition.ticks;
          self->_currentPlayheadTick.store(currentTick);

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

        if (!self->_shouldStopPlayback.load()) {
          self->_internalIsPlaying = NO;
        }
      });
}

- (void)stopPlaybackThread {
  _shouldStopPlayback.store(true);
  if (_playbackThread && _playbackThread->joinable()) {
    _playbackThread->join();
  }
  _playbackThread.reset();
}

- (void)setVolume:(float)volume forBus:(uint8_t)busIndex {
  [_audioEngine setVolume:volume forBus:busIndex];
}

- (void)setPatternTemplateId:(NSString *)patternTemplateId {
  if (patternTemplateId != nil && [patternTemplateId length] > 0) {
    _selectedPatternTemplateId = std::string([patternTemplateId UTF8String]);
  } else {
    _selectedPatternTemplateId = kDefaultPatternTemplateId;
  }
}

- (BOOL)bounceAudioToPath:(NSString *)path
               assetsPath:(NSString *)assetsPath
                  volumes:(NSArray<NSNumber *> *)volumes {
  const Sonatrix::Core::MIDI::MIDIStream masterStream =
      CompileInteractiveArrangement(_chordTrack, _selectedPatternTemplateId);

  std::vector<float> cVols;
  for (NSNumber *vol in volumes) {
    cVols.push_back([vol floatValue]);
  }

  std::string cppOutputPath = [path UTF8String];
  std::string cppAssetsPath = [assetsPath UTF8String];

  return Sonatrix::Core::Audio::AudioExporter::BounceOffline(
      cppOutputPath, masterStream.events, cppAssetsPath, cVols, 44100.0,
      _tempoBPM);
}

- (BOOL)exportMIDIToPath:(NSString *)path {
  const Sonatrix::Core::MIDI::MIDIStream masterStream =
      CompileInteractiveArrangement(_chordTrack, _selectedPatternTemplateId);

  std::string cppOutputPath = [path UTF8String];
  return Sonatrix::Core::MIDI::MIDIExporter::ExportToSMF(cppOutputPath,
                                                         masterStream.events,
                                                         static_cast<uint16_t>(
                                                             Sonatrix::Core::STANDARD_PPQN),
                                                         _tempoBPM);
}

- (NSArray<NSNumber *> *)suggestGuitarFretsForRoot:(uint8_t)rootKey
                                           quality:(uint8_t)quality {
  const auto event = MakeChordEvent(rootKey, quality, 0.0);
  const auto frets = SuggestAcousticVoicingFrets(event.chord);

  NSMutableArray<NSNumber *> *result =
      [NSMutableArray arrayWithCapacity:frets.size()];
  for (const int8_t fret : frets) {
    [result addObject:@(fret)];
  }
  return result;
}

@end
