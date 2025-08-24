#include "core/config_manager.hpp"
#include "core/logger.hpp"
#include <iostream>

int main() {
    auto& logger = BBP::Logger::getInstance();
    auto& config = BBP::ConfigManager::getInstance();
    
    logger.setLogLevel(BBP::LogLevel::INFO);
    std::string correlation_id = logger.generateCorrelationId();
    logger.setCorrelationId(correlation_id);
    
    LOG_INFO("config_example", "BB-Pipeline Configuration Manager Example");
    
    // Load configuration from YAML string
    const std::string yaml_config = R"(
application:
  name: BB-Pipeline
  version: "1.0.0"
  debug: true
  
database:
  host: localhost
  port: 5432
  username: bbp_user
  password: ${DB_PASSWORD}
  ssl_enabled: true
  connection_pool_size: 10

rate_limiting:
  default_rps: 10.0
  burst_capacity: 20
  global_limit: 100.0
  enabled: true

logging:
  level: info
  file: /var/log/bbp.log
  max_size: 100
  rotate: true

modules:
  - subhunter
  - httpxpp
  - discovery
  - jsintel
)";
    
    LOG_INFO("config_example", "Loading configuration from YAML");
    bool loaded = config.loadFromString(yaml_config);
    if (!loaded) {
        LOG_ERROR("config_example", "Failed to load configuration");
        return 1;
    }
    
    // Demonstrate configuration access
    LOG_INFO("config_example", "Reading configuration values:");
    
    auto app_name = CONFIG_GET_SECTION("application", "name").as<std::string>();
    auto app_version = CONFIG_GET_SECTION("application", "version").as<std::string>();
    auto debug_mode = CONFIG_GET_SECTION("application", "debug").as<bool>();
    
    std::cout << "Application: " << app_name << " v" << app_version << std::endl;
    std::cout << "Debug mode: " << (debug_mode ? "enabled" : "disabled") << std::endl;
    
    auto db_host = CONFIG_GET_SECTION("database", "host").as<std::string>();
    auto db_port = CONFIG_GET_SECTION("database", "port").as<int>();
    auto db_ssl = CONFIG_GET_SECTION("database", "ssl_enabled").as<bool>();
    
    std::cout << "Database: " << db_host << ":" << db_port 
              << " (SSL: " << (db_ssl ? "yes" : "no") << ")" << std::endl;
    
    auto rate_limit = CONFIG_GET_SECTION("rate_limiting", "default_rps").as<double>();
    std::cout << "Default rate limit: " << rate_limit << " req/s" << std::endl;
    
    auto modules = CONFIG_GET_SECTION("modules", "value").as<std::vector<std::string>>();
    std::cout << "Enabled modules: ";
    for (size_t i = 0; i < modules.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << modules[i];
    }
    std::cout << std::endl;
    
    // Demonstrate configuration modification
    LOG_INFO("config_example", "Modifying configuration");
    CONFIG_SET_SECTION("application", "debug", false);
    CONFIG_SET_SECTION("rate_limiting", "default_rps", 20.0);
    
    // Add validation rules
    LOG_INFO("config_example", "Adding validation rules");
    std::vector<BBP::ConfigManager::ValidationRule> rules = {
        {"application.name", "string", true, std::nullopt, std::nullopt, std::nullopt, {}, "Application name"},
        {"database.port", "int", true, std::nullopt, 1.0, 65535.0, {}, "Database port"},
        {"rate_limiting.default_rps", "double", true, std::nullopt, 0.1, 1000.0, {}, "Default RPS"},
        {"logging.level", "string", true, std::nullopt, std::nullopt, std::nullopt, 
         {"debug", "info", "warn", "error"}, "Log level"}
    };
    
    config.addValidationRules(rules);
    
    // Validate configuration
    std::vector<std::string> errors;
    bool is_valid = config.validate(errors);
    
    if (is_valid) {
        LOG_INFO("config_example", "Configuration validation passed");
    } else {
        LOG_WARN("config_example", "Configuration validation failed:");
        for (const auto& error : errors) {
            std::cout << "  - " << error << std::endl;
        }
    }
    
    // Demonstrate configuration dump
    LOG_INFO("config_example", "Configuration dump:");
    std::string config_dump = config.dump();
    std::cout << config_dump << std::endl;
    
    // Save configuration to file
    std::string output_file = "/tmp/bbp_example_config.yaml";
    if (config.saveToFile(output_file)) {
        LOG_INFO("config_example", "Configuration saved to: " + output_file);
    } else {
        LOG_ERROR("config_example", "Failed to save configuration");
    }
    
    LOG_INFO("config_example", "Configuration Manager example completed");
    return 0;
}