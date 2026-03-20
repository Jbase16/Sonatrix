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
  uint8_t lastPitch = 0; // State for register continuity

  // REGISTER POLICY: B1 (35) to C4 (60)
  // Our physical anchors are sparse: B1, C3, C4.
  // We prioritize the E2 (40) to G3 (55) range for melodic continuity.

  for (size_t mirIdx = 0; mirIdx < clip.basePattern->events.size(); ++mirIdx) {
    const auto &mir = clip.basePattern->events[mirIdx];
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
      int rootPitch = 36 + rootOffset; // C2 is 36 (assuming 60=C4)

      // Determine interval from actionParameter
      // 0 = Root, 1 = Perfect Fifth, 2 = Octave Up, 3 = Approach Below, 4 = Approach Above
      int interval = mir.actionParameter;
      int calculatedPitch = rootPitch;

      if (interval == 1) {
          calculatedPitch = rootPitch + 7;
          if (calculatedPitch > 55) calculatedPitch -= 12; // Avoid excessive height
      } else if (interval == 2) {
          calculatedPitch = rootPitch + 12; 
      } else if (interval == 3 || interval == 4) {
          // HARMONIC LOOK-AHEAD
          // If this is an approach note, try to target the NEXT chord's root
          int nextChordRoot = rootPitch; 
          if (currentChordIndex + 1 < static_cast<int>(chordTimeline.size())) {
              nextChordRoot = 36 + static_cast<int>(chordTimeline[currentChordIndex+1].chord.root);
          } else if (mirIdx + 1 < clip.basePattern->events.size()) {
              // If no next chord event, but more notes in pattern, assume it targets current root
              nextChordRoot = rootPitch;
          }

          calculatedPitch = (interval == 3) ? (nextChordRoot - 1) : (nextChordRoot + 1);
      }

      // REGISTER CONTINUITY
      // If we have a lastPitch, try to keep this note in the nearest octave to it.
      if (lastPitch > 0) {
          int distCurr = std::abs(calculatedPitch - static_cast<int>(lastPitch));
          int distUp = std::abs((calculatedPitch + 12) - static_cast<int>(lastPitch));
          int distDown = std::abs((calculatedPitch - 12) - static_cast<int>(lastPitch));

          if (distUp < distCurr && distUp < distDown && (calculatedPitch + 12) <= 60) {
              calculatedPitch += 12;
          } else if (distDown < distCurr && distDown < distUp && (calculatedPitch - 12) >= 30) {
              calculatedPitch -= 12;
          }
      }

      // Final bounding: Keep performance between B1 (35) and C4 (60)
      // This ensures we stay within a reasonable distance of our sparse anchors.
      while (calculatedPitch < 35) calculatedPitch += 12;
      while (calculatedPitch > 60) calculatedPitch -= 12;

      targetPitch = static_cast<uint8_t>(calculatedPitch);
      lastPitch = targetPitch;
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
