// EN: Implementation of the Scope class. Handles loading scope entries from a CSV file and printing
// them. FR : Implémentation de la classe Scope. Gère le chargement des entrées de scope depuis un fichier CSV et leur affichage.

#include "../include/types/scope.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "../include/types/scopeEntry.hpp"

namespace Scope {

// EN: Constructor. Loads scope entries from the given CSV file.
// FR : Constructeur. Charge les entrées de scope depuis le fichier CSV donné.
Scope::Scope(std::string fileName) {
    std::ifstream file(fileName);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file");
    }
    std::string line;

    // EN: Skip the header line.
    // FR : Ignore la ligne d'en-tête.
    std::getline(file, line);
    while (std::getline(file, line)) {
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        // EN: Split the line by commas.
        // FR : Sépare la ligne par des virgules.
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }

        // EN: Only process lines with 12 fields.
        // FR : Ne traite que les lignes avec 12 champs.
        if (fields.size() != 12) continue;

        // EN: Replace empty fields with empty strings.
        // FR : Remplace les champs vides par des chaînes vides.
        for (auto& f : fields) {
            if (f.empty()) f = "";
        }

        // EN: Convert boolean fields.
        // FR : Convertit les champs booléens.
        bool eligible_for_bounty = (fields[3] == "true" || fields[3] == "1");
        bool eligible_for_submission = (fields[4] == "true" || fields[4] == "1");

        // EN: Create a ScopeEntry and add it to the vector.
        // FR : Crée un ScopeEntry et l'ajoute au vecteur.
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
