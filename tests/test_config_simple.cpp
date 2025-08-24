#include "../src/include/core/config_manager.hpp"
#include <cassert>
#include <iostream>

const std::string SIMPLE_YAML = R"(
database:
  host: localhost
  port: 5432

logging:
  level: info
  enabled: true
)";

int main() {
    std::cout << "Running Simple Configuration Manager Test..." << std::endl;
    
    try {
        auto& config = BBP::ConfigManager::getInstance();
        config.reset();
        
        // Test basic loading
        bool loaded = config.loadFromString(SIMPLE_YAML);
        assert(loaded);
        
        // Test basic get operations
        auto host = config.get("database", "host").as<std::string>();
        auto port = config.get("database", "port").as<int>();
        auto enabled = config.get("logging", "enabled").as<bool>();
        
        assert(host == "localhost");
        assert(port == 5432);
        assert(enabled == true);
        
        std::cout << "✓ Configuration Manager basic test passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}