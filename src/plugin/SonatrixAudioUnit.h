#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface SonatrixAudioUnit : AUAudioUnit

// The entry point for Logic Pro to instantiate our C++ DSP core
- (instancetype)initWithComponentDescription:(AudioComponentDescription)componentDescription
                                     options:(AudioComponentInstantiationOptions)options
                                       error:(NSError **)outError;

@end

NS_ASSUME_NONNULL_END
