#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>
#include <atomic>
#include "csv/merger_engine.hpp"

using namespace BBP::CSV;
using namespace testing;

// Test fixture for MergerEngine tests
// Fixture de test pour les tests MergerEngine
class MergerEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test files
        // Créer un répertoire temporaire pour les fichiers de test
        test_dir = std::filesystem::temp_directory_path() / "merger_engine_test";
        std::filesystem::create_directories(test_dir);
    }
    
    void TearDown() override {
        // Clean up temporary files
        // Nettoyer les fichiers temporaires
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }
    
    // Helper function to create CSV file
    // Fonction d'aide pour créer un fichier CSV
    void createCSVFile(const std::string& filename, const std::vector<std::string>& lines) {
        std::ofstream file(test_dir / filename);
        for (const auto& line : lines) {
            file << line << "\n";
        }
    }
    
    // Helper function to read CSV file content
    // Fonction d'aide pour lire le contenu d'un fichier CSV
    std::string readFile(const std::string& filename) {
        std::ifstream file(test_dir / filename);
        return std::string(std::istreambuf_iterator<char>(file), 
                          std::istreambuf_iterator<char>());
    }
    
    std::filesystem::path test_dir;
};

// Tests for MergeConfig class
// Tests pour la classe MergeConfig
class MergeConfigTest : public ::testing::Test {};

TEST_F(MergeConfigTest, DefaultConfiguration) {
    MergeConfig config;
    EXPECT_EQ(config.strategy, MergeStrategy::APPEND);
    EXPECT_EQ(config.deduplication, DeduplicationStrategy::NONE);
    EXPECT_EQ(config.conflict_resolution, ConflictResolution::KEEP_FIRST);
    EXPECT_FALSE(config.preserve_order);
    EXPECT_TRUE(config.include_headers);
    EXPECT_EQ(config.fuzzy_threshold, 0.8);
    EXPECT_TRUE(config.key_columns.empty());
    EXPECT_EQ(config.delimiter, ',');
    EXPECT_EQ(config.quote_char, '"');
}

TEST_F(MergeConfigTest, ValidationValidConfig) {
    MergeConfig config;
    config.strategy = MergeStrategy::SMART_MERGE;
    config.deduplication = DeduplicationStrategy::KEY_BASED;
    config.key_columns = {"id", "name"};
    
    auto result = config.validate();
    EXPECT_EQ(result, MergeError::SUCCESS);
}

TEST_F(MergeConfigTest, ValidationInvalidFuzzyThreshold) {
    MergeConfig config;
    config.fuzzy_threshold = 1.5; // Invalid threshold > 1.0
    
    auto result = config.validate();
    EXPECT_EQ(result, MergeError::INVALID_CONFIG);
}

TEST_F(MergeConfigTest, ValidationKeyBasedWithoutColumns) {
    MergeConfig config;
    config.deduplication = DeduplicationStrategy::KEY_BASED;
    config.key_columns.clear(); // No key columns specified
    
    auto result = config.validate();
    EXPECT_EQ(result, MergeError::INVALID_CONFIG);
}

TEST_F(MergeConfigTest, ValidationTimeBasedWithoutColumn) {
    MergeConfig config;
    config.strategy = MergeStrategy::TIME_BASED;
    config.time_column.clear(); // No time column specified
    
    auto result = config.validate();
    EXPECT_EQ(result, MergeError::INVALID_CONFIG);
}

// Tests for MergeStatistics class
// Tests pour la classe MergeStatistics
class MergeStatisticsTest : public ::testing::Test {};

TEST_F(MergeStatisticsTest, InitialState) {
    MergeStatistics stats;
    EXPECT_EQ(stats.getTotalRows(), 0);
    EXPECT_EQ(stats.getDuplicatesFound(), 0);
    EXPECT_EQ(stats.getDuplicatesRemoved(), 0);
    EXPECT_EQ(stats.getConflictsResolved(), 0);
    EXPECT_EQ(stats.getSourcesProcessed(), 0);
    EXPECT_EQ(stats.getErrorsEncountered(), 0);
}

TEST_F(MergeStatisticsTest, IncrementOperations) {
    MergeStatistics stats;
    
    stats.incrementTotalRows(10);
    EXPECT_EQ(stats.getTotalRows(), 10);
    
    stats.incrementDuplicatesFound(5);
    EXPECT_EQ(stats.getDuplicatesFound(), 5);
    
    stats.incrementDuplicatesRemoved(3);
    EXPECT_EQ(stats.getDuplicatesRemoved(), 3);
    
    stats.incrementConflictsResolved(2);
    EXPECT_EQ(stats.getConflictsResolved(), 2);
    
    stats.incrementSourcesProcessed();
    EXPECT_EQ(stats.getSourcesProcessed(), 1);
    
    stats.incrementErrorsEncountered();
    EXPECT_EQ(stats.getErrorsEncountered(), 1);
}

TEST_F(MergeStatisticsTest, ThreadSafety) {
    MergeStatistics stats;
    const int num_threads = 10;
    const int increments_per_thread = 100;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&stats, increments_per_thread]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                stats.incrementTotalRows(1);
                stats.incrementDuplicatesFound(1);
                stats.incrementSourcesProcessed();
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(stats.getTotalRows(), num_threads * increments_per_thread);
    EXPECT_EQ(stats.getDuplicatesFound(), num_threads * increments_per_thread);
    EXPECT_EQ(stats.getSourcesProcessed(), num_threads * increments_per_thread);
}

TEST_F(MergeStatisticsTest, Reset) {
    MergeStatistics stats;
    stats.incrementTotalRows(10);
    stats.incrementDuplicatesFound(5);
    
    stats.reset();
    
    EXPECT_EQ(stats.getTotalRows(), 0);
    EXPECT_EQ(stats.getDuplicatesFound(), 0);
}

// Tests for DuplicateResolver class
// Tests pour la classe DuplicateResolver
class DuplicateResolverTest : public ::testing::Test {};

TEST_F(DuplicateResolverTest, ExactMatch) {
    DuplicateResolver resolver;
    
    std::vector<std::string> row1 = {"1", "John", "Doe"};
    std::vector<std::string> row2 = {"1", "John", "Doe"};
    std::vector<std::string> row3 = {"2", "Jane", "Smith"};
    
    EXPECT_TRUE(resolver.isExactMatch(row1, row2));
    EXPECT_FALSE(resolver.isExactMatch(row1, row3));
}

TEST_F(DuplicateResolverTest, KeyBasedMatch) {
    DuplicateResolver resolver;
    std::vector<std::string> headers = {"id", "name", "surname"};
    std::vector<std::string> key_columns = {"id"};
    
    std::vector<std::string> row1 = {"1", "John", "Doe"};
    std::vector<std::string> row2 = {"1", "Johnny", "Smith"};
    std::vector<std::string> row3 = {"2", "Jane", "Smith"};
    
    EXPECT_TRUE(resolver.isKeyBasedMatch(row1, row2, headers, key_columns));
    EXPECT_FALSE(resolver.isKeyBasedMatch(row1, row3, headers, key_columns));
}

TEST_F(DuplicateResolverTest, LevenshteinDistance) {
    DuplicateResolver resolver;
    
    EXPECT_EQ(resolver.levenshteinDistance("kitten", "sitting"), 3);
    EXPECT_EQ(resolver.levenshteinDistance("hello", "hello"), 0);
    EXPECT_EQ(resolver.levenshteinDistance("", "abc"), 3);
    EXPECT_EQ(resolver.levenshteinDistance("abc", ""), 3);
}

TEST_F(DuplicateResolverTest, JaccardSimilarity) {
    DuplicateResolver resolver;
    
    double similarity1 = resolver.jaccardSimilarity("hello", "hallo");
    EXPECT_NEAR(similarity1, 0.6, 0.1); // 3 out of 5 characters match
    
    double similarity2 = resolver.jaccardSimilarity("identical", "identical");
    EXPECT_NEAR(similarity2, 1.0, 0.01);
    
    double similarity3 = resolver.jaccardSimilarity("abc", "xyz");
    EXPECT_NEAR(similarity3, 0.0, 0.01);
}

TEST_F(DuplicateResolverTest, FuzzyMatch) {
    DuplicateResolver resolver;
    
    std::vector<std::string> row1 = {"1", "John Doe", "Engineer"};
    std::vector<std::string> row2 = {"1", "Jon Doe", "Engineer"};
    std::vector<std::string> row3 = {"2", "Jane Smith", "Designer"};
    
    EXPECT_TRUE(resolver.isFuzzyMatch(row1, row2, 0.8));
    EXPECT_FALSE(resolver.isFuzzyMatch(row1, row3, 0.8));
}

TEST_F(DuplicateResolverTest, ContentHash) {
    DuplicateResolver resolver;
    
    std::vector<std::string> row1 = {"1", "John", "Doe"};
    std::vector<std::string> row2 = {"1", "John", "Doe"};
    std::vector<std::string> row3 = {"2", "Jane", "Smith"};
    
    std::string hash1 = resolver.computeContentHash(row1);
    std::string hash2 = resolver.computeContentHash(row2);
    std::string hash3 = resolver.computeContentHash(row3);
    
    EXPECT_EQ(hash1, hash2);
    EXPECT_NE(hash1, hash3);
}

// Tests for MergerEngine class
// Tests pour la classe MergerEngine
TEST_F(MergerEngineTest, AppendStrategy) {
    createCSVFile("file1.csv", {
        "id,name,email",
        "1,John,john@example.com",
        "2,Jane,jane@example.com"
    });
    
    createCSVFile("file2.csv", {
        "id,name,email",
        "3,Bob,bob@example.com",
        "4,Alice,alice@example.com"
    });
    
    MergeConfig config;
    config.strategy = MergeStrategy::APPEND;
    
    std::vector<InputSource> sources = {
        {test_dir / "file1.csv", 1},
        {test_dir / "file2.csv", 1}
    };
    
    MergerEngine engine(config);
    std::ostringstream output;
    
    auto result = engine.merge(sources, output);
    EXPECT_EQ(result, MergeError::SUCCESS);
    
    std::string output_str = output.str();
    EXPECT_THAT(output_str, HasSubstr("id,name,email"));
    EXPECT_THAT(output_str, HasSubstr("1,John,john@example.com"));
    EXPECT_THAT(output_str, HasSubstr("3,Bob,bob@example.com"));
}

TEST_F(MergerEngineTest, SmartMergeWithDeduplication) {
    createCSVFile("file1.csv", {
        "id,name,email",
        "1,John,john@example.com",
        "2,Jane,jane@example.com"
    });
    
    createCSVFile("file2.csv", {
        "id,name,email",
        "1,John,john@example.com", // Duplicate
        "3,Bob,bob@example.com"
    });
    
    MergeConfig config;
    config.strategy = MergeStrategy::SMART_MERGE;
    config.deduplication = DeduplicationStrategy::EXACT_MATCH;
    
    std::vector<InputSource> sources = {
        {test_dir / "file1.csv", 1},
        {test_dir / "file2.csv", 1}
    };
    
    MergerEngine engine(config);
    std::ostringstream output;
    
    auto result = engine.merge(sources, output);
    EXPECT_EQ(result, MergeError::SUCCESS);
    
    std::string output_str = output.str();
    size_t john_count = 0;
    size_t pos = 0;
    while ((pos = output_str.find("John", pos)) != std::string::npos) {
        john_count++;
        pos++;
    }
    EXPECT_EQ(john_count, 1); // Should appear only once due to deduplication
}

TEST_F(MergerEngineTest, PriorityMerge) {
    createCSVFile("file1.csv", {
        "id,name,email",
        "1,John,john@example.com"
    });
    
    createCSVFile("file2.csv", {
        "id,name,email",
        "1,Johnny,johnny@example.com"
    });
    
    MergeConfig config;
    config.strategy = MergeStrategy::PRIORITY_MERGE;
    config.deduplication = DeduplicationStrategy::KEY_BASED;
    config.key_columns = {"id"};
    config.conflict_resolution = ConflictResolution::KEEP_FIRST;
    
    std::vector<InputSource> sources = {
        {test_dir / "file1.csv", 2}, // Higher priority
        {test_dir / "file2.csv", 1}  // Lower priority
    };
    
    MergerEngine engine(config);
    std::ostringstream output;
    
    auto result = engine.merge(sources, output);
    EXPECT_EQ(result, MergeError::SUCCESS);
    
    std::string output_str = output.str();
    EXPECT_THAT(output_str, HasSubstr("John")); // Should keep higher priority record
    EXPECT_THAT(output_str, Not(HasSubstr("Johnny")));
}

TEST_F(MergerEngineTest, TimeBasedMerge) {
    createCSVFile("file1.csv", {
        "id,name,timestamp",
        "1,John,2024-01-01T10:00:00Z"
    });
    
    createCSVFile("file2.csv", {
        "id,name,timestamp",
        "1,Johnny,2024-01-02T10:00:00Z" // Newer timestamp
    });
    
    MergeConfig config;
    config.strategy = MergeStrategy::TIME_BASED;
    config.deduplication = DeduplicationStrategy::KEY_BASED;
    config.key_columns = {"id"};
    config.time_column = "timestamp";
    config.conflict_resolution = ConflictResolution::KEEP_NEWEST;
    
    std::vector<InputSource> sources = {
        {test_dir / "file1.csv", 1},
        {test_dir / "file2.csv", 1}
    };
    
    MergerEngine engine(config);
    std::ostringstream output;
    
    auto result = engine.merge(sources, output);
    EXPECT_EQ(result, MergeError::SUCCESS);
    
    std::string output_str = output.str();
    EXPECT_THAT(output_str, HasSubstr("Johnny")); // Should keep newer record
    EXPECT_THAT(output_str, Not(HasSubstr("John,")));
}

TEST_F(MergerEngineTest, SchemaAwareMerge) {
    createCSVFile("file1.csv", {
        "id,name,email",
        "1,John,john@example.com"
    });
    
    createCSVFile("file2.csv", {
        "id,name,phone",
        "2,Jane,+1234567890"
    });
    
    MergeConfig config;
    config.strategy = MergeStrategy::SCHEMA_AWARE;
    
    std::vector<InputSource> sources = {
        {test_dir / "file1.csv", 1},
        {test_dir / "file2.csv", 1}
    };
    
    MergerEngine engine(config);
    std::ostringstream output;
    
    auto result = engine.merge(sources, output);
    EXPECT_EQ(result, MergeError::SUCCESS);
    
    std::string output_str = output.str();
    EXPECT_THAT(output_str, HasSubstr("id,name,email,phone")); // Unified schema
    EXPECT_THAT(output_str, HasSubstr("1,John,john@example.com,"));
    EXPECT_THAT(output_str, HasSubstr("2,Jane,,+1234567890"));
}

TEST_F(MergerEngineTest, ErrorHandlingFileNotFound) {
    MergeConfig config;
    std::vector<InputSource> sources = {
        {test_dir / "nonexistent.csv", 1}
    };
    
    MergerEngine engine(config);
    std::ostringstream output;
    
    auto result = engine.merge(sources, output);
    EXPECT_EQ(result, MergeError::FILE_NOT_FOUND);
}

TEST_F(MergerEngineTest, ErrorHandlingInvalidCSV) {
    createCSVFile("invalid.csv", {
        "id,name,email",
        "1,John", // Missing field
        "2,Jane,jane@example.com"
    });
    
    MergeConfig config;
    std::vector<InputSource> sources = {
        {test_dir / "invalid.csv", 1}
    };
    
    MergerEngine engine(config);
    std::ostringstream output;
    
    auto result = engine.merge(sources, output);
    // Should handle gracefully and continue processing
    EXPECT_EQ(result, MergeError::SUCCESS);
}

TEST_F(MergerEngineTest, Statistics) {
    createCSVFile("file1.csv", {
        "id,name",
        "1,John",
        "2,Jane"
    });
    
    createCSVFile("file2.csv", {
        "id,name",
        "1,John", // Duplicate
        "3,Bob"
    });
    
    MergeConfig config;
    config.strategy = MergeStrategy::SMART_MERGE;
    config.deduplication = DeduplicationStrategy::EXACT_MATCH;
    
    std::vector<InputSource> sources = {
        {test_dir / "file1.csv", 1},
        {test_dir / "file2.csv", 1}
    };
    
    MergerEngine engine(config);
    std::ostringstream output;
    
    engine.merge(sources, output);
    
    const auto& stats = engine.getStatistics();
    EXPECT_EQ(stats.getSourcesProcessed(), 2);
    EXPECT_GT(stats.getTotalRows(), 0);
    EXPECT_GT(stats.getDuplicatesFound(), 0);
}

// Tests for MergeUtils namespace
// Tests pour le namespace MergeUtils
class MergeUtilsTest : public ::testing::Test {};

TEST_F(MergeUtilsTest, ParseCSVLine) {
    std::string line = "field1,\"field2 with spaces\",field3";
    auto fields = MergeUtils::parseCSVLine(line, ',', '"');
    
    EXPECT_EQ(fields.size(), 3);
    EXPECT_EQ(fields[0], "field1");
    EXPECT_EQ(fields[1], "field2 with spaces");
    EXPECT_EQ(fields[2], "field3");
}

TEST_F(MergeUtilsTest, ParseCSVLineWithQuotedComma) {
    std::string line = "field1,\"field2, with comma\",field3";
    auto fields = MergeUtils::parseCSVLine(line, ',', '"');
    
    EXPECT_EQ(fields.size(), 3);
    EXPECT_EQ(fields[0], "field1");
    EXPECT_EQ(fields[1], "field2, with comma");
    EXPECT_EQ(fields[2], "field3");
}

TEST_F(MergeUtilsTest, EscapeCSVField) {
    EXPECT_EQ(MergeUtils::escapeCSVField("simple", ',', '"'), "simple");
    EXPECT_EQ(MergeUtils::escapeCSVField("with,comma", ',', '"'), "\"with,comma\"");
    EXPECT_EQ(MergeUtils::escapeCSVField("with\"quote", ',', '"'), "\"with\"\"quote\"");
    EXPECT_EQ(MergeUtils::escapeCSVField("with\nnewline", ',', '"'), "\"with\nnewline\"");
}

TEST_F(MergeUtilsTest, FormatCSVLine) {
    std::vector<std::string> fields = {"field1", "field with space", "field,with,comma"};
    std::string line = MergeUtils::formatCSVLine(fields, ',', '"');
    EXPECT_EQ(line, "field1,\"field with space\",\"field,with,comma\"");
}

TEST_F(MergeUtilsTest, Trim) {
    EXPECT_EQ(MergeUtils::trim("  hello  "), "hello");
    EXPECT_EQ(MergeUtils::trim("hello"), "hello");
    EXPECT_EQ(MergeUtils::trim("  "), "");
    EXPECT_EQ(MergeUtils::trim(""), "");
}

TEST_F(MergeUtilsTest, ToLower) {
    EXPECT_EQ(MergeUtils::toLower("HELLO"), "hello");
    EXPECT_EQ(MergeUtils::toLower("Hello World"), "hello world");
    EXPECT_EQ(MergeUtils::toLower("123ABC"), "123abc");
}

TEST_F(MergeUtilsTest, FindColumnIndex) {
    std::vector<std::string> headers = {"id", "name", "email"};
    
    EXPECT_EQ(MergeUtils::findColumnIndex(headers, "id"), 0);
    EXPECT_EQ(MergeUtils::findColumnIndex(headers, "name"), 1);
    EXPECT_EQ(MergeUtils::findColumnIndex(headers, "email"), 2);
    EXPECT_EQ(MergeUtils::findColumnIndex(headers, "nonexistent"), -1);
}

TEST_F(MergeUtilsTest, IsValidTimestamp) {
    EXPECT_TRUE(MergeUtils::isValidTimestamp("2024-01-01T10:00:00Z"));
    EXPECT_TRUE(MergeUtils::isValidTimestamp("2024-12-31T23:59:59Z"));
    EXPECT_FALSE(MergeUtils::isValidTimestamp("invalid-timestamp"));
    EXPECT_FALSE(MergeUtils::isValidTimestamp("2024-13-01T10:00:00Z")); // Invalid month
}

TEST_F(MergeUtilsTest, CompareTimestamps) {
    std::string ts1 = "2024-01-01T10:00:00Z";
    std::string ts2 = "2024-01-02T10:00:00Z";
    std::string ts3 = "2024-01-01T10:00:00Z";
    
    EXPECT_LT(MergeUtils::compareTimestamps(ts1, ts2), 0); // ts1 < ts2
    EXPECT_GT(MergeUtils::compareTimestamps(ts2, ts1), 0); // ts2 > ts1
    EXPECT_EQ(MergeUtils::compareTimestamps(ts1, ts3), 0); // ts1 == ts3
}

// Integration tests
// Tests d'intégration
class MergerEngineIntegrationTest : public MergerEngineTest {};

TEST_F(MergerEngineIntegrationTest, ComplexRealWorldScenario) {
    // Create multiple CSV files with overlapping data
    // Créer plusieurs fichiers CSV avec des données qui se chevauchent
    createCSVFile("subdomains.csv", {
        "schema_ver,domain,subdomain,source,timestamp",
        "1,example.com,www.example.com,subfinder,2024-01-01T10:00:00Z",
        "1,example.com,api.example.com,subfinder,2024-01-01T10:01:00Z"
    });
    
    createCSVFile("probe.csv", {
        "schema_ver,url,status_code,title,tech_stack,timestamp",
        "1,https://www.example.com,200,Example Site,nginx,2024-01-01T11:00:00Z",
        "1,https://api.example.com,200,API,express,2024-01-01T11:01:00Z"
    });
    
    createCSVFile("discovery.csv", {
        "schema_ver,url,path,status_code,content_length,timestamp",
        "1,https://www.example.com,/admin,403,1024,2024-01-01T12:00:00Z",
        "1,https://api.example.com,/v1/users,200,2048,2024-01-01T12:01:00Z"
    });
    
    MergeConfig config;
    config.strategy = MergeStrategy::SCHEMA_AWARE;
    config.deduplication = DeduplicationStrategy::KEY_BASED;
    config.key_columns = {"url"};
    config.conflict_resolution = ConflictResolution::KEEP_NEWEST;
    config.time_column = "timestamp";
    
    std::vector<InputSource> sources = {
        {test_dir / "subdomains.csv", 3},
        {test_dir / "probe.csv", 2},
        {test_dir / "discovery.csv", 1}
    };
    
    MergerEngine engine(config);
    std::ostringstream output;
    
    auto result = engine.merge(sources, output);
    EXPECT_EQ(result, MergeError::SUCCESS);
    
    std::string output_str = output.str();
    
    // Verify unified schema
    // Vérifier le schéma unifié
    EXPECT_THAT(output_str, HasSubstr("schema_ver"));
    EXPECT_THAT(output_str, HasSubstr("timestamp"));
    
    // Verify data from all sources is present
    // Vérifier que les données de toutes les sources sont présentes
    EXPECT_THAT(output_str, HasSubstr("www.example.com"));
    EXPECT_THAT(output_str, HasSubstr("api.example.com"));
    EXPECT_THAT(output_str, HasSubstr("/admin"));
    EXPECT_THAT(output_str, HasSubstr("/v1/users"));
}

TEST_F(MergerEngineIntegrationTest, LargeDatasetPerformance) {
    // Create large CSV files to test performance
    // Créer de gros fichiers CSV pour tester les performances
    std::vector<std::string> large_file;
    large_file.push_back("id,name,email,timestamp");
    
    for (int i = 0; i < 10000; ++i) {
        large_file.push_back(
            std::to_string(i) + ",User" + std::to_string(i) + 
            ",user" + std::to_string(i) + "@example.com,2024-01-01T10:00:00Z"
        );
    }
    
    createCSVFile("large1.csv", large_file);
    
    // Modify some records for second file
    // Modifier quelques enregistrements pour le deuxième fichier
    for (int i = 5000; i < 15000; ++i) {
        if (i == 5000) continue; // Skip header for second iteration
        large_file[i - 4999] = 
            std::to_string(i) + ",ModifiedUser" + std::to_string(i) + 
            ",modified" + std::to_string(i) + "@example.com,2024-01-02T10:00:00Z";
    }
    
    createCSVFile("large2.csv", large_file);
    
    MergeConfig config;
    config.strategy = MergeStrategy::SMART_MERGE;
    config.deduplication = DeduplicationStrategy::KEY_BASED;
    config.key_columns = {"id"};
    config.conflict_resolution = ConflictResolution::KEEP_NEWEST;
    config.time_column = "timestamp";
    
    std::vector<InputSource> sources = {
        {test_dir / "large1.csv", 1},
        {test_dir / "large2.csv", 1}
    };
    
    MergerEngine engine(config);
    std::ostringstream output;
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = engine.merge(sources, output);
    auto end = std::chrono::high_resolution_clock::now();
    
    EXPECT_EQ(result, MergeError::SUCCESS);
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Performance should be reasonable (less than 5 seconds for this dataset)
    // Les performances devraient être raisonnables (moins de 5 secondes pour ce jeu de données)
    EXPECT_LT(duration.count(), 5000);
    
    const auto& stats = engine.getStatistics();
    EXPECT_GT(stats.getTotalRows(), 10000);
    EXPECT_EQ(stats.getSourcesProcessed(), 2);
}

// Error handling integration tests
// Tests d'intégration de gestion d'erreurs
TEST_F(MergerEngineIntegrationTest, MixedValidInvalidFiles) {
    createCSVFile("valid.csv", {
        "id,name,email",
        "1,John,john@example.com",
        "2,Jane,jane@example.com"
    });
    
    createCSVFile("invalid.csv", {
        "id,name,email",
        "1,John", // Missing field
        "malformed line without proper CSV format",
        "3,Bob,bob@example.com"
    });
    
    MergeConfig config;
    config.strategy = MergeStrategy::APPEND;
    
    std::vector<InputSource> sources = {
        {test_dir / "valid.csv", 1},
        {test_dir / "invalid.csv", 1}
    };
    
    MergerEngine engine(config);
    std::ostringstream output;
    
    auto result = engine.merge(sources, output);
    EXPECT_EQ(result, MergeError::SUCCESS); // Should continue despite some invalid data
    
    const auto& stats = engine.getStatistics();
    EXPECT_GT(stats.getErrorsEncountered(), 0); // Should report errors
    EXPECT_EQ(stats.getSourcesProcessed(), 2); // Should process both files
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}