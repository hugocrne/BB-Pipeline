#include "core/logger.hpp"
#include <unordered_map>

int main() {
    auto& logger = BBP::Logger::getInstance();
    
    logger.setLogLevel(BBP::LogLevel::DEBUG);
    
    std::string correlation_id = logger.generateCorrelationId();
    logger.setCorrelationId(correlation_id);
    
    logger.addGlobalMetadata("version", "1.0.0");
    logger.addGlobalMetadata("environment", "production");
    
    LOG_INFO("main", "BB-Pipeline started successfully");
    
    std::unordered_map<std::string, std::string> metadata = {
        {"target", "example.com"},
        {"scope", "subdomain_enumeration"}
    };
    
    LOG_INFO_META("subhunter", "Starting subdomain enumeration", metadata);
    LOG_DEBUG("subhunter", "Using passive DNS sources");
    LOG_WARN("subhunter", "Rate limit reached, applying backoff");
    LOG_ERROR("subhunter", "Failed to resolve DNS for target domain");
    
    logger.flush();
    
    return 0;
}