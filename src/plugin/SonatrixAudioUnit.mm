#import "SonatrixAudioUnit.h"
#include <memory>
#import <os/log.h>
#include <vector>

#include "../core/EngineConfig.h"
#include "../core/audio/VoiceManager.h"
#include "../core/midi/MIDIEvent.h"

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
  _voiceManager = std::make_unique<Sonatrix::Core::Audio::VoiceManager>();
  _voiceManager->InitializeTestTones();

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

        Sonatrix::Core::MIDI::MIDIEvent ev;
        // Timestamp parsed conceptually; processed synchronously in block
        ev.data1 = midiEvent->data[1];
        ev.data2 = midiEvent->data[2];

        if (status == 0x90)
          ev.type = Sonatrix::Core::MIDI::MIDIEventType::NoteOn;
        else if (status == 0x80)
          ev.type = Sonatrix::Core::MIDI::MIDIEventType::NoteOff;
        else
          ev.type = Sonatrix::Core::MIDI::MIDIEventType::ControlChange;

        events.push_back(ev);
      }
      event = event->head.next;
    }

    manager->ProcessMIDI(events, manager->GetTestArticulation());

    // Initialize output buffers to zero
    for (UInt32 i = 0; i < outputData->mNumberBuffers; ++i) {
      memset(outputData->mBuffers[i].mData, 0,
             outputData->mBuffers[i].mDataByteSize);
    }

    // 4. Execute C++ VoiceManager Synthesis
    manager->RenderAudio((float *)outputData->mBuffers[0].mData, frameCount, 2);

    return noErr;
  } copy];
}

@end
