#pragma once

#include "GrooveTemplate.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace Sonatrix {
namespace Core {

class PatternLibrary {
public:
    static PatternLibrary& GetInstance() {
        static PatternLibrary instance;
        return instance;
    }

    // Loads definitions from a JSON file using nlohmann/json
    bool LoadFromJSON(const std::string& absolutePath);
    
    const std::vector<std::shared_ptr<GrooveTemplate>>& GetAllTemplates() const { return templates_; }
    std::shared_ptr<GrooveTemplate> GetTemplate(const std::string& id) const;

private:
    PatternLibrary() = default;
    ~PatternLibrary() = default;

    PatternLibrary(const PatternLibrary&) = delete;
    PatternLibrary& operator=(const PatternLibrary&) = delete;

    std::vector<std::shared_ptr<GrooveTemplate>> templates_;
    std::map<std::string, std::shared_ptr<GrooveTemplate>> templateMap_;
};

} // namespace Core
} // namespace Sonatrix
