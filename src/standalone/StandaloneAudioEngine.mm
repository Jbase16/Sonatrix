//
//  StandaloneAudioEngine.mm
//  Sonatrix
//

#import "StandaloneAudioEngine.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreMIDI/CoreMIDI.h>
#include <memory>
#include <vector>

#include "../core/audio/VoiceManager.h"
#include "../core/concurrency/SPSCQueue.h"
#include "../core/midi/MIDIEvent.h"

namespace {

std::string ResolveBassMockKitPath(NSBundle *bundle) {
  NSFileManager *fileManager = [NSFileManager defaultManager];

  if (bundle != nil) {
    NSString *resourcePath = [bundle resourcePath];
    if (resourcePath != nil) {
      NSString *bundledPath =
          [resourcePath stringByAppendingPathComponent:@"Assets/samples/bass_mock"];
      if ([fileManager fileExistsAtPath:bundledPath]) {
        return std::string([bundledPath UTF8String]);
      }
    }
  }

  NSString *repoPath = @"/Users/jason/Developer/Sonatrix/assets/samples/bass_mock";
  if ([fileManager fileExistsAtPath:repoPath]) {
    return std::string([repoPath UTF8String]);
  }

  return {};
}

} // namespace

@interface StandaloneAudioEngine ()
@property(nonatomic, strong) AVAudioEngine *engine;
- (Sonatrix::Core::Audio::VoiceManager *)getVoiceManager;
- (Sonatrix::Core::Concurrency::SPSCQueue<Sonatrix::Core::MIDI::MIDIEvent> *)
    getMidiQueue;
@end

@implementation StandaloneAudioEngine {
  std::unique_ptr<Sonatrix::Core::Audio::VoiceManager> _voiceManager;
  std::unique_ptr<
      Sonatrix::Core::Concurrency::SPSCQueue<Sonatrix::Core::MIDI::MIDIEvent>>
      _midiQueue;
  MIDIClientRef _midiClient;
  MIDIPortRef _midiInputPort;
}

- (Sonatrix::Core::Audio::VoiceManager *)getVoiceManager {
  return _voiceManager.get();
}

- (Sonatrix::Core::Concurrency::SPSCQueue<Sonatrix::Core::MIDI::MIDIEvent> *)
    getMidiQueue {
  return _midiQueue.get();
}

- (void)pushMIDIEventStatus:(uint8_t)status
                      data1:(uint8_t)data1
                      data2:(uint8_t)data2 {
  [self pushMIDIEventStatus:status data1:data1 data2:data2 channel:0];
}

- (void)pushMIDIEventStatus:(uint8_t)status
                      data1:(uint8_t)data1
                      data2:(uint8_t)data2
                    channel:(uint8_t)channel {
  if (!_midiQueue)
    return;
  Sonatrix::Core::MIDI::MIDIEvent ev;
  ev.data1 = data1;
  ev.data2 = data2;
  ev.channel = channel;
  if ((status & 0xF0) == 0x90 && data2 > 0) {
    ev.type = Sonatrix::Core::MIDI::MIDIEventType::NoteOn;
  } else if ((status & 0xF0) == 0x80 ||
             ((status & 0xF0) == 0x90 && data2 == 0)) {
    ev.type = Sonatrix::Core::MIDI::MIDIEventType::NoteOff;
  } else {
    ev.type = Sonatrix::Core::MIDI::MIDIEventType::ControlChange;
  }
  _midiQueue->Push(ev);
}

- (void)setVolume:(float)volume forBus:(uint8_t)busIndex {
  if (_voiceManager) {
    _voiceManager->GetMixer().SetBusVolume(static_cast<Sonatrix::Core::Audio::MixerBus>(busIndex), volume);
  }
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _voiceManager = std::make_unique<Sonatrix::Core::Audio::VoiceManager>();
    const std::string kitPath = ResolveBassMockKitPath([NSBundle mainBundle]);
    if (!kitPath.empty()) {
      _voiceManager->LoadInstrumentKit(kitPath);
    } else {
      NSLog(@"StandaloneAudioEngine: Failed to resolve bass_mock sample kit path.");
    }
    _midiQueue = std::make_unique<Sonatrix::Core::Concurrency::SPSCQueue<
        Sonatrix::Core::MIDI::MIDIEvent>>(1024);

    _engine = [[AVAudioEngine alloc] init];
    [self setupAudio];
    [self setupMIDI];
  }
  return self;
}

- (void)setupAudio {
  AVAudioFormat *format =
      [[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100.0
                                                     channels:2];

  // Create an AVAudioSourceNode to pull from the C++ VoiceManager
  Sonatrix::Core::Audio::VoiceManager *manager = _voiceManager.get();
  Sonatrix::Core::Concurrency::SPSCQueue<Sonatrix::Core::MIDI::MIDIEvent>
      *queue = _midiQueue.get();

  AVAudioSourceNode *sourceNode = [[AVAudioSourceNode alloc]
      initWithFormat:format
         renderBlock:^OSStatus(BOOL *_Nonnull isSilence,
                               const AudioTimeStamp *_Nonnull timestamp,
                               AVAudioFrameCount frameCount,
                               AudioBufferList *_Nonnull outputData) {
           // --- Process queued MIDI exactly at the start of the audio frame
           // logic ---
           std::vector<Sonatrix::Core::MIDI::MIDIEvent> events;
           Sonatrix::Core::MIDI::MIDIEvent ev;
           while (queue->Pop(ev)) {
             events.push_back(ev);
           }
           if (!events.empty()) {
             manager->ProcessMIDI(events, manager->GetKitArticulation());
           }

           // Zero out buffers
           float *channels[8];
           UInt32 numChannels = MIN(outputData->mNumberBuffers, 8);
           for (UInt32 i = 0; i < numChannels; ++i) {
             memset(outputData->mBuffers[i].mData, 0,
                    outputData->mBuffers[i].mDataByteSize);
             channels[i] = (float *)outputData->mBuffers[i].mData;
           }

           // Render C++ Synthesizer
           manager->RenderAudio(channels, frameCount, numChannels);

           *isSilence = NO;

           return noErr;
         }];

  [_engine attachNode:sourceNode];
  [_engine connect:sourceNode to:_engine.mainMixerNode format:format];
}

// C-style callback for CoreMIDI (Runs on a High Priority Hardware Thread)
static void MIDIInputCallback(const MIDIPacketList *pktlist,
                              void *readProcRefCon, void *srcConnRefCon) {
  StandaloneAudioEngine *engine =
      (__bridge StandaloneAudioEngine *)readProcRefCon;
  Sonatrix::Core::Concurrency::SPSCQueue<Sonatrix::Core::MIDI::MIDIEvent>
      *queue = [engine getMidiQueue];
  if (!queue)
    return;

  const MIDIPacket *packet = &pktlist->packet[0];
  uint8_t runningStatus = 0;

  for (int i = 0; i < pktlist->numPackets; ++i) {
    int index = 0;
    while (index < packet->length) {
      uint8_t byte = packet->data[index];
      if (byte & 0x80) {
        // High bit set: New Status byte
        runningStatus = byte;
        index++;
      }

      uint8_t type = runningStatus & 0xF0;
      if (type == 0x90 || type == 0x80) {
        if (index + 1 < packet->length) {
          uint8_t data1 = packet->data[index];
          uint8_t data2 = packet->data[index + 1];

          if (type == 0x90 && data2 == 0)
            type = 0x80;

          Sonatrix::Core::MIDI::MIDIEvent ev;
          ev.data1 = data1;
          ev.data2 = data2;
          ev.channel = static_cast<uint8_t>((runningStatus & 0x0F) + 1);
          ev.type = (type == 0x90)
                        ? Sonatrix::Core::MIDI::MIDIEventType::NoteOn
                        : Sonatrix::Core::MIDI::MIDIEventType::NoteOff;
          queue->Push(ev);

          index += 2;
        } else {
          break;
        }
      } else if (type == 0xC0 || type == 0xD0) {
        index += 1; // 1 data byte
      } else if (type == 0xF0) {
        break; // System messages, just ignore rest of packet
      } else {
        if (index + 1 < packet->length) {
          index += 2; // Default 2 data bytes (CC, PitchBend, etc)
        } else {
          break;
        }
      }
    }
    packet = MIDIPacketNext(packet);
  }
}

- (void)setupMIDI {
  // Create MIDI Client
  OSStatus status = MIDIClientCreate(CFSTR("Sonatrix Standalone Client"), NULL,
                                     NULL, &_midiClient);
  if (status != noErr)
    return;

  // Create Input Port
  status = MIDIInputPortCreate(_midiClient, CFSTR("Sonatrix Input Port"),
                               MIDIInputCallback, (__bridge void *)self,
                               &_midiInputPort);
  if (status != noErr)
    return;

  // Connect all existing hardware MIDI sources to our input port
  ItemCount sourceCount = MIDIGetNumberOfSources();
  for (ItemCount i = 0; i < sourceCount; ++i) {
    MIDIEndpointRef src = MIDIGetSource(i);
    MIDIPortConnectSource(_midiInputPort, src, NULL);
  }
}

- (void)start {
  NSError *error = nil;
  [_engine startAndReturnError:&error];
  if (error) {
    NSLog(@"Failed to start AVAudioEngine: %@", error.localizedDescription);
  }
}

- (void)stop {
  [_engine stop];
  // Dispose MIDI
  if (_midiInputPort)
    MIDIPortDispose(_midiInputPort);
  if (_midiClient)
    MIDIClientDispose(_midiClient);
}

@end
