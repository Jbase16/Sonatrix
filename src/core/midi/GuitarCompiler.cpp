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
      EmitStrum(stream, lockedTime, mir.type, mir.velocityBase,
                BeatsToTime(mir.lengthBeats), activeVoicing);
    }
  }

  stream.SortByTime();
  return stream;
}

void GuitarCompiler::EmitStrum(
    MIDIStream &outStream, MusicalTime baseTime, ArticulationType direction,
    uint8_t baseVelocity, MusicalTime duration,
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing) const {

  // Sequential string sweep timing
  constexpr int64_t dispersionTicks = 15;

  // Shorter guitar-like restrum sustain.
  // Do NOT simply hold every string for the full MIR event length.
  const MusicalTime maxStrumLength = BeatsToTime(0.28);

  auto minTime = [](const MusicalTime &a, const MusicalTime &b) {
    return (a.ticks < b.ticks) ? a : b;
  };

  const MusicalTime noteLength = minTime(duration, maxStrumLength);

  struct NoteTarget {
    int pitch;
    int stringIndex;
  };

  std::vector<NoteTarget> stringTargets;
  stringTargets.reserve(6);

  for (int stringIndex = 0; stringIndex < 6; ++stringIndex) {
    const int pitch = voicing.GetMidiPitch(stringIndex);
    if (pitch != -1) {
      stringTargets.push_back({pitch, stringIndex});
    }
  }

  // Reverse for true upstroke
  if (direction == ArticulationType::GuitarUpstroke) {
    std::reverse(stringTargets.begin(), stringTargets.end());
  }

  // Track the newest ringing note per physical string within this emitted strum.
  // This lets us choke same-string retriggers before re-articulating them.
  std::array<int, 6> lastPitchPerString;
  std::array<bool, 6> hasActivePerString;
  lastPitchPerString.fill(-1);
  hasActivePerString.fill(false);

  int64_t accumulatedOffset = 0;

  for (const auto &target : stringTargets) {
    const MusicalTime triggerTime = baseTime + MusicalTime(accumulatedOffset);
    const uint8_t strChannel = static_cast<uint8_t>(target.stringIndex + 1);

    // If this physical string already has a ringing note in this emitted sequence,
    // choke it just before re-triggering.
    if (hasActivePerString[target.stringIndex]) {
      MusicalTime chokeTime = triggerTime - MusicalTime(1);
      outStream.events.push_back(
          {chokeTime,
           MIDIEventType::NoteOff,
           strChannel,
           static_cast<uint8_t>(lastPitchPerString[target.stringIndex]),
           0});
    }

    // Note On
    outStream.events.push_back(
        {triggerTime,
         MIDIEventType::NoteOn,
         strChannel,
         static_cast<uint8_t>(target.pitch),
         baseVelocity});

    // Note Off
    const MusicalTime endTime = triggerTime + noteLength;
    outStream.events.push_back(
        {endTime,
         MIDIEventType::NoteOff,
         strChannel,
         static_cast<uint8_t>(target.pitch),
         0});

    hasActivePerString[target.stringIndex] = true;
    lastPitchPerString[target.stringIndex] = target.pitch;

    accumulatedOffset += dispersionTicks;
  }
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix