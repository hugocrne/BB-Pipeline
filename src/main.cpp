// EN: Entry point of the program. Loads the scope from a CSV file and prints it.
// FR : Point d'entrée du programme. Charge le scope depuis un fichier CSV et l'affiche.

#include <iostream>
#include <string>

#include "./include/types/scope.hpp"

int main() {
    // EN: Create a Scope object from the CSV file.
    // FR : Crée un objet Scope à partir du fichier CSV.
    Scope::Scope scope("data/scope.csv");

    // EN: Print the entire scope to the console.
    // FR : Affiche tout le scope dans la console.
    // std::cout << "Scope: " << scope << std::endl;

    return 0;
}
