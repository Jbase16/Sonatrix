#include "PianoCompiler.h"

namespace Sonatrix {
namespace Core {
namespace MIDI {

std::unique_ptr<IMIRCompiler> CreatePianoEngine() {
  return std::make_unique<PianoCompiler>();
}

PianoCompiler::PianoCompiler(PianoStyle style, SopranoContour contour)
    : m_style(style), m_contour(contour) {
}

MIDIStream PianoCompiler::CompileClip(
    const EditorClip &clip, const std::vector<ChordTrackEvent> &chordTimeline,
    Sonatrix::Core::ML::DynamicGrooveVector *grooveVectorContext) const {
  
  MIDIStream stream;

  // Solve voice leading with style and contour intelligence
  PianoVoicingPlanner planner(m_style, m_contour);
  std::vector<PianoVoicing> solvedTimeline = planner.SolveTimeline(chordTimeline);

  // Map MIR Events to the optimized Voicing
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

    if (currentChordIndex < 0 || currentChordIndex >= static_cast<int>(solvedTimeline.size())) continue;

    const PianoVoicing& voicing = solvedTimeline[currentChordIndex];
    if (!voicing.IsValid()) continue;

    // Map Action Parameter to Semantic Role
    PianoTargetRole role = static_cast<PianoTargetRole>(mir.actionParameter);
    uint8_t pitch = voicing.GetPitch(role);
    if (pitch == 0) continue;

    // Render MIDI
    stream.events.push_back({eventTime, MIDIEventType::NoteOn, 0, pitch, mir.velocityBase});
    
    uint32_t durTicks = (mir.lengthBeats > 0) ? static_cast<uint32_t>(mir.lengthBeats * 480) : 480;
    stream.events.push_back({eventTime + MusicalTime(durTicks), MIDIEventType::NoteOff, 0, pitch, 0});
  }

  stream.SortByTime();
  return stream;
}

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
