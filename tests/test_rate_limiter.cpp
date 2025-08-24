#include "../src/include/core/rate_limiter.hpp"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

void testBasicTokenBucket() {
    std::cout << "=== Test Basic Token Bucket ===" << std::endl;
    
    auto& limiter = BBP::RateLimiter::getInstance();
    limiter.reset();
    
    limiter.setBucketConfig("test.com", 5.0, 10.0); // 5 req/s, burst 10
    
    for (int i = 0; i < 10; ++i) {
        assert(limiter.tryAcquire("test.com", 1));
    }
    
    assert(!limiter.tryAcquire("test.com", 1));
    
    auto stats = limiter.getStats("test.com");
    std::cout << "Stats: total=" << stats.total_requests 
              << ", denied=" << stats.denied_requests 
              << ", tokens=" << stats.current_tokens << std::endl;
    assert(stats.total_requests == 11);
    assert(stats.denied_requests == 1);
    assert(stats.current_tokens < 1.0); // Should be close to 0, allowing for minimal refill
    
    std::cout << "✓ Basic token bucket test passed" << std::endl;
}

void testTokenRefill() {
    std::cout << "\n=== Test Token Refill ===" << std::endl;
    
    auto& limiter = BBP::RateLimiter::getInstance();
    limiter.reset();
    
    limiter.setBucketConfig("refill.com", 10.0, 5.0); // 10 req/s, burst 5
    
    for (int i = 0; i < 5; ++i) {
        assert(limiter.tryAcquire("refill.com", 1));
    }
    assert(!limiter.tryAcquire("refill.com", 1));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(600)); // 0.6s should add ~6 tokens
    
    for (int i = 0; i < 5; ++i) {
        assert(limiter.tryAcquire("refill.com", 1));
    }
    
    std::cout << "✓ Token refill test passed" << std::endl;
}

void testAdaptiveBackoff() {
    std::cout << "\n=== Test Adaptive Backoff ===" << std::endl;
    
    auto& limiter = BBP::RateLimiter::getInstance();
    limiter.reset();
    
    BBP::RateLimiter::BackoffConfig config;
    config.initial_delay_ms = 100.0;
    config.max_delay_ms = 1000.0;
    config.multiplier = 2.0;
    
    limiter.setBackoffConfig("backoff.com", config);
    limiter.setBucketConfig("backoff.com", 1.0, 1.0);
    
    limiter.reportFailure("backoff.com");
    assert(limiter.getCurrentDelay("backoff.com") == 100.0);
    assert(limiter.isRateLimited("backoff.com"));
    
    limiter.reportFailure("backoff.com");
    assert(limiter.getCurrentDelay("backoff.com") == 200.0);
    
    limiter.reportFailure("backoff.com");
    assert(limiter.getCurrentDelay("backoff.com") == 400.0);
    
    limiter.reportSuccess("backoff.com");
    assert(limiter.getCurrentDelay("backoff.com") == 200.0);
    
    limiter.resetBackoff("backoff.com");
    assert(limiter.getCurrentDelay("backoff.com") == 0.0);
    assert(!limiter.isRateLimited("backoff.com"));
    
    std::cout << "✓ Adaptive backoff test passed" << std::endl;
}

void testWaitTime() {
    std::cout << "\n=== Test Wait Time Calculation ===" << std::endl;
    
    auto& limiter = BBP::RateLimiter::getInstance();
    limiter.reset();
    
    limiter.setBucketConfig("wait.com", 2.0, 2.0); // 2 req/s, burst 2
    
    assert(limiter.getWaitTime("wait.com", 1) == std::chrono::milliseconds(0));
    
    limiter.tryAcquire("wait.com", 1);
    limiter.tryAcquire("wait.com", 1);
    
    auto wait_time = limiter.getWaitTime("wait.com", 1);
    assert(wait_time.count() > 0);
    assert(wait_time.count() <= 600); // Should be around 500ms (1/2 req/s)
    
    std::cout << "✓ Wait time calculation test passed" << std::endl;
}

void testGlobalRateLimit() {
    std::cout << "\n=== Test Global Rate Limit ===" << std::endl;
    
    auto& limiter = BBP::RateLimiter::getInstance();
    limiter.reset();
    
    limiter.setGlobalRateLimit(5.0); // 5 req/s globally, burst ~10
    limiter.setBucketConfig("global1.com", 10.0, 10.0);
    limiter.setBucketConfig("global2.com", 10.0, 10.0);
    
    // Consume all global tokens
    for (int i = 0; i < 10; ++i) {
        limiter.tryAcquire("global1.com", 1);
    }
    
    // Should be denied due to global limit
    assert(!limiter.tryAcquire("global2.com", 1));
    
    std::cout << "✓ Global rate limit test passed" << std::endl;
}

void testConcurrentAccess() {
    std::cout << "\n=== Test Concurrent Access ===" << std::endl;
    
    auto& limiter = BBP::RateLimiter::getInstance();
    limiter.reset();
    
    limiter.setBucketConfig("concurrent.com", 10.0, 20.0);
    
    const int num_threads = 4;
    const int requests_per_thread = 10;
    std::vector<std::thread> threads;
    std::vector<int> successful_requests(num_threads, 0);
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&limiter, &successful_requests, i, requests_per_thread]() {
            for (int j = 0; j < requests_per_thread; ++j) {
                if (limiter.tryAcquire("concurrent.com", 1)) {
                    successful_requests[i]++;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    int total_successful = 0;
    for (int count : successful_requests) {
        total_successful += count;
    }
    
    assert(total_successful <= 20); // Should not exceed burst capacity
    
    auto stats = limiter.getStats("concurrent.com");
    assert(stats.total_requests == num_threads * requests_per_thread);
    
    std::cout << "✓ Concurrent access test passed" << std::endl;
}

void testBackoffDecay() {
    std::cout << "\n=== Test Backoff Decay Over Time ===" << std::endl;
    
    auto& limiter = BBP::RateLimiter::getInstance();
    limiter.reset();
    
    BBP::RateLimiter::BackoffConfig config;
    config.initial_delay_ms = 200.0;
    
    limiter.setBackoffConfig("decay.com", config);
    limiter.setBucketConfig("decay.com", 1.0, 1.0);
    
    limiter.reportFailure("decay.com");
    double initial_delay = limiter.getCurrentDelay("decay.com");
    assert(initial_delay == 200.0);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(150)); // Still within backoff period
    
    assert(!limiter.tryAcquire("decay.com", 1)); // Should still be blocked
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Total 250ms, should clear backoff
    
    assert(limiter.tryAcquire("decay.com", 1)); // Should work now
    
    std::cout << "✓ Backoff decay test passed" << std::endl;
}

void testPerformance() {
    std::cout << "\n=== Test Performance ===" << std::endl;
    
    auto& limiter = BBP::RateLimiter::getInstance();
    limiter.reset();
    
    limiter.setBucketConfig("perf.com", 1000.0, 1000.0);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    const int iterations = 10000;
    for (int i = 0; i < iterations; ++i) {
        limiter.tryAcquire("perf.com", 1);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double ops_per_second = (iterations * 1000000.0) / duration.count();
    std::cout << "Rate limiter performance: " << ops_per_second << " ops/sec" << std::endl;
    
    assert(ops_per_second > 100000); // Should handle at least 100k ops/sec
    
    std::cout << "✓ Performance test passed" << std::endl;
}

int main() {
    std::cout << "Running Rate Limiter Tests...\n" << std::endl;
    
    try {
        testBasicTokenBucket();
        testTokenRefill();
        testAdaptiveBackoff();
        testWaitTime();
        testGlobalRateLimit();
        testConcurrentAccess();
        testBackoffDecay();
        testPerformance();
        
        std::cout << "\n🎉 All Rate Limiter tests passed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}