#include "../src/include/core/logger.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

void testBasicLogging() {
    std::cout << "=== Test Basic Logging ===" << std::endl;
    
    auto& logger = BBP::Logger::getInstance();
    logger.setLogLevel(BBP::LogLevel::DEBUG);
    
    LOG_DEBUG("test_module", "Debug message test");
    LOG_INFO("test_module", "Info message test");
    LOG_WARN("test_module", "Warning message test");
    LOG_ERROR("test_module", "Error message test");
    
    std::cout << "✓ Basic logging test passed" << std::endl;
}

void testLogLevels() {
    std::cout << "\n=== Test Log Levels ===" << std::endl;
    
    auto& logger = BBP::Logger::getInstance();
    
    logger.setLogLevel(BBP::LogLevel::WARN);
    std::cout << "Log level set to WARN - should only see WARN and ERROR:" << std::endl;
    
    LOG_DEBUG("level_test", "This debug should NOT appear");
    LOG_INFO("level_test", "This info should NOT appear");
    LOG_WARN("level_test", "This warning SHOULD appear");
    LOG_ERROR("level_test", "This error SHOULD appear");
    
    logger.setLogLevel(BBP::LogLevel::DEBUG);
    
    std::cout << "✓ Log levels test passed" << std::endl;
}

void testCorrelationId() {
    std::cout << "\n=== Test Correlation ID ===" << std::endl;
    
    auto& logger = BBP::Logger::getInstance();
    
    std::string correlation_id = logger.generateCorrelationId();
    std::cout << "Generated correlation ID: " << correlation_id << std::endl;
    
    assert(correlation_id.length() > 30);
    assert(correlation_id.find('-') != std::string::npos);
    
    logger.setCorrelationId(correlation_id);
    LOG_INFO("corr_test", "Message with correlation ID");
    
    std::cout << "✓ Correlation ID test passed" << std::endl;
}

void testMetadata() {
    std::cout << "\n=== Test Metadata ===" << std::endl;
    
    auto& logger = BBP::Logger::getInstance();
    
    logger.addGlobalMetadata("version", "1.0.0");
    logger.addGlobalMetadata("environment", "test");
    
    std::unordered_map<std::string, std::string> local_metadata = {
        {"user_id", "12345"},
        {"action", "login"}
    };
    
    LOG_INFO_META("meta_test", "Message with metadata", local_metadata);
    
    std::cout << "✓ Metadata test passed" << std::endl;
}

void testFileLogging() {
    std::cout << "\n=== Test File Logging ===" << std::endl;
    
    auto& logger = BBP::Logger::getInstance();
    std::string log_file = "/tmp/bbp_test.log";
    
    logger.setOutputFile(log_file);
    
    LOG_INFO("file_test", "This message should be written to file");
    LOG_ERROR("file_test", "This error should also be written to file");
    
    logger.flush();
    
    std::ifstream file(log_file);
    assert(file.is_open());
    
    std::string line;
    int line_count = 0;
    while (std::getline(file, line)) {
        line_count++;
        assert(line.find("\"level\":") != std::string::npos);
        assert(line.find("\"timestamp\":") != std::string::npos);
        assert(line.find("\"module\":\"file_test\"") != std::string::npos);
    }
    file.close();
    
    assert(line_count >= 2);
    
    std::remove(log_file.c_str());
    
    std::cout << "✓ File logging test passed" << std::endl;
}

void testNDJSONFormat() {
    std::cout << "\n=== Test NDJSON Format ===" << std::endl;
    
    auto& logger = BBP::Logger::getInstance();
    std::string log_file = "/tmp/bbp_ndjson_test.log";
    
    logger.setOutputFile(log_file);
    logger.setCorrelationId("test-correlation-id");
    
    std::unordered_map<std::string, std::string> metadata = {
        {"url", "https://example.com"},
        {"status_code", "200"}
    };
    
    LOG_INFO_META("ndjson_test", "Testing NDJSON format", metadata);
    logger.flush();
    
    std::ifstream file(log_file);
    std::string json_line;
    std::getline(file, json_line);
    file.close();
    
    assert(json_line.find("\"timestamp\":") != std::string::npos);
    assert(json_line.find("\"level\":\"INFO\"") != std::string::npos);
    assert(json_line.find("\"message\":\"Testing NDJSON format\"") != std::string::npos);
    assert(json_line.find("\"module\":\"ndjson_test\"") != std::string::npos);
    assert(json_line.find("\"correlation_id\":\"test-correlation-id\"") != std::string::npos);
    assert(json_line.find("\"url\":\"https://example.com\"") != std::string::npos);
    assert(json_line.find("\"status_code\":\"200\"") != std::string::npos);
    assert(json_line.find("\"thread_id\":") != std::string::npos);
    
    std::remove(log_file.c_str());
    
    std::cout << "✓ NDJSON format test passed" << std::endl;
}

void testThreadSafety() {
    std::cout << "\n=== Test Thread Safety ===" << std::endl;
    
    auto& logger = BBP::Logger::getInstance();
    std::string log_file = "/tmp/bbp_thread_test.log";
    logger.setOutputFile(log_file);
    
    const int num_threads = 5;
    const int messages_per_thread = 10;
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&logger, i, messages_per_thread]() {
            for (int j = 0; j < messages_per_thread; ++j) {
                std::string msg = "Thread " + std::to_string(i) + " message " + std::to_string(j);
                LOG_INFO("thread_test", msg);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    logger.flush();
    
    std::ifstream file(log_file);
    std::string line;
    int total_lines = 0;
    while (std::getline(file, line)) {
        total_lines++;
        assert(line.find("\"module\":\"thread_test\"") != std::string::npos);
    }
    file.close();
    
    assert(total_lines == num_threads * messages_per_thread);
    
    std::remove(log_file.c_str());
    
    std::cout << "✓ Thread safety test passed" << std::endl;
}

int main() {
    std::cout << "Running Logger System Tests...\n" << std::endl;
    
    try {
        testBasicLogging();
        testLogLevels();
        testCorrelationId();
        testMetadata();
        testFileLogging();
        testNDJSONFormat();
        testThreadSafety();
        
        std::cout << "\n🎉 All Logger System tests passed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}