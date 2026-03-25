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
static uint8_t ShapeVelocity(uint8_t patternVel, PianoTargetRole role, uint8_t pitch, bool sparseLH) {
    int baseVel;
    switch (role) {
      case PianoTargetRole::LH_Root:      baseVel = 84; break;
      case PianoTargetRole::LH_Fifth:
      case PianoTargetRole::LH_ShellLow:  baseVel = 74; break;
      case PianoTargetRole::LH_Octave:    baseVel = 70; break;
      case PianoTargetRole::RH_GuideLow:  baseVel = 68; break;
      case PianoTargetRole::RH_Inner:     baseVel = 62; break;
      case PianoTargetRole::RH_GuideHigh: baseVel = 64; break;
      case PianoTargetRole::RH_Top:       baseVel = 70; break;
      default:                            baseVel = 66; break;
    }

    if (pitch >= 72)      baseVel -= 4;
    else if (pitch >= 67) baseVel -= 2;
    else if (pitch <= 42) baseVel += 4;
    else if (pitch <= 48) baseVel += 2;

    if (sparseLH && role >= PianoTargetRole::RH_GuideLow) {
        baseVel -= 4;
    }

    int deviation = static_cast<int>(patternVel) - 80;
    int outVel = baseVel + static_cast<int>(deviation * 0.35f);

    return static_cast<uint8_t>(std::clamp(outVel, 36, 108));
}

// Try to resolve a pitch for the given role from the voicing.
// If the primary role is empty, try musical fallbacks.
static uint8_t ResolvePitch(const PianoVoicing& voicing, PianoTargetRole role) {
    uint8_t pitch = voicing.GetPitch(role);
    if (pitch != 0) return pitch;

    switch (role) {
      // LH fallbacks: Fifth ↔ ShellLow (these are the same musical intent)
      case PianoTargetRole::LH_Fifth:
        return voicing.GetPitch(PianoTargetRole::LH_ShellLow);
      case PianoTargetRole::LH_ShellLow:
        return voicing.GetPitch(PianoTargetRole::LH_Fifth);

      // RH: no scavenging. If the planner left a slot empty, respect it.
      default:
        return 0;
    }
    return 0;
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

    // Reset dedup set and compute cluster anchor when beat changes
    // (Cluster logic was removed to avoid clone-chord effect. Notes now find their nearest respective anchors).
    
    if (currentChordIndex < 0 || currentChordIndex >= static_cast<int>(solvedTimeline.size())) continue;

    const PianoVoicing& voicing = solvedTimeline[currentChordIndex];
    if (!voicing.IsValid()) continue;

    // Detect sparse LH: only root, no second support note
    bool sparseLH = (voicing.GetPitch(PianoTargetRole::LH_Fifth) == 0 &&
                     voicing.GetPitch(PianoTargetRole::LH_ShellLow) == 0);

    // Resolve pitch with fallback chain
    PianoTargetRole role = static_cast<PianoTargetRole>(mir.actionParameter);
    uint8_t pitch = ResolvePitch(voicing, role);
    if (pitch == 0) continue;

    // Dedup: skip if this pitch was already emitted at this beat
    if (!pitchesAtCurrentBeat.insert(pitch).second) continue;

    // Shape velocity by role and register
    uint8_t outVel = ShapeVelocity(mir.velocityBase, role, pitch, sparseLH);

    // Humanize timing (simulating spread pianistic attack)
    // 1 ms = 1.92 ticks at 120bpm (960 ppqn)
    int32_t msOffset = 0;
    if (role == PianoTargetRole::LH_Fifth || role == PianoTargetRole::LH_ShellLow) msOffset = 6;
    else if (role == PianoTargetRole::RH_Inner || role == PianoTargetRole::RH_GuideLow) msOffset = 12;
    else if (role == PianoTargetRole::RH_GuideHigh || role == PianoTargetRole::RH_Top) msOffset = 18;
    
    int32_t tickOffset = msOffset * 2; // rough approx
    MusicalTime humanizedTime = eventTime + MusicalTime(tickOffset);

    // Render MIDI — pass role in channel for debug trace.
    // anchorOverride is 0 (engine will dynamically select best valid velocity zone per individual note).
    uint8_t outChannel = static_cast<uint8_t>(role);
    stream.events.push_back({humanizedTime, MIDIEventType::NoteOn, outChannel, pitch, outVel, 0});
    
    uint32_t durTicks = (mir.lengthBeats > 0)
        ? static_cast<uint32_t>(mir.lengthBeats * STANDARD_PPQN)
        : static_cast<uint32_t>(STANDARD_PPQN);
    stream.events.push_back({humanizedTime + MusicalTime(durTicks), MIDIEventType::NoteOff, outChannel, pitch, 0, 0});
  }

  stream.SortByTime();
  return stream;
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
