#include "DrumCompiler.h"
#include <iostream>
#include <random>

namespace Sonatrix {
namespace Core {
namespace MIDI {

std::unique_ptr<IMIRCompiler> CreateDrumEngine() {
  return std::make_unique<DrumCompiler>();
}

MIDIStream DrumCompiler::CompileClip(
    const EditorClip &clip,
    const std::vector<ChordTrackEvent> & /*chordTimeline*/,
    Sonatrix::Core::ML::DynamicGrooveVector *grooveVectorContext) const {
  MIDIStream stream;

  // Setup pseudo-random generator to mock ML-based humanization
  // A sophisticated implementation would query the CoreML Neural Latent space
  // here.
  std::mt19937 rng(42); // Deterministic seed for consistent rendering
  std::normal_distribution<double> timingDist(0.0, 15.0);  // 15 ticks std dev
  std::normal_distribution<double> velocityDist(1.0, 0.1); // +/- 10% velocity

  for (const auto &mir : clip.basePattern->events) {
    MusicalTime eventAbsoluteTime = clip.timelineStart + mir.offsetMap;

    // This simulates applying a Delta Override
    bool isMuted = false;
    for (const auto &delta : clip.overrides) {
      if (delta.startOffset == mir.offsetMap &&
          delta.op == DeltaOperation::MuteStroke) {
        isMuted = true;
        break;
      }
    }

    if (!isMuted) {
      // Generate Neural/Human timing deviation
      int64_t deviation = static_cast<int64_t>(timingDist(rng));
      double velMult = std::max(0.5, std::min(1.5, velocityDist(rng)));

      // Record this offset to the global Matrix so Bass/Guitar can phase-lock
      // to it
      if (grooveVectorContext) {
        Sonatrix::Core::ML::GrooveOffset offset;
        offset.anchorTime = eventAbsoluteTime;
        offset.deviationTicks = deviation;
        offset.velocityMultiplier = velMult;
        grooveVectorContext->EmplaceOffset(offset);
      }

      // Map abstract drum types to General MIDI
      uint8_t gmPitch = 36; // Default Kick (actionParameter 1)
      if (mir.type == ArticulationType::DrumHit) {
        if (mir.actionParameter == 2)
          gmPitch = 38; // Snare
        else if (mir.actionParameter == 3)
          gmPitch = 42; // HiHat Closed
        else if (mir.actionParameter == 4)
          gmPitch = 46; // HiHat Open
      }

      // Apply physical deviation to the generated note
      MusicalTime humanizedTime = eventAbsoluteTime + MusicalTime(deviation);
      uint8_t finalVel =
          static_cast<uint8_t>(std::min(127.0, mir.velocityBase * velMult));

      stream.events.push_back({humanizedTime, MIDIEventType::NoteOn, 9, gmPitch,
                               finalVel}); // Ch 10 (idx 9)

      // Drums are typically one-shots, but we emit a dummy NoteOff
      MusicalTime offTime =
          humanizedTime + MusicalTime(480); // 1/8th note approx
      stream.events.push_back({offTime, MIDIEventType::NoteOff, 9, gmPitch, 0});
    }
  }

  stream.SortByTime();
  return stream;
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
