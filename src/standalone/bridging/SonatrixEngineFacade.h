//
//  SonatrixEngineFacade.h
//  Sonatrix
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

// Facade to handle bridging SwiftUI state -> C++ Compilers -> AVAudioEngine
@interface SonatrixEngineFacade : NSObject

@property(nonatomic, readonly) BOOL isPlaying;
@property(nonatomic, readonly) double currentPlayheadTick;
@property(nonatomic, readonly) double tempoBPM;

- (instancetype)init;

// Transport Controls
- (void)play;
- (void)playFromTick:(double)tickOffset;
- (void)stop;
- (void)seekToTick:(double)tickOffset;
- (void)setTempoBPM:(double)tempoBPM;

// Arrangement Controls (Mocked for Phase 11)
- (void)clearChords;
- (void)addChordWithRoot:(uint8_t)rootKey
                 quality:(uint8_t)quality
              tickOffset:(double)offset;
- (void)addChordWithRoot:(uint8_t)rootKey
                 quality:(uint8_t)quality
              tickOffset:(double)offset
             guitarFrets:(nullable NSArray<NSNumber *> *)guitarFrets
               noteOrder:(nullable NSArray<NSNumber *> *)noteOrder
          noteVelocities:(nullable NSArray<NSNumber *> *)noteVelocities;

// Request the C++ backend to compile the current arrangement and schedule it
- (void)compileAndSchedule;

// Pattern Selection
- (void)setPatternTemplateId:(NSString *)patternTemplateId;

// Audio Mixer Controls
- (void)setVolume:(float)volume forBus:(uint8_t)busIndex;

// Offline Export
- (BOOL)bounceAudioToPath:(NSString *)path
               assetsPath:(NSString *)assetsPath
                  volumes:(NSArray<NSNumber *> *)volumes;

// MIDI Export
- (BOOL)exportMIDIToPath:(NSString *)path;

// Editing Helpers
- (NSArray<NSNumber *> *)suggestGuitarFretsForRoot:(uint8_t)rootKey
                                           quality:(uint8_t)quality;

@end

NS_ASSUME_NONNULL_END
