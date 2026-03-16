#include "BassCompiler.h"

namespace Sonatrix {
namespace Core {
namespace MIDI {

std::unique_ptr<IMIRCompiler> CreateBassEngine() {
  return std::make_unique<BassCompiler>();
}

MIDIStream BassCompiler::CompileClip(
    const EditorClip &clip, const std::vector<ChordTrackEvent> &chordTimeline,
    Sonatrix::Core::ML::DynamicGrooveVector *grooveVectorContext) const {
  MIDIStream stream;

  // Mock Bass Range: E1 (40) to G2 (55)

  for (const auto &mir : clip.basePattern->events) {
    MusicalTime eventAbsoluteTime = clip.timelineStart + mir.offsetMap;

    // 1. Determine Harmonic Target Pitch
    int currentChordIndex = -1;
    for (size_t i = 0; i < chordTimeline.size(); ++i) {
      if (eventAbsoluteTime >= chordTimeline[i].position) {
        currentChordIndex = static_cast<int>(i);
      } else {
        break;
      }
    }

    uint8_t targetPitch = 40; // Default Low E
    if (currentChordIndex >= 0 &&
        currentChordIndex < static_cast<int>(chordTimeline.size())) {
      const auto &activeChord = chordTimeline[currentChordIndex].chord;
      int rootOffset = static_cast<int>(activeChord.root);

      // Octave calculation logic (try to stay between E1 and G2)
      // Octave 1 base is C1 (36).
      int calculatedPitch = 36 + rootOffset;
      if (calculatedPitch < 40)
        calculatedPitch += 12; // Shift up if too low
      targetPitch = static_cast<uint8_t>(calculatedPitch);
    }

    // 2. Query Phase-Locking Groove Vector
    MusicalTime lockedTime = eventAbsoluteTime;
    double lockedVelocityMult = 1.0;

    if (grooveVectorContext) {
      if (auto groove =
              grooveVectorContext->GetOffsetForSubbeat(eventAbsoluteTime)) {
        lockedTime = lockedTime + MusicalTime(groove->deviationTicks);
        lockedVelocityMult = groove->velocityMultiplier;
      }
    }

    // 3. Apply Delta Overrides (e.g., Slap/Pop modifier, Mutes)
    bool isMuted = false;
    for (const auto &delta : clip.overrides) {
      if (delta.startOffset == mir.offsetMap &&
          delta.op == DeltaOperation::MuteStroke) {
        isMuted = true;
        break;
      }
    }

    if (!isMuted) {
      uint8_t finalVel = static_cast<uint8_t>(
          std::min(127.0, mir.velocityBase * lockedVelocityMult));

      // Emit Note On
      stream.events.push_back({lockedTime, MIDIEventType::NoteOn, 1,
                               targetPitch, finalVel}); // Ch 2 (idx 1) for Bass

      // 4. Emit Note Off
      // Use mir.lengthBeats if available, otherwise default to 1 beat (480 ticks)
      uint32_t durationTicks = (mir.lengthBeats > 0)
                                   ? static_cast<uint32_t>(mir.lengthBeats * 480)
                                   : 480;
      MusicalTime offTime = lockedTime + MusicalTime(durationTicks);
      stream.events.push_back(
          {offTime, MIDIEventType::NoteOff, 1, targetPitch, 0});
    }
  }

  stream.SortByTime();
  return stream;
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
