// EN: Implementation of the Scope class. Handles loading scope entries from a CSV file and printing
// them. FR : Implémentation de la classe Scope. Gère le chargement des entrées de scope depuis un
// fichier CSV et leur affichage.

#include "../include/types/scope.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "../include/types/scopeEntry.hpp"

// EN: Utility function to parse a CSV field into int, double, or string for std::variant.
// FR : Fonction utilitaire pour parser un champ CSV en int, double ou string pour std::variant.
static std::variant<int, double, std::string> parseVariant(const std::string& value) {
    if (value.empty()) return std::string("");
    char* endptr = nullptr;
    long int_val = strtol(value.c_str(), &endptr, 10);
    if (*endptr == '\0') return static_cast<int>(int_val);
    double double_val = strtod(value.c_str(), &endptr);
    if (*endptr == '\0') return double_val;
    return value;
}

namespace Scope {

// EN: Constructor. Loads scope entries from the given CSV file.
// FR : Constructeur. Charge les entrées de scope depuis le fichier CSV donné.
Scope::Scope(std::string fileName) {
    std::ifstream file(fileName);
    if (!file.is_open()) {
        std::cerr << "[ERROR] Could not open file: " << fileName << std::endl;
        throw std::runtime_error("Could not open file");
    }
    std::string line;
    int line_num = 0;
    std::getline(file, line);  // Skip header
    while (std::getline(file, line)) {
        line_num++;
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        // EN: Split the line by commas.
        // FR : Sépare la ligne par des virgules.
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }
        if (fields.size() != 12) {
            std::cerr << "[WARNING] Line " << line_num << " malformed (" << fields.size()
                      << " fields): " << line << std::endl;
            continue;
        }
        for (auto& f : fields) {
            if (f.empty()) f = "";
        }
        bool eligible_for_bounty = (fields[3] == "true" || fields[3] == "1");
        bool eligible_for_submission = (fields[4] == "true" || fields[4] == "1");
        ScopeEntry::ScopeEntry entry(fields[0],
                                     fields[1],
                                     parseVariant(fields[2]),
                                     eligible_for_bounty,
                                     eligible_for_submission,
                                     parseVariant(fields[5]),
                                     parseVariant(fields[6]),
                                     fields[7],
                                     fields[8],
                                     parseVariant(fields[9]),
                                     fields[10],
                                     fields[11]);
        entries.push_back(entry);
    }
}

// EN: Destructor. Clears the entries vector.
// FR : Destructeur. Vide le vecteur entries.
Scope::~Scope() {
    entries.clear();
}

// EN: Output operator. Prints all scope entries.
// FR : Opérateur d'affichage. Affiche toutes les entrées du scope.
std::ostream& operator<<(std::ostream& os, const Scope& scope) {
    os << "Scope entries: [\n";
    for (const auto& entry : scope.entries) {
        os << "  { " << entry << " }\n";
    }
    os << "]";
    return os;
}

}  // namespace Scope
