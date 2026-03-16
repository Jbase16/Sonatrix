#pragma once

#include "PlaybackInstrument.h"

#include <memory>
#include <string>

namespace Sonatrix {
namespace Core {
namespace Audio {

class VoiceManager;

std::unique_ptr<VoiceManager>
CreateLoadedVoiceManager(PlaybackInstrument instrument,
                         const std::string &assetsAbsolutePath);

} // namespace Audio
} // namespace Core
} // namespace Sonatrix
