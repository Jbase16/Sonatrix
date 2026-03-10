#include "GuitarCompiler.h"
#include "../engines/guitar/VoicingGraphSolver.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <unordered_map>

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

  // 1. Solve the global voice-leading path for the full chord timeline
  Sonatrix::Core::Engines::Guitar::VoicingGraphSolver solver;
  std::vector<Sonatrix::Core::Engines::Guitar::GuitarVoicing> optimalVoicings;

  if (!chordTimeline.empty()) {
    optimalVoicings = solver.SolveVoiceLeading(chordTimeline);
  }

  Sonatrix::Core::Engines::Guitar::GuitarVoicing fallbackVoicing;

  for (const auto &mir : clip.basePattern->events) {
    const MusicalTime eventAbsoluteTime = clip.timelineStart + mir.offsetMap;

    // Groove-lock if a groove vector exists
    MusicalTime lockedTime = eventAbsoluteTime;
    if (grooveVectorContext) {
      if (auto groove =
              grooveVectorContext->GetOffsetForSubbeat(eventAbsoluteTime)) {
        lockedTime = lockedTime + MusicalTime(groove->deviationTicks);
      }
    }

    // Find active chord index
    int currentChordIndex = -1;
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

    bool isMuted = false;
    for (const auto &delta : clip.overrides) {
      if (delta.startOffset == mir.offsetMap &&
          delta.op == DeltaOperation::MuteStroke) {
        isMuted = true;
        break;
      }
    }

    if (!isMuted) {
      // Reverted to the correct 5-argument signature
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

  // 40 ticks at 960 PPQ = ~20ms per string. This creates a beautiful, natural glide.
  int64_t dispersionTicks = 40;

  struct NoteTarget {
    int pitch;
    int stringIndex;
  };
  std::vector<NoteTarget> stringTargets;
  for (int i = 0; i < 6; ++i) {
    int pitch = voicing.GetMidiPitch(i);
    if (pitch != -1) {
      stringTargets.push_back({pitch, i});
    }
  }

  // To play an upstroke, simply reverse the physical string order.
  // The time offset will remain positive, so the high strings are plucked FIRST.
  if (direction == ArticulationType::GuitarUpstroke) {
    std::reverse(stringTargets.begin(), stringTargets.end());
  }

  int64_t accumulatedOffset = 0;
  for (const auto &target : stringTargets) {
    MusicalTime triggerTime = baseTime + MusicalTime(accumulatedOffset);
    uint8_t strChannel = static_cast<uint8_t>(target.stringIndex + 1);

    // Note On
    outStream.events.push_back({triggerTime, MIDIEventType::NoteOn, strChannel,
                                static_cast<uint8_t>(target.pitch),
                                baseVelocity});

    // Make the duration longer (e.g. 2 full beats) so it rings out into the
    // next strum
    MusicalTime endTime = triggerTime + BeatsToTime(2.0);
    outStream.events.push_back({endTime, MIDIEventType::NoteOff, strChannel,
                                static_cast<uint8_t>(target.pitch), 0});

    accumulatedOffset += dispersionTicks;
  }
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix