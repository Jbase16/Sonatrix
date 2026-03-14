//
//  StandaloneAudioEngine.h
//  Sonatrix
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// A simple Objective-C++ wrapper to interface with the C++ VoiceManager
// and CoreMIDI for the Standalone App.
@interface StandaloneAudioEngine : NSObject

- (instancetype)init;
- (void)start;
- (void)stop;

// Exposes the real-time queue to programmatic arrangement playback components
- (void)pushMIDIEventStatus:(uint8_t)status
                      data1:(uint8_t)data1
                      data2:(uint8_t)data2;

- (void)pushMIDIEventStatus:(uint8_t)status
                      data1:(uint8_t)data1
                      data2:(uint8_t)data2
                    channel:(uint8_t)channel;

// Audio Mixer
- (void)setVolume:(float)volume forBus:(uint8_t)busIndex;

@end

NS_ASSUME_NONNULL_END
