//
//  SonatrixEngineFacade.h
//  Sonatrix
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Facade to handle bridging SwiftUI state -> C++ Compilers -> AVAudioEngine
@interface SonatrixEngineFacade : NSObject

@property(nonatomic, readonly) BOOL isPlaying;

- (instancetype)init;

// Transport Controls
- (void)play;
- (void)stop;

// Arrangement Controls (Mocked for Phase 11)
- (void)clearChords;
- (void)addChordWithRoot:(uint8_t)rootKey
                 quality:(uint8_t)quality
              tickOffset:(double)offset;

// Request the C++ backend to compile the current arrangement and schedule it
- (void)compileAndSchedule;

@end

NS_ASSUME_NONNULL_END
