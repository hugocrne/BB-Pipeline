// EN: Implementation of the ScopeEntry class. Handles construction and printing of a scope entry.
// FR : Implémentation de la classe ScopeEntry. Gère la construction et l'affichage d'une entrée de scope.

#include "../../include/types/scopeEntry.hpp"

#include <ostream>

namespace ScopeEntry {

// EN: Constructor. Initializes all fields of a scope entry.
// FR : Constructeur. Initialise tous les champs d'une entrée de scope.
ScopeEntry::ScopeEntry(const std::string& identifier,
                       const std::string& asset_type,
                       const std::variant<int, double, std::string>& instruction,
                       bool eligible_for_bounty,
                       bool eligible_for_submission,
                       const std::variant<int, double, std::string>& availability_requirement,
                       const std::variant<int, double, std::string>& confidentiality_requirement,
                       const std::string& integrity_requirement,
                       const std::string& max_severity,
                       const std::variant<int, double, std::string>& system_tags,
                       const std::string& created_at,
                       const std::string& updated_at)
    : identifier(identifier),
      asset_type(asset_type),
      instruction(instruction),
      eligible_for_bounty(eligible_for_bounty),
      eligible_for_submission(eligible_for_submission),
      availability_requirement(availability_requirement),
      confidentiality_requirement(confidentiality_requirement),
      integrity_requirement(integrity_requirement),
      max_severity(max_severity),
      system_tags(system_tags),
      created_at(created_at),
      updated_at(updated_at) {}

// EN: Output operator. Prints all fields of a scope entry.
// FR : Opérateur d'affichage. Affiche tous les champs d'une entrée de scope.
std::ostream& operator<<(std::ostream& os, const ScopeEntry& entry) {
    os << "identifier: " << entry.getIdentifier() << ", ";
    os << "asset_type: " << entry.getAssetType() << ", ";
    os << "instruction: ";
    if (std::holds_alternative<int>(entry.getInstruction()))
        os << std::get<int>(entry.getInstruction());
    else if (std::holds_alternative<double>(entry.getInstruction()))
        os << std::get<double>(entry.getInstruction());
    else
        os << std::get<std::string>(entry.getInstruction());
    os << ", eligible_for_bounty: " << entry.isEligibleForBounty();
    os << ", eligible_for_submission: " << entry.isEligibleForSubmission();
    os << ", availability_requirement: ";
    if (std::holds_alternative<int>(entry.getAvailabilityRequirement()))
        os << std::get<int>(entry.getAvailabilityRequirement());
    else if (std::holds_alternative<double>(entry.getAvailabilityRequirement()))
        os << std::get<double>(entry.getAvailabilityRequirement());
    else
        os << std::get<std::string>(entry.getAvailabilityRequirement());
    os << ", confidentiality_requirement: ";
    if (std::holds_alternative<int>(entry.getConfidentialityRequirement()))
        os << std::get<int>(entry.getConfidentialityRequirement());
    else if (std::holds_alternative<double>(entry.getConfidentialityRequirement()))
        os << std::get<double>(entry.getConfidentialityRequirement());
    else
        os << std::get<std::string>(entry.getConfidentialityRequirement());
    os << ", integrity_requirement: " << entry.getIntegrityRequirement();
    os << ", max_severity: " << entry.getMaxSeverity();
    os << ", system_tags: ";
    if (std::holds_alternative<int>(entry.getSystemTags()))
        os << std::get<int>(entry.getSystemTags());
    else if (std::holds_alternative<double>(entry.getSystemTags()))
        os << std::get<double>(entry.getSystemTags());
    else
        os << std::get<std::string>(entry.getSystemTags());
    os << ", created_at: " << entry.getCreatedAt();
    os << ", updated_at: " << entry.getUpdatedAt();
    return os;
}

}  // namespace ScopeEntry
