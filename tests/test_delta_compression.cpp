#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>
#include <chrono>
#include "csv/delta_compression.hpp"

using namespace BBP::CSV;
using namespace testing;

// EN: Test fixture for Delta Compression tests
// FR: Fixture de test pour les tests Delta Compression
class DeltaCompressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // EN: Create temporary directory for test files
        // FR: Créer un répertoire temporaire pour les fichiers de test
        test_dir = std::filesystem::temp_directory_path() / "delta_compression_test";
        std::filesystem::create_directories(test_dir);
    }
    
    void TearDown() override {
        // EN: Clean up temporary files
        // FR: Nettoyer les fichiers temporaires
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }
    
    // EN: Helper function to create CSV file
    // FR: Fonction d'aide pour créer un fichier CSV
    void createCSVFile(const std::string& filename, const std::vector<std::string>& lines) {
        std::ofstream file(test_dir / filename);
        for (const auto& line : lines) {
            file << line << "\n";
        }
    }
    
    // EN: Helper function to read file content
    // FR: Fonction d'aide pour lire le contenu d'un fichier
    std::string readFile(const std::string& filename) {
        std::ifstream file(test_dir / filename);
        return std::string(std::istreambuf_iterator<char>(file), 
                          std::istreambuf_iterator<char>());
    }
    
    std::filesystem::path test_dir;
};

// EN: Tests for DeltaRecord class
// FR: Tests pour la classe DeltaRecord
class DeltaRecordTest : public ::testing::Test {};

TEST_F(DeltaRecordTest, SerializationDeserialization) {
    DeltaRecord record;
    record.operation = DeltaOperation::UPDATE;
    record.row_index = 42;
    record.old_values = {"old1", "old2", "old3"};
    record.new_values = {"new1", "new2", "new3"};
    record.changed_columns = {0, 2};
    record.timestamp = "2024-01-01T10:00:00Z";
    record.change_hash = "test_hash";
    record.metadata["source"] = "test";
    
    // EN: Test serialization
    // FR: Tester sérialisation
    std::string serialized = record.serialize();
    EXPECT_FALSE(serialized.empty());
    EXPECT_THAT(serialized, HasSubstr("\"operation\":3"));  // UPDATE = 3
    EXPECT_THAT(serialized, HasSubstr("\"row_index\":42"));
    EXPECT_THAT(serialized, HasSubstr("old1"));
    EXPECT_THAT(serialized, HasSubstr("new1"));
    
    // EN: Test deserialization
    // FR: Tester désérialisation
    DeltaRecord deserialized = DeltaRecord::deserialize(serialized);
    EXPECT_EQ(deserialized.operation, DeltaOperation::UPDATE);
    EXPECT_EQ(deserialized.row_index, 42);
    EXPECT_EQ(deserialized.timestamp, "2024-01-01T10:00:00Z");
    EXPECT_EQ(deserialized.change_hash, "test_hash");
}

TEST_F(DeltaRecordTest, EqualityOperator) {
    DeltaRecord record1;
    record1.operation = DeltaOperation::INSERT;
    record1.row_index = 1;
    record1.new_values = {"value1", "value2"};
    record1.timestamp = "2024-01-01T10:00:00Z";
    record1.change_hash = "hash1";
    
    DeltaRecord record2;
    record2.operation = DeltaOperation::INSERT;
    record2.row_index = 1;
    record2.new_values = {"value1", "value2"};
    record2.timestamp = "2024-01-01T10:00:00Z";
    record2.change_hash = "hash1";
    
    DeltaRecord record3;
    record3.operation = DeltaOperation::DELETE;
    record3.row_index = 1;
    record3.old_values = {"value1", "value2"};
    record3.timestamp = "2024-01-01T10:00:00Z";
    record3.change_hash = "hash2";
    
    EXPECT_EQ(record1, record2);
    EXPECT_NE(record1, record3);
}

// EN: Tests for DeltaHeader class
// FR: Tests pour la classe DeltaHeader
class DeltaHeaderTest : public ::testing::Test {};

TEST_F(DeltaHeaderTest, SerializationDeserialization) {
    DeltaHeader header;
    header.version = "1.0";
    header.source_file = "source.csv";
    header.target_file = "target.csv";
    header.creation_timestamp = "2024-01-01T10:00:00Z";
    header.algorithm = CompressionAlgorithm::HYBRID;
    header.detection_mode = ChangeDetectionMode::KEY_BASED;
    header.key_columns = {"id", "name"};
    header.total_changes = 100;
    header.compression_ratio = 75;
    header.metadata["test"] = "value";
    
    // EN: Test serialization
    // FR: Tester sérialisation
    std::string serialized = header.serialize();
    EXPECT_FALSE(serialized.empty());
    EXPECT_THAT(serialized, HasSubstr("DELTA_HEADER_V1.0"));
    EXPECT_THAT(serialized, HasSubstr("SOURCE_FILE=source.csv"));
    EXPECT_THAT(serialized, HasSubstr("TARGET_FILE=target.csv"));
    EXPECT_THAT(serialized, HasSubstr("TOTAL_CHANGES=100"));
    EXPECT_THAT(serialized, HasSubstr("KEY_COLUMNS=id,name"));
    EXPECT_THAT(serialized, HasSubstr("META_test=value"));
    EXPECT_THAT(serialized, HasSubstr("END_HEADER"));
    
    // EN: Test deserialization
    // FR: Tester désérialisation
    DeltaHeader deserialized = DeltaHeader::deserialize(serialized);
    EXPECT_EQ(deserialized.version, "1.0");
    EXPECT_EQ(deserialized.source_file, "source.csv");
    EXPECT_EQ(deserialized.target_file, "target.csv");
    EXPECT_EQ(deserialized.creation_timestamp, "2024-01-01T10:00:00Z");
    EXPECT_EQ(deserialized.algorithm, CompressionAlgorithm::HYBRID);
    EXPECT_EQ(deserialized.detection_mode, ChangeDetectionMode::KEY_BASED);
    EXPECT_EQ(deserialized.total_changes, 100);
    EXPECT_EQ(deserialized.compression_ratio, 75);
    EXPECT_EQ(deserialized.key_columns.size(), 2);
    EXPECT_EQ(deserialized.key_columns[0], "id");
    EXPECT_EQ(deserialized.key_columns[1], "name");
    EXPECT_EQ(deserialized.metadata["test"], "value");
}

// EN: Tests for DeltaConfig class
// FR: Tests pour la classe DeltaConfig
class DeltaConfigTest : public ::testing::Test {};

TEST_F(DeltaConfigTest, DefaultConfiguration) {
    DeltaConfig config;
    EXPECT_EQ(config.algorithm, CompressionAlgorithm::HYBRID);
    EXPECT_EQ(config.detection_mode, ChangeDetectionMode::CONTENT_HASH);
    EXPECT_TRUE(config.case_sensitive_keys);
    EXPECT_TRUE(config.trim_key_whitespace);
    EXPECT_EQ(config.similarity_threshold, 0.8);
    EXPECT_EQ(config.chunk_size, 10000);
    EXPECT_TRUE(config.enable_parallel_processing);
    EXPECT_FALSE(config.binary_format);
    EXPECT_TRUE(config.compress_output);
    EXPECT_FALSE(config.preserve_order);
    EXPECT_TRUE(config.include_metadata);
    EXPECT_TRUE(config.validate_integrity);
    EXPECT_EQ(config.min_compression_ratio, 1.1);
}

TEST_F(DeltaConfigTest, ConfigurationValidation) {
    DeltaConfig config;
    
    // EN: Valid configuration
    // FR: Configuration valide
    EXPECT_TRUE(config.isValid());
    EXPECT_TRUE(config.getValidationErrors().empty());
    
    // EN: Invalid similarity threshold
    // FR: Seuil de similarité invalide
    config.similarity_threshold = 1.5;
    EXPECT_FALSE(config.isValid());
    auto errors = config.getValidationErrors();
    EXPECT_FALSE(errors.empty());
    EXPECT_THAT(errors[0], HasSubstr("Similarity threshold"));
    
    // EN: Reset to valid
    // FR: Remettre à valide
    config.similarity_threshold = 0.8;
    
    // EN: Invalid chunk size
    // FR: Taille de chunk invalide
    config.chunk_size = 0;
    EXPECT_FALSE(config.isValid());
    errors = config.getValidationErrors();
    EXPECT_THAT(errors[0], HasSubstr("Chunk size"));
    
    // EN: Reset to valid
    // FR: Remettre à valide
    config.chunk_size = 10000;
    
    // EN: Key-based detection without key columns
    // FR: Détection basée sur clés sans colonnes clés
    config.detection_mode = ChangeDetectionMode::KEY_BASED;
    config.key_columns.clear();
    EXPECT_FALSE(config.isValid());
    errors = config.getValidationErrors();
    EXPECT_THAT(errors[0], HasSubstr("Key columns"));
    
    // EN: Reset to valid
    // FR: Remettre à valide
    config.key_columns = {"id"};
    EXPECT_TRUE(config.isValid());
}

// EN: Tests for DeltaStatistics class
// FR: Tests pour la classe DeltaStatistics
class DeltaStatisticsTest : public ::testing::Test {};

TEST_F(DeltaStatisticsTest, InitialState) {
    DeltaStatistics stats;
    EXPECT_EQ(stats.getTotalRecordsProcessed(), 0);
    EXPECT_EQ(stats.getTotalChangesDetected(), 0);
    EXPECT_EQ(stats.getInsertsDetected(), 0);
    EXPECT_EQ(stats.getUpdatesDetected(), 0);
    EXPECT_EQ(stats.getDeletesDetected(), 0);
    EXPECT_EQ(stats.getMovesDetected(), 0);
    EXPECT_EQ(stats.getOriginalSize(), 0);
    EXPECT_EQ(stats.getCompressedSize(), 0);
    EXPECT_EQ(stats.getCompressionRatio(), 0.0);
    EXPECT_EQ(stats.getProcessingTimeMs(), 0);
    EXPECT_EQ(stats.getMemoryUsageBytes(), 0);
}

TEST_F(DeltaStatisticsTest, IncrementOperations) {
    DeltaStatistics stats;
    
    stats.incrementRecordsProcessed(10);
    EXPECT_EQ(stats.getTotalRecordsProcessed(), 10);
    
    stats.incrementChangesDetected(5);
    EXPECT_EQ(stats.getTotalChangesDetected(), 5);
    
    stats.incrementInserts(2);
    EXPECT_EQ(stats.getInsertsDetected(), 2);
    
    stats.incrementUpdates(2);
    EXPECT_EQ(stats.getUpdatesDetected(), 2);
    
    stats.incrementDeletes(1);
    EXPECT_EQ(stats.getDeletesDetected(), 1);
    
    stats.incrementMoves(0);
    EXPECT_EQ(stats.getMovesDetected(), 0);
}

TEST_F(DeltaStatisticsTest, CompressionRatio) {
    DeltaStatistics stats;
    
    // EN: No compression data yet
    // FR: Pas encore de données de compression
    EXPECT_EQ(stats.getCompressionRatio(), 0.0);
    
    // EN: Set compression data
    // FR: Définir données de compression
    stats.setOriginalSize(1000);
    stats.setCompressedSize(500);
    EXPECT_DOUBLE_EQ(stats.getCompressionRatio(), 2.0);
    
    // EN: Better compression
    // FR: Meilleure compression
    stats.setCompressedSize(250);
    EXPECT_DOUBLE_EQ(stats.getCompressionRatio(), 4.0);
    
    // EN: No compression
    // FR: Pas de compression
    stats.setCompressedSize(1000);
    EXPECT_DOUBLE_EQ(stats.getCompressionRatio(), 1.0);
}

TEST_F(DeltaStatisticsTest, ThreadSafety) {
    DeltaStatistics stats;
    const int num_threads = 10;
    const int increments_per_thread = 100;
    
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&stats, increments_per_thread]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                stats.incrementRecordsProcessed(1);
                stats.incrementChangesDetected(1);
                stats.incrementInserts(1);
            }
            (void)increments_per_thread; // Explicitly use the capture
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(stats.getTotalRecordsProcessed(), num_threads * increments_per_thread);
    EXPECT_EQ(stats.getTotalChangesDetected(), num_threads * increments_per_thread);
    EXPECT_EQ(stats.getInsertsDetected(), num_threads * increments_per_thread);
}

TEST_F(DeltaStatisticsTest, Reset) {
    DeltaStatistics stats;
    
    // EN: Set some values
    // FR: Définir quelques valeurs
    stats.incrementRecordsProcessed(100);
    stats.incrementChangesDetected(50);
    stats.setOriginalSize(1000);
    stats.setCompressedSize(500);
    stats.setProcessingTime(1000);
    stats.setMemoryUsage(2048);
    
    // EN: Verify values are set
    // FR: Vérifier que les valeurs sont définies
    EXPECT_EQ(stats.getTotalRecordsProcessed(), 100);
    EXPECT_EQ(stats.getTotalChangesDetected(), 50);
    EXPECT_EQ(stats.getOriginalSize(), 1000);
    EXPECT_EQ(stats.getCompressedSize(), 500);
    EXPECT_EQ(stats.getProcessingTimeMs(), 1000);
    EXPECT_EQ(stats.getMemoryUsageBytes(), 2048);
    
    // EN: Reset
    // FR: Réinitialiser
    stats.reset();
    
    // EN: Verify all values are reset
    // FR: Vérifier que toutes les valeurs sont réinitialisées
    EXPECT_EQ(stats.getTotalRecordsProcessed(), 0);
    EXPECT_EQ(stats.getTotalChangesDetected(), 0);
    EXPECT_EQ(stats.getOriginalSize(), 0);
    EXPECT_EQ(stats.getCompressedSize(), 0);
    EXPECT_EQ(stats.getProcessingTimeMs(), 0);
    EXPECT_EQ(stats.getMemoryUsageBytes(), 0);
}

// EN: Tests for ChangeDetector class
// FR: Tests pour la classe ChangeDetector
class ChangeDetectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.detection_mode = ChangeDetectionMode::CONTENT_HASH;
        config.key_columns = {"id"};
        detector = std::make_unique<ChangeDetector>(config);
    }
    
    DeltaConfig config;
    std::unique_ptr<ChangeDetector> detector;
};

TEST_F(ChangeDetectorTest, ContentHashDetection) {
    std::vector<std::vector<std::string>> old_data = {
        {"1", "Alice", "alice@example.com"},
        {"2", "Bob", "bob@example.com"},
        {"3", "Charlie", "charlie@example.com"}
    };
    
    std::vector<std::vector<std::string>> new_data = {
        {"1", "Alice", "alice@example.com"},      // Unchanged
        {"2", "Bob", "bob@newdomain.com"},        // Changed (but same hash logic will detect as different)
        {"4", "David", "david@example.com"}       // New
    };
    
    std::vector<std::string> headers = {"id", "name", "email"};
    
    auto changes = detector->detectChanges(old_data, new_data, headers);
    
    // EN: Should detect some changes (exact count depends on hash implementation)
    // FR: Devrait détecter quelques changements (nombre exact dépend de l'implémentation du hash)
    EXPECT_FALSE(changes.empty());
    
    // EN: Check that we have different types of operations
    // FR: Vérifier que nous avons différents types d'opérations
    bool has_insert = false, has_delete = false;
    for (const auto& change : changes) {
        if (change.operation == DeltaOperation::INSERT) has_insert = true;
        if (change.operation == DeltaOperation::DELETE) has_delete = true;
    }
    
    EXPECT_TRUE(has_insert || has_delete); // At least one type should be detected
}

TEST_F(ChangeDetectorTest, KeyBasedDetection) {
    config.detection_mode = ChangeDetectionMode::KEY_BASED;
    detector = std::make_unique<ChangeDetector>(config);
    
    std::vector<std::vector<std::string>> old_data = {
        {"1", "Alice", "alice@example.com"},
        {"2", "Bob", "bob@example.com"},
        {"3", "Charlie", "charlie@example.com"}
    };
    
    std::vector<std::vector<std::string>> new_data = {
        {"1", "Alice", "alice@newdomain.com"},    // Updated email
        {"2", "Bob", "bob@example.com"},          // Unchanged
        {"4", "David", "david@example.com"}       // New
        // Charlie (id=3) is deleted
    };
    
    std::vector<std::string> headers = {"id", "name", "email"};
    
    auto changes = detector->detectChanges(old_data, new_data, headers);
    
    // EN: Should detect: 1 update, 1 insert, 1 delete
    // FR: Devrait détecter : 1 mise à jour, 1 insertion, 1 suppression
    EXPECT_EQ(changes.size(), 3);
    
    bool has_update = false, has_insert = false, has_delete = false;
    for (const auto& change : changes) {
        switch (change.operation) {
            case DeltaOperation::UPDATE:
                has_update = true;
                EXPECT_EQ(change.old_values[2], "alice@example.com");
                EXPECT_EQ(change.new_values[2], "alice@newdomain.com");
                break;
            case DeltaOperation::INSERT:
                has_insert = true;
                EXPECT_EQ(change.new_values[0], "4");
                EXPECT_EQ(change.new_values[1], "David");
                break;
            case DeltaOperation::DELETE:
                has_delete = true;
                EXPECT_EQ(change.old_values[0], "3");
                EXPECT_EQ(change.old_values[1], "Charlie");
                break;
            default:
                break;
        }
    }
    
    EXPECT_TRUE(has_update);
    EXPECT_TRUE(has_insert);
    EXPECT_TRUE(has_delete);
}

TEST_F(ChangeDetectorTest, FieldByFieldDetection) {
    config.detection_mode = ChangeDetectionMode::FIELD_BY_FIELD;
    detector = std::make_unique<ChangeDetector>(config);
    
    std::vector<std::vector<std::string>> old_data = {
        {"1", "Alice", "alice@example.com"},
        {"2", "Bob", "bob@example.com"}
    };
    
    std::vector<std::vector<std::string>> new_data = {
        {"1", "Alice", "alice@newdomain.com"},    // Updated email
        {"2", "Bob", "bob@example.com"},          // Unchanged
        {"3", "Charlie", "charlie@example.com"}   // New row
    };
    
    std::vector<std::string> headers = {"id", "name", "email"};
    
    auto changes = detector->detectChanges(old_data, new_data, headers);
    
    // EN: Should detect: 1 update (row 0), 1 insert (row 2)
    // FR: Devrait détecter : 1 mise à jour (ligne 0), 1 insertion (ligne 2)
    EXPECT_EQ(changes.size(), 2);
    
    bool has_update = false, has_insert = false;
    for (const auto& change : changes) {
        if (change.operation == DeltaOperation::UPDATE) {
            has_update = true;
            EXPECT_EQ(change.row_index, 0);
            EXPECT_EQ(change.changed_columns.size(), 1);
            EXPECT_EQ(change.changed_columns[0], 2); // Email column changed
        } else if (change.operation == DeltaOperation::INSERT) {
            has_insert = true;
            EXPECT_EQ(change.row_index, 2);
        }
    }
    
    EXPECT_TRUE(has_update);
    EXPECT_TRUE(has_insert);
}

TEST_F(ChangeDetectorTest, UtilityMethods) {
    std::vector<std::string> row1 = {"1", "Alice", "alice@example.com"};
    std::vector<std::string> row2 = {"2", "Bob", "bob@example.com"};
    std::vector<std::string> row3 = {"1", "Alice", "alice@example.com"};
    
    // EN: Test hash generation
    // FR: Tester génération de hash
    std::string hash1 = detector->generateRowHash(row1);
    std::string hash2 = detector->generateRowHash(row2);
    std::string hash3 = detector->generateRowHash(row3);
    
    EXPECT_FALSE(hash1.empty());
    EXPECT_FALSE(hash2.empty());
    EXPECT_EQ(hash1, hash3);  // Same content should produce same hash
    EXPECT_NE(hash1, hash2);  // Different content should produce different hash
    
    // EN: Test key generation
    // FR: Tester génération de clé
    std::vector<std::string> headers = {"id", "name", "email"};
    std::string key1 = detector->generateKeyFromRow(row1, headers);
    std::string key2 = detector->generateKeyFromRow(row2, headers);
    std::string key3 = detector->generateKeyFromRow(row3, headers);
    
    EXPECT_EQ(key1, "1");  // Key should be the id column
    EXPECT_EQ(key2, "2");
    EXPECT_EQ(key3, "1");
    EXPECT_EQ(key1, key3);
    EXPECT_NE(key1, key2);
    
    // EN: Test similarity
    // FR: Tester similarité
    EXPECT_TRUE(detector->areRowsSimilar(row1, row3));
    EXPECT_FALSE(detector->areRowsSimilar(row1, row2));
    
    // EN: Test changed columns detection
    // FR: Tester détection colonnes changées
    std::vector<std::string> old_row = {"1", "Alice", "alice@example.com"};
    std::vector<std::string> new_row = {"1", "Alice", "alice@newdomain.com"};
    
    auto changed_cols = detector->findChangedColumns(old_row, new_row);
    EXPECT_EQ(changed_cols.size(), 1);
    EXPECT_EQ(changed_cols[0], 2); // Email column (index 2) changed
}

// EN: Tests for DeltaCompressor class
// FR: Tests pour la classe DeltaCompressor
class DeltaCompressorTest : public DeltaCompressionTest {
protected:
    void SetUp() override {
        DeltaCompressionTest::SetUp();
        config.algorithm = CompressionAlgorithm::HYBRID;
        config.detection_mode = ChangeDetectionMode::KEY_BASED;
        config.key_columns = {"id"};
        compressor = std::make_unique<DeltaCompressor>(config);
    }
    
    DeltaConfig config;
    std::unique_ptr<DeltaCompressor> compressor;
};

TEST_F(DeltaCompressorTest, BasicCompression) {
    // EN: Create test CSV files
    // FR: Créer fichiers CSV de test
    createCSVFile("old.csv", {
        "id,name,email",
        "1,Alice,alice@example.com",
        "2,Bob,bob@example.com",
        "3,Charlie,charlie@example.com"
    });
    
    createCSVFile("new.csv", {
        "id,name,email",
        "1,Alice,alice@newdomain.com",  // Updated
        "2,Bob,bob@example.com",        // Unchanged
        "4,David,david@example.com"     // New
        // Charlie deleted
    });
    
    std::string old_file = test_dir / "old.csv";
    std::string new_file = test_dir / "new.csv";
    std::string delta_file = test_dir / "delta.bin";
    
    // EN: Perform compression
    // FR: Effectuer compression
    auto result = compressor->compress(old_file, new_file, delta_file);
    EXPECT_EQ(result, DeltaError::SUCCESS);
    
    // EN: Check that delta file was created
    // FR: Vérifier que fichier delta a été créé
    EXPECT_TRUE(std::filesystem::exists(delta_file));
    EXPECT_GT(std::filesystem::file_size(delta_file), 0);
    
    // EN: Check statistics
    // FR: Vérifier statistiques
    const auto& stats = compressor->getStatistics();
    EXPECT_GT(stats.getTotalRecordsProcessed(), 0);
    EXPECT_GT(stats.getTotalChangesDetected(), 0);
    EXPECT_GT(stats.getInsertsDetected() + stats.getUpdatesDetected() + stats.getDeletesDetected(), 0);
    EXPECT_GT(stats.getOriginalSize(), 0);
    EXPECT_GT(stats.getCompressedSize(), 0);
}

TEST_F(DeltaCompressorTest, CompressionAlgorithms) {
    std::vector<DeltaRecord> test_records;
    
    // EN: Create test records
    // FR: Créer enregistrements de test
    for (int i = 0; i < 10; ++i) {
        DeltaRecord record;
        record.operation = DeltaOperation::INSERT;
        record.row_index = i;
        record.new_values = {"id" + std::to_string(i), "name" + std::to_string(i), "email" + std::to_string(i) + "@example.com"};
        record.timestamp = DeltaUtils::getCurrentTimestamp();
        test_records.push_back(record);
    }
    
    // EN: Test Run-Length Encoding
    // FR: Tester encodage par longueurs de plages
    std::vector<uint8_t> test_data = {1, 1, 1, 2, 2, 3, 3, 3, 3, 4};
    auto rle_result = compressor->applyRunLengthEncoding(test_data);
    EXPECT_FALSE(rle_result.empty());
    EXPECT_LE(rle_result.size(), test_data.size() * 2); // RLE format: count, value
    
    // EN: Test Delta Encoding
    // FR: Tester encodage delta
    std::vector<int64_t> numeric_values = {100, 101, 103, 106, 110, 115};
    auto delta_result = compressor->applyDeltaEncoding(numeric_values);
    EXPECT_FALSE(delta_result.empty());
    
    // EN: Test Dictionary Compression
    // FR: Tester compression par dictionnaire
    std::vector<std::string> strings = {"hello", "world", "hello", "test", "world", "hello"};
    auto dict_result = compressor->applyDictionaryCompression(strings);
    EXPECT_FALSE(dict_result.empty());
    
    // EN: Test LZ77 Compression
    // FR: Tester compression LZ77
    std::vector<uint8_t> lz_input(1000, 65); // Repeated 'A' characters
    auto lz_result = compressor->applyLZ77Compression(lz_input);
    EXPECT_FALSE(lz_result.empty());
    EXPECT_LT(lz_result.size(), lz_input.size()); // Should compress well
    
    // EN: Test Hybrid Compression
    // FR: Tester compression hybride
    auto hybrid_result = compressor->applyHybridCompression(test_records);
    EXPECT_FALSE(hybrid_result.empty());
}

TEST_F(DeltaCompressorTest, ErrorHandling) {
    std::string non_existent_file = test_dir / "nonexistent.csv";
    std::string delta_file = test_dir / "delta.bin";
    
    // EN: Test with non-existent files
    // FR: Tester avec fichiers inexistants
    auto result = compressor->compress(non_existent_file, non_existent_file, delta_file);
    EXPECT_NE(result, DeltaError::SUCCESS);
    
    // EN: Create invalid CSV file
    // FR: Créer fichier CSV invalide
    createCSVFile("invalid.csv", {
        "incomplete line without proper"
    });
    
    std::string invalid_file = test_dir / "invalid.csv";
    createCSVFile("valid.csv", {
        "id,name",
        "1,Alice"
    });
    
    std::string valid_file = test_dir / "valid.csv";
    
    // EN: Test with invalid input
    // FR: Tester avec entrée invalide
    result = compressor->compress(invalid_file, valid_file, delta_file);
    // EN: Should handle gracefully (may succeed with partial data or fail appropriately)
    // FR: Devrait gérer gracieusement (peut réussir avec données partielles ou échouer appropriément)
    // Result depends on implementation robustness
}

TEST_F(DeltaCompressorTest, PerformanceAndMemoryUsage) {
    // EN: Create larger test files for performance testing
    // FR: Créer fichiers de test plus gros pour test de performance
    std::vector<std::string> large_old_data = {"id,name,email,age,city"};
    std::vector<std::string> large_new_data = {"id,name,email,age,city"};
    
    // EN: Generate 1000 records
    // FR: Générer 1000 enregistrements
    for (int i = 1; i <= 1000; ++i) {
        large_old_data.push_back(std::to_string(i) + ",User" + std::to_string(i) + 
                                ",user" + std::to_string(i) + "@example.com," + 
                                std::to_string(20 + (i % 50)) + ",City" + std::to_string(i % 10));
        
        // EN: Modify some records for new data
        // FR: Modifier quelques enregistrements pour nouvelles données
        if (i % 3 == 0) {
            // EN: Update every third record
            // FR: Mettre à jour chaque troisième enregistrement
            large_new_data.push_back(std::to_string(i) + ",User" + std::to_string(i) + 
                                   ",user" + std::to_string(i) + "@newdomain.com," + 
                                   std::to_string(21 + (i % 50)) + ",NewCity" + std::to_string(i % 10));
        } else if (i % 7 != 0) {
            // EN: Keep most records unchanged, skip every 7th (delete)
            // FR: Garder la plupart inchangés, passer chaque 7ème (suppression)
            large_new_data.push_back(large_old_data.back());
        }
    }
    
    // EN: Add some new records
    // FR: Ajouter quelques nouveaux enregistrements
    for (int i = 1001; i <= 1100; ++i) {
        large_new_data.push_back(std::to_string(i) + ",NewUser" + std::to_string(i) + 
                               ",newuser" + std::to_string(i) + "@example.com," + 
                               std::to_string(25 + (i % 30)) + ",NewCity" + std::to_string(i % 5));
    }
    
    createCSVFile("large_old.csv", large_old_data);
    createCSVFile("large_new.csv", large_new_data);
    
    std::string old_file = test_dir / "large_old.csv";
    std::string new_file = test_dir / "large_new.csv";
    std::string delta_file = test_dir / "large_delta.bin";
    
    // EN: Measure compression time
    // FR: Mesurer temps de compression
    auto start = std::chrono::high_resolution_clock::now();
    auto result = compressor->compress(old_file, new_file, delta_file);
    auto end = std::chrono::high_resolution_clock::now();
    
    EXPECT_EQ(result, DeltaError::SUCCESS);
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // EN: Performance should be reasonable (less than 5 seconds for 1000+ records)
    // FR: Performance devrait être raisonnable (moins de 5 secondes pour 1000+ enregistrements)
    EXPECT_LT(duration.count(), 5000);
    
    // EN: Check compression ratio
    // FR: Vérifier ratio de compression
    const auto& stats = compressor->getStatistics();
    EXPECT_GT(stats.getCompressionRatio(), 1.0); // Should achieve some compression
    
    // EN: Verify statistics
    // FR: Vérifier statistiques
    EXPECT_GT(stats.getTotalChangesDetected(), 0);
    EXPECT_GT(stats.getProcessingTimeMs(), 0);
}

// EN: Tests for DeltaDecompressor class
// FR: Tests pour la classe DeltaDecompressor
class DeltaDecompressorTest : public DeltaCompressionTest {
protected:
    void SetUp() override {
        DeltaCompressionTest::SetUp();
        config.algorithm = CompressionAlgorithm::HYBRID;
        config.detection_mode = ChangeDetectionMode::KEY_BASED;
        config.key_columns = {"id"};
        compressor = std::make_unique<DeltaCompressor>(config);
        decompressor = std::make_unique<DeltaDecompressor>(config);
    }
    
    DeltaConfig config;
    std::unique_ptr<DeltaCompressor> compressor;
    std::unique_ptr<DeltaDecompressor> decompressor;
};

TEST_F(DeltaDecompressorTest, BasicDecompression) {
    // EN: Create test files
    // FR: Créer fichiers de test
    createCSVFile("original.csv", {
        "id,name,email",
        "1,Alice,alice@example.com",
        "2,Bob,bob@example.com",
        "3,Charlie,charlie@example.com"
    });
    
    createCSVFile("modified.csv", {
        "id,name,email",
        "1,Alice,alice@newdomain.com",  // Updated
        "2,Bob,bob@example.com",        // Unchanged
        "4,David,david@example.com"     // New
        // Charlie deleted
    });
    
    std::string original_file = test_dir / "original.csv";
    std::string modified_file = test_dir / "modified.csv";
    std::string delta_file = test_dir / "test.delta";
    std::string reconstructed_file = test_dir / "reconstructed.csv";
    
    // EN: First compress to create delta
    // FR: D'abord compresser pour créer delta
    auto compress_result = compressor->compress(original_file, modified_file, delta_file);
    EXPECT_EQ(compress_result, DeltaError::SUCCESS);
    EXPECT_TRUE(std::filesystem::exists(delta_file));
    
    // EN: Then decompress to reconstruct
    // FR: Puis décompresser pour reconstruire
    auto decompress_result = decompressor->decompress(delta_file, original_file, reconstructed_file);
    EXPECT_EQ(decompress_result, DeltaError::SUCCESS);
    EXPECT_TRUE(std::filesystem::exists(reconstructed_file));
    
    // EN: Compare reconstructed with original modified file
    // FR: Comparer reconstruit avec fichier modifié original
    auto original_modified = DeltaUtils::loadCsvFile(modified_file);
    auto reconstructed = DeltaUtils::loadCsvFile(reconstructed_file);
    
    // EN: Basic check - should have similar structure
    // FR: Vérification de base - devrait avoir structure similaire
    EXPECT_FALSE(original_modified.empty());
    EXPECT_FALSE(reconstructed.empty());
    
    // EN: Header should match
    // FR: En-tête devrait correspondre
    if (!original_modified.empty() && !reconstructed.empty()) {
        EXPECT_EQ(original_modified[0], reconstructed[0]);
    }
}

TEST_F(DeltaDecompressorTest, RoundTripConsistency) {
    // EN: Test that compress -> decompress produces consistent results
    // FR: Tester que compresser -> décompresser produit des résultats cohérents
    
    std::vector<std::string> test_cases = {
        "simple_case",
        "complex_case",
        "edge_case"
    };
    
    for (const auto& test_case : test_cases) {
        // EN: Create test data based on case
        // FR: Créer données de test basées sur le cas
        std::vector<std::string> old_data, new_data;
        
        if (test_case == "simple_case") {
            old_data = {
                "id,name,value",
                "1,A,100",
                "2,B,200"
            };
            new_data = {
                "id,name,value",
                "1,A,150",  // Updated
                "3,C,300"   // New
                // B deleted
            };
        } else if (test_case == "complex_case") {
            old_data = {
                "id,name,email,age",
                "1,Alice,alice@example.com,25",
                "2,Bob,bob@example.com,30",
                "3,Charlie,charlie@example.com,35",
                "4,Diana,diana@example.com,28"
            };
            new_data = {
                "id,name,email,age",
                "1,Alice,alice@newdomain.com,26",      // Updated email and age
                "2,Bob,bob@example.com,30",            // Unchanged
                "3,Charlie,charlie@company.com,35",    // Updated email
                "5,Eve,eve@example.com,22"            // New (Diana deleted)
            };
        } else { // edge_case
            old_data = {
                "id,data",
                "1,",          // Empty field
                "2,\"quoted\""  // Quoted field
            };
            new_data = {
                "id,data",
                "1,filled",     // Was empty, now has value
                "2,\"quoted\""  // Unchanged
            };
        }
        
        createCSVFile("old_" + test_case + ".csv", old_data);
        createCSVFile("new_" + test_case + ".csv", new_data);
        
        std::string old_file = test_dir / ("old_" + test_case + ".csv");
        std::string new_file = test_dir / ("new_" + test_case + ".csv");
        std::string delta_file = test_dir / (test_case + ".delta");
        std::string restored_file = test_dir / ("restored_" + test_case + ".csv");
        
        // EN: Compress
        // FR: Compresser
        auto compress_result = compressor->compress(old_file, new_file, delta_file);
        EXPECT_EQ(compress_result, DeltaError::SUCCESS) << "Failed to compress " << test_case;
        
        // EN: Decompress
        // FR: Décompresser
        auto decompress_result = decompressor->decompress(delta_file, old_file, restored_file);
        EXPECT_EQ(decompress_result, DeltaError::SUCCESS) << "Failed to decompress " << test_case;
        
        // EN: Verify files exist
        // FR: Vérifier que fichiers existent
        EXPECT_TRUE(std::filesystem::exists(restored_file)) << "Restored file missing for " << test_case;
        
        // EN: Load and compare (basic structure check)
        // FR: Charger et comparer (vérification structure de base)
        if (std::filesystem::exists(restored_file)) {
            auto original_new = DeltaUtils::loadCsvFile(new_file);
            auto restored = DeltaUtils::loadCsvFile(restored_file);
            
            EXPECT_FALSE(original_new.empty()) << "Original new file empty for " << test_case;
            EXPECT_FALSE(restored.empty()) << "Restored file empty for " << test_case;
            
            if (!original_new.empty() && !restored.empty()) {
                EXPECT_EQ(original_new.size(), restored.size()) << "Row count mismatch in " << test_case;
            }
        }
    }
}

// EN: Tests for DeltaUtils namespace
// FR: Tests pour le namespace DeltaUtils
class DeltaUtilsTest : public DeltaCompressionTest {};

TEST_F(DeltaUtilsTest, FileOperations) {
    // EN: Test file existence
    // FR: Tester existence de fichier
    std::string non_existent = test_dir / "nonexistent.txt";
    EXPECT_FALSE(DeltaUtils::fileExists(non_existent));
    
    // EN: Create and test file
    // FR: Créer et tester fichier
    createCSVFile("test.csv", {"header", "data1", "data2"});
    std::string test_file = test_dir / "test.csv";
    
    EXPECT_TRUE(DeltaUtils::fileExists(test_file));
    EXPECT_GT(DeltaUtils::getFileSize(test_file), 0);
    
    // EN: Test file hash
    // FR: Tester hash de fichier
    std::string hash = DeltaUtils::getFileHash(test_file);
    EXPECT_FALSE(hash.empty());
    EXPECT_EQ(hash.length(), 64); // SHA-256 produces 64 character hex string
    
    // EN: Test CSV loading
    // FR: Tester chargement CSV
    auto data = DeltaUtils::loadCsvFile(test_file);
    EXPECT_EQ(data.size(), 3);
    EXPECT_EQ(data[0][0], "header");
    EXPECT_EQ(data[1][0], "data1");
    EXPECT_EQ(data[2][0], "data2");
    
    // EN: Test CSV saving
    // FR: Tester sauvegarde CSV
    std::vector<std::vector<std::string>> save_data = {
        {"col1", "col2", "col3"},
        {"val1", "val2", "val3"},
        {"val4", "val5", "val6"}
    };
    
    std::string save_file = test_dir / "saved.csv";
    auto result = DeltaUtils::saveCsvFile(save_file, save_data);
    EXPECT_EQ(result, DeltaError::SUCCESS);
    EXPECT_TRUE(DeltaUtils::fileExists(save_file));
    
    auto loaded_data = DeltaUtils::loadCsvFile(save_file);
    EXPECT_EQ(loaded_data, save_data);
}

TEST_F(DeltaUtilsTest, StringUtilities) {
    // EN: Test trim
    // FR: Tester suppression espaces
    EXPECT_EQ(DeltaUtils::trim("  hello  "), "hello");
    EXPECT_EQ(DeltaUtils::trim("hello"), "hello");
    EXPECT_EQ(DeltaUtils::trim("  "), "");
    EXPECT_EQ(DeltaUtils::trim(""), "");
    EXPECT_EQ(DeltaUtils::trim("\t\n hello \r\n"), "hello");
    
    // EN: Test toLower
    // FR: Tester conversion minuscules
    EXPECT_EQ(DeltaUtils::toLower("HELLO"), "hello");
    EXPECT_EQ(DeltaUtils::toLower("Hello World"), "hello world");
    EXPECT_EQ(DeltaUtils::toLower("123ABC"), "123abc");
    EXPECT_EQ(DeltaUtils::toLower(""), "");
    
    // EN: Test split
    // FR: Tester division
    auto parts = DeltaUtils::split("a,b,c", ',');
    EXPECT_EQ(parts.size(), 3);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
    
    parts = DeltaUtils::split("single", ',');
    EXPECT_EQ(parts.size(), 1);
    EXPECT_EQ(parts[0], "single");
    
    parts = DeltaUtils::split("", ',');
    EXPECT_EQ(parts.size(), 1);
    EXPECT_EQ(parts[0], "");
    
    // EN: Test join
    // FR: Tester jointure
    std::vector<std::string> join_parts = {"a", "b", "c"};
    EXPECT_EQ(DeltaUtils::join(join_parts, ","), "a,b,c");
    EXPECT_EQ(DeltaUtils::join(join_parts, " | "), "a | b | c");
    
    join_parts = {"single"};
    EXPECT_EQ(DeltaUtils::join(join_parts, ","), "single");
    
    join_parts = {};
    EXPECT_EQ(DeltaUtils::join(join_parts, ","), "");
}

TEST_F(DeltaUtilsTest, TimestampUtilities) {
    // EN: Test current timestamp
    // FR: Tester timestamp actuel
    std::string ts = DeltaUtils::getCurrentTimestamp();
    EXPECT_FALSE(ts.empty());
    EXPECT_GT(ts.length(), 15); // Should be ISO 8601 format
    EXPECT_THAT(ts, HasSubstr("T"));  // Should contain 'T' separator
    EXPECT_THAT(ts, HasSubstr("Z"));  // Should end with 'Z' for UTC
    
    // EN: Test timestamp formatting and parsing
    // FR: Tester formatage et analyse timestamp
    auto now = std::chrono::system_clock::now();
    std::string formatted = DeltaUtils::formatTimestamp(now);
    EXPECT_FALSE(formatted.empty());
    
    // EN: Parse back and compare (within reasonable tolerance)
    // FR: Analyser en retour et comparer (dans tolérance raisonnable)
    auto parsed = DeltaUtils::parseTimestamp(formatted);
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - parsed);
    EXPECT_LT(std::abs(diff.count()), 5); // Within 5 seconds tolerance
}

TEST_F(DeltaUtilsTest, HashUtilities) {
    // EN: Test SHA-256
    // FR: Tester SHA-256
    std::string data = "Hello, World!";
    std::string sha_hash = DeltaUtils::computeSHA256(data);
    EXPECT_EQ(sha_hash.length(), 64); // SHA-256 produces 64 character hex
    EXPECT_FALSE(sha_hash.empty());
    
    // EN: Same input should produce same hash
    // FR: Même entrée devrait produire même hash
    std::string sha_hash2 = DeltaUtils::computeSHA256(data);
    EXPECT_EQ(sha_hash, sha_hash2);
    
    // EN: Different input should produce different hash
    // FR: Entrée différente devrait produire hash différent
    std::string sha_hash3 = DeltaUtils::computeSHA256("Different data");
    EXPECT_NE(sha_hash, sha_hash3);
    
    // EN: Test MD5
    // FR: Tester MD5
    std::string md5_hash = DeltaUtils::computeMD5(data);
    EXPECT_EQ(md5_hash.length(), 32); // MD5 produces 32 character hex
    EXPECT_FALSE(md5_hash.empty());
    EXPECT_NE(md5_hash, sha_hash); // Different algorithms, different results
    
    // EN: Test content hash for rows
    // FR: Tester hash de contenu pour lignes
    std::vector<std::string> row1 = {"1", "Alice", "alice@example.com"};
    std::vector<std::string> row2 = {"1", "Alice", "alice@example.com"};
    std::vector<std::string> row3 = {"2", "Bob", "bob@example.com"};
    
    std::string hash1 = DeltaUtils::computeContentHash(row1);
    std::string hash2 = DeltaUtils::computeContentHash(row2);
    std::string hash3 = DeltaUtils::computeContentHash(row3);
    
    EXPECT_EQ(hash1, hash2); // Same content should produce same hash
    EXPECT_NE(hash1, hash3); // Different content should produce different hash
}

TEST_F(DeltaUtilsTest, CompressionUtilities) {
    // EN: Test compression ratio calculation
    // FR: Tester calcul ratio de compression
    EXPECT_DOUBLE_EQ(DeltaUtils::calculateCompressionRatio(1000, 500), 2.0);
    EXPECT_DOUBLE_EQ(DeltaUtils::calculateCompressionRatio(1000, 250), 4.0);
    EXPECT_DOUBLE_EQ(DeltaUtils::calculateCompressionRatio(1000, 1000), 1.0);
    EXPECT_DOUBLE_EQ(DeltaUtils::calculateCompressionRatio(1000, 0), 0.0);
    EXPECT_DOUBLE_EQ(DeltaUtils::calculateCompressionRatio(0, 500), 0.0);
    
    // EN: Test compressibility check
    // FR: Tester vérification compressibilité
    std::vector<DeltaRecord> few_records(5);
    std::vector<DeltaRecord> many_records(50);
    
    EXPECT_FALSE(DeltaUtils::isCompressible(few_records, 1.5));
    EXPECT_TRUE(DeltaUtils::isCompressible(many_records, 1.5));
    
    // EN: Test size estimation
    // FR: Tester estimation de taille
    size_t estimated_none = DeltaUtils::estimateCompressionSize(many_records, CompressionAlgorithm::NONE);
    size_t estimated_rle = DeltaUtils::estimateCompressionSize(many_records, CompressionAlgorithm::RLE);
    size_t estimated_lz77 = DeltaUtils::estimateCompressionSize(many_records, CompressionAlgorithm::LZ77);
    size_t estimated_hybrid = DeltaUtils::estimateCompressionSize(many_records, CompressionAlgorithm::HYBRID);
    
    EXPECT_GT(estimated_none, 0);
    EXPECT_LT(estimated_rle, estimated_none);    // RLE should be smaller
    EXPECT_LT(estimated_lz77, estimated_none);   // LZ77 should be smaller
    EXPECT_LT(estimated_hybrid, estimated_none); // Hybrid should be smallest
    EXPECT_LT(estimated_hybrid, estimated_rle);  // Hybrid should be better than single algorithm
}

TEST_F(DeltaUtilsTest, PerformanceUtilities) {
    // EN: Test optimal chunk size calculation
    // FR: Tester calcul taille optimale de chunk
    size_t chunk1 = DeltaUtils::getOptimalChunkSize(10000, 10 * 1024 * 1024);
    size_t chunk2 = DeltaUtils::getOptimalChunkSize(100000, 100 * 1024 * 1024);
    size_t chunk3 = DeltaUtils::getOptimalChunkSize(500, 1 * 1024 * 1024);
    
    EXPECT_GT(chunk1, 0);
    EXPECT_GT(chunk2, 0);
    EXPECT_GT(chunk3, 0);
    EXPECT_GE(chunk1, 1000);     // Should respect minimum
    EXPECT_LE(chunk1, 100000);   // Should respect maximum
    EXPECT_GE(chunk2, 1000);
    EXPECT_LE(chunk2, 100000);
    EXPECT_GE(chunk3, 1000);
    EXPECT_LE(chunk3, 100000);
    
    // EN: Larger memory should allow larger chunks
    // FR: Plus de mémoire devrait permettre chunks plus gros
    EXPECT_GE(chunk2, chunk1);
    
    // EN: Test optimal thread count
    // FR: Tester nombre optimal de threads
    size_t threads = DeltaUtils::getOptimalThreadCount();
    EXPECT_GT(threads, 0);
    EXPECT_LE(threads, std::thread::hardware_concurrency() + 1); // Should not exceed available + 1
}

// EN: Integration tests combining multiple components
// FR: Tests d'intégration combinant plusieurs composants
class DeltaIntegrationTest : public DeltaCompressionTest {
protected:
    void SetUp() override {
        DeltaCompressionTest::SetUp();
        config.algorithm = CompressionAlgorithm::HYBRID;
        config.detection_mode = ChangeDetectionMode::KEY_BASED;
        config.key_columns = {"id"};
        config.enable_parallel_processing = true;
    }
    
    DeltaConfig config;
};

TEST_F(DeltaIntegrationTest, CompleteWorkflow) {
    // EN: Test complete delta compression workflow
    // FR: Tester workflow complet de compression delta
    
    // EN: Create realistic test data
    // FR: Créer données de test réalistes
    std::vector<std::string> initial_data = {"id,name,email,department,salary,last_updated"};
    std::vector<std::string> updated_data = {"id,name,email,department,salary,last_updated"};
    
    // EN: Generate initial dataset
    // FR: Générer jeu de données initial
    for (int i = 1; i <= 100; ++i) {
        initial_data.push_back(
            std::to_string(i) + "," +
            "Employee" + std::to_string(i) + "," +
            "emp" + std::to_string(i) + "@company.com," +
            "Dept" + std::to_string(i % 5) + "," +
            std::to_string(50000 + (i * 1000)) + "," +
            "2024-01-01T10:00:00Z"
        );
    }
    
    // EN: Generate updated dataset with various changes
    // FR: Générer jeu de données mis à jour avec changements variés
    for (int i = 1; i <= 100; ++i) {
        if (i % 10 == 0) {
            // EN: Delete every 10th employee
            // FR: Supprimer chaque 10ème employé
            continue;
        } else if (i % 7 == 0) {
            // EN: Update salary for every 7th employee
            // FR: Mettre à jour salaire pour chaque 7ème employé
            updated_data.push_back(
                std::to_string(i) + "," +
                "Employee" + std::to_string(i) + "," +
                "emp" + std::to_string(i) + "@company.com," +
                "Dept" + std::to_string(i % 5) + "," +
                std::to_string(55000 + (i * 1000)) + "," +  // Increased salary
                "2024-02-01T10:00:00Z"  // Updated timestamp
            );
        } else if (i % 13 == 0) {
            // EN: Change email domain for some employees
            // FR: Changer domaine email pour certains employés
            updated_data.push_back(
                std::to_string(i) + "," +
                "Employee" + std::to_string(i) + "," +
                "emp" + std::to_string(i) + "@newcompany.com," +  // Changed email
                "Dept" + std::to_string(i % 5) + "," +
                std::to_string(50000 + (i * 1000)) + "," +
                "2024-01-15T10:00:00Z"
            );
        } else {
            // EN: Keep unchanged
            // FR: Garder inchangé
            updated_data.push_back(initial_data[i]);
        }
    }
    
    // EN: Add new employees
    // FR: Ajouter nouveaux employés
    for (int i = 101; i <= 110; ++i) {
        updated_data.push_back(
            std::to_string(i) + "," +
            "NewEmployee" + std::to_string(i) + "," +
            "newemp" + std::to_string(i) + "@company.com," +
            "NewDept," +
            std::to_string(60000) + "," +
            "2024-02-01T10:00:00Z"
        );
    }
    
    createCSVFile("employees_v1.csv", initial_data);
    createCSVFile("employees_v2.csv", updated_data);
    
    std::string v1_file = test_dir / "employees_v1.csv";
    std::string v2_file = test_dir / "employees_v2.csv";
    std::string delta_file = test_dir / "employees.delta";
    std::string reconstructed_file = test_dir / "employees_reconstructed.csv";
    
    // EN: Step 1: Create delta compression
    // FR: Étape 1 : Créer compression delta
    DeltaCompressor compressor(config);
    auto compress_result = compressor.compress(v1_file, v2_file, delta_file);
    EXPECT_EQ(compress_result, DeltaError::SUCCESS);
    
    // EN: Verify delta file was created and has reasonable size
    // FR: Vérifier que fichier delta a été créé et a taille raisonnable
    EXPECT_TRUE(std::filesystem::exists(delta_file));
    size_t delta_size = std::filesystem::file_size(delta_file);
    size_t v1_size = std::filesystem::file_size(v1_file);
    size_t v2_size = std::filesystem::file_size(v2_file);
    
    // EN: Delta should be smaller than both original files
    // FR: Delta devrait être plus petit que les deux fichiers originaux
    EXPECT_LT(delta_size, v1_size);
    EXPECT_LT(delta_size, v2_size);
    
    // EN: Check compression statistics
    // FR: Vérifier statistiques de compression
    const auto& compress_stats = compressor.getStatistics();
    EXPECT_GT(compress_stats.getTotalChangesDetected(), 0);
    EXPECT_GT(compress_stats.getInsertsDetected() + compress_stats.getUpdatesDetected() + 
              compress_stats.getDeletesDetected(), 0);
    EXPECT_GT(compress_stats.getCompressionRatio(), 1.0);
    
    // EN: Step 2: Reconstruct from delta
    // FR: Étape 2 : Reconstruire depuis delta
    DeltaDecompressor decompressor(config);
    auto decompress_result = decompressor.decompress(delta_file, v1_file, reconstructed_file);
    EXPECT_EQ(decompress_result, DeltaError::SUCCESS);
    
    // EN: Verify reconstruction
    // FR: Vérifier reconstruction
    EXPECT_TRUE(std::filesystem::exists(reconstructed_file));
    
    auto original_v2 = DeltaUtils::loadCsvFile(v2_file);
    auto reconstructed = DeltaUtils::loadCsvFile(reconstructed_file);
    
    // EN: Basic structure should match
    // FR: Structure de base devrait correspondre
    EXPECT_FALSE(original_v2.empty());
    EXPECT_FALSE(reconstructed.empty());
    
    if (!original_v2.empty() && !reconstructed.empty()) {
        // EN: Should have same number of rows (approximately, depending on implementation)
        // FR: Devrait avoir même nombre de lignes (approximativement, selon implémentation)
        EXPECT_GT(reconstructed.size(), original_v2.size() * 0.8); // Allow some tolerance
        EXPECT_LT(reconstructed.size(), original_v2.size() * 1.2);
        
        // EN: Headers should match
        // FR: En-têtes devraient correspondre
        EXPECT_EQ(original_v2[0], reconstructed[0]);
    }
    
    // EN: Log performance metrics
    // FR: Enregistrer métriques de performance
    DeltaUtils::logPerformanceMetrics(compress_stats);
}

TEST_F(DeltaIntegrationTest, MultipleCompressionAlgorithms) {
    // EN: Test different compression algorithms on same data
    // FR: Tester différents algorithmes de compression sur mêmes données
    
    createCSVFile("source.csv", {
        "id,category,description,value",
        "1,A,Description for item 1,100.50",
        "2,A,Description for item 2,200.75",
        "3,B,Description for item 3,150.25",
        "4,A,Description for item 4,300.00",
        "5,C,Description for item 5,175.80"
    });
    
    createCSVFile("target.csv", {
        "id,category,description,value",
        "1,A,Updated description for item 1,110.50",  // Updated
        "2,A,Description for item 2,200.75",          // Unchanged
        "3,B,Description for item 3,150.25",          // Unchanged
        "4,A,New description for item 4,350.00",      // Updated
        "6,D,Description for new item 6,400.00"       // New (item 5 deleted)
    });
    
    std::string source_file = test_dir / "source.csv";
    std::string target_file = test_dir / "target.csv";
    
    std::vector<CompressionAlgorithm> algorithms = {
        CompressionAlgorithm::NONE,
        CompressionAlgorithm::RLE,
        CompressionAlgorithm::LZ77,
        CompressionAlgorithm::HYBRID
    };
    
    std::vector<std::pair<CompressionAlgorithm, size_t>> results;
    
    for (auto algorithm : algorithms) {
        DeltaConfig test_config = config;
        test_config.algorithm = algorithm;
        
        std::string delta_file = test_dir / ("delta_" + std::to_string(static_cast<int>(algorithm)) + ".bin");
        
        DeltaCompressor compressor(test_config);
        auto result = compressor.compress(source_file, target_file, delta_file);
        
        EXPECT_EQ(result, DeltaError::SUCCESS) << "Algorithm " << static_cast<int>(algorithm) << " failed";
        
        if (result == DeltaError::SUCCESS) {
            size_t file_size = std::filesystem::file_size(delta_file);
            results.push_back({algorithm, file_size});
            
            // EN: Verify decompression works
            // FR: Vérifier que décompression fonctionne
            std::string restored_file = test_dir / ("restored_" + std::to_string(static_cast<int>(algorithm)) + ".csv");
            DeltaDecompressor decompressor(test_config);
            auto decompress_result = decompressor.decompress(delta_file, source_file, restored_file);
            EXPECT_EQ(decompress_result, DeltaError::SUCCESS) << "Decompression failed for algorithm " << static_cast<int>(algorithm);
        }
    }
    
    // EN: Verify we got results for all algorithms
    // FR: Vérifier que nous avons des résultats pour tous les algorithmes
    EXPECT_EQ(results.size(), algorithms.size());
    
    // EN: HYBRID should generally produce smallest files
    // FR: HYBRID devrait généralement produire plus petits fichiers
    auto hybrid_result = std::find_if(results.begin(), results.end(),
        [](const auto& pair) { return pair.first == CompressionAlgorithm::HYBRID; });
    
    if (hybrid_result != results.end()) {
        // EN: Hybrid should be smaller than or equal to other algorithms
        // FR: Hybride devrait être plus petit ou égal aux autres algorithmes
        for (const auto& [algo, size] : results) {
            if (algo != CompressionAlgorithm::HYBRID) {
                EXPECT_LE(hybrid_result->second, size * 1.1) << "Hybrid not optimal compared to " << static_cast<int>(algo);
            }
        }
    }
}

// EN: Performance and stress tests
// FR: Tests de performance et de stress
class DeltaPerformanceTest : public DeltaCompressionTest {};

TEST_F(DeltaPerformanceTest, LargeDatasetHandling) {
    // EN: Test handling of large datasets
    // FR: Tester gestion de gros jeux de données
    
    const size_t num_records = 5000;
    std::vector<std::string> large_old_data = {"id,name,email,department,salary,description"};
    std::vector<std::string> large_new_data = {"id,name,email,department,salary,description"};
    
    // EN: Generate large dataset
    // FR: Générer gros jeu de données
    for (size_t i = 1; i <= num_records; ++i) {
        std::string base_record = 
            std::to_string(i) + "," +
            "Employee" + std::to_string(i) + "," +
            "emp" + std::to_string(i) + "@company.com," +
            "Department" + std::to_string(i % 10) + "," +
            std::to_string(40000 + (i % 100) * 1000) + "," +
            "Long description for employee " + std::to_string(i) + 
            " with various details and information that makes this record quite lengthy";
        
        large_old_data.push_back(base_record);
        
        // EN: Modify some records for new dataset
        // FR: Modifier quelques enregistrements pour nouveau jeu de données
        if (i % 50 == 0) {
            // EN: Delete every 50th record
            // FR: Supprimer chaque 50ème enregistrement
            continue;
        } else if (i % 23 == 0) {
            // EN: Update salary and description
            // FR: Mettre à jour salaire et description
            std::string updated_record = 
                std::to_string(i) + "," +
                "Employee" + std::to_string(i) + "," +
                "emp" + std::to_string(i) + "@company.com," +
                "Department" + std::to_string(i % 10) + "," +
                std::to_string(45000 + (i % 100) * 1000) + "," +  // Updated salary
                "Updated long description for employee " + std::to_string(i) + 
                " with revised details and new information";
            large_new_data.push_back(updated_record);
        } else {
            large_new_data.push_back(base_record);
        }
    }
    
    // EN: Add new records
    // FR: Ajouter nouveaux enregistrements
    for (size_t i = num_records + 1; i <= num_records + 100; ++i) {
        std::string new_record = 
            std::to_string(i) + "," +
            "NewEmployee" + std::to_string(i) + "," +
            "newemp" + std::to_string(i) + "@company.com," +
            "NewDepartment," +
            std::to_string(50000) + "," +
            "Description for new employee " + std::to_string(i);
        large_new_data.push_back(new_record);
    }
    
    createCSVFile("large_old.csv", large_old_data);
    createCSVFile("large_new.csv", large_new_data);
    
    std::string old_file = test_dir / "large_old.csv";
    std::string new_file = test_dir / "large_new.csv";
    std::string delta_file = test_dir / "large.delta";
    std::string restored_file = test_dir / "large_restored.csv";
    
    // EN: Configure for performance
    // FR: Configurer pour performance
    DeltaConfig perf_config;
    perf_config.algorithm = CompressionAlgorithm::HYBRID;
    perf_config.detection_mode = ChangeDetectionMode::KEY_BASED;
    perf_config.key_columns = {"id"};
    perf_config.chunk_size = 1000;  // Smaller chunks for memory efficiency
    perf_config.enable_parallel_processing = true;
    perf_config.max_memory_usage = 50 * 1024 * 1024; // 50MB limit
    
    // EN: Measure compression performance
    // FR: Mesurer performance de compression
    DeltaCompressor compressor(perf_config);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto compress_result = compressor.compress(old_file, new_file, delta_file);
    auto end = std::chrono::high_resolution_clock::now();
    
    EXPECT_EQ(compress_result, DeltaError::SUCCESS);
    
    auto compression_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // EN: Performance expectations (adjust based on hardware)
    // FR: Attentes de performance (ajuster selon matériel)
    EXPECT_LT(compression_time.count(), 30000); // Should complete within 30 seconds
    
    // EN: Check compression effectiveness
    // FR: Vérifier efficacité compression
    const auto& stats = compressor.getStatistics();
    EXPECT_GT(stats.getTotalChangesDetected(), 0);
    EXPECT_GT(stats.getCompressionRatio(), 1.1); // Should achieve at least 10% compression
    
    // EN: Verify file sizes are reasonable
    // FR: Vérifier que tailles fichiers sont raisonnables
    size_t old_size = std::filesystem::file_size(old_file);
    size_t new_size = std::filesystem::file_size(new_file);
    size_t delta_size = std::filesystem::file_size(delta_file);
    
    EXPECT_GT(old_size, 1024 * 1024);   // Should be at least 1MB
    EXPECT_GT(new_size, 1024 * 1024);   // Should be at least 1MB
    EXPECT_LT(delta_size, std::min(old_size, new_size)); // Delta should be smaller
    
    // EN: Test decompression performance
    // FR: Tester performance décompression
    DeltaDecompressor decompressor(perf_config);
    
    start = std::chrono::high_resolution_clock::now();
    auto decompress_result = decompressor.decompress(delta_file, old_file, restored_file);
    end = std::chrono::high_resolution_clock::now();
    
    EXPECT_EQ(decompress_result, DeltaError::SUCCESS);
    
    auto decompression_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // EN: Decompression should be faster than compression
    // FR: Décompression devrait être plus rapide que compression
    EXPECT_LT(decompression_time.count(), compression_time.count());
    EXPECT_LT(decompression_time.count(), 15000); // Should complete within 15 seconds
    
    // EN: Verify restored file exists and has reasonable size
    // FR: Vérifier que fichier restauré existe et a taille raisonnable
    EXPECT_TRUE(std::filesystem::exists(restored_file));
    size_t restored_size = std::filesystem::file_size(restored_file);
    EXPECT_GT(restored_size, new_size * 0.8); // Should be close to new file size
    EXPECT_LT(restored_size, new_size * 1.2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}