#include "../src/include/core/config_manager.hpp"
#include <cassert>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

const std::string TEST_YAML = R"(
database:
  host: localhost
  port: 5432
  username: testuser
  password: ${DB_PASSWORD}
  ssl_enabled: true
  connection_pool_size: 10

logging:
  level: info
  file: /var/log/bbp.log
  max_size: 100
  rotate: true

rate_limiting:
  default_rps: 10.0
  burst_capacity: 20
  enabled: true
  domains:
    - example.com
    - test.com

api:
  timeout: 30
  retries: 3
  user_agent: "BB-Pipeline/1.0"
)";

void testBasicConfigOperations() {
    std::cout << "=== Test Basic Config Operations ===" << std::endl;
    
    auto& config = BBP::ConfigManager::getInstance();
    config.reset();
    
    // Test setting and getting values
    config.set("database", "host", BBP::ConfigValue(std::string("localhost")));
    config.set("database", "port", BBP::ConfigValue(5432));
    config.set("logging", "enabled", BBP::ConfigValue(true));
    
    assert(config.has("database", "host"));
    assert(config.has("database", "port"));
    assert(config.has("logging", "enabled"));
    
    auto host = config.get("database", "host").as<std::string>();
    auto port = config.get("database", "port").as<int>();
    auto enabled = config.get("logging", "enabled").as<bool>();
    
    assert(host == "localhost");
    assert(port == 5432);
    assert(enabled == true);
    
    std::cout << "✓ Basic config operations test passed" << std::endl;
}

void testConfigValue() {
    std::cout << "\n=== Test ConfigValue ===" << std::endl;
    
    // Test different types
    BBP::ConfigValue bool_val(true);
    BBP::ConfigValue int_val(42);
    BBP::ConfigValue double_val(3.14);
    BBP::ConfigValue string_val(std::string("hello"));
    BBP::ConfigValue array_val(std::vector<std::string>{"a", "b", "c"});
    
    assert(bool_val.as<bool>() == true);
    assert(int_val.as<int>() == 42);
    assert(double_val.as<double>() == 3.14);
    assert(string_val.as<std::string>() == "hello");
    
    auto array = array_val.as<std::vector<std::string>>();
    assert(array.size() == 3);
    assert(array[0] == "a");
    
    // Test tryAs and asOrDefault
    assert(int_val.tryAs<int>().has_value());
    assert(!int_val.tryAs<std::string>().has_value());
    assert(int_val.asOrDefault<int>(999) == 42);
    assert(int_val.asOrDefault<std::string>("default") == "default");
    
    // Test toString
    assert(bool_val.toString() == "true");
    assert(int_val.toString() == "42");
    assert(string_val.toString() == "hello");
    
    std::cout << "✓ ConfigValue test passed" << std::endl;
}

void testYamlLoading() {
    std::cout << "\n=== Test YAML Loading ===" << std::endl;
    
    auto& config = BBP::ConfigManager::getInstance();
    config.reset();
    
    // Test loading from string
    bool loaded = config.loadFromString(TEST_YAML);
    assert(loaded);
    
    // Test loaded values
    assert(config.get("database", "host").as<std::string>() == "localhost");
    assert(config.get("database", "port").as<int>() == 5432);
    assert(config.get("database", "ssl_enabled").as<bool>() == true);
    assert(config.get("logging", "level").as<std::string>() == "info");
    assert(config.get("rate_limiting", "default_rps").as<double>() == 10.0);
    
    auto domains = config.get("rate_limiting", "domains").as<std::vector<std::string>>();
    assert(domains.size() == 2);
    assert(domains[0] == "example.com");
    assert(domains[1] == "test.com");
    
    std::cout << "✓ YAML loading test passed" << std::endl;
}

void testFileOperations() {
    std::cout << "\n=== Test File Operations ===" << std::endl;
    
    auto& config = BBP::ConfigManager::getInstance();
    config.reset();
    
    std::string test_file = "/tmp/bbp_config_test.yaml";
    
    // Write test YAML to file
    std::ofstream file(test_file);
    file << TEST_YAML;
    file.close();
    
    // Load from file
    assert(config.loadFromFile(test_file));
    
    // Verify loaded data
    assert(config.get("database", "host").as<std::string>() == "localhost");
    assert(config.get("logging", "level").as<std::string>() == "info");
    
    // Modify configuration
    config.set("database", "host", BBP::ConfigValue(std::string("modified_host")));
    config.set("new_section", "new_key", BBP::ConfigValue(std::string("new_value")));
    
    // Save to new file
    std::string output_file = "/tmp/bbp_config_output.yaml";
    assert(config.saveToFile(output_file));
    
    // Load again and verify with a separate instance
    auto& config2 = BBP::ConfigManager::getInstance();
    config2.reset();
    assert(config2.loadFromFile(output_file));
    assert(config2.get("database", "host").as<std::string>() == "modified_host");
    assert(config2.get("new_section", "new_key").as<std::string>() == "new_value");
    
    // Cleanup
    std::remove(test_file.c_str());
    std::remove(output_file.c_str());
    
    std::cout << "✓ File operations test passed" << std::endl;
}

void testConfigSections() {
    std::cout << "\n=== Test Config Sections ===" << std::endl;
    
    auto& config = BBP::ConfigManager::getInstance();
    config.reset();
    config.loadFromString(TEST_YAML);
    
    // Test section operations
    auto database_section = config.getSection("database");
    assert(database_section.has("host"));
    assert(database_section.has("port"));
    assert(database_section.size() >= 5);
    
    auto keys = database_section.keys();
    assert(!keys.empty());
    
    // Test section names
    auto section_names = config.getSectionNames();
    assert(!section_names.empty());
    bool found_database = false;
    for (const auto& name : section_names) {
        if (name == "database") {
            found_database = true;
            break;
        }
    }
    assert(found_database);
    
    // Test section merge
    BBP::ConfigSection new_section;
    new_section.set("new_key", BBP::ConfigValue(std::string("new_value")));
    database_section.merge(new_section, true);
    assert(database_section.has("new_key"));
    
    std::cout << "✓ Config sections test passed" << std::endl;
}

void testValidation() {
    std::cout << "\n=== Test Validation ===" << std::endl;
    
    auto& config = BBP::ConfigManager::getInstance();
    config.reset();
    config.loadFromString(TEST_YAML);
    
    // Add validation rules
    std::vector<BBP::ConfigManager::ValidationRule> rules = {
        {"database.host", "string", true, std::nullopt, std::nullopt, std::nullopt, {}, "Database host"},
        {"database.port", "int", true, std::nullopt, 1.0, 65535.0, {}, "Database port"},
        {"logging.level", "string", true, std::nullopt, std::nullopt, std::nullopt, 
         {"debug", "info", "warn", "error"}, "Log level"},
        {"rate_limiting.default_rps", "double", true, std::nullopt, 0.1, 1000.0, {}, "Default RPS"},
        {"missing_key", "string", true, std::nullopt, std::nullopt, std::nullopt, {}, "Missing key"}
    };
    
    config.addValidationRules(rules);
    
    // Test validation
    std::vector<std::string> errors;
    bool is_valid = config.validate(errors);
    
    // Should have at least one error for missing_key
    assert(!is_valid);
    assert(!errors.empty());
    
    bool found_missing_error = false;
    for (const auto& error : errors) {
        if (error.find("missing_key") != std::string::npos) {
            found_missing_error = true;
            break;
        }
    }
    assert(found_missing_error);
    
    std::cout << "✓ Validation test passed" << std::endl;
}

void testTemplates() {
    std::cout << "\n=== Test Templates ===" << std::endl;
    
    auto& config = BBP::ConfigManager::getInstance();
    config.reset();
    
    // Create a template
    BBP::ConfigSection template_section;
    template_section.set("timeout", BBP::ConfigValue(30));
    template_section.set("retries", BBP::ConfigValue(3));
    template_section.set("user_agent", BBP::ConfigValue(std::string("BB-Pipeline")));
    
    config.addTemplate("http_defaults", template_section);
    
    // Create a section without template values
    config.set("api", "endpoint", BBP::ConfigValue(std::string("/api/v1")));
    
    // Apply template
    assert(config.applyTemplate("http_defaults"));
    
    // Check that template values were applied
    assert(config.has("api", "timeout"));
    assert(config.has("api", "retries"));
    assert(config.has("api", "user_agent"));
    assert(config.get("api", "timeout").as<int>() == 30);
    
    std::cout << "✓ Templates test passed" << std::endl;
}

void testMacros() {
    std::cout << "\n=== Test Convenience Macros ===" << std::endl;
    
    auto& config = BBP::ConfigManager::getInstance();
    config.reset();
    
    // Test macros
    CONFIG_SET("test_key", std::string("test_value"));
    CONFIG_SET_SECTION("test_section", "test_key", 42);
    
    auto value1 = CONFIG_GET("test_key").as<std::string>();
    auto value2 = CONFIG_GET_SECTION("test_section", "test_key").as<int>();
    
    assert(value1 == "test_value");
    assert(value2 == 42);
    
    std::cout << "✓ Convenience macros test passed" << std::endl;
}

void testDump() {
    std::cout << "\n=== Test Configuration Dump ===" << std::endl;
    
    auto& config = BBP::ConfigManager::getInstance();
    config.reset();
    config.loadFromString(TEST_YAML);
    
    std::string dump = config.dump();
    assert(!dump.empty());
    assert(dump.find("database") != std::string::npos);
    assert(dump.find("localhost") != std::string::npos);
    
    std::cout << "Configuration dump preview:" << std::endl;
    std::cout << dump.substr(0, 200) << "..." << std::endl;
    
    std::cout << "✓ Configuration dump test passed" << std::endl;
}

int main() {
    std::cout << "Running Configuration Manager Tests...\n" << std::endl;
    
    try {
        testBasicConfigOperations();
        testConfigValue();
        testYamlLoading();
        testFileOperations();
        testConfigSections();
        testValidation();
        testTemplates();
        testMacros();
        testDump();
        
        std::cout << "\n🎉 All Configuration Manager tests passed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}