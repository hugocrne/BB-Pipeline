// EN: Comprehensive unit tests for BatchWriter CSV functionality
// FR: Tests unitaires complets pour la fonctionnalité CSV BatchWriter

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "csv/batch_writer.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>

using namespace BBP::CSV;

class BatchWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_filename_ = "test_output.csv";
        test_filename_compressed_ = "test_output.csv.gz";
        cleanup();
    }

    void TearDown() override {
        cleanup();
    }

    void cleanup() {
        std::vector<std::string> files_to_remove = {
            test_filename_,
            test_filename_compressed_,
            test_filename_ + ".bak",
            test_filename_compressed_ + ".bak",
            test_filename_ + ".tmp"
        };
        
        for (const auto& file : files_to_remove) {
            std::filesystem::remove(file);
        }
    }

    std::string readFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return "";
        }
        return std::string((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
    }

    std::string test_filename_;
    std::string test_filename_compressed_;
};

// EN: WriterConfig tests
// FR: Tests WriterConfig

TEST_F(BatchWriterTest, WriterConfigDefault) {
    WriterConfig config;
    EXPECT_TRUE(config.isValid());
    EXPECT_EQ(config.delimiter, ',');
    EXPECT_EQ(config.quote_char, '"');
    EXPECT_EQ(config.escape_char, '"');
    EXPECT_EQ(config.line_ending, "\n");
    EXPECT_FALSE(config.always_quote);
    EXPECT_TRUE(config.write_header);
    EXPECT_EQ(config.buffer_size, 65536);
    EXPECT_EQ(config.compression, CompressionType::NONE);
}

TEST_F(BatchWriterTest, WriterConfigValidation) {
    WriterConfig config;
    
    // EN: Test invalid buffer size
    // FR: Test taille de buffer invalide
    config.buffer_size = 0;
    EXPECT_FALSE(config.isValid());
    
    // EN: Test invalid max rows
    // FR: Test nombre max de lignes invalide
    config.buffer_size = 8192;
    config.max_rows_in_buffer = 0;
    EXPECT_FALSE(config.isValid());
    
    // EN: Test invalid compression level
    // FR: Test niveau de compression invalide
    config.max_rows_in_buffer = 1000;
    config.compression_level = 10;
    EXPECT_FALSE(config.isValid());
    
    config.compression_level = 6;
    EXPECT_TRUE(config.isValid());
}

TEST_F(BatchWriterTest, CompressionDetection) {
    WriterConfig config;
    
    EXPECT_EQ(config.detectCompressionFromFilename("test.csv"), CompressionType::NONE);
    EXPECT_EQ(config.detectCompressionFromFilename("test.csv.gz"), CompressionType::GZIP);
    EXPECT_EQ(config.detectCompressionFromFilename("test.csv.gzip"), CompressionType::GZIP);
    EXPECT_EQ(config.detectCompressionFromFilename("test.csv.z"), CompressionType::ZLIB);
    EXPECT_EQ(config.detectCompressionFromFilename("TEST.CSV.GZ"), CompressionType::GZIP);
}

// EN: CsvRow tests
// FR: Tests CsvRow

TEST_F(BatchWriterTest, CsvRowBasicOperations) {
    CsvRow row;
    EXPECT_TRUE(row.isEmpty());
    EXPECT_EQ(row.getFieldCount(), 0);

    row.addField("field1");
    row.addField("field2");
    EXPECT_FALSE(row.isEmpty());
    EXPECT_EQ(row.getFieldCount(), 2);
    EXPECT_EQ(row.getField(0), "field1");
    EXPECT_EQ(row.getField(1), "field2");
    EXPECT_EQ(row.getField(2), ""); // EN: Out of bounds / FR: Hors limites
}

TEST_F(BatchWriterTest, CsvRowConstructors) {
    // EN: Test vector constructor
    // FR: Test constructeur vecteur
    std::vector<std::string> fields = {"a", "b", "c"};
    CsvRow row1(fields);
    EXPECT_EQ(row1.getFieldCount(), 3);
    EXPECT_EQ(row1.getField(1), "b");

    // EN: Test move constructor
    // FR: Test constructeur de déplacement
    CsvRow row2(std::move(fields));
    EXPECT_EQ(row2.getFieldCount(), 3);

    // EN: Test initializer list constructor
    // FR: Test constructeur liste d'initialisation
    CsvRow row3{"x", "y", "z"};
    EXPECT_EQ(row3.getFieldCount(), 3);
    EXPECT_EQ(row3.getField(0), "x");
}

TEST_F(BatchWriterTest, CsvRowOperators) {
    CsvRow row;
    
    // EN: Test << operator
    // FR: Test opérateur <<
    row << "first" << "second";
    EXPECT_EQ(row.getFieldCount(), 2);
    EXPECT_EQ(row[0], "first");
    EXPECT_EQ(row[1], "second");

    // EN: Test [] operator
    // FR: Test opérateur []
    row[2] = "third";
    EXPECT_EQ(row.getFieldCount(), 3);
    EXPECT_EQ(row[2], "third");
}

TEST_F(BatchWriterTest, CsvRowIteration) {
    CsvRow row{"a", "b", "c"};
    
    std::vector<std::string> collected;
    for (const auto& field : row) {
        collected.push_back(field);
    }
    
    EXPECT_EQ(collected.size(), 3);
    EXPECT_EQ(collected[0], "a");
    EXPECT_EQ(collected[1], "b");
    EXPECT_EQ(collected[2], "c");
}

TEST_F(BatchWriterTest, CsvRowToString) {
    WriterConfig config;
    CsvRow row{"simple", "needs,quote", "has\"quote"};
    
    std::string result = row.toString(config);
    EXPECT_EQ(result, "simple,\"needs,quote\",\"has\"\"quote\"");
    
    // EN: Test always quote
    // FR: Test toujours quoter
    config.always_quote = true;
    result = row.toString(config);
    EXPECT_EQ(result, "\"simple\",\"needs,quote\",\"has\"\"quote\"");
}

// EN: WriterStatistics tests
// FR: Tests WriterStatistics

TEST_F(BatchWriterTest, WriterStatisticsBasic) {
    WriterStatistics stats;
    
    EXPECT_EQ(stats.getRowsWritten(), 0);
    EXPECT_EQ(stats.getBytesWritten(), 0);
    EXPECT_EQ(stats.getFlushCount(), 0);

    stats.incrementRowsWritten();
    stats.addBytesWritten(100);
    stats.incrementFlushCount();

    EXPECT_EQ(stats.getRowsWritten(), 1);
    EXPECT_EQ(stats.getBytesWritten(), 100);
    EXPECT_EQ(stats.getFlushCount(), 1);
}

TEST_F(BatchWriterTest, WriterStatisticsPerformance) {
    WriterStatistics stats;
    stats.startTiming();
    
    // EN: Simulate some work
    // FR: Simule du travail
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    for (int i = 0; i < 10; i++) stats.incrementRowsWritten();
    stats.addBytesWritten(1000);
    
    stats.stopTiming();
    
    EXPECT_GT(stats.getRowsPerSecond(), 0);
    EXPECT_GT(stats.getBytesPerSecond(), 0);
}

TEST_F(BatchWriterTest, WriterStatisticsCompression) {
    WriterStatistics stats;
    
    stats.addBytesCompressed(1000, 300);
    EXPECT_NEAR(stats.getCompressionRatio(), 0.3, 0.001);

    stats.addBytesCompressed(2000, 600);
    EXPECT_NEAR(stats.getCompressionRatio(), 0.3, 0.001); // EN: Still 30% ratio / FR: Toujours ratio 30%
}

TEST_F(BatchWriterTest, WriterStatisticsReport) {
    WriterStatistics stats;
    for (int i = 0; i < 100; i++) stats.incrementRowsWritten();
    stats.addBytesWritten(10000);
    stats.incrementFlushCount();
    
    std::string report = stats.generateReport();
    EXPECT_THAT(report, testing::HasSubstr("100"));
    EXPECT_THAT(report, testing::HasSubstr("10000"));
    EXPECT_THAT(report, testing::HasSubstr("Batch Writer Statistics"));
}

// EN: BatchWriter basic functionality tests
// FR: Tests fonctionnalités basiques BatchWriter

TEST_F(BatchWriterTest, BatchWriterBasicFile) {
    BatchWriter writer;
    
    EXPECT_EQ(writer.openFile(test_filename_), WriterError::SUCCESS);
    EXPECT_TRUE(writer.isOpen());
    
    std::vector<std::string> headers = {"Name", "Age", "City"};
    EXPECT_EQ(writer.writeHeader(headers), WriterError::SUCCESS);
    EXPECT_EQ(writer.writeRow({"John", "25", "Paris"}), WriterError::SUCCESS);
    EXPECT_EQ(writer.writeRow({"Jane", "30", "London"}), WriterError::SUCCESS);
    
    EXPECT_EQ(writer.closeFile(), WriterError::SUCCESS);
    EXPECT_FALSE(writer.isOpen());

    // EN: Verify file content
    // FR: Vérifie le contenu du fichier
    std::string content = readFile(test_filename_);
    EXPECT_THAT(content, testing::HasSubstr("Name,Age,City"));
    EXPECT_THAT(content, testing::HasSubstr("John,25,Paris"));
    EXPECT_THAT(content, testing::HasSubstr("Jane,30,London"));
}

TEST_F(BatchWriterTest, BatchWriterStream) {
    std::ostringstream oss;
    BatchWriter writer;
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    std::vector<std::string> headers2 = {"Col1", "Col2"};
    EXPECT_EQ(writer.writeHeader(headers2), WriterError::SUCCESS);
    EXPECT_EQ(writer.writeRow({"Value1", "Value2"}), WriterError::SUCCESS);
    
    writer.flush();
    
    std::string content = oss.str();
    EXPECT_THAT(content, testing::HasSubstr("Col1,Col2"));
    EXPECT_THAT(content, testing::HasSubstr("Value1,Value2"));
}

TEST_F(BatchWriterTest, BatchWriterQuoting) {
    std::ostringstream oss;
    WriterConfig config;
    BatchWriter writer(config);
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    
    // EN: Test various quoting scenarios
    // FR: Test divers scénarios de quotation
    EXPECT_EQ(writer.writeRow({"simple", "has,comma", "has\"quote", "has\nnewline"}), WriterError::SUCCESS);
    writer.flush();
    
    std::string content = oss.str();
    EXPECT_THAT(content, testing::HasSubstr("simple,\"has,comma\",\"has\"\"quote\",\"has\nnewline\""));
}

TEST_F(BatchWriterTest, BatchWriterBatchOperations) {
    std::ostringstream oss;
    BatchWriter writer;
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    
    std::vector<CsvRow> rows = {
        CsvRow{"a1", "b1", "c1"},
        CsvRow{"a2", "b2", "c2"},
        CsvRow{"a3", "b3", "c3"}
    };
    
    EXPECT_EQ(writer.writeRows(rows), WriterError::SUCCESS);
    writer.flush();
    
    std::string content = oss.str();
    EXPECT_THAT(content, testing::HasSubstr("a1,b1,c1"));
    EXPECT_THAT(content, testing::HasSubstr("a2,b2,c2"));
    EXPECT_THAT(content, testing::HasSubstr("a3,b3,c3"));
}

// EN: Configuration and advanced features tests
// FR: Tests configuration et fonctionnalités avancées

TEST_F(BatchWriterTest, CustomDelimiter) {
    std::ostringstream oss;
    WriterConfig config;
    config.delimiter = ';';
    BatchWriter writer(config);
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    EXPECT_EQ(writer.writeRow({"a", "b", "c"}), WriterError::SUCCESS);
    writer.flush();
    
    EXPECT_THAT(oss.str(), testing::HasSubstr("a;b;c"));
}

TEST_F(BatchWriterTest, CustomLineEnding) {
    std::ostringstream oss;
    WriterConfig config;
    config.line_ending = "\r\n";
    BatchWriter writer(config);
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    EXPECT_EQ(writer.writeRow({"a", "b"}), WriterError::SUCCESS);
    writer.flush();
    
    EXPECT_THAT(oss.str(), testing::HasSubstr("a,b\r\n"));
}

TEST_F(BatchWriterTest, AlwaysQuote) {
    std::ostringstream oss;
    WriterConfig config;
    config.always_quote = true;
    BatchWriter writer(config);
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    EXPECT_EQ(writer.writeRow({"simple", "value"}), WriterError::SUCCESS);
    writer.flush();
    
    EXPECT_THAT(oss.str(), testing::HasSubstr("\"simple\",\"value\""));
}

TEST_F(BatchWriterTest, NoHeader) {
    std::ostringstream oss;
    WriterConfig config;
    config.write_header = false;
    BatchWriter writer(config);
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    std::vector<std::string> headers3 = {"H1", "H2"};
    EXPECT_EQ(writer.writeHeader(headers3), WriterError::SUCCESS); // EN: Should not write / FR: Ne devrait pas écrire
    EXPECT_EQ(writer.writeRow({"v1", "v2"}), WriterError::SUCCESS);
    writer.flush();
    
    std::string content = oss.str();
    EXPECT_THAT(content, testing::Not(testing::HasSubstr("H1,H2")));
    EXPECT_THAT(content, testing::HasSubstr("v1,v2"));
}

// EN: Buffer and flush tests
// FR: Tests buffer et flush

TEST_F(BatchWriterTest, BufferManagement) {
    WriterConfig config;
    config.max_rows_in_buffer = 3;
    config.flush_trigger = FlushTrigger::ROW_COUNT;
    config.flush_row_threshold = 2;
    
    std::ostringstream oss;
    BatchWriter writer(config);
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    
    EXPECT_EQ(writer.getBufferedRowCount(), 0);
    
    writer.writeRow({"r1c1", "r1c2"});
    EXPECT_EQ(writer.getBufferedRowCount(), 1);
    
    writer.writeRow({"r2c1", "r2c2"});
    // EN: Should auto-flush after 2 rows
    // FR: Devrait auto-flush après 2 lignes
    EXPECT_EQ(writer.getBufferedRowCount(), 0);
}

TEST_F(BatchWriterTest, ManualFlush) {
    WriterConfig config;
    config.flush_trigger = FlushTrigger::MANUAL;
    
    std::ostringstream oss;
    BatchWriter writer(config);
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    
    writer.writeRow({"test", "data"});
    EXPECT_EQ(writer.getBufferedRowCount(), 1);
    
    writer.flush();
    EXPECT_EQ(writer.getBufferedRowCount(), 0);
    
    EXPECT_THAT(oss.str(), testing::HasSubstr("test,data"));
}

TEST_F(BatchWriterTest, SizeBasedFlush) {
    WriterConfig config;
    config.flush_trigger = FlushTrigger::BUFFER_SIZE;
    config.flush_size_threshold = 50; // EN: Very small threshold / FR: Seuil très petit
    
    std::ostringstream oss;
    BatchWriter writer(config);
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    
    // EN: Write enough data to trigger size-based flush
    // FR: Écrit assez de données pour déclencher flush basé sur la taille
    writer.writeRow({"very_long_field_content", "another_long_field_content"});
    
    // EN: Should have flushed automatically
    // FR: Devrait avoir flushé automatiquement
    EXPECT_EQ(writer.getBufferedRowCount(), 0);
}

// EN: Error handling tests
// FR: Tests gestion d'erreur

TEST_F(BatchWriterTest, InvalidFilename) {
    BatchWriter writer;
    
    EXPECT_EQ(writer.openFile(""), WriterError::FILE_OPEN_ERROR);
    EXPECT_EQ(writer.openFile("invalid<>filename"), WriterError::FILE_OPEN_ERROR);
}

TEST_F(BatchWriterTest, WriteWithoutOpenFile) {
    BatchWriter writer;
    
    EXPECT_EQ(writer.writeRow({"test"}), WriterError::FILE_WRITE_ERROR);
}

TEST_F(BatchWriterTest, DoubleFileOpen) {
    BatchWriter writer;
    
    EXPECT_EQ(writer.openFile(test_filename_), WriterError::SUCCESS);
    EXPECT_EQ(writer.openFile("another_file.csv"), WriterError::FILE_OPEN_ERROR);
    
    writer.closeFile();
}

TEST_F(BatchWriterTest, LargeFieldHandling) {
    WriterConfig config;
    config.max_field_size = 10;
    config.continue_on_error = true;
    
    std::ostringstream oss;
    BatchWriter writer(config);
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    
    // EN: This should generate an error but continue
    // FR: Ceci devrait générer une erreur mais continuer
    EXPECT_EQ(writer.writeRow({"short", "this_is_a_very_long_field_that_exceeds_limit"}), WriterError::SUCCESS);
    
    auto stats = writer.getStatistics();
    EXPECT_GT(stats.getRowsWithErrors(), 0);
}

// EN: Statistics and performance tests
// FR: Tests statistiques et performance

TEST_F(BatchWriterTest, StatisticsAccuracy) {
    BatchWriter writer;
    
    EXPECT_EQ(writer.openFile(test_filename_), WriterError::SUCCESS);
    
    std::vector<std::string> headers4 = {"A", "B"};
    writer.writeHeader(headers4);
    writer.writeRow({"1", "2"});
    writer.writeRow({"3", "4"});
    writer.closeFile();
    
    auto stats = writer.getStatistics();
    EXPECT_EQ(stats.getRowsWritten(), 3); // EN: 1 header + 2 data rows / FR: 1 en-tête + 2 lignes de données
    EXPECT_GT(stats.getBytesWritten(), 0);
    EXPECT_GT(stats.getFlushCount(), 0);
}

TEST_F(BatchWriterTest, PerformanceMetrics) {
    BatchWriter writer;
    std::ostringstream oss;
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    
    // EN: Write many rows to get meaningful performance metrics
    // FR: Écrit beaucoup de lignes pour obtenir des métriques de performance significatives
    for (int i = 0; i < 1000; ++i) {
        writer.writeRow({"field1_" + std::to_string(i), "field2_" + std::to_string(i)});
    }
    
    writer.closeFile();
    
    auto stats = writer.getStatistics();
    EXPECT_GT(stats.getRowsPerSecond(), 0);
    EXPECT_GT(stats.getBytesPerSecond(), 0);
    EXPECT_EQ(stats.getRowsWritten(), 1000);
}

// EN: Utility function tests
// FR: Tests fonctions utilitaires

TEST_F(BatchWriterTest, FieldEscaping) {
    WriterConfig config;
    
    EXPECT_EQ(BatchWriter::escapeField("simple", config), "simple");
    EXPECT_EQ(BatchWriter::escapeField("has,comma", config), "\"has,comma\"");
    EXPECT_EQ(BatchWriter::escapeField("has\"quote", config), "\"has\"\"quote\"");
    EXPECT_EQ(BatchWriter::escapeField("has\nnewline", config), "\"has\nnewline\"");
}

TEST_F(BatchWriterTest, QuotingNeed) {
    WriterConfig config;
    
    EXPECT_FALSE(BatchWriter::needsQuoting("simple", config));
    EXPECT_TRUE(BatchWriter::needsQuoting("has,comma", config));
    EXPECT_TRUE(BatchWriter::needsQuoting("has\"quote", config));
    EXPECT_TRUE(BatchWriter::needsQuoting("has\nnewline", config));
    EXPECT_TRUE(BatchWriter::needsQuoting(" leading_space", config));
    EXPECT_TRUE(BatchWriter::needsQuoting("trailing_space ", config));
    
    config.always_quote = true;
    EXPECT_TRUE(BatchWriter::needsQuoting("simple", config));
    
    config.always_quote = false;
    config.quote_empty_fields = true;
    EXPECT_TRUE(BatchWriter::needsQuoting("", config));
}

TEST_F(BatchWriterTest, FilenameValidation) {
    EXPECT_TRUE(BatchWriter::isValidFilename("valid_file.csv"));
    EXPECT_TRUE(BatchWriter::isValidFilename("/path/to/file.csv"));
    EXPECT_FALSE(BatchWriter::isValidFilename(""));
    EXPECT_FALSE(BatchWriter::isValidFilename("invalid<file"));
    EXPECT_FALSE(BatchWriter::isValidFilename("invalid>file"));
    EXPECT_FALSE(BatchWriter::isValidFilename("invalid:file"));
    EXPECT_FALSE(BatchWriter::isValidFilename("invalid|file"));
}

// EN: Move semantics and resource management tests
// FR: Tests sémantiques de déplacement et gestion des ressources

TEST_F(BatchWriterTest, MoveConstructor) {
    BatchWriter writer1;
    EXPECT_EQ(writer1.openFile(test_filename_), WriterError::SUCCESS);
    writer1.writeRow({"test", "data"});
    
    BatchWriter writer2 = std::move(writer1);
    EXPECT_TRUE(writer2.isOpen());
    EXPECT_FALSE(writer1.isOpen());
    
    writer2.writeRow({"more", "data"});
    writer2.closeFile();
    
    std::string content = readFile(test_filename_);
    EXPECT_THAT(content, testing::HasSubstr("test,data"));
    EXPECT_THAT(content, testing::HasSubstr("more,data"));
}

TEST_F(BatchWriterTest, MoveAssignment) {
    BatchWriter writer1;
    BatchWriter writer2;
    
    EXPECT_EQ(writer1.openFile(test_filename_), WriterError::SUCCESS);
    writer1.writeRow({"original", "data"});
    
    writer2 = std::move(writer1);
    EXPECT_TRUE(writer2.isOpen());
    EXPECT_FALSE(writer1.isOpen());
    
    writer2.closeFile();
}

// EN: Background flush tests (if background flush is implemented)
// FR: Tests flush en arrière-plan (si le flush en arrière-plan est implémenté)

TEST_F(BatchWriterTest, BackgroundFlush) {
    WriterConfig config;
    config.flush_trigger = FlushTrigger::TIME_INTERVAL;
    config.flush_interval = std::chrono::milliseconds(100);
    
    std::ostringstream oss;
    BatchWriter writer(config);
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    writer.enableAutoFlush(true);
    
    writer.writeRow({"auto", "flush"});
    EXPECT_EQ(writer.getBufferedRowCount(), 1);
    
    // EN: Wait for background flush
    // FR: Attend le flush en arrière-plan
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // EN: Should have been flushed by background thread
    // FR: Devrait avoir été flushé par le thread en arrière-plan
    EXPECT_EQ(writer.getBufferedRowCount(), 0);
    
    writer.enableAutoFlush(false);
}

// EN: Integration test with real file operations
// FR: Test d'intégration avec opérations de fichier réelles

TEST_F(BatchWriterTest, RealFileIntegration) {
    BatchWriter writer;
    
    // EN: Test complete workflow
    // FR: Test workflow complet
    EXPECT_EQ(writer.openFile(test_filename_), WriterError::SUCCESS);
    
    // EN: Write header
    // FR: Écrit l'en-tête
    std::vector<std::string> headers = {"ID", "Name", "Email", "Age"};
    EXPECT_EQ(writer.writeHeader(headers), WriterError::SUCCESS);
    
    // EN: Write test data
    // FR: Écrit les données de test
    std::vector<std::vector<std::string>> test_data = {
        {"1", "John Doe", "john@example.com", "25"},
        {"2", "Jane Smith", "jane@example.com", "30"},
        {"3", "Bob Johnson", "bob@example.com", "35"},
        {"4", "Alice Brown", "alice@example.com", "28"}
    };
    
    for (const auto& row_data : test_data) {
        EXPECT_EQ(writer.writeRow(row_data), WriterError::SUCCESS);
    }
    
    EXPECT_EQ(writer.closeFile(), WriterError::SUCCESS);
    
    // EN: Verify file exists and has content
    // FR: Vérifie que le fichier existe et a du contenu
    EXPECT_TRUE(std::filesystem::exists(test_filename_));
    
    std::string content = readFile(test_filename_);
    EXPECT_THAT(content, testing::HasSubstr("ID,Name,Email,Age"));
    EXPECT_THAT(content, testing::HasSubstr("John Doe"));
    EXPECT_THAT(content, testing::HasSubstr("jane@example.com"));
    
    // EN: Verify statistics
    // FR: Vérifie les statistiques
    auto stats = writer.getStatistics();
    EXPECT_EQ(stats.getRowsWritten(), 5); // EN: 1 header + 4 data rows / FR: 1 en-tête + 4 lignes de données
    EXPECT_GT(stats.getBytesWritten(), 0);
}

// EN: Edge cases and boundary conditions
// FR: Cas limites et conditions aux limites

TEST_F(BatchWriterTest, EmptyRows) {
    std::ostringstream oss;
    BatchWriter writer;
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    
    CsvRow empty_row;
    EXPECT_EQ(writer.writeRow(empty_row), WriterError::SUCCESS);
    
    CsvRow row_with_empty_fields{"", "", ""};
    EXPECT_EQ(writer.writeRow(row_with_empty_fields), WriterError::SUCCESS);
    
    writer.flush();
    
    auto stats = writer.getStatistics();
    EXPECT_EQ(stats.getRowsSkipped(), 1); // EN: Empty row should be skipped / FR: Ligne vide devrait être sautée
    EXPECT_EQ(stats.getRowsWritten(), 1); // EN: Row with empty fields should be written / FR: Ligne avec champs vides devrait être écrite
}

TEST_F(BatchWriterTest, VeryLargeContent) {
    std::ostringstream oss;
    BatchWriter writer;
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    
    // EN: Create very large field content
    // FR: Crée un contenu de champ très large
    std::string large_field(1000, 'X');
    EXPECT_EQ(writer.writeRow({large_field, "normal"}), WriterError::SUCCESS);
    
    writer.flush();
    EXPECT_THAT(oss.str(), testing::HasSubstr(large_field));
}

// EN: Concurrent access tests (basic thread safety)
// FR: Tests accès concurrent (sécurité thread basique)

TEST_F(BatchWriterTest, BasicThreadSafety) {
    std::ostringstream oss;
    BatchWriter writer;
    
    EXPECT_EQ(writer.openStream(oss), WriterError::SUCCESS);
    
    std::vector<std::thread> threads;
    const int num_threads = 4;
    const int rows_per_thread = 25;
    
    // EN: Launch multiple threads writing concurrently
    // FR: Lance plusieurs threads écrivant de manière concurrente
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&writer, t]() {
            for (int i = 0; i < rows_per_thread; ++i) {
                std::string row_id = std::to_string(t) + "_" + std::to_string(i);
                writer.writeRow({"thread", row_id, "data"});
            }
        });
    }
    
    // EN: Wait for all threads to complete
    // FR: Attend que tous les threads se terminent
    for (auto& thread : threads) {
        thread.join();
    }
    
    writer.closeFile();
    
    auto stats = writer.getStatistics();
    EXPECT_EQ(stats.getRowsWritten(), num_threads * rows_per_thread);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}