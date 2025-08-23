#include <cassert>
#include <iostream>
#include <string>
#include <variant>

#include "../src/include/types/scope.hpp"
#include "../src/include/types/scopeEntry.hpp"

int main() {
    // Test ScopeEntry
    ScopeEntry::ScopeEntry entry("id1",
                                 "asset",
                                 "instr",
                                 true,
                                 false,
                                 "avail",
                                 "conf",
                                 "integrity",
                                 "severity",
                                 "tags",
                                 "2025-08-23",
                                 "2025-08-23");
    assert(entry.getIdentifier() == "id1");
    assert(entry.getAssetType() == "asset");
    assert(std::get<std::string>(entry.getInstruction()) == "instr");
    assert(entry.isEligibleForBounty() == true);
    assert(entry.isEligibleForSubmission() == false);
    assert(std::get<std::string>(entry.getAvailabilityRequirement()) == "avail");
    assert(std::get<std::string>(entry.getConfidentialityRequirement()) == "conf");
    assert(entry.getIntegrityRequirement() == "integrity");
    assert(entry.getMaxSeverity() == "severity");
    assert(std::get<std::string>(entry.getSystemTags()) == "tags");
    assert(entry.getCreatedAt() == "2025-08-23");
    assert(entry.getUpdatedAt() == "2025-08-23");
    std::cout << "ScopeEntry construction and getters OK" << std::endl;

    // Test Scope loading from CSV
    Scope::Scope scope("tests/data/test_scope.csv");
    const auto& entries = scope.getEntries();
    assert(!entries.empty());
    // Test first entry (adapt values to your test_scope.csv)
    const auto& e = entries[0];
    std::cout << "First entry: " << e.getIdentifier() << std::endl;
    // Ajoute ici des asserts selon le contenu réel du CSV
    std::cout << "Scope CSV loading and getters OK" << std::endl;
    return 0;
}
