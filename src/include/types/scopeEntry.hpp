// EN: Declaration of the ScopeEntry class. Represents a single entry in the scope with all its
// fields. FR : Déclaration de la classe ScopeEntry. Représente une entrée unique du scope avec tous ses champs.

#pragma once

#include <ostream>
#include <string>
#include <variant>

namespace ScopeEntry {

class ScopeEntry {
  private:
    // EN: Unique identifier for the entry.
    // FR : Identifiant unique de l'entrée.
    const std::string identifier;
    // EN: Type of asset.
    // FR : Type d'actif.
    const std::string asset_type;
    // EN: Instruction field (can be int, double, or string).
    // FR : Champ instruction (peut être int, double ou string).
    const std::variant<int, double, std::string> instruction;
    // EN: Is eligible for bounty?
    // FR : Éligible à la prime ?
    const bool eligible_for_bounty;
    // EN: Is eligible for submission?
    // FR : Éligible à la soumission ?
    const bool eligible_for_submission;
    // EN: Availability requirement (can be int, double, or string).
    // FR : Exigence de disponibilité (peut être int, double ou string).
    const std::variant<int, double, std::string> availability_requirement;
    // EN: Confidentiality requirement (can be int, double, or string).
    // FR : Exigence de confidentialité (peut être int, double ou string).
    const std::variant<int, double, std::string> confidentiality_requirement;
    // EN: Integrity requirement.
    // FR : Exigence d'intégrité.
    const std::string integrity_requirement;
    // EN: Maximum severity.
    // FR : Sévérité maximale.
    const std::string max_severity;
    // EN: System tags (can be int, double, or string).
    // FR : Tags système (peut être int, double ou string).
    const std::variant<int, double, std::string> system_tags;
    // EN: Creation date.
    // FR : Date de création.
    const std::string created_at;
    // EN: Last update date.
    // FR : Date de dernière mise à jour.
    const std::string updated_at;

  public:
    // EN: Constructor. Initializes all fields.
    // FR : Constructeur. Initialise tous les champs.
    ScopeEntry(const std::string& identifier,
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
               const std::string& updated_at);
    ~ScopeEntry() = default;
    // Getters
    // EN: Getter for identifier.
    // FR : Accesseur pour l'identifiant.
    const std::string& getIdentifier() const { return identifier; }
    // EN: Getter for asset type.
    // FR : Accesseur pour le type d'actif.
    const std::string& getAssetType() const { return asset_type; }
    // EN: Getter for instruction.
    // FR : Accesseur pour l'instruction.
    const std::variant<int, double, std::string>& getInstruction() const { return instruction; }
    // EN: Getter for bounty eligibility.
    // FR : Accesseur pour l'éligibilité à la prime.
    bool isEligibleForBounty() const { return eligible_for_bounty; }
    // EN: Getter for submission eligibility.
    // FR : Accesseur pour l'éligibilité à la soumission.
    bool isEligibleForSubmission() const { return eligible_for_submission; }
    // EN: Getter for availability requirement.
    // FR : Accesseur pour l'exigence de disponibilité.
    const std::variant<int, double, std::string>& getAvailabilityRequirement() const {
        return availability_requirement;
    }
    // EN: Getter for confidentiality requirement.
    // FR : Accesseur pour l'exigence de confidentialité.
    const std::variant<int, double, std::string>& getConfidentialityRequirement() const {
        return confidentiality_requirement;
    }
    // EN: Getter for integrity requirement.
    // FR : Accesseur pour l'exigence d'intégrité.
    const std::string& getIntegrityRequirement() const { return integrity_requirement; }
    // EN: Getter for max severity.
    // FR : Accesseur pour la sévérité maximale.
    const std::string& getMaxSeverity() const { return max_severity; }
    // EN: Getter for system tags.
    // FR : Accesseur pour les tags système.
    const std::variant<int, double, std::string>& getSystemTags() const { return system_tags; }
    // EN: Getter for creation date.
    // FR : Accesseur pour la date de création.
    const std::string& getCreatedAt() const { return created_at; }
    // EN: Getter for update date.
    // FR : Accesseur pour la date de mise à jour.
    const std::string& getUpdatedAt() const { return updated_at; }
    // EN: Output operator for printing the entry.
    // FR : Opérateur d'affichage pour imprimer l'entrée.
    friend std::ostream& operator<<(std::ostream& os, const ScopeEntry& entry);
};

// EN: Output operator declaration.
// FR : Déclaration de l'opérateur d'affichage.
std::ostream& operator<<(std::ostream& os, const ScopeEntry& entry);

}  // namespace ScopeEntry
