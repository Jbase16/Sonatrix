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

- (instancetype)init {
  self = [super init];
  if (self) {
    _voiceManager = std::make_unique<Sonatrix::Core::Audio::VoiceManager>();
    _voiceManager->InitializeTestTones();
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
             manager->ProcessMIDI(events, manager->GetTestArticulation());
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

  for (int i = 0; i < pktlist->numPackets; ++i) {
    // Parse basic 3-byte MIDI messages
    if (packet->length >= 3) {
      uint8_t status = packet->data[0] & 0xF0;
      if (status == 0x90 || status == 0x80) { // Note On / Note Off
        Sonatrix::Core::MIDI::MIDIEvent ev;
        // ev.timestamp = packet->timeStamp; // Abstract
        ev.data1 = packet->data[1];
        ev.data2 = packet->data[2];
        ev.type = (status == 0x90)
                      ? Sonatrix::Core::MIDI::MIDIEventType::NoteOn
                      : Sonatrix::Core::MIDI::MIDIEventType::NoteOff;

        queue->Push(ev);
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
