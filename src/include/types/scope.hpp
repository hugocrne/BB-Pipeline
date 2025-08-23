// EN: Declaration of the Scope class. Represents a collection of scope entries loaded from a CSV
// file. FR : Déclaration de la classe Scope. Représente une collection d'entrées de scope chargées depuis un fichier CSV.

#pragma once

#include <list>
#include <ostream>
#include <string>
#include <vector>

#include "./scopeEntry.hpp"

namespace Scope {

class Scope {
  private:
    // EN: Vector containing all scope entries.
    // FR : Vecteur contenant toutes les entrées de scope.
    std::vector<ScopeEntry::ScopeEntry> entries;

  public:
    // EN: Constructor. Loads entries from a CSV file.
    // FR : Constructeur. Charge les entrées depuis un fichier CSV.
    Scope(std::string fileName);
    // EN: Destructor. Clears the entries vector.
    // FR : Destructeur. Vide le vecteur entries.
    ~Scope();
    // EN: Getter for entries vector.
    // FR : Accesseur pour le vecteur entries.
    const std::vector<ScopeEntry::ScopeEntry>& getEntries() const { return entries; }

    // EN: Output operator for printing the scope.
    // FR : Opérateur d'affichage pour imprimer le scope.
    friend std::ostream& operator<<(std::ostream& os, const Scope& scope);
};

// EN: Output operator declaration.
// FR : Déclaration de l'opérateur d'affichage.
std::ostream& operator<<(std::ostream& os, const Scope& scope);

}  // namespace Scope
