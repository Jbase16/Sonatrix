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
    const EditorClip &clip, const std::vector<ChordTrackEvent> &chordTimeline,
    Sonatrix::Core::ML::DynamicGrooveVector *grooveVectorContext) const {
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

    // Phase-Lock to the Drum Groove Vector
    MusicalTime lockedTime = eventAbsoluteTime;
    if (grooveVectorContext) {
      if (auto groove =
              grooveVectorContext->GetOffsetForSubbeat(eventAbsoluteTime)) {
        lockedTime = lockedTime + MusicalTime(groove->deviationTicks);
      }
    }

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
      EmitStrum(stream, lockedTime, mir.type, mir.velocityBase, activeVoicing);
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

  // Build array of physical string targets (keeping both pitch and physical string index)
  struct NoteTarget { int pitch; int stringIndex; };
  std::vector<NoteTarget> stringTargets;
  for (int i = 0; i < 6; ++i) {
    int pitch = voicing.GetMidiPitch(i);
    if (pitch != -1) {
      stringTargets.push_back({pitch, i});
    }
  }

  if (direction == ArticulationType::GuitarUpstroke) {
    std::reverse(stringTargets.begin(), stringTargets.end());
  }

  int64_t accumulatedOffset = 0;
  for (const auto& target : stringTargets) {
    MusicalTime triggerTime = baseTime + MusicalTime(accumulatedOffset);

    // Encode physical string info (+1 to avoid generic channel 0 collision) as MIDI channel
    uint8_t strChannel = static_cast<uint8_t>(target.stringIndex + 1);

    // Note On
    outStream.events.push_back({triggerTime, MIDIEventType::NoteOn, strChannel,
                                static_cast<uint8_t>(target.pitch), baseVelocity});

    // Note Off (Generic 1.0 beat duration for demo)
    MusicalTime endTime = triggerTime + BeatsToTime(1.0);
    outStream.events.push_back(
        {endTime, MIDIEventType::NoteOff, strChannel, static_cast<uint8_t>(target.pitch), 0});

    accumulatedOffset += dispersionTicks;
  }
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
