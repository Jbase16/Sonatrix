#include "PianoCompiler.h"

namespace Sonatrix {
namespace Core {
namespace Engines {

std::unique_ptr<MIDI::IMIRCompiler> CreatePianoEngine() {
  return std::make_unique<PianoCompiler>();
}

MIDI::MIDIStream PianoCompiler::CompileClip(
    const EditorClip &clip, const std::vector<ChordTrackEvent> &chordTimeline,
    Sonatrix::Core::ML::DynamicGrooveVector *grooveVectorContext) const {
  MIDI::MIDIStream stream;

  // In a real implementation we maintain state across the clip:
  std::vector<uint8_t> currentVoicing;
  ActiveChordContext mockContext; // Mocked active context

  // 1. Voice Leading Pass
  for (const auto &mir : clip.basePattern->events) {
    if (mir.type != ArticulationType::GenericNote &&
        mir.type != ArticulationType::PianoChord) {
      continue;
    }

    MusicalTime triggerTime = clip.timelineStart + mir.offsetMap;

    // Mocking the smooth voice algorithm execution
    currentVoicing = CalculateSmoothVoicing(currentVoicing, mockContext);

    // Render Notes
    for (uint8_t currentNote : currentVoicing) {
      stream.events.push_back({triggerTime, MIDI::MIDIEventType::NoteOn, 0,
                               currentNote, mir.velocityBase});

      MusicalTime offTime = triggerTime + BeatsToTime(mir.lengthBeats);
      stream.events.push_back(
          {offTime, MIDI::MIDIEventType::NoteOff, 0, currentNote, 0});
    }
  }

  // 2. Automated Pedal Pass (CC64)
  SynthesizePedal(stream, clip.timelineStart,
                  clip.timelineStart + BeatsToTime(4.0), chordTimeline);

  stream.SortByTime();
  return stream;
}

std::vector<uint8_t> PianoCompiler::CalculateSmoothVoicing(
    const std::vector<uint8_t> & /*previousVoicing*/,
    const ActiveChordContext & /*targetChord*/
) const {
  // PhD Component: Voice leading combinatorial optimizer constraint logic.
  // E.g., given a previous voicing, we penalize aggregate intervallic leaps.

  // Mocked output: Hand-voiced C Major triad over C2 bass
  return {36, 60, 64, 67};
}

void PianoCompiler::SynthesizePedal(
    MIDI::MIDIStream &outStream, MusicalTime startTime, MusicalTime /*endTime*/,
    const std::vector<ChordTrackEvent> & /*chordTimeline*/
) const {
  // CC64: 127 = Down, 0 = Up
  // A real implementation scans the duration and looks for chord transitions.
  // If Harmony changes but Bass is identical: "Half-pedal" (CC 64 -> 60 -> 127)
  // If Harmony changes and Bass changes: Full pedal clear (CC 64 -> 0 -> 127)

  // Mock simple sustain for the clip duration
  outStream.events.push_back(
      {startTime, MIDI::MIDIEventType::ControlChange, 0, 64, 127});
}

} // namespace Engines
} // namespace Core
} // namespace Sonatrix
