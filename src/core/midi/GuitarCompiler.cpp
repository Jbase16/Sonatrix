#include "GuitarCompiler.h"
#include "../engines/guitar/VoicingGraphSolver.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <limits>

namespace Sonatrix {
namespace Core {
namespace MIDI {

namespace {

constexpr int kLowestStringFlag = 64;
constexpr int64_t kStrumBoundaryGraceTicks = 120;

bool ContainsInt(const std::vector<int> &values, int value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}

bool GuitarCompilerDebugEnabled() {
  static const bool enabled = []() {
    const char *env = std::getenv("SONATRIX_GUITAR_DEBUG");
    return env != nullptr && env[0] != '\0' && env[0] != '0';
  }();
  return enabled;
}

} // namespace

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
    optimalVoicings = solver.SolveVoiceLeading(
        chordTimeline, clip.basePattern->guitarVoicingMode);
    
    if (GuitarCompilerDebugEnabled()) {
      std::cout << "--- CHOSEN VOICINGS ---" << std::endl;
      for (size_t i = 0; i < optimalVoicings.size(); ++i) {
         std::cout << "Chord " << i << ": ";
         for(int s = 0; s < 6; ++s) {
            int f = optimalVoicings[i].frets[s];
            if (f == -1) std::cout << "X ";
            else std::cout << f << " ";
         }
         std::cout << "| avg=" << optimalVoicings[i].GetAverageFret()
                   << " span=" << optimalVoicings[i].GetFretSpan()
                   << " open=" << optimalVoicings[i].GetNumOpenStrings()
                   << " sounding=" << optimalVoicings[i].GetNumSoundingStrings()
                   << std::endl;
      }
    }
  }

  Sonatrix::Core::Engines::Guitar::GuitarVoicing fallbackVoicing;
  int previousChordIndex = -2;
  std::vector<int> usedFigurePitches;
  std::vector<int> usedFigureStrings;

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
    const ChordTrackEvent *activeChordEvent = nullptr;
    MusicalTime nextChordTime(
        std::numeric_limits<int64_t>::max() / 4);
    if (currentChordIndex >= 0 &&
        currentChordIndex < static_cast<int>(optimalVoicings.size())) {
      activeVoicing = optimalVoicings[currentChordIndex];
      activeChordEvent = &chordTimeline[static_cast<size_t>(currentChordIndex)];
      const int nextChordIndex = currentChordIndex + 1;
      if (nextChordIndex < static_cast<int>(chordTimeline.size())) {
        nextChordTime =
            chordTimeline[static_cast<size_t>(nextChordIndex)].position;
      }
    }

    if (currentChordIndex != previousChordIndex) {
      usedFigurePitches.clear();
      usedFigureStrings.clear();
      previousChordIndex = currentChordIndex;
    }

    const bool isPickingEvent =
        mir.type == ArticulationType::GuitarPluck ||
        mir.type == ArticulationType::GuitarPinch;
    if (mir.type == ArticulationType::GuitarPinch) {
      // A pinch starts a new picked figure.
      usedFigurePitches.clear();
      usedFigureStrings.clear();
    } else if (!isPickingEvent) {
      usedFigurePitches.clear();
      usedFigureStrings.clear();
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
      auto resolvedTargets =
          ResolveTargetsForEvent(mir, activeVoicing, usedFigurePitches,
                                 usedFigureStrings);
      DebugPrintResolvedEvent(eventAbsoluteTime, currentChordIndex, mir,
                              activeVoicing, resolvedTargets);
      EmitStrum(stream, lockedTime, mir.type, mir.velocityBase,
                BeatsToTime(mir.lengthBeats), resolvedTargets,
                activeChordEvent, nextChordTime);

      if (isPickingEvent) {
        for (const auto &target : resolvedTargets) {
          if (!ContainsInt(usedFigurePitches, target.pitch)) {
            usedFigurePitches.push_back(target.pitch);
          }
          if (!ContainsInt(usedFigureStrings, target.stringIndex)) {
            usedFigureStrings.push_back(target.stringIndex);
          }
        }
      }
    }
  }

  stream.SortByTime();
  return stream;
}

void GuitarCompiler::EmitStrum(
    MIDIStream &outStream, MusicalTime baseTime, ArticulationType direction,
    uint8_t baseVelocity, MusicalTime duration,
    const std::vector<NoteTarget> &resolvedTargets,
    const ChordTrackEvent *activeChordEvent,
    MusicalTime nextChordTime) const {

  // 40 ticks at 960 PPQ = ~20ms per string. This creates a beautiful, natural glide.
  int64_t dispersionTicks = 40;
  std::vector<NoteTarget> stringTargets =
      SortTargetsForChordEvent(resolvedTargets, activeChordEvent);

  // To play an upstroke, simply reverse the physical string order.
  // The time offset will remain positive, so the high strings are plucked FIRST.
  if (direction == ArticulationType::GuitarUpstroke) {
    std::reverse(stringTargets.begin(), stringTargets.end());
  }

  const MusicalTime minStrumDuration = BeatsToTime(0.20);
  const MusicalTime minPickDuration = BeatsToTime(0.35);
  const bool isStrum =
      direction == ArticulationType::GuitarDownstroke ||
      direction == ArticulationType::GuitarUpstroke;
  const MusicalTime minDuration = isStrum ? minStrumDuration : minPickDuration;

  int64_t accumulatedOffset = 0;
  for (const auto &target : stringTargets) {
    MusicalTime triggerTime = baseTime + MusicalTime(accumulatedOffset);
    uint8_t strChannel = static_cast<uint8_t>(target.stringIndex + 1);
    const uint8_t noteVelocity =
        ResolveTargetVelocity(baseVelocity, target, activeChordEvent);

    // Note On
    outStream.events.push_back({triggerTime, MIDIEventType::NoteOn, strChannel,
                                static_cast<uint8_t>(target.pitch),
                                noteVelocity});

    MusicalTime desiredDuration(
        std::max(duration.ticks, minDuration.ticks));
    MusicalTime unclampedEnd = triggerTime + desiredDuration;
    MusicalTime endTime = unclampedEnd;

    if (nextChordTime.ticks > triggerTime.ticks) {
      const int64_t clampTick =
          isStrum ? (nextChordTime.ticks + kStrumBoundaryGraceTicks)
                  : nextChordTime.ticks;
      endTime = MusicalTime(std::min(unclampedEnd.ticks, clampTick));
    }

    if (endTime.ticks <= triggerTime.ticks) {
      endTime = triggerTime + MusicalTime(1);
    }

    outStream.events.push_back({endTime, MIDIEventType::NoteOff, strChannel,
                                static_cast<uint8_t>(target.pitch), 0});

    accumulatedOffset += dispersionTicks;
  }
}

std::vector<GuitarCompiler::SoundingString>
GuitarCompiler::GetSoundingStringsByLane(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing) const {
  std::vector<SoundingString> soundingStrings;
  soundingStrings.reserve(6);

  for (int stringIndex = 0; stringIndex < 6; ++stringIndex) {
    const int pitch = voicing.GetMidiPitch(stringIndex);
    if (pitch != -1) {
      soundingStrings.push_back({stringIndex, pitch});
    }
  }

  return soundingStrings;
}

int GuitarCompiler::GetLaneString(
    const std::vector<SoundingString> &soundingStrings, int lane) const {
  if (soundingStrings.empty()) {
    return -1;
  }

  const int clampedLane = std::clamp(
      lane, 0, static_cast<int>(soundingStrings.size()) - 1);
  return soundingStrings[static_cast<size_t>(clampedLane)].stringIndex;
}

int GuitarCompiler::ResolveDefaultLaneString(
    GuitarTargetRole role,
    const std::vector<SoundingString> &soundingStrings) const {
  if (soundingStrings.empty()) {
    return -1;
  }

  auto findSoundingFromPreference = [&](std::initializer_list<int> preference) {
    for (int preferredString : preference) {
      const auto it = std::find_if(
          soundingStrings.begin(), soundingStrings.end(),
          [&](const SoundingString &s) { return s.stringIndex == preferredString; });
      if (it != soundingStrings.end()) {
        return it->stringIndex;
      }
    }
    return -1;
  };

  const int laneCount = static_cast<int>(soundingStrings.size());
  switch (role) {
  case GuitarTargetRole::Bass:
    return GetLaneString(soundingStrings, 0);
  case GuitarTargetRole::AltBass:
    return GetLaneString(soundingStrings, 1);
  case GuitarTargetRole::InnerLow:
    if (const int preferred = findSoundingFromPreference({2, 1, 3, 0, 4, 5});
        preferred != -1) {
      return preferred;
    }
    return GetLaneString(soundingStrings, std::min(1, laneCount - 1));
  case GuitarTargetRole::InnerHigh:
    if (const int preferred = findSoundingFromPreference({3, 4, 5, 2, 1, 0});
        preferred != -1) {
      return preferred;
    }
    return GetLaneString(soundingStrings, std::max(0, laneCount - 3));
  case GuitarTargetRole::Treble:
    if (const int preferred = findSoundingFromPreference({4, 5, 3, 2, 1, 0});
        preferred != -1) {
      return preferred;
    }
    return GetLaneString(soundingStrings, std::max(0, laneCount - 2));
  case GuitarTargetRole::Top:
    return GetLaneString(soundingStrings, laneCount - 1);
  case GuitarTargetRole::None:
  default:
    return -1;
  }
}

std::vector<int> GuitarCompiler::GetRoleCandidateStrings(
    GuitarTargetRole role,
    const std::vector<SoundingString> &soundingStrings) const {
  std::vector<int> orderedCandidates;
  if (soundingStrings.empty()) {
    return orderedCandidates;
  }

  const int defaultString = ResolveDefaultLaneString(role, soundingStrings);
  if (defaultString != -1) {
    orderedCandidates.push_back(defaultString);
  }

  auto appendIfSoundingAndUnique = [&](int stringIndex) {
    const bool isSounding = std::any_of(
        soundingStrings.begin(), soundingStrings.end(),
        [&](const SoundingString &s) { return s.stringIndex == stringIndex; });
    if (!isSounding) {
      return;
    }
    if (std::find(orderedCandidates.begin(), orderedCandidates.end(),
                  stringIndex) == orderedCandidates.end()) {
      orderedCandidates.push_back(stringIndex);
    }
  };

  std::vector<int> preferenceOrder;
  switch (role) {
  case GuitarTargetRole::Bass:
    preferenceOrder = {0, 1, 2, 3, 4, 5};
    break;
  case GuitarTargetRole::AltBass:
    preferenceOrder = {1, 0, 2, 3, 4, 5};
    break;
  case GuitarTargetRole::InnerLow:
    preferenceOrder = {2, 1, 3, 0, 4, 5};
    break;
  case GuitarTargetRole::InnerHigh:
    // Keep this anchored in the G/B/e region for stable acoustic picking lanes.
    preferenceOrder = {3, 4, 5, 2, 1, 0};
    break;
  case GuitarTargetRole::Treble:
    // Prefer stable upper lanes B/e/G before falling downward.
    preferenceOrder = {4, 5, 3, 2, 1, 0};
    break;
  case GuitarTargetRole::Top:
    preferenceOrder = {5, 4, 3, 2, 1, 0};
    break;
  case GuitarTargetRole::None:
  default:
    break;
  }

  for (int stringIndex : preferenceOrder) {
    appendIfSoundingAndUnique(stringIndex);
  }

  return orderedCandidates;
}

int GuitarCompiler::ResolveBassString(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches,
    const std::vector<int> &usedFigureStrings) const {
  const auto soundingStrings = GetSoundingStringsByLane(voicing);
  const auto candidates =
      GetRoleCandidateStrings(GuitarTargetRole::Bass, soundingStrings);
  if (candidates.empty()) {
    return -1;
  }

  return candidates.front();
}

int GuitarCompiler::ResolveAltBassString(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches,
    const std::vector<int> &usedFigureStrings) const {
  return ResolveFlexibleRoleString(GuitarTargetRole::AltBass, voicing,
                                   usedFigurePitches, usedFigureStrings);
}

int GuitarCompiler::ResolveTopString(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches,
    const std::vector<int> &usedFigureStrings) const {
  const auto soundingStrings = GetSoundingStringsByLane(voicing);
  const auto candidates =
      GetRoleCandidateStrings(GuitarTargetRole::Top, soundingStrings);
  if (candidates.empty()) {
    return -1;
  }

  return candidates.front();
}

int GuitarCompiler::ResolveFlexibleRoleString(
    GuitarTargetRole role,
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches,
    const std::vector<int> &usedFigureStrings) const {
  const auto soundingStrings = GetSoundingStringsByLane(voicing);
  const auto candidates = GetRoleCandidateStrings(role, soundingStrings);
  if (candidates.empty()) {
    return -1;
  }

  const int defaultString = candidates.front();
  const int defaultPitch = voicing.GetMidiPitch(defaultString);
  if (!ContainsInt(usedFigureStrings, defaultString) &&
      !ContainsInt(usedFigurePitches, defaultPitch)) {
    return defaultString;
  }

  for (int candidateString : candidates) {
    const int pitch = voicing.GetMidiPitch(candidateString);
    if (!ContainsInt(usedFigureStrings, candidateString) &&
        !ContainsInt(usedFigurePitches, pitch)) {
      return candidateString;
    }
  }

  for (int candidateString : candidates) {
    if (!ContainsInt(usedFigureStrings, candidateString)) {
      return candidateString;
    }
  }

  for (int candidateString : candidates) {
    const int pitch = voicing.GetMidiPitch(candidateString);
    if (!ContainsInt(usedFigurePitches, pitch)) {
      return candidateString;
    }
  }

  return defaultString;
}

int GuitarCompiler::ResolveTrebleString(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches,
    const std::vector<int> &usedFigureStrings) const {
  return ResolveFlexibleRoleString(GuitarTargetRole::Treble, voicing,
                                   usedFigurePitches, usedFigureStrings);
}

int GuitarCompiler::ResolveInnerLowString(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches,
    const std::vector<int> &usedFigureStrings) const {
  return ResolveFlexibleRoleString(GuitarTargetRole::InnerLow, voicing,
                                   usedFigurePitches, usedFigureStrings);
}

int GuitarCompiler::ResolveInnerHighString(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches,
    const std::vector<int> &usedFigureStrings) const {
  return ResolveFlexibleRoleString(GuitarTargetRole::InnerHigh, voicing,
                                   usedFigurePitches, usedFigureStrings);
}

int GuitarCompiler::ResolveRoleString(
    GuitarTargetRole role,
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches,
    const std::vector<int> &usedFigureStrings) const {
  switch (role) {
  case GuitarTargetRole::Bass:
    return ResolveBassString(voicing, usedFigurePitches, usedFigureStrings);
  case GuitarTargetRole::AltBass:
    return ResolveAltBassString(voicing, usedFigurePitches, usedFigureStrings);
  case GuitarTargetRole::InnerLow:
    return ResolveInnerLowString(voicing, usedFigurePitches, usedFigureStrings);
  case GuitarTargetRole::InnerHigh:
    return ResolveInnerHighString(voicing, usedFigurePitches,
                                  usedFigureStrings);
  case GuitarTargetRole::Treble:
    return ResolveTrebleString(voicing, usedFigurePitches, usedFigureStrings);
  case GuitarTargetRole::Top:
    return ResolveTopString(voicing, usedFigurePitches, usedFigureStrings);
  case GuitarTargetRole::None:
  default:
    return -1;
  }
}

const char *GuitarCompiler::GetRoleName(GuitarTargetRole role) const {
  switch (role) {
  case GuitarTargetRole::Bass:
    return "Bass";
  case GuitarTargetRole::AltBass:
    return "AltBass";
  case GuitarTargetRole::InnerLow:
    return "InnerLow";
  case GuitarTargetRole::InnerHigh:
    return "InnerHigh";
  case GuitarTargetRole::Treble:
    return "Treble";
  case GuitarTargetRole::Top:
    return "Top";
  case GuitarTargetRole::None:
  default:
    return "None";
  }
}

std::vector<GuitarCompiler::NoteTarget>
GuitarCompiler::ResolveTargetsForEvent(
    const MIREvent &event,
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches,
    const std::vector<int> &usedFigureStrings) const {
  std::vector<NoteTarget> stringTargets;

  if (event.UsesGuitarTargetRoles()) {
    std::vector<int> localUsedPitches = usedFigurePitches;
    std::vector<int> localUsedStrings = usedFigureStrings;

    auto appendRoleTarget = [&](GuitarTargetRole role) {
      if (role == GuitarTargetRole::None) {
        return;
      }

      const int stringIndex = ResolveRoleString(role, voicing, localUsedPitches,
                                                localUsedStrings);
      if (stringIndex == -1) {
        return;
      }

      const bool alreadyAdded = std::any_of(
          stringTargets.begin(), stringTargets.end(),
          [&](const NoteTarget &existing) {
            return existing.stringIndex == stringIndex;
          });
      if (alreadyAdded) {
        return;
      }

      const int pitch = voicing.GetMidiPitch(stringIndex);
      if (pitch == -1) {
        return;
      }

      stringTargets.push_back({pitch, stringIndex, role});
      if (!ContainsInt(localUsedPitches, pitch)) {
        localUsedPitches.push_back(pitch);
      }
      if (!ContainsInt(localUsedStrings, stringIndex)) {
        localUsedStrings.push_back(stringIndex);
      }
    };

    appendRoleTarget(event.guitarTargetRole);
    appendRoleTarget(event.guitarSecondaryTargetRole);

    if (!stringTargets.empty()) {
      return stringTargets;
    }
  }

  if (event.type == ArticulationType::GuitarPluck) {
    if (event.actionParameter == kLowestStringFlag) {
      const int lowest = voicing.GetLowestSoundingString();
      if (lowest != -1) {
        stringTargets.push_back({voicing.GetMidiPitch(lowest), lowest,
                                 GuitarTargetRole::None});
      }
    } else if (event.actionParameter >= 0 && event.actionParameter < 6) {
      const int pitch = voicing.GetMidiPitch(event.actionParameter);
      if (pitch != -1) {
        stringTargets.push_back(
            {pitch, event.actionParameter, GuitarTargetRole::None});
      }
    }
  } else if (event.type == ArticulationType::GuitarPinch) {
    int bitmask = event.actionParameter;
    if ((bitmask & kLowestStringFlag) != 0) {
      const int lowest = voicing.GetLowestSoundingString();
      if (lowest != -1) {
        bitmask |= (1 << lowest);
      }
    }

    for (int stringIndex = 0; stringIndex < 6; ++stringIndex) {
      if ((bitmask & (1 << stringIndex)) != 0) {
        const int pitch = voicing.GetMidiPitch(stringIndex);
        if (pitch != -1) {
          stringTargets.push_back({pitch, stringIndex, GuitarTargetRole::None});
        }
      }
    }
  } else {
    for (int stringIndex = 0; stringIndex < 6; ++stringIndex) {
      const int pitch = voicing.GetMidiPitch(stringIndex);
      if (pitch != -1) {
        stringTargets.push_back({pitch, stringIndex, GuitarTargetRole::None});
      }
    }
  }

  return stringTargets;
}

std::vector<GuitarCompiler::NoteTarget> GuitarCompiler::SortTargetsForChordEvent(
    std::vector<NoteTarget> targets, const ChordTrackEvent *activeChordEvent) const {
  if (targets.size() <= 1 || activeChordEvent == nullptr ||
      !activeChordEvent->guitarEditData.hasCustomVoicing) {
    return targets;
  }

  const auto &order = activeChordEvent->guitarEditData.noteOrder;
  std::stable_sort(
      targets.begin(), targets.end(),
      [&](const NoteTarget &lhs, const NoteTarget &rhs) {
        return order[static_cast<size_t>(lhs.stringIndex)] <
               order[static_cast<size_t>(rhs.stringIndex)];
      });
  return targets;
}

uint8_t GuitarCompiler::ResolveTargetVelocity(
    uint8_t baseVelocity, const NoteTarget &target,
    const ChordTrackEvent *activeChordEvent) const {
  if (activeChordEvent == nullptr || !activeChordEvent->guitarEditData.hasCustomVoicing ||
      target.stringIndex < 0 || target.stringIndex >= 6) {
    return baseVelocity;
  }

  const uint8_t storedVelocity =
      activeChordEvent->guitarEditData.noteVelocity[static_cast<size_t>(target.stringIndex)];
  const int scaledVelocity =
      static_cast<int>(baseVelocity) * static_cast<int>(storedVelocity) / 100;
  return static_cast<uint8_t>(std::clamp(scaledVelocity, 1, 127));
}

void GuitarCompiler::DebugPrintResolvedEvent(
    MusicalTime absoluteTime, int chordIndex, const MIREvent &event,
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<NoteTarget> &resolvedTargets) const {
  if (!event.UsesGuitarTargetRoles()) {
    return;
  }

  if (!GuitarCompilerDebugEnabled()) {
    return;
  }

  std::cout << "[GuitarCompiler] tick=" << absoluteTime.ticks
            << " chord=" << chordIndex << " voicing=[";
  for (int stringIndex = 0; stringIndex < 6; ++stringIndex) {
    if (stringIndex > 0) {
      std::cout << ' ';
    }
    const int fret = voicing.frets[stringIndex];
    if (fret == -1) {
      std::cout << 'X';
    } else {
      std::cout << fret;
    }
  }
  std::cout << "] type=";

  switch (event.type) {
  case ArticulationType::GuitarPinch:
    std::cout << "GuitarPinch";
    break;
  case ArticulationType::GuitarPluck:
    std::cout << "GuitarPluck";
    break;
  default:
    std::cout << "Other";
    break;
  }

  std::cout << " targets=";
  if (resolvedTargets.empty()) {
    std::cout << "none";
  } else {
    for (size_t i = 0; i < resolvedTargets.size(); ++i) {
      if (i > 0) {
        std::cout << ", ";
      }
      std::cout << GetRoleName(resolvedTargets[i].role)
                << ":string=" << resolvedTargets[i].stringIndex
                << " midi=" << resolvedTargets[i].pitch;
    }
  }
  std::cout << std::endl;
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
