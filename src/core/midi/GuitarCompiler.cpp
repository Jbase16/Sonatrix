#include "GuitarCompiler.h"
#include "../engines/guitar/VoicingGraphSolver.h"

#include <algorithm>
#include <iostream>
#include <iterator>

namespace Sonatrix {
namespace Core {
namespace MIDI {

namespace {

constexpr int kLowestStringFlag = 64;

bool ContainsPitch(const std::vector<int> &usedFigurePitches, int pitch) {
  return std::find(usedFigurePitches.begin(), usedFigurePitches.end(), pitch) !=
         usedFigurePitches.end();
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
    optimalVoicings = solver.SolveVoiceLeading(chordTimeline);
    
    // Debug output to verify what frets the solver actually chose
    std::cout << "--- CHOSEN VOICINGS ---" << std::endl;
    for (size_t i = 0; i < optimalVoicings.size(); ++i) {
       std::cout << "Chord " << i << ": ";
       for(int s = 0; s < 6; ++s) {
          int f = optimalVoicings[i].frets[s];
          if (f == -1) std::cout << "X ";
          else std::cout << f << " ";
       }
       std::cout << std::endl;
    }
  }

  Sonatrix::Core::Engines::Guitar::GuitarVoicing fallbackVoicing;
  int previousChordIndex = -2;
  std::vector<int> usedFigurePitches;

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

    if (currentChordIndex != previousChordIndex) {
      usedFigurePitches.clear();
      previousChordIndex = currentChordIndex;
    }

    const bool isPickingEvent =
        mir.type == ArticulationType::GuitarPluck ||
        mir.type == ArticulationType::GuitarPinch;
    if (mir.type == ArticulationType::GuitarPinch) {
      usedFigurePitches.clear();
    } else if (!isPickingEvent) {
      usedFigurePitches.clear();
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
          ResolveTargetsForEvent(mir, activeVoicing, usedFigurePitches);
      DebugPrintResolvedEvent(eventAbsoluteTime, currentChordIndex, mir,
                              activeVoicing, resolvedTargets);
      EmitStrum(stream, lockedTime, mir.type, mir.velocityBase,
                BeatsToTime(mir.lengthBeats), resolvedTargets);

      if (isPickingEvent) {
        for (const auto &target : resolvedTargets) {
          if (!ContainsPitch(usedFigurePitches, target.pitch)) {
            usedFigurePitches.push_back(target.pitch);
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
    const std::vector<NoteTarget> &resolvedTargets) const {

  // 40 ticks at 960 PPQ = ~20ms per string. This creates a beautiful, natural glide.
  int64_t dispersionTicks = 40;
  std::vector<NoteTarget> stringTargets = resolvedTargets;

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

    // Make the note ring out for the duration specified by the MIR Event, OR at least 
    // 2.0 beats to ensure open acoustic chords don't cut off prematurely before the choke.
    // FIXED: Using explicit constructor for MusicalTime to handle int64_t return from std::max.
    MusicalTime actualDuration(std::max(duration.ticks, BeatsToTime(2.0).ticks));
    MusicalTime endTime = triggerTime + actualDuration;

    outStream.events.push_back({endTime, MIDIEventType::NoteOff, strChannel,
                                static_cast<uint8_t>(target.pitch), 0});

    accumulatedOffset += dispersionTicks;
  }
}

std::vector<GuitarCompiler::SoundingString>
GuitarCompiler::GetSoundingStringsByPitch(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing) const {
  std::vector<SoundingString> soundingStrings;
  soundingStrings.reserve(6);

  for (int stringIndex = 0; stringIndex < 6; ++stringIndex) {
    const int pitch = voicing.GetMidiPitch(stringIndex);
    if (pitch != -1) {
      soundingStrings.push_back({stringIndex, pitch});
    }
  }

  std::sort(soundingStrings.begin(), soundingStrings.end(),
            [](const SoundingString &a, const SoundingString &b) {
              if (a.pitch != b.pitch) {
                return a.pitch < b.pitch;
              }
              return a.stringIndex < b.stringIndex;
            });

  return soundingStrings;
}

int GuitarCompiler::ResolveBassString(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing) const {
  const auto soundingStrings = GetSoundingStringsByPitch(voicing);
  return soundingStrings.empty() ? -1 : soundingStrings.front().stringIndex;
}

int GuitarCompiler::ResolveAltBassString(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing) const {
  const auto soundingStrings = GetSoundingStringsByPitch(voicing);
  if (soundingStrings.size() >= 2) {
    return soundingStrings[1].stringIndex;
  }
  return soundingStrings.empty() ? -1 : soundingStrings.front().stringIndex;
}

int GuitarCompiler::ResolveTopString(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches) const {
  auto soundingStrings = GetSoundingStringsByPitch(voicing);
  std::reverse(soundingStrings.begin(), soundingStrings.end());

  for (const auto &candidate : soundingStrings) {
    if (!ContainsPitch(usedFigurePitches, candidate.pitch)) {
      return candidate.stringIndex;
    }
  }

  return soundingStrings.empty() ? -1 : soundingStrings.front().stringIndex;
}

int GuitarCompiler::ResolveTrebleString(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches) const {
  auto soundingStrings = GetSoundingStringsByPitch(voicing);
  if (soundingStrings.empty()) {
    return -1;
  }

  std::reverse(soundingStrings.begin(), soundingStrings.end());

  std::vector<SoundingString> candidates;
  if (soundingStrings.size() > 1) {
    candidates.insert(candidates.end(), std::next(soundingStrings.begin()),
                      soundingStrings.end());
  }
  candidates.push_back(soundingStrings.front());

  for (const auto &candidate : candidates) {
    if (!ContainsPitch(usedFigurePitches, candidate.pitch)) {
      return candidate.stringIndex;
    }
  }

  return candidates.front().stringIndex;
}

int GuitarCompiler::ResolveInnerLowString(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches) const {
  const auto soundingStrings = GetSoundingStringsByPitch(voicing);
  if (soundingStrings.empty()) {
    return -1;
  }

  const int bassString = ResolveBassString(voicing);
  const int topString = ResolveTopString(voicing);

  std::vector<SoundingString> candidates;
  for (const auto &candidate : soundingStrings) {
    if (candidate.stringIndex != bassString && candidate.stringIndex != topString) {
      candidates.push_back(candidate);
    }
  }

  if (candidates.empty()) {
    for (const auto &candidate : soundingStrings) {
      if (candidate.stringIndex != bassString) {
        candidates.push_back(candidate);
      }
    }
  }

  if (candidates.empty()) {
    candidates = soundingStrings;
  }

  for (const auto &candidate : candidates) {
    if (!ContainsPitch(usedFigurePitches, candidate.pitch)) {
      return candidate.stringIndex;
    }
  }

  return candidates.front().stringIndex;
}

int GuitarCompiler::ResolveInnerHighString(
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches) const {
  auto soundingStrings = GetSoundingStringsByPitch(voicing);
  if (soundingStrings.empty()) {
    return -1;
  }

  const int bassString = ResolveBassString(voicing);
  const int topString = ResolveTopString(voicing);

  std::reverse(soundingStrings.begin(), soundingStrings.end());

  std::vector<SoundingString> candidates;
  for (const auto &candidate : soundingStrings) {
    if (candidate.stringIndex != bassString && candidate.stringIndex != topString) {
      candidates.push_back(candidate);
    }
  }

  if (candidates.empty()) {
    for (const auto &candidate : soundingStrings) {
      if (candidate.stringIndex != topString) {
        candidates.push_back(candidate);
      }
    }
  }

  if (candidates.empty()) {
    candidates = soundingStrings;
  }

  for (const auto &candidate : candidates) {
    if (!ContainsPitch(usedFigurePitches, candidate.pitch)) {
      return candidate.stringIndex;
    }
  }

  return candidates.front().stringIndex;
}

int GuitarCompiler::ResolveRoleString(
    GuitarTargetRole role,
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<int> &usedFigurePitches) const {
  switch (role) {
  case GuitarTargetRole::Bass:
    return ResolveBassString(voicing);
  case GuitarTargetRole::AltBass:
    return ResolveAltBassString(voicing);
  case GuitarTargetRole::InnerLow:
    return ResolveInnerLowString(voicing, usedFigurePitches);
  case GuitarTargetRole::InnerHigh:
    return ResolveInnerHighString(voicing, usedFigurePitches);
  case GuitarTargetRole::Treble:
    return ResolveTrebleString(voicing, usedFigurePitches);
  case GuitarTargetRole::Top:
    return ResolveTopString(voicing, usedFigurePitches);
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
    const std::vector<int> &usedFigurePitches) const {
  std::vector<NoteTarget> stringTargets;

  if (event.UsesGuitarTargetRoles()) {
    std::vector<int> localUsedPitches = usedFigurePitches;

    auto appendRoleTarget = [&](GuitarTargetRole role) {
      if (role == GuitarTargetRole::None) {
        return;
      }

      const int stringIndex = ResolveRoleString(role, voicing, localUsedPitches);
      if (stringIndex == -1) {
        return;
      }

      const int pitch = voicing.GetMidiPitch(stringIndex);
      if (pitch == -1) {
        return;
      }

      stringTargets.push_back({pitch, stringIndex, role});
      if (!ContainsPitch(localUsedPitches, pitch)) {
        localUsedPitches.push_back(pitch);
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

void GuitarCompiler::DebugPrintResolvedEvent(
    MusicalTime absoluteTime, int chordIndex, const MIREvent &event,
    const Sonatrix::Core::Engines::Guitar::GuitarVoicing &voicing,
    const std::vector<NoteTarget> &resolvedTargets) const {
  if (!event.UsesGuitarTargetRoles()) {
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
