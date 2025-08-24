#include "core/rate_limiter.hpp"
#include "core/logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    auto& logger = BBP::Logger::getInstance();
    auto& limiter = BBP::RateLimiter::getInstance();
    
    logger.setLogLevel(BBP::LogLevel::DEBUG);
    std::string correlation_id = logger.generateCorrelationId();
    logger.setCorrelationId(correlation_id);
    
    LOG_INFO("rate_limiter_example", "Configuring rate limiter for example.com");
    
    // Configure rate limiting: 5 requests per second, burst capacity of 10
    limiter.setBucketConfig("example.com", 5.0, 10.0);
    
    // Configure adaptive backoff
    BBP::RateLimiter::BackoffConfig backoff_config;
    backoff_config.initial_delay_ms = 500.0;
    backoff_config.max_delay_ms = 5000.0;
    backoff_config.multiplier = 2.0;
    limiter.setBackoffConfig("example.com", backoff_config);
    
    // Set global rate limit
    limiter.setGlobalRateLimit(20.0);
    
    LOG_INFO("rate_limiter_example", "Starting rate limited requests simulation");
    
    for (int i = 0; i < 15; ++i) {
        if (limiter.tryAcquire("example.com", 1)) {
            LOG_INFO("rate_limiter_example", "Request " + std::to_string(i+1) + " allowed");
        } else {
            auto wait_time = limiter.getWaitTime("example.com", 1);
            LOG_WARN("rate_limiter_example", "Request " + std::to_string(i+1) + 
                    " denied, wait time: " + std::to_string(wait_time.count()) + "ms");
            
            // Wait if needed
            if (wait_time.count() > 0) {
                std::this_thread::sleep_for(wait_time);
                if (limiter.tryAcquire("example.com", 1)) {
                    LOG_INFO("rate_limiter_example", "Request " + std::to_string(i+1) + " allowed after wait");
                }
            }
        }
    }
    
    // Simulate failures and backoff
    LOG_INFO("rate_limiter_example", "Simulating failures and adaptive backoff");
    
    limiter.reportFailure("example.com");
    LOG_WARN("rate_limiter_example", "Failure reported, current delay: " + 
             std::to_string(limiter.getCurrentDelay("example.com")) + "ms");
    
    limiter.reportFailure("example.com");
    LOG_WARN("rate_limiter_example", "Second failure, current delay: " + 
             std::to_string(limiter.getCurrentDelay("example.com")) + "ms");
    
    limiter.reportSuccess("example.com");
    LOG_INFO("rate_limiter_example", "Success reported, current delay: " + 
             std::to_string(limiter.getCurrentDelay("example.com")) + "ms");
    
    // Show final stats
    auto stats = limiter.getStats("example.com");
    LOG_INFO("rate_limiter_example", "Final stats - Total: " + std::to_string(stats.total_requests) +
             ", Denied: " + std::to_string(stats.denied_requests) +
             ", Backoff triggered: " + std::to_string(stats.backoff_triggered) +
             ", Current tokens: " + std::to_string(stats.current_tokens));
    
    return 0;
}