#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include "csv/merger_engine.hpp"

// Simple test program for merger engine
// Programme de test simple pour le moteur de fusion
int main() {
    using namespace BBP::CSV;
    
    std::cout << "Testing Merger Engine...\n";
    
    // Test 1: MergeConfig creation and validation
    // Test 1: Création et validation de MergeConfig
    std::cout << "Test 1: MergeConfig validation... ";
    MergeConfig config;
    
    if (config.isValid()) {
        std::cout << "PASS\n";
    } else {
        std::cout << "FAIL\n";
        auto errors = config.getValidationErrors();
        for (const auto& error : errors) {
            std::cout << "  Error: " << error << "\n";
        }
        return 1;
    }
    
    // Test 2: MergeStatistics basic operations
    // Test 2: Opérations de base MergeStatistics
    std::cout << "Test 2: MergeStatistics operations... ";
    MergeStatistics stats;
    
    if (stats.getTotalRowsProcessed() == 0 && 
        stats.getDuplicatesRemoved() == 0 &&
        stats.getConflictsResolved() == 0) {
        std::cout << "PASS\n";
    } else {
        std::cout << "FAIL\n";
        return 1;
    }
    
    // Test 3: Create a simple merger engine
    // Test 3: Créer un moteur de fusion simple
    std::cout << "Test 3: MergerEngine creation... ";
    try {
        MergerEngine engine(config);
        std::cout << "PASS\n";
    } catch (const std::exception& e) {
        std::cout << "FAIL: " << e.what() << "\n";
        return 1;
    }
    
    // Test 4: Test merge with empty sources (should fail gracefully)
    // Test 4: Test fusion avec sources vides (doit échouer gracieusement)
    std::cout << "Test 4: Empty merge error handling... ";
    try {
        MergerEngine engine(config);
        std::ostringstream output;
        
        // No sources added, so merge should fail with appropriate error
        auto result = engine.mergeToStream(output);
        if (result == MergeError::INVALID_CONFIG) {
            std::cout << "PASS\n";
        } else if (result != MergeError::SUCCESS) {
            std::cout << "PASS (Different error code, but handled correctly)\n";
        } else {
            std::cout << "FAIL: Should have failed with no sources\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cout << "FAIL: Exception: " << e.what() << "\n";
        return 1;
    }
    
    // Test 5: Test CSV file creation and simple merge
    // Test 5: Test création fichier CSV et fusion simple
    std::cout << "Test 5: Simple file merge... ";
    try {
        // Create temporary test directory
        // Créer répertoire de test temporaire
        std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "merger_test";
        std::filesystem::create_directories(test_dir);
        
        // Create test CSV files
        // Créer fichiers CSV de test
        {
            std::ofstream file1(test_dir / "test1.csv");
            file1 << "id,name,value\n";
            file1 << "1,Alice,100\n";
            file1 << "2,Bob,200\n";
        }
        
        {
            std::ofstream file2(test_dir / "test2.csv");
            file2 << "id,name,value\n";
            file2 << "3,Charlie,300\n";
            file2 << "4,David,400\n";
        }
        
        // Test merge
        // Test fusion
        MergeConfig append_config;
        append_config.merge_strategy = MergeStrategy::APPEND;
        
        MergerEngine engine(append_config);
        
        // Add sources
        InputSource source1;
        source1.filepath = test_dir / "test1.csv";
        source1.name = "test1";
        source1.priority = 1;
        engine.addInputSource(source1);
        
        InputSource source2;
        source2.filepath = test_dir / "test2.csv";
        source2.name = "test2";
        source2.priority = 1;
        engine.addInputSource(source2);
        
        std::ostringstream output;
        auto result = engine.mergeToStream(output);
        
        if (result == MergeError::SUCCESS) {
            std::string output_str = output.str();
            // Basic check: output should contain data from both files
            // Vérification de base: la sortie doit contenir des données des deux fichiers
            if (output_str.find("Alice") != std::string::npos &&
                output_str.find("Charlie") != std::string::npos) {
                std::cout << "PASS\n";
            } else {
                std::cout << "FAIL: Merge output doesn't contain expected data\n";
                std::cout << "Output: " << output_str << "\n";
                return 1;
            }
        } else {
            std::cout << "FAIL: Merge failed\n";
            return 1;
        }
        
        // Clean up
        // Nettoyage
        std::filesystem::remove_all(test_dir);
        
    } catch (const std::exception& e) {
        std::cout << "FAIL: Exception: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "All tests passed!\n";
    return 0;
}