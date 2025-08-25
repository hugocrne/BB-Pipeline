// EN: Simple integration test for Error Recovery - Network failure simulation
// FR: Test d'intégration simple pour Error Recovery - Simulation d'échecs réseau

#include <iostream>
#include <string>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include "infrastructure/system/error_recovery.hpp"
#include "infrastructure/logging/logger.hpp"

using namespace BBP;
using namespace std::chrono_literals;

// EN: Simulate various network operations with controlled failure rates
// FR: Simule diverses opérations réseau avec taux d'échec contrôlés
class NetworkSimulator {
public:
    NetworkSimulator() : rng_(std::random_device{}()) {}
    
    // EN: Simulate HTTP request with 30% failure rate initially, then success
    // FR: Simule requête HTTP avec 30% de taux d'échec initial, puis succès
    std::string httpRequest(const std::string& url) {
        request_count_++;
        
        // EN: Simulate network delay
        // FR: Simule délai réseau
        std::this_thread::sleep_for(std::chrono::milliseconds(10 + (rng_() % 20)));
        
        // EN: Fail first few attempts to test retry logic
        // FR: Échoue les premières tentatives pour tester la logique de retry
        if (request_count_ <= 2) {
            if (url.find("timeout") != std::string::npos) {
                throw std::runtime_error("timeout error");
            } else if (url.find("refused") != std::string::npos) {
                throw std::runtime_error("connection refused");
            } else if (url.find("503") != std::string::npos) {
                throw std::runtime_error("503 service unavailable");
            }
        }
        
        return "HTTP/1.1 200 OK\nContent: Success for " + url;
    }
    
    // EN: Simulate DNS resolution with occasional failures
    // FR: Simule résolution DNS avec échecs occasionnels
    std::string resolveDns(const std::string& hostname) {
        dns_count_++;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5 + (rng_() % 10)));
        
        if (dns_count_ == 1 && hostname.find("example") != std::string::npos) {
            throw std::runtime_error("dns resolution failed");
        }
        
        return "192.168.1." + std::to_string((rng_() % 254) + 1);
    }
    
    // EN: Simulate database connection with temporary failures
    // FR: Simule connexion base de données avec échecs temporaires
    bool connectDatabase(const std::string& connection_string) {
        (void)connection_string; // EN: Suppress unused parameter warning / FR: Supprime l'avertissement paramètre non utilisé
        db_count_++;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(20 + (rng_() % 30)));
        
        if (db_count_ <= 1) {
            throw std::runtime_error("temporary database unavailable");
        }
        
        return true;
    }
    
    void reset() {
        request_count_ = 0;
        dns_count_ = 0;
        db_count_ = 0;
    }

private:
    std::atomic<int> request_count_{0};
    std::atomic<int> dns_count_{0};
    std::atomic<int> db_count_{0};
    std::mt19937 rng_;
};

int main() {
    std::cout << "=== BB-Pipeline Error Recovery Integration Test ===\n\n";
    
    // EN: Setup logger
    // FR: Configure le logger
    auto& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::INFO);
    
    // EN: Get error recovery manager instance
    // FR: Obtient l'instance du gestionnaire de récupération d'erreur
    auto& recovery = ErrorRecoveryManager::getInstance();
    
    // EN: Configure for network operations
    // FR: Configure pour les opérations réseau
    auto network_config = ErrorRecoveryUtils::createNetworkRetryConfig();
    network_config.max_attempts = 4;
    network_config.initial_delay = 50ms;
    network_config.max_delay = 2000ms;
    network_config.enable_jitter = true;
    network_config.jitter_factor = 0.2;
    
    recovery.configure(network_config);
    recovery.setDetailedLogging(true);
    
    std::cout << "1. Error Recovery Manager configured for network operations\n";
    
    NetworkSimulator simulator;
    
    // EN: Test HTTP operations with retry
    // FR: Test des opérations HTTP avec retry
    std::cout << "\n2. Testing HTTP operations with automatic retry...\n";
    
    std::vector<std::string> test_urls = {
        "https://example.com/timeout",
        "https://api.service.com/refused", 
        "https://backend.server.com/503",
        "https://stable.service.com/api"
    };
    
    int successful_requests = 0;
    for (const auto& url : test_urls) {
        try {
            simulator.reset();
            std::cout << "   Requesting: " << url << "...\n";
            
            auto result = recovery.executeWithRetry("http_request", [&simulator, &url]() {
                return simulator.httpRequest(url);
            });
            
            std::cout << "   ✓ Success: " << result.substr(0, 50) << "...\n";
            successful_requests++;
            
        } catch (const std::exception& e) {
            std::cout << "   ✗ Failed: " << e.what() << "\n";
        }
    }
    
    std::cout << "   HTTP Success Rate: " << successful_requests << "/" << test_urls.size() << "\n";
    
    // EN: Test DNS operations
    // FR: Test des opérations DNS
    std::cout << "\n3. Testing DNS resolution with retry...\n";
    
    std::vector<std::string> hostnames = {
        "example.com",
        "api.service.net",
        "stable.domain.org"
    };
    
    int successful_dns = 0;
    for (const auto& hostname : hostnames) {
        try {
            simulator.reset();
            std::cout << "   Resolving: " << hostname << "...\n";
            
            auto ip = recovery.executeWithRetry("dns_resolution", [&simulator, &hostname]() {
                return simulator.resolveDns(hostname);
            });
            
            std::cout << "   ✓ Resolved: " << hostname << " -> " << ip << "\n";
            successful_dns++;
            
        } catch (const std::exception& e) {
            std::cout << "   ✗ Failed: " << e.what() << "\n";
        }
    }
    
    std::cout << "   DNS Success Rate: " << successful_dns << "/" << hostnames.size() << "\n";
    
    // EN: Test database connections
    // FR: Test des connexions base de données
    std::cout << "\n4. Testing database connections with retry...\n";
    
    auto db_config = ErrorRecoveryUtils::createDatabaseRetryConfig();
    
    std::vector<std::string> db_connections = {
        "postgresql://localhost:5432/app",
        "mysql://db.server.com:3306/data"
    };
    
    int successful_db = 0;
    for (const auto& conn_str : db_connections) {
        try {
            simulator.reset();
            std::cout << "   Connecting: " << conn_str << "...\n";
            
            auto connected = recovery.executeWithRetry("db_connection", db_config, [&simulator, &conn_str]() {
                return simulator.connectDatabase(conn_str);
            });
            
            if (connected) {
                std::cout << "   ✓ Connected: " << conn_str << "\n";
                successful_db++;
            }
            
        } catch (const std::exception& e) {
            std::cout << "   ✗ Failed: " << e.what() << "\n";
        }
    }
    
    std::cout << "   Database Success Rate: " << successful_db << "/" << db_connections.size() << "\n";
    
    // EN: Test async operations
    // FR: Test des opérations asynchrones
    std::cout << "\n5. Testing async operations with retry...\n";
    
    std::vector<std::future<std::string>> futures;
    
    for (int i = 0; i < 3; ++i) {
        simulator.reset();
        futures.push_back(recovery.executeAsyncWithRetry("async_http_" + std::to_string(i), 
            [&simulator, i]() {
                return simulator.httpRequest("https://async.service.com/endpoint/" + std::to_string(i));
            }));
    }
    
    int async_success = 0;
    for (size_t i = 0; i < futures.size(); ++i) {
        try {
            auto result = futures[i].get();
            std::cout << "   ✓ Async operation " << i << " completed successfully\n";
            async_success++;
        } catch (const std::exception& e) {
            std::cout << "   ✗ Async operation " << i << " failed: " << e.what() << "\n";
        }
    }
    
    std::cout << "   Async Success Rate: " << async_success << "/" << futures.size() << "\n";
    
    // EN: Test circuit breaker
    // FR: Test du circuit breaker
    std::cout << "\n6. Testing circuit breaker functionality...\n";
    
    recovery.setCircuitBreakerThreshold(2);
    
    // EN: Cause failures to trigger circuit breaker
    // FR: Cause des échecs pour déclencher le circuit breaker
    for (int i = 0; i < 2; ++i) {
        try {
            recovery.executeWithRetry("circuit_test", []() {
                throw std::invalid_argument("permanent failure");
            });
        } catch (...) {
            std::cout << "   Expected failure " << (i + 1) << "/2\n";
        }
    }
    
    if (recovery.isCircuitBreakerOpen()) {
        std::cout << "   ✓ Circuit breaker opened after threshold failures\n";
        
        // EN: Test that circuit breaker blocks further operations
        // FR: Test que le circuit breaker bloque les opérations suivantes
        try {
            recovery.executeWithRetry("blocked_operation", []() {
                return 42;
            });
            std::cout << "   ✗ Circuit breaker should have blocked this operation\n";
        } catch (const NonRecoverableError& e) {
            std::cout << "   ✓ Circuit breaker correctly blocked operation\n";
        }
        
        // EN: Reset and test normal operation
        // FR: Remet à zéro et teste l'opération normale
        recovery.resetCircuitBreaker();
        try {
            auto result = recovery.executeWithRetry("after_reset", []() {
                return 100;
            });
            std::cout << "   ✓ Operations work after circuit breaker reset (result: " << result << ")\n";
        } catch (const std::exception& e) {
            std::cout << "   ✗ Operation failed after reset: " << e.what() << "\n";
        }
    } else {
        std::cout << "   ✗ Circuit breaker did not open as expected\n";
    }
    
    // EN: Display final statistics
    // FR: Affiche les statistiques finales
    auto stats = recovery.getStatistics();
    std::cout << "\n=== Final Error Recovery Statistics ===\n";
    std::cout << "Total operations: " << stats.total_operations << "\n";
    std::cout << "Successful operations: " << stats.successful_operations << "\n";
    std::cout << "Failed operations: " << stats.failed_operations << "\n";
    std::cout << "Total retries: " << stats.total_retries << "\n";
    std::cout << "Total retry time: " << stats.total_retry_time.count() << "ms\n";
    
    if (stats.total_retries > 0) {
        std::cout << "Average retry time: " << stats.average_retry_time.count() << "ms\n";
    }
    
    std::cout << "\nError type breakdown:\n";
    for (const auto& [error_type, count] : stats.error_counts) {
        std::string type_name;
        switch (error_type) {
            case RecoverableErrorType::NETWORK_TIMEOUT:
                type_name = "Network Timeout";
                break;
            case RecoverableErrorType::CONNECTION_REFUSED:
                type_name = "Connection Refused";
                break;
            case RecoverableErrorType::DNS_RESOLUTION:
                type_name = "DNS Resolution";
                break;
            case RecoverableErrorType::TEMPORARY_FAILURE:
                type_name = "Temporary Failure";
                break;
            case RecoverableErrorType::CUSTOM:
                type_name = "Custom";
                break;
            default:
                type_name = "Other";
                break;
        }
        std::cout << "  " << type_name << ": " << count << "\n";
    }
    
    // EN: Calculate overall success rate
    // FR: Calcule le taux de succès global
    double success_rate = 0.0;
    if (stats.total_operations > 0) {
        success_rate = (double)stats.successful_operations / stats.total_operations * 100.0;
    }
    
    std::cout << "\nOverall Success Rate: " << success_rate << "%\n";
    
    if (success_rate > 70.0) {
        std::cout << "\n=== Error Recovery Integration Test Completed Successfully ===\n";
        std::cout << "Error recovery system is working correctly for network failures!\n";
        return 0;
    } else {
        std::cout << "\n=== Error Recovery Integration Test Failed ===\n";
        std::cout << "Success rate too low: " << success_rate << "%\n";
        return 1;
    }
}