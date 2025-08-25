#include "../src/include/core/cache_system.hpp"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

void testBasicCacheOperations() {
    std::cout << "=== Test Basic Cache Operations ===" << std::endl;
    
    auto& cache = BBP::CacheSystem::getInstance();
    cache.clear();
    
    // Test storing and retrieving
    std::unordered_map<std::string, std::string> headers = {
        {"content-type", "text/html"},
        {"etag", "\"123456\""},
        {"cache-control", "max-age=3600"}
    };
    
    std::string url = "https://example.com/test";
    std::string content = "<html><body>Test content</body></html>";
    
    cache.store(url, content, headers);
    
    assert(cache.has(url));
    
    auto cached_entry = cache.get(url);
    assert(cached_entry.has_value());
    assert(cached_entry->content == content);
    assert(cached_entry->etag.has_value());
    assert(*cached_entry->etag == "\"123456\"");
    
    std::cout << "✓ Basic cache operations test passed" << std::endl;
}

void testCacheConfiguration() {
    std::cout << "\n=== Test Cache Configuration ===" << std::endl;
    
    auto& cache = BBP::CacheSystem::getInstance();
    cache.clear();
    
    BBP::CacheConfig config;
    config.default_ttl = std::chrono::seconds(1800); // 30 minutes
    config.max_entries = 5000;
    config.enable_compression = true;
    config.enable_stale_while_revalidate = true;
    
    cache.configure(config);
    
    const auto& current_config = cache.getConfig();
    assert(current_config.default_ttl == std::chrono::seconds(1800));
    assert(current_config.max_entries == 5000);
    assert(current_config.enable_compression == true);
    assert(current_config.enable_stale_while_revalidate == true);
    
    std::cout << "✓ Cache configuration test passed" << std::endl;
}

void testETagValidation() {
    std::cout << "\n=== Test ETag Validation ===" << std::endl;
    
    auto& cache = BBP::CacheSystem::getInstance();
    cache.clear();
    
    std::string url = "https://example.com/etag-test";
    std::string content = "Content with ETag";
    
    std::unordered_map<std::string, std::string> headers = {
        {"etag", "\"abc123\""},
        {"cache-control", "max-age=60"}
    };
    
    cache.store(url, content, headers);
    
    // Test conditional headers generation
    auto conditional_headers = cache.getConditionalHeaders(url);
    assert(conditional_headers.find("If-None-Match") != conditional_headers.end());
    assert(conditional_headers["If-None-Match"] == "\"abc123\"");
    
    // Test validation with same ETag (fresh)
    std::unordered_map<std::string, std::string> response_headers = {
        {"etag", "\"abc123\""}
    };
    
    auto validation_result = cache.validate(url, response_headers);
    assert(validation_result == BBP::ValidationResult::FRESH);
    
    // Test validation with different ETag (modified)
    response_headers["etag"] = "\"def456\"";
    validation_result = cache.validate(url, response_headers);
    assert(validation_result == BBP::ValidationResult::MODIFIED);
    
    std::cout << "✓ ETag validation test passed" << std::endl;
}

void testLastModifiedValidation() {
    std::cout << "\n=== Test Last-Modified Validation ===" << std::endl;
    
    auto& cache = BBP::CacheSystem::getInstance();
    cache.clear();
    
    std::string url = "https://example.com/lastmod-test";
    std::string content = "Content with Last-Modified";
    
    std::unordered_map<std::string, std::string> headers = {
        {"last-modified", "Wed, 21 Oct 2015 07:28:00 GMT"},
        {"cache-control", "max-age=60"}
    };
    
    cache.store(url, content, headers);
    
    // Test conditional headers generation
    auto conditional_headers = cache.getConditionalHeaders(url);
    assert(conditional_headers.find("If-Modified-Since") != conditional_headers.end());
    assert(conditional_headers["If-Modified-Since"] == "Wed, 21 Oct 2015 07:28:00 GMT");
    
    // Test validation with same Last-Modified (fresh)
    std::unordered_map<std::string, std::string> response_headers = {
        {"last-modified", "Wed, 21 Oct 2015 07:28:00 GMT"}
    };
    
    auto validation_result = cache.validate(url, response_headers);
    assert(validation_result == BBP::ValidationResult::FRESH);
    
    // Test validation with different Last-Modified (modified)
    response_headers["last-modified"] = "Thu, 22 Oct 2015 07:28:00 GMT";
    validation_result = cache.validate(url, response_headers);
    assert(validation_result == BBP::ValidationResult::MODIFIED);
    
    std::cout << "✓ Last-Modified validation test passed" << std::endl;
}

void testTTLExpiration() {
    std::cout << "\n=== Test TTL Expiration ===" << std::endl;
    
    auto& cache = BBP::CacheSystem::getInstance();
    cache.clear();
    
    BBP::CacheConfig config;
    config.default_ttl = std::chrono::seconds(1);
    config.min_ttl = std::chrono::seconds(1);  // Allow 1 second TTL
    config.enable_stale_while_revalidate = false;
    cache.configure(config);
    
    std::string url = "https://example.com/ttl-test";
    std::string content = "Content that expires quickly";
    
    std::unordered_map<std::string, std::string> headers = {
        {"cache-control", "max-age=1"}
    };
    
    cache.store(url, content, headers);
    
    // Should be available immediately
    auto cached_entry = cache.get(url);
    assert(cached_entry.has_value());
    
    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Should be expired and not available
    cached_entry = cache.get(url);
    assert(!cached_entry.has_value());
    
    std::cout << "✓ TTL expiration test passed" << std::endl;
}

void testStaleWhileRevalidate() {
    std::cout << "\n=== Test Stale-While-Revalidate ===" << std::endl;
    
    auto& cache = BBP::CacheSystem::getInstance();
    cache.clear();
    
    BBP::CacheConfig config;
    config.default_ttl = std::chrono::seconds(1);
    config.min_ttl = std::chrono::seconds(1);  // Allow 1 second TTL
    config.enable_stale_while_revalidate = true;
    config.stale_max_age = std::chrono::seconds(5);
    cache.configure(config);
    
    std::string url = "https://example.com/stale-test";
    std::string content = "Stale content test";
    
    std::unordered_map<std::string, std::string> headers = {
        {"cache-control", "max-age=1"}
    };
    
    cache.store(url, content, headers);
    
    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Should be available as stale
    auto cached_entry = cache.get(url);
    assert(cached_entry.has_value());
    assert(cached_entry->is_stale == true);
    
    std::cout << "✓ Stale-while-revalidate test passed" << std::endl;
}

void testCacheStats() {
    std::cout << "\n=== Test Cache Statistics ===" << std::endl;
    
    auto& cache = BBP::CacheSystem::getInstance();
    cache.clear();
    
    std::string url1 = "https://example.com/stats-test-1";
    std::string url2 = "https://example.com/stats-test-2";
    std::string content = "Stats test content";
    
    std::unordered_map<std::string, std::string> headers = {
        {"cache-control", "max-age=3600"}
    };
    
    // Store entries
    cache.store(url1, content, headers);
    cache.store(url2, content, headers);
    
    // Generate hits and misses
    cache.get(url1); // hit
    cache.get(url1); // hit
    cache.get("https://example.com/nonexistent"); // miss
    
    auto stats = cache.getStats();
    assert(stats.entries_count == 2);
    assert(stats.cache_hits == 2);
    assert(stats.cache_misses == 1);
    assert(stats.total_requests == 3);
    assert(stats.hit_ratio > 0.6); // Should be 2/3 ≈ 0.67
    
    std::cout << "Cache stats: " << stats.entries_count << " entries, " 
              << stats.cache_hits << " hits, " << stats.cache_misses << " misses, "
              << "hit ratio: " << stats.hit_ratio << std::endl;
    
    std::cout << "✓ Cache statistics test passed" << std::endl;
}

void testLRUEviction() {
    std::cout << "\n=== Test LRU Eviction ===" << std::endl;
    
    auto& cache = BBP::CacheSystem::getInstance();
    cache.clear();
    
    BBP::CacheConfig config;
    config.max_entries = 3;
    cache.configure(config);
    
    std::unordered_map<std::string, std::string> headers = {
        {"cache-control", "max-age=3600"}
    };
    
    // Fill cache to capacity
    cache.store("https://example.com/lru-1", "Content 1", headers);
    cache.store("https://example.com/lru-2", "Content 2", headers);
    cache.store("https://example.com/lru-3", "Content 3", headers);
    
    // Access entry 1 to make it recently used
    cache.get("https://example.com/lru-1");
    
    // Add one more entry (should evict LRU which is entry 2)
    cache.store("https://example.com/lru-4", "Content 4", headers);
    
    // Entry 2 should be evicted
    assert(!cache.has("https://example.com/lru-2"));
    assert(cache.has("https://example.com/lru-1"));
    assert(cache.has("https://example.com/lru-3"));
    assert(cache.has("https://example.com/lru-4"));
    
    auto stats = cache.getStats();
    assert(stats.entries_count == 3);
    assert(stats.evictions >= 1);
    
    std::cout << "✓ LRU eviction test passed" << std::endl;
}

void testValidationUpdate() {
    std::cout << "\n=== Test Validation Update ===" << std::endl;
    
    auto& cache = BBP::CacheSystem::getInstance();
    cache.clear();
    
    std::string url = "https://example.com/validation-update";
    std::string content = "Original content";
    
    std::unordered_map<std::string, std::string> headers = {
        {"etag", "\"v1\""},
        {"cache-control", "max-age=60"}
    };
    
    cache.store(url, content, headers);
    
    // Update after validation with new headers
    std::unordered_map<std::string, std::string> new_headers = {
        {"etag", "\"v2\""},
        {"cache-control", "max-age=3600"}
    };
    
    cache.updateAfterValidation(url, new_headers);
    
    // Get updated conditional headers
    auto conditional_headers = cache.getConditionalHeaders(url);
    assert(conditional_headers["If-None-Match"] == "\"v2\"");
    
    std::cout << "✓ Validation update test passed" << std::endl;
}

void testEventCallback() {
    std::cout << "\n=== Test Event Callback ===" << std::endl;
    
    auto& cache = BBP::CacheSystem::getInstance();
    cache.clear();
    
    std::vector<std::pair<std::string, std::string>> events;
    
    cache.setEventCallback([&events](const std::string& event, const std::string& url) {
        events.emplace_back(event, url);
    });
    
    std::string url = "https://example.com/events";
    std::string content = "Event test content";
    
    std::unordered_map<std::string, std::string> headers = {
        {"cache-control", "max-age=3600"}
    };
    
    cache.store(url, content, headers);
    cache.get(url); // Should trigger hit event
    cache.get("https://example.com/nonexistent"); // Should trigger miss event
    cache.remove(url); // Should trigger removed event
    
    assert(events.size() >= 3);
    
    bool found_store = false, found_hit = false, found_miss = false, found_removed = false;
    for (const auto& [event, event_url] : events) {
        if (event == "store") found_store = true;
        if (event == "hit") found_hit = true;
        if (event == "miss") found_miss = true;
        if (event == "removed") found_removed = true;
    }
    
    assert(found_store && found_hit && found_miss && found_removed);
    
    std::cout << "✓ Event callback test passed" << std::endl;
}

void testCleanup() {
    std::cout << "\n=== Test Cleanup ===" << std::endl;
    
    auto& cache = BBP::CacheSystem::getInstance();
    cache.clear();
    
    BBP::CacheConfig config;
    config.default_ttl = std::chrono::seconds(1);
    config.min_ttl = std::chrono::seconds(1);  // Allow 1 second TTL  
    config.enable_stale_while_revalidate = false;
    cache.configure(config);
    
    std::unordered_map<std::string, std::string> headers = {
        {"cache-control", "max-age=1"}
    };
    
    // Store multiple entries
    for (int i = 0; i < 5; ++i) {
        std::string url = "https://example.com/cleanup-" + std::to_string(i);
        cache.store(url, "Content " + std::to_string(i), headers);
    }
    
    assert(cache.getStats().entries_count == 5);
    
    // Wait for expiration
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Force cleanup
    size_t cleaned_count = cache.cleanup();
    assert(cleaned_count == 5);
    assert(cache.getStats().entries_count == 0);
    
    std::cout << "✓ Cleanup test passed" << std::endl;
}

int main() {
    std::cout << "Running Cache System Tests...\n" << std::endl;
    
    try {
        testBasicCacheOperations();
        testCacheConfiguration();
        testETagValidation();
        testLastModifiedValidation();
        testTTLExpiration();
        testStaleWhileRevalidate();
        testCacheStats();
        testLRUEviction();
        testValidationUpdate();
        testEventCallback();
        testCleanup();
        
        std::cout << "\n🎉 All Cache System tests passed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}