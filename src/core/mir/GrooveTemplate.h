#pragma once

#include "MIRPattern.h"
#include <string>
#include <map>
#include <memory>
#include <vector>

namespace Sonatrix {
namespace Core {

// Represents a cohesive musical pattern across multiple instruments
struct GrooveTemplate {
    std::string id;
    std::string name;
    std::string genre;
    std::string timeSignature{"4/4"};
    
    // The individual patterns mapped by target engine
    std::map<MIRPattern::TargetEngine, std::shared_ptr<MIRPattern>> patterns;
};

} // namespace Core
} // namespace Sonatrix
