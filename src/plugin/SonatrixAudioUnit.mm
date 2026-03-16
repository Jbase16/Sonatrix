#import "SonatrixAudioUnit.h"
#include <memory>
#import <os/log.h>
#include <vector>

#include "../core/EngineConfig.h"
#include "../core/audio/PlaybackInstrument.h"
#include "../core/audio/VoiceManager.h"
#include "../core/audio/VoiceManagerFactory.h"
#include "../core/midi/MIDIEvent.h"

namespace {

struct ResolvedPlaybackKit {
  Sonatrix::Core::Audio::PlaybackInstrument instrument{
      Sonatrix::Core::Audio::PlaybackInstrument::Guitar};
  std::string path;
};

ResolvedPlaybackKit ResolvePlaybackKit(NSBundle *bundle) {
  NSFileManager *fileManager = [NSFileManager defaultManager];

  if (bundle != nil) {
    NSString *resourcePath = [bundle resourcePath];
    if (resourcePath != nil) {
      NSString *bundledGuitarPath =
          [resourcePath stringByAppendingPathComponent:@"Assets/Exciters/FS_Guitars"];
      if ([fileManager fileExistsAtPath:bundledGuitarPath]) {
        return {Sonatrix::Core::Audio::PlaybackInstrument::Guitar,
                std::string([bundledGuitarPath UTF8String])};
      }

      NSString *bundledBassPath =
          [resourcePath stringByAppendingPathComponent:@"Assets/samples/bass_mock"];
      if ([fileManager fileExistsAtPath:bundledBassPath]) {
        return {Sonatrix::Core::Audio::PlaybackInstrument::MockBass,
                std::string([bundledBassPath UTF8String])};
      }
    }
  }

  NSString *repoGuitarPath = @"/Users/jason/Developer/Sonatrix/assets/Exciters/FS_Guitars";
  if ([fileManager fileExistsAtPath:repoGuitarPath]) {
    return {Sonatrix::Core::Audio::PlaybackInstrument::Guitar,
            std::string([repoGuitarPath UTF8String])};
  }

  NSString *repoBassPath = @"/Users/jason/Developer/Sonatrix/assets/samples/bass_mock";
  if ([fileManager fileExistsAtPath:repoBassPath]) {
    return {Sonatrix::Core::Audio::PlaybackInstrument::MockBass,
            std::string([repoBassPath UTF8String])};
  }

  return {};
}

} // namespace

@interface SonatrixAudioUnit ()
@end

@implementation SonatrixAudioUnit {
  std::unique_ptr<Sonatrix::Core::Audio::VoiceManager> _voiceManager;
  AUAudioUnitBusArray *_inputBusses;
  AUAudioUnitBusArray *_outputBusses;
}

- (instancetype)
    initWithComponentDescription:(AudioComponentDescription)componentDescription
                         options:(AudioComponentInstantiationOptions)options
                           error:(NSError **)outError {
  self = [super initWithComponentDescription:componentDescription
                                     options:options
                                       error:outError];
  if (self == nil) {
    return nil;
  }

  // Initialize the C++ Core Engine
  const auto kit =
      ResolvePlaybackKit([NSBundle bundleForClass:[SonatrixAudioUnit class]]);
  _voiceManager =
      Sonatrix::Core::Audio::CreateLoadedVoiceManager(kit.instrument, kit.path);
  if (!_voiceManager || kit.path.empty()) {
    os_log_error(OS_LOG_DEFAULT,
                 "SonatrixAudioUnit: Failed to resolve playback instrument kit path.");
  }

  // Setup Audio Busses (Stereo Output)
  AVAudioFormat *defaultFormat =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100.0
                                                     channels:2];
  AUAudioUnitBus *outputBus =
      [[AUAudioUnitBus alloc] initWithFormat:defaultFormat error:nil];
  _outputBusses =
      [[AUAudioUnitBusArray alloc] initWithAudioUnit:self
                                             busType:AUAudioUnitBusTypeOutput
                                              busses:@[ outputBus ]];

  return self;
}

- (AUAudioUnitBusArray *)inputBusses {
  return _inputBusses; // Sonatrix does not take audio input
}

- (AUAudioUnitBusArray *)outputBusses {
  return _outputBusses;
}

- (BOOL)allocateRenderResourcesAndReturnError:(NSError **)outError {
  if (![super allocateRenderResourcesAndReturnError:outError]) {
    return NO;
  }
  // Prepare C++ lock-free queues and allocate Memory here
  return YES;
}

- (void)deallocateRenderResources {
  [super deallocateRenderResources];
}

- (AUInternalRenderBlock)internalRenderBlock {
  // Capture state required for the real-time C++ render block
  Sonatrix::Core::Audio::VoiceManager *manager = _voiceManager.get();

  return [^AUAudioUnitStatus(
      AudioUnitRenderActionFlags *actionFlags, const AudioTimeStamp *timestamp,
      AVAudioFrameCount frameCount, NSInteger outputBusNumber,
      AudioBufferList *outputData, const AURenderEvent *realtimeEventListHead,
      AURenderPullInputBlock pullInputBlock) {
    float *channels[8];
    UInt32 numChannels = MIN(outputData->mNumberBuffers, 8);
    for (UInt32 i = 0; i < numChannels; ++i) {
      memset(outputData->mBuffers[i].mData, 0,
             outputData->mBuffers[i].mDataByteSize);
      channels[i] = (float *)outputData->mBuffers[i].mData;
    }

    if (manager == nullptr) {
      return noErr;
    }

    // 1. Process Host Musical Context (Tempo, Position)
    // 2. Consume events from the Lock-Free UI Queue (e.g., User changed a
    // chord)

    // 3. Process incoming real-time MIDI from the host
    std::vector<Sonatrix::Core::MIDI::MIDIEvent> events;

    const AURenderEvent *event = realtimeEventListHead;
    while (event != nullptr) {
      if (event->head.eventType == AURenderEventMIDI) {
        const AUMIDIEvent *midiEvent = (const AUMIDIEvent *)event;
        const uint8_t status = midiEvent->data[0] & 0xF0;
        const uint8_t channel = static_cast<uint8_t>((midiEvent->data[0] & 0x0F) + 1);

        Sonatrix::Core::MIDI::MIDIEvent ev;
        // Timestamp parsed conceptually; processed synchronously in block
        ev.data1 = midiEvent->data[1];
        ev.data2 = midiEvent->data[2];
        ev.channel = channel;

        if (status == 0x90 && ev.data2 > 0)
          ev.type = Sonatrix::Core::MIDI::MIDIEventType::NoteOn;
        else if (status == 0x80 || (status == 0x90 && ev.data2 == 0))
          ev.type = Sonatrix::Core::MIDI::MIDIEventType::NoteOff;
        else
          ev.type = Sonatrix::Core::MIDI::MIDIEventType::ControlChange;

        events.push_back(ev);
      }
      event = event->head.next;
    }

    manager->ProcessMIDI(events);

    // 4. Execute C++ VoiceManager Synthesis
    manager->RenderAudio(channels, frameCount, numChannels);

    return noErr;
  } copy];
}

@end
