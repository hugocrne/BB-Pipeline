#include <cassert>
#include <iostream>
#include <string>
#include <variant>

#include "../src/include/types/scopeEntry.hpp"

int main() {
    ScopeEntry::ScopeEntry entry("id2",
                                 "asset2",
                                 "instr2",
                                 false,
                                 true,
                                 "avail2",
                                 "conf2",
                                 "integrity2",
                                 "severity2",
                                 "tags2",
                                 "2025-08-23",
                                 "2025-08-23");
    assert(entry.getIdentifier() == "id2");
    assert(entry.getAssetType() == "asset2");
    assert(std::get<std::string>(entry.getInstruction()) == "instr2");
    assert(entry.isEligibleForBounty() == false);
    assert(entry.isEligibleForSubmission() == true);
    assert(std::get<std::string>(entry.getAvailabilityRequirement()) == "avail2");
    assert(std::get<std::string>(entry.getConfidentialityRequirement()) == "conf2");
    assert(entry.getIntegrityRequirement() == "integrity2");
    assert(entry.getMaxSeverity() == "severity2");
    assert(std::get<std::string>(entry.getSystemTags()) == "tags2");
    assert(entry.getCreatedAt() == "2025-08-23");
    assert(entry.getUpdatedAt() == "2025-08-23");
    std::cout << "ScopeEntry construction and getters OK" << std::endl;
    return 0;
}
