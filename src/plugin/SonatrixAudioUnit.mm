#import "SonatrixAudioUnit.h"

@implementation SonatrixAudioUnit
- (instancetype)initWithComponentDescription:(AudioComponentDescription)componentDescription options:(AudioComponentInstantiationOptions)options error:(NSError **)outError {
    self = [super initWithComponentDescription:componentDescription options:options error:outError];
    if (self) {
        // Initialize C++ Core here
    }
    return self;
}
@end
