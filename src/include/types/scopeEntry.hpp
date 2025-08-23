#pragma once

#include <ostream>
#include <string>
#include <variant>

namespace ScopeEntry {

class ScopeEntry {
  private:
    const std::string identifier;
    const std::string asset_type;
    const std::variant<int, double, std::string> instruction;
    const bool eligible_for_bounty;
    const bool eligible_for_submission;
    const std::variant<int, double, std::string> availability_requirement;
    const std::variant<int, double, std::string> confidentiality_requirement;
    const std::string integrity_requirement;
    const std::string max_severity;
    const std::variant<int, double, std::string> system_tags;
    const std::string created_at;
    const std::string updated_at;

  public:
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
    const std::string& getIdentifier() const { return identifier; }
    const std::string& getAssetType() const { return asset_type; }
    const std::variant<int, double, std::string>& getInstruction() const { return instruction; }
    bool isEligibleForBounty() const { return eligible_for_bounty; }
    bool isEligibleForSubmission() const { return eligible_for_submission; }
    const std::variant<int, double, std::string>& getAvailabilityRequirement() const {
        return availability_requirement;
    }
    const std::variant<int, double, std::string>& getConfidentialityRequirement() const {
        return confidentiality_requirement;
    }
    const std::string& getIntegrityRequirement() const { return integrity_requirement; }
    const std::string& getMaxSeverity() const { return max_severity; }
    const std::variant<int, double, std::string>& getSystemTags() const { return system_tags; }
    const std::string& getCreatedAt() const { return created_at; }
    const std::string& getUpdatedAt() const { return updated_at; }
    // Helper for printing std::variant<int, double, std::string>
    static std::string variantToString(const std::variant<int, double, std::string>& v) {
        return std::visit(
            [](auto&& arg) -> std::string {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, int>) {
                    return std::to_string(arg);
                } else if constexpr (std::is_same_v<T, double>) {
                    return std::to_string(arg);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return arg;
                } else {
                    return "";
                }
            },
            v);
    }

    friend std::ostream& operator<<(std::ostream& os, const ScopeEntry& entry);
};

std::ostream& operator<<(std::ostream& os, const ScopeEntry& entry);

}  // namespace ScopeEntry
