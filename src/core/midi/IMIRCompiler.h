#pragma once

#include "MIDIEvent.h"
#include "../mir/MIRPattern.h"
#include "../mir/DeltaGraph.h"
#include "../arrangement/ChordTrack.h"

namespace Sonatrix {
namespace Core {
namespace MIDI {

// -----------------------------------------------------------------------------
// MIR Compiler Interface
// The base class for all Performance Engines (Guitar, Piano, etc).
// Their responsibility is taking a high-level intent (MIR) and a Harmonic Context,
// resolving any non-destructive User Edits (DeltaGraph), and outputting Literal MIDI.
// -----------------------------------------------------------------------------

class IMIRCompiler {
public:
    virtual ~IMIRCompiler() = default;
    
    // Compiles a single clip (Pattern + User Edits) against the current Chords
    // into a stream of literal MIDI events.
    virtual MIDIStream CompileClip(
        const EditorClip& clip, 
        const std::vector<ChordTrackEvent>& chordTimeline
    ) const = 0;
};

// Example Factory or Registry could manage engine implementations internally
// e.g., std::unique_ptr<IMIRCompiler> CreateGuitarEngine();

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
