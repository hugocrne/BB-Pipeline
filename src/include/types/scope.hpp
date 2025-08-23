#pragma once

#include <list>
#include <ostream>
#include <string>
#include <vector>

#include "./scope.hpp"
#include "./scopeEntry.hpp"

namespace Scope {

class Scope;

class Scope {
  private:
    std::vector<ScopeEntry::ScopeEntry> entries;

  public:
    Scope(std::string fileName);
    ~Scope();
    // Getter for entries
    const std::vector<ScopeEntry::ScopeEntry>& getEntries() const { return entries; }

    friend std::ostream& operator<<(std::ostream& os, const Scope& scope);
};

std::ostream& operator<<(std::ostream& os, const Scope& scope);

}  // namespace Scope
