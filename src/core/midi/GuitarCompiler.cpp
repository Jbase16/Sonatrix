#include "GuitarCompiler.h"
#include "../engines/guitar/VoicingGraphSolver.h"
#include <iostream>

namespace Sonatrix {
namespace Core {
namespace MIDI {

std::unique_ptr<IMIRCompiler> CreateGuitarEngine() {
  return std::make_unique<GuitarCompiler>();
}

MIDIStream GuitarCompiler::CompileClip(
    const EditorClip &clip,
    const std::vector<ChordTrackEvent> &chordTimeline) const {
  MIDIStream stream;

  // 1. Solve the global voice leading path for the entire chord timeline
  Sonatrix::Core::Engines::Guitar::VoicingGraphSolver solver;
  std::vector<Sonatrix::Core::Engines::Guitar::GuitarVoicing> optimalVoicings;

  // Safety check: if there is no chord timeline, we can't generate chords.
  if (!chordTimeline.empty()) {
    optimalVoicings = solver.SolveVoiceLeading(chordTimeline);
  }

  // If the solver failed, we fall back to a completely silent voicing to avoid
  // a crash.
  Sonatrix::Core::Engines::Guitar::GuitarVoicing fallbackVoicing;

  for (const auto &mir : clip.basePattern->events) {
    MusicalTime eventAbsoluteTime = clip.timelineStart + mir.offsetMap;

    int currentChordIndex = -1;
    // Find the active chord for this specific time
    for (size_t i = 0; i < chordTimeline.size(); ++i) {
      if (eventAbsoluteTime >= chordTimeline[i].position) {
        currentChordIndex = static_cast<int>(i);
      } else {
        break;
      }
    }

    Sonatrix::Core::Engines::Guitar::GuitarVoicing activeVoicing =
        fallbackVoicing;
    if (currentChordIndex >= 0 &&
        currentChordIndex < static_cast<int>(optimalVoicings.size())) {
      activeVoicing = optimalVoicings[currentChordIndex];
    }

    // This simulates applying a Delta Override (e.g. MuteStroke)
    bool isMuted = false;
    for (const auto &delta : clip.overrides) {
      // Delta Graph Collision Detection
      if (delta.startOffset == mir.offsetMap &&
          delta.op == DeltaOperation::MuteStroke) {
        isMuted = true;
        break;
      }
    }

    if (!isMuted) {
      EmitStrum(stream, eventAbsoluteTime, mir.type, mir.velocityBase,
                activeVoicing);
    }
  }

  stream.SortByTime();
  return stream;
}

void GuitarCompiler::EmitStrum(
    MIDIStream &outStream, MusicalTime baseTime, ArticulationType direction,
    uint8_t baseVelocity,
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing) const {

  // We disperse the strums sequentially by 10-15 ticks to mimic a pick crossing
  // 6 strings.
  int64_t dispersionTicks =
      (direction == ArticulationType::GuitarDownstroke) ? 15 : -15;

  // Build array of physical string pitches to strum
  std::vector<int> stringPitches;
  for (int i = 0; i < 6; ++i) {
    int pitch = voicing.GetMidiPitch(i);
    if (pitch != -1) {
      stringPitches.push_back(pitch);
    }
  }

  if (direction == ArticulationType::GuitarUpstroke) {
    std::reverse(stringPitches.begin(), stringPitches.end());
  }

  int64_t accumulatedOffset = 0;
  for (int pitch : stringPitches) {
    MusicalTime triggerTime = baseTime + MusicalTime(accumulatedOffset);

    // Note On
    outStream.events.push_back({triggerTime, MIDIEventType::NoteOn, 0,
                                static_cast<uint8_t>(pitch), baseVelocity});

    // Note Off (Generic 1.0 beat duration for demo)
    MusicalTime endTime = triggerTime + BeatsToTime(1.0);
    outStream.events.push_back(
        {endTime, MIDIEventType::NoteOff, 0, static_cast<uint8_t>(pitch), 0});

    accumulatedOffset += dispersionTicks;
  }
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
