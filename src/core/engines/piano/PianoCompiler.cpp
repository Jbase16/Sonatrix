#include "PianoCompiler.h"

#include <algorithm>
#include <set>

namespace Sonatrix {
namespace Core {
namespace MIDI {

std::unique_ptr<IMIRCompiler> CreatePianoEngine() {
  return std::make_unique<PianoCompiler>();
}

PianoCompiler::PianoCompiler(PianoStyle style, SopranoContour contour)
    : m_style(style), m_contour(contour) {
}

// Determine output velocity from role and register, using the pattern velocity
// as a dynamic intensity signal (how hard the pattern "wants" this beat) while
// applying role/register shaping relative to a neutral baseline.
static uint8_t ShapeVelocity(uint8_t patternVel, PianoTargetRole role, uint8_t pitch) {
    // Normalize pattern velocity to a 0.0–1.0 intensity factor.
    // The pattern's velocity encodes rhythmic emphasis, not voice balance.
    float intensity = static_cast<float>(patternVel) / 127.0f;

    // Role-based target velocity (at full intensity).
    // These are the "ideal" velocities for each voice role on a real piano.
    int baseVel;
    switch (role) {
      case PianoTargetRole::LH_Root:      baseVel = 90; break;
      case PianoTargetRole::LH_Fifth:
      case PianoTargetRole::LH_ShellLow:  baseVel = 82; break;
      case PianoTargetRole::LH_Octave:    baseVel = 78; break;
      case PianoTargetRole::RH_GuideLow:  baseVel = 72; break;
      case PianoTargetRole::RH_Inner:     baseVel = 64; break;
      case PianoTargetRole::RH_GuideHigh: baseVel = 68; break;
      case PianoTargetRole::RH_Top:       baseVel = 74; break;
      default:                            baseVel = 72; break;
    }

    // Register-based correction: high notes are perceptually louder on piano.
    if (pitch >= 72)      baseVel -= 8;
    else if (pitch >= 67) baseVel -= 4;
    else if (pitch <= 42) baseVel += 8;
    else if (pitch <= 48) baseVel += 4;

    // Scale by pattern intensity
    int outVel = static_cast<int>(baseVel * intensity);
    return static_cast<uint8_t>(std::clamp(outVel, 1, 127));
}

// Try to resolve a pitch for the given role from the voicing.
// If the primary role is empty, try musical fallbacks.
static uint8_t ResolvePitch(const PianoVoicing& voicing, PianoTargetRole role) {
    uint8_t pitch = voicing.GetPitch(role);
    if (pitch != 0) return pitch;

    switch (role) {
      // LH fallbacks: Fifth ↔ ShellLow
      case PianoTargetRole::LH_Fifth:
        return voicing.GetPitch(PianoTargetRole::LH_ShellLow);
      case PianoTargetRole::LH_ShellLow:
        return voicing.GetPitch(PianoTargetRole::LH_Fifth);

      // RH fallbacks: try to fill from occupied neighbors
      case PianoTargetRole::RH_GuideLow:
        pitch = voicing.GetPitch(PianoTargetRole::RH_Inner);
        if (pitch != 0) return pitch;
        return voicing.GetPitch(PianoTargetRole::RH_GuideHigh);

      case PianoTargetRole::RH_GuideHigh:
        pitch = voicing.GetPitch(PianoTargetRole::RH_Inner);
        if (pitch != 0) return pitch;
        return voicing.GetPitch(PianoTargetRole::RH_GuideLow);

      case PianoTargetRole::RH_Inner:
        // Inner is optional — don't scavenge from guide tones
        return 0;

      case PianoTargetRole::RH_Top:
        // Soprano should never fall back — if it's empty, the voicing is broken
        return 0;

      default:
        return 0;
    }
}

MIDIStream PianoCompiler::CompileClip(
    const EditorClip &clip, const std::vector<ChordTrackEvent> &chordTimeline,
    Sonatrix::Core::ML::DynamicGrooveVector *grooveVectorContext) const {
  
  MIDIStream stream;

  // Solve voice leading with style and contour intelligence
  PianoVoicingPlanner planner(m_style, m_contour);
  std::vector<PianoVoicing> solvedTimeline = planner.SolveTimeline(chordTimeline);

  // Map MIR Events to the optimized Voicing
  // Track pitches already emitted per beat to prevent doubled notes from fallback
  MusicalTime lastEventTime{-1};
  std::set<uint8_t> pitchesAtCurrentBeat;

  for (const auto &mir : clip.basePattern->events) {
    MusicalTime eventTime = clip.timelineStart + mir.offsetMap;

    // Reset dedup set when beat changes
    if (eventTime != lastEventTime) {
      pitchesAtCurrentBeat.clear();
      lastEventTime = eventTime;
    }

    // Find active chord index
    int currentChordIndex = -1;
    for (size_t i = 0; i < chordTimeline.size(); ++i) {
      if (eventTime >= chordTimeline[i].position) {
        currentChordIndex = static_cast<int>(i);
      } else {
        break;
      }
    }

    if (currentChordIndex < 0 || currentChordIndex >= static_cast<int>(solvedTimeline.size())) continue;

    const PianoVoicing& voicing = solvedTimeline[currentChordIndex];
    if (!voicing.IsValid()) continue;

    // Resolve pitch with fallback chain
    PianoTargetRole role = static_cast<PianoTargetRole>(mir.actionParameter);
    uint8_t pitch = ResolvePitch(voicing, role);
    if (pitch == 0) continue;

    // Dedup: skip if this pitch was already emitted at this beat
    if (!pitchesAtCurrentBeat.insert(pitch).second) continue;

    // Shape velocity by role and register
    uint8_t outVel = ShapeVelocity(mir.velocityBase, role, pitch);

    // Render MIDI — use STANDARD_PPQN consistently for tick resolution
    stream.events.push_back({eventTime, MIDIEventType::NoteOn, 0, pitch, outVel});
    
    uint32_t durTicks = (mir.lengthBeats > 0)
        ? static_cast<uint32_t>(mir.lengthBeats * STANDARD_PPQN)
        : static_cast<uint32_t>(STANDARD_PPQN);
    stream.events.push_back({eventTime + MusicalTime(durTicks), MIDIEventType::NoteOff, 0, pitch, 0});
  }

  stream.SortByTime();
  return stream;
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
