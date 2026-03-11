#include "PatternLibrary.h"
#include "../utils/json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace Sonatrix {
namespace Core {

NLOHMANN_JSON_SERIALIZE_ENUM( ArticulationType, {
    {ArticulationType::GenericNote, "GenericNote"},
    {ArticulationType::PianoChord, "PianoChord"},
    {ArticulationType::PianoArpeggioUp, "PianoArpeggioUp"},
    {ArticulationType::PianoArpeggioDown, "PianoArpeggioDown"},
    {ArticulationType::GuitarDownstroke, "GuitarDownstroke"},
    {ArticulationType::GuitarUpstroke, "GuitarUpstroke"},
    {ArticulationType::GuitarPluck, "GuitarPluck"},
    {ArticulationType::GuitarPinch, "GuitarPinch"},
    {ArticulationType::GuitarMute, "GuitarMute"},
    {ArticulationType::GuitarPalmDrop, "GuitarPalmDrop"},
    {ArticulationType::BassSlap, "BassSlap"},
    {ArticulationType::BassPop, "BassPop"},
    {ArticulationType::StringSwell, "StringSwell"},
    {ArticulationType::StringSpiccato, "StringSpiccato"},
    {ArticulationType::DrumHit, "DrumHit"},
    {ArticulationType::DrumGhostNote, "DrumGhostNote"}
})

NLOHMANN_JSON_SERIALIZE_ENUM( GuitarTargetRole, {
    {GuitarTargetRole::None, "None"},
    {GuitarTargetRole::Bass, "Bass"},
    {GuitarTargetRole::AltBass, "AltBass"},
    {GuitarTargetRole::InnerLow, "InnerLow"},
    {GuitarTargetRole::InnerHigh, "InnerHigh"},
    {GuitarTargetRole::Treble, "Treble"},
    {GuitarTargetRole::Top, "Top"}
})

NLOHMANN_JSON_SERIALIZE_ENUM( MIRPattern::TargetEngine, {
    {MIRPattern::TargetEngine::Guitar, "Guitar"},
    {MIRPattern::TargetEngine::Piano, "Piano"},
    {MIRPattern::TargetEngine::Bass, "Bass"},
    {MIRPattern::TargetEngine::Strings, "Strings"},
    {MIRPattern::TargetEngine::Drums, "Drums"}
})


bool PatternLibrary::LoadFromJSON(const std::string& absolutePath) {
    std::ifstream file(absolutePath);
    if (!file.is_open()) {
        std::cerr << "Sonatrix: Failed to open Pattern Library JSON: " << absolutePath << "\n";
        return false;
    }

    try {
        json j;
        file >> j;
        
        if (!j.contains("templates") || !j["templates"].is_array()) {
            std::cerr << "Sonatrix: Invalid JSON schema, missing 'templates' array.\n";
            return false;
        }
        
        for (const auto& tmplJson : j["templates"]) {
            auto tmpl = std::make_shared<GrooveTemplate>();
            tmpl->id = tmplJson.value("id", "unknown_id");
            tmpl->name = tmplJson.value("name", "Unknown Pattern");
            tmpl->genre = tmplJson.value("genre", "Generic");
            tmpl->timeSignature = tmplJson.value("timeSignature", "4/4");
            
            if (tmplJson.contains("patterns") && tmplJson["patterns"].is_object()) {
                for (auto& [engineKey, patternJson] : tmplJson["patterns"].items()) {
                    auto engineVal = json(engineKey).get<MIRPattern::TargetEngine>();
                    
                    auto pattern = std::make_shared<MIRPattern>();
                    pattern->intendedEngine = engineVal;
                    
                    double lengthBeats = patternJson.value("totalLengthBeats", 4.0);
                    pattern->totalLength = BeatsToTime(lengthBeats);
                    
                    if (patternJson.contains("events") && patternJson["events"].is_array()) {
                        for (const auto& evJson : patternJson["events"]) {
                            MIREvent ev;
                            double offsetBeats = evJson.value("offsetBeats", 0.0);
                            ev.offsetMap = BeatsToTime(offsetBeats);
                            
                            ev.lengthBeats = evJson.value("lengthBeats", 0.25);
                            ev.velocityBase = evJson.value("velocityBase", 100);
                            
                            if (evJson.contains("type")) {
                                ev.type = evJson["type"].get<ArticulationType>();
                            }
                            ev.actionParameter = evJson.value("actionParameter", 0);

                            if (evJson.contains("targetRole")) {
                                ev.guitarTargetRole =
                                    evJson["targetRole"].get<GuitarTargetRole>();
                            } else if (evJson.contains("guitarTargetRole")) {
                                ev.guitarTargetRole =
                                    evJson["guitarTargetRole"].get<GuitarTargetRole>();
                            }

                            if (evJson.contains("secondaryTargetRole")) {
                                ev.guitarSecondaryTargetRole =
                                    evJson["secondaryTargetRole"].get<GuitarTargetRole>();
                            } else if (evJson.contains("guitarSecondaryTargetRole")) {
                                ev.guitarSecondaryTargetRole =
                                    evJson["guitarSecondaryTargetRole"].get<GuitarTargetRole>();
                            }
                            
                            pattern->events.push_back(ev);
                        }
                    }
                    
                    tmpl->patterns[engineVal] = pattern;
                }
            }
            
            templates_.push_back(tmpl);
            templateMap_[tmpl->id] = tmpl;
        }
        
        std::cout << "Sonatrix: Loaded " << templates_.size() << " GrooveTemplates from JSON.\n";
        return true;
        
    } catch (const json::parse_error& e) {
        std::cerr << "Sonatrix: JSON parse error: " << e.what() << "\n";
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Sonatrix: Error loading Pattern Library: " << e.what() << "\n";
        return false;
    }
}

std::shared_ptr<GrooveTemplate> PatternLibrary::GetTemplate(const std::string& id) const {
    auto it = templateMap_.find(id);
    if (it != templateMap_.end()) {
        return it->second;
    }
    return nullptr;
}


} // namespace Core
} // namespace Sonatrix
