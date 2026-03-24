#pragma once

#include "src/core/midi/IMIRCompiler.h"
#include "PianoVoicingPlanner.h"

#include <memory>

namespace Sonatrix {
namespace Core {
namespace MIDI {

class PianoCompiler : public IMIRCompiler {
public:
  explicit PianoCompiler(PianoStyle style = PianoStyle::PopBlock,
                         SopranoContour contour = SopranoContour::Hold);
  ~PianoCompiler() override = default;

  MIDIStream CompileClip(
      const EditorClip &clip,
      const std::vector<ChordTrackEvent> &chordTimeline,
      Sonatrix::Core::ML::DynamicGrooveVector *grooveVectorContext = nullptr) const override;

private:
  PianoStyle m_style;
  SopranoContour m_contour;
};

std::unique_ptr<IMIRCompiler> CreatePianoEngine();

} // namespace MIDI
} // namespace Core
} // namespace Sonatrix
