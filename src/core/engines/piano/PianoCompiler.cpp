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

    // Sparse LH compensation: when LH has no second voice,
    // the RH dominates perceptually. Pull it back.
    if (sparseLH && role >= PianoTargetRole::RH_GuideLow) {
        baseVel -= 8;
    }

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
// Evaluate candiate anchors for a right-hand cluster and return root key that minimizes shift penalty
static uint8_t PickClusterAnchor(const std::vector<uint8_t>& rhPitches) {
    if (rhPitches.empty()) return 0;

    const uint8_t candidates[] = {
        36, 39, 42, 45, 
        48, 51, 54, 57, 
        60, 63, 66, 69, 
        72, 75, 78, 81
    };
    uint8_t bestAnchor = 60;
    float bestScore = 99999.0f;

    for (uint8_t anchor : candidates) {
        float score = 0.0f;
        for (uint8_t pitch : rhPitches) {
            int shift = static_cast<int>(pitch) - static_cast<int>(anchor);
            score += std::abs(shift);
            if (std::abs(shift) > 5) score += 20.0f; // Heavy penalty for stretching > 5 semitones
            if (shift < -2) score += 5.0f; // Penalty for playing lower than the sample allows natively
        }
        if (score < bestScore) {
            bestScore = score;
            bestAnchor = anchor;
        }
    }
    return bestAnchor;
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
  std::vector<uint8_t> currentRHNotes;
  uint8_t currentRHAnchor = 0;

  for (const auto &mir : clip.basePattern->events) {
    MusicalTime eventTime = clip.timelineStart + mir.offsetMap;

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
    if (eventTime != lastEventTime) {
      pitchesAtCurrentBeat.clear();
      currentRHNotes.clear();

      if (currentChordIndex >= 0 && currentChordIndex < static_cast<int>(solvedTimeline.size())) {
        const PianoVoicing& v = solvedTimeline[currentChordIndex];
        if (v.IsValid()) {
          // Pre-fetch all RH notes occurring at this exact beat to find optimal cluster anchor
          for (const auto& ev : clip.basePattern->events) {
            if (clip.timelineStart + ev.offsetMap == eventTime) {
              PianoTargetRole r = static_cast<PianoTargetRole>(ev.actionParameter);
              if (r >= PianoTargetRole::RH_GuideLow) {
                uint8_t p = ResolvePitch(v, r);
                if (p != 0) currentRHNotes.push_back(p);
              }
            }
          }
        }
      }
      currentRHAnchor = PickClusterAnchor(currentRHNotes);
      lastEventTime = eventTime;
    }

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

    // Render MIDI — pass role in channel for debug trace, and anchorOverride for audio engine
    uint8_t outChannel = static_cast<uint8_t>(role); // Hijack the unused channel field to pass role downwards
    uint8_t anchorOverride = (role >= PianoTargetRole::RH_GuideLow) ? currentRHAnchor : 0;
    stream.events.push_back({eventTime, MIDIEventType::NoteOn, outChannel, pitch, outVel, anchorOverride});
    
    uint32_t durTicks = (mir.lengthBeats > 0)
        ? static_cast<uint32_t>(mir.lengthBeats * STANDARD_PPQN)
        : static_cast<uint32_t>(STANDARD_PPQN);
    stream.events.push_back({eventTime + MusicalTime(durTicks), MIDIEventType::NoteOff, outChannel, pitch, 0, 0});
  }

  stream.SortByTime();
  return stream;
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
