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

@end

NS_ASSUME_NONNULL_END
