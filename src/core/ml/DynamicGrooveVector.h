#pragma once

#include "../mir/MusicalTime.h"
#include <vector>
#include <optional>

namespace Sonatrix {
namespace Core {
namespace ML {

// -----------------------------------------------------------------------------
// The Dynamic Groove Vector
// 
// Represents the "Pocket" or human feel of a drum performance.
// Extracted via neural latent space embeddings, it provides absolute timing
// and velocity offsets for any given sub-beat.
// 
// Other engines (Bass, Guitar) interrogate this vector to "phase-lock" 
// their mathematical generation to the human drummer's actual pull/push.
// -----------------------------------------------------------------------------

struct GrooveOffset {
    // The strict mathematical grid position (e.g. exactly Beat 2.0)
    MusicalTime anchorTime;
    
    // The humanistic deviation from the grid (can be negative for "rushing" or positive for "dragging")
    int64_t deviationTicks{0};
    
    // An optional velocity multiplier (e.g. 1.0 = normal, 1.2 = accented)
    // to allow a bass player to intentionally dig in harder when the drummer accents the snare.
    double velocityMultiplier{1.0};
};

class DynamicGrooveVector {
public:
    DynamicGrooveVector() = default;
    
    // Read method for down-stream engines (Bass, Guitar)
    // Given an intended mathematical sub-beat, find the closest groove influence.
    std::optional<GrooveOffset> GetOffsetForSubbeat(const MusicalTime& targetTime) const;
    
    // Write method for the Drum Engine (when it compiles a new MIR clip)
    void EmplaceOffset(const GrooveOffset& offset);
    
    // Clear the vector (e.g., when the arrangement changes)
    void Clear();

private:
    std::vector<GrooveOffset> offsets_;
};

} // namespace ML
} // namespace Core
} // namespace Sonatrix
