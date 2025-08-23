#include "../include/types/scope.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "../include/types/scopeEntry.hpp"

namespace Scope {

Scope::Scope(std::string fileName) {
    std::ifstream file(fileName);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }
    std::string line;

    std::getline(file, line);
    while (std::getline(file, line)) {
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }

        if (fields.size() != 12) continue;

        for (auto& f : fields) {
            if (f.empty()) f = "";
        }

        bool eligible_for_bounty = (fields[3] == "true" || fields[3] == "1");
        bool eligible_for_submission = (fields[4] == "true" || fields[4] == "1");

        ScopeEntry::ScopeEntry entry(fields[0],
                                     fields[1],
                                     fields[2],
                                     eligible_for_bounty,
                                     eligible_for_submission,
                                     fields[5],
                                     fields[6],
                                     fields[7],
                                     fields[8],
                                     fields[9],
                                     fields[10],
                                     fields[11]);
        entries.push_back(entry);
    }
}

Scope::~Scope() {
    entries.clear();
}

std::ostream& operator<<(std::ostream& os, const Scope& scope) {
    os << "Scope entries: [\n";
    for (const auto& entry : scope.entries) {
        os << "  { " << entry << " }\n";
    }
    os << "]";
    return os;
}

}  // namespace Scope
