// EN: Simple integration test for SignalHandler - manual testing
// FR: Test d'intégration simple pour SignalHandler - test manuel

#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include "../include/infrastructure/system/signal_handler.hpp"
#include "../include/infrastructure/logging/logger.hpp"

using namespace BBP;
using namespace std::chrono_literals;

int main() {
    std::cout << "=== BB-Pipeline Signal Handler Integration Test ===\n\n";
    
    // EN: Setup logger
    // FR: Configure le logger
    auto& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::DEBUG);
    
    // EN: Get signal handler instance
    // FR: Obtient l'instance du gestionnaire de signaux
    auto& handler = SignalHandler::getInstance();
    
    // EN: Configure with short timeouts for testing
    // FR: Configure avec des timeouts courts pour les tests
    SignalHandlerConfig config;
    config.shutdown_timeout = 3000ms;
    config.csv_flush_timeout = 1000ms;
    config.enable_emergency_flush = true;
    config.log_signal_details = true;
    
    handler.configure(config);
    
    std::cout << "1. Registering test callbacks...\n";
    
    // EN: Register cleanup callback
    // FR: Enregistre un callback de nettoyage
    handler.registerCleanupCallback("test_cleanup", []() {
        std::cout << "   [CLEANUP] Test cleanup executed\n";
        std::this_thread::sleep_for(100ms);  // EN: Simulate work / FR: Simule du travail
    });
    
    // EN: Register CSV flush callback
    // FR: Enregistre un callback de flush CSV
    handler.registerCsvFlushCallback("/tmp/test_signal_handler.csv", [](const std::string& path) {
        std::cout << "   [CSV FLUSH] Writing CSV: " << path << "\n";
        
        // EN: Actually write a test CSV file
        // FR: Écrit vraiment un fichier CSV de test
        std::ofstream csv_file(path);
        csv_file << "timestamp,event,details\n";
        csv_file << "2025-08-25T10:30:45Z,test_event,signal_handler_test\n";
        csv_file << "2025-08-25T10:30:46Z,shutdown_initiated,graceful_shutdown\n";
        csv_file.flush();
        csv_file.close();
        
        std::cout << "   [CSV FLUSH] CSV file written successfully\n";
    });
    
    std::cout << "2. Initializing signal handler (registers SIGINT/SIGTERM)...\n";
    handler.initialize();
    
    std::cout << "3. Starting main work loop...\n";
    std::cout << "   Press Ctrl+C to trigger graceful shutdown\n";
    std::cout << "   Or wait 10 seconds for automatic shutdown\n\n";
    
    // EN: Main work loop
    // FR: Boucle de travail principale
    int work_counter = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    while (!handler.isShutdownRequested()) {
        // EN: Simulate work
        // FR: Simule du travail
        std::cout << "Working... " << ++work_counter << " (Press Ctrl+C to shutdown)\r" << std::flush;
        std::this_thread::sleep_for(500ms);
        
        // EN: Auto-shutdown after 10 seconds for testing
        // FR: Arrêt automatique après 10 secondes pour les tests
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed > 10s) {
            std::cout << "\n\n4. Auto-shutdown triggered (10 seconds elapsed)\n";
            handler.triggerShutdown(SIGTERM);
            break;
        }
        
        // EN: Check if shutdown is in progress
        // FR: Vérifie si l'arrêt est en cours
        if (handler.isShuttingDown()) {
            std::cout << "\n\n4. Shutdown in progress...\n";
            break;
        }
    }
    
    std::cout << "5. Waiting for graceful shutdown completion...\n";
    handler.waitForShutdown();
    
    // EN: Display statistics
    // FR: Affiche les statistiques
    auto stats = handler.getStats();
    std::cout << "\n=== Shutdown Statistics ===\n";
    std::cout << "Signals received: " << stats.signals_received << "\n";
    std::cout << "Successful shutdowns: " << stats.successful_shutdowns << "\n";
    std::cout << "Timeout shutdowns: " << stats.timeout_shutdowns << "\n";
    std::cout << "Last shutdown duration: " << stats.last_shutdown_duration.count() << "ms\n";
    std::cout << "Total CSV flush time: " << stats.total_csv_flush_time.count() << "ms\n";
    std::cout << "Cleanup callbacks registered: " << stats.cleanup_callbacks_registered << "\n";
    std::cout << "CSV flush callbacks registered: " << stats.csv_flush_callbacks_registered << "\n";
    
    std::cout << "\nSignal counts:\n";
    for (const auto& [signal, count] : stats.signal_counts) {
        std::string signal_name = (signal == SIGINT) ? "SIGINT" :
                                 (signal == SIGTERM) ? "SIGTERM" :
                                 "Signal " + std::to_string(signal);
        std::cout << "  " << signal_name << ": " << count << "\n";
    }
    
    // EN: Check if CSV file was created
    // FR: Vérifie si le fichier CSV a été créé
    std::cout << "\n=== CSV File Verification ===\n";
    std::ifstream csv_check("/tmp/test_signal_handler.csv");
    if (csv_check.is_open()) {
        std::cout << "CSV file created successfully!\n";
        std::cout << "Content:\n";
        std::string line;
        while (std::getline(csv_check, line)) {
            std::cout << "  " << line << "\n";
        }
        csv_check.close();
        
        // EN: Clean up test file
        // FR: Nettoie le fichier de test
        std::remove("/tmp/test_signal_handler.csv");
        std::cout << "Test CSV file cleaned up.\n";
    } else {
        std::cout << "ERROR: CSV file was not created!\n";
        return 1;
    }
    
    std::cout << "\n=== Test Completed Successfully ===\n";
    std::cout << "Signal Handler is working correctly!\n";
    
    return 0;
}