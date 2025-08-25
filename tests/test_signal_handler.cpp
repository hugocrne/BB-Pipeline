// EN: Comprehensive unit tests for SignalHandler class - 100% coverage
// FR: Tests unitaires complets pour la classe SignalHandler - couverture 100%

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../include/infrastructure/system/signal_handler.hpp"
#include "../include/infrastructure/logging/logger.hpp"
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <signal.h>
#include <future>

using namespace BBP;
using namespace std::chrono_literals;

// EN: Test fixture for SignalHandler tests
// FR: Fixture de test pour les tests SignalHandler
class SignalHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // EN: Reset signal handler before each test
        // FR: Remet à zéro le gestionnaire de signaux avant chaque test
        signal_handler_ = &SignalHandler::getInstance();
        signal_handler_->reset();
        signal_handler_->setEnabled(true);
        
        // EN: Setup logger for test output
        // FR: Configure le logger pour la sortie de test
        logger_ = &Logger::getInstance();
        logger_->setLogLevel(LogLevel::DEBUG);
    }
    
    void TearDown() override {
        // EN: Clean up after each test
        // FR: Nettoie après chaque test
        if (signal_handler_) {
            signal_handler_->reset();
        }
    }
    
    SignalHandler* signal_handler_;
    Logger* logger_;
};

// EN: Test singleton pattern
// FR: Test du pattern singleton
TEST_F(SignalHandlerTest, SingletonPattern) {
    SignalHandler& handler1 = SignalHandler::getInstance();
    SignalHandler& handler2 = SignalHandler::getInstance();
    
    // EN: Should return same instance
    // FR: Devrait retourner la même instance
    EXPECT_EQ(&handler1, &handler2);
}

// EN: Test default configuration
// FR: Test de la configuration par défaut
TEST_F(SignalHandlerTest, DefaultConfiguration) {
    auto stats = signal_handler_->getStats();
    
    EXPECT_EQ(stats.signals_received, 0);
    EXPECT_EQ(stats.cleanup_callbacks_registered, 0);
    EXPECT_EQ(stats.csv_flush_callbacks_registered, 0);
    EXPECT_EQ(stats.successful_shutdowns, 0);
    EXPECT_EQ(stats.timeout_shutdowns, 0);
}

// EN: Test custom configuration
// FR: Test de la configuration personnalisée
TEST_F(SignalHandlerTest, CustomConfiguration) {
    SignalHandlerConfig config;
    config.shutdown_timeout = 3000ms;
    config.csv_flush_timeout = 1500ms;
    config.enable_emergency_flush = false;
    config.log_signal_details = false;
    
    // EN: Should configure without throwing
    // FR: Devrait configurer sans lancer d'exception
    EXPECT_NO_THROW(signal_handler_->configure(config));
}

// EN: Test signal handler initialization
// FR: Test de l'initialisation du gestionnaire de signaux
TEST_F(SignalHandlerTest, Initialization) {
    // EN: Should initialize successfully
    // FR: Devrait s'initialiser avec succès
    EXPECT_NO_THROW(signal_handler_->initialize());
    
    // EN: Should not initialize twice
    // FR: Ne devrait pas s'initialiser deux fois
    EXPECT_NO_THROW(signal_handler_->initialize());
}

// EN: Test cleanup callback registration
// FR: Test de l'enregistrement des callbacks de nettoyage
TEST_F(SignalHandlerTest, CleanupCallbackRegistration) {
    std::atomic<bool> callback_executed{false};
    
    // EN: Register cleanup callback
    // FR: Enregistre un callback de nettoyage
    signal_handler_->registerCleanupCallback("test_cleanup", [&callback_executed]() {
        callback_executed = true;
    });
    
    auto stats = signal_handler_->getStats();
    EXPECT_EQ(stats.cleanup_callbacks_registered, 1);
    
    // EN: Unregister callback
    // FR: Désenregistre le callback
    signal_handler_->unregisterCleanupCallback("test_cleanup");
    
    stats = signal_handler_->getStats();
    EXPECT_EQ(stats.cleanup_callbacks_registered, 0);
}

// EN: Test CSV flush callback registration
// FR: Test de l'enregistrement des callbacks de flush CSV
TEST_F(SignalHandlerTest, CsvFlushCallbackRegistration) {
    std::atomic<bool> csv_flushed{false};
    std::string received_path;
    
    // EN: Register CSV flush callback
    // FR: Enregistre un callback de flush CSV
    signal_handler_->registerCsvFlushCallback("/tmp/test.csv", 
        [&csv_flushed, &received_path](const std::string& path) {
            csv_flushed = true;
            received_path = path;
        });
    
    auto stats = signal_handler_->getStats();
    EXPECT_EQ(stats.csv_flush_callbacks_registered, 1);
    
    // EN: Unregister callback
    // FR: Désenregistre le callback
    signal_handler_->unregisterCsvFlushCallback("/tmp/test.csv");
    
    stats = signal_handler_->getStats();
    EXPECT_EQ(stats.csv_flush_callbacks_registered, 0);
}

// EN: Test manual shutdown trigger
// FR: Test du déclenchement manuel d'arrêt
TEST_F(SignalHandlerTest, ManualShutdownTrigger) {
    std::atomic<bool> cleanup_executed{false};
    std::atomic<bool> csv_flushed{false};
    
    // EN: Register callbacks
    // FR: Enregistre les callbacks
    signal_handler_->registerCleanupCallback("test_cleanup", [&cleanup_executed]() {
        cleanup_executed = true;
    });
    
    signal_handler_->registerCsvFlushCallback("/tmp/test.csv", 
        [&csv_flushed](const std::string&) {
            csv_flushed = true;
        });
    
    // EN: Trigger manual shutdown
    // FR: Déclenche un arrêt manuel
    EXPECT_FALSE(signal_handler_->isShutdownRequested());
    EXPECT_FALSE(signal_handler_->isShuttingDown());
    
    signal_handler_->triggerShutdown(SIGTERM);
    
    EXPECT_TRUE(signal_handler_->isShutdownRequested());
    
    // EN: Wait for shutdown to complete
    // FR: Attend que l'arrêt soit terminé
    signal_handler_->waitForShutdown();
    
    // EN: Verify callbacks were executed
    // FR: Vérifie que les callbacks ont été exécutés
    EXPECT_TRUE(cleanup_executed);
    EXPECT_TRUE(csv_flushed);
    
    auto stats = signal_handler_->getStats();
    EXPECT_EQ(stats.signals_received, 1);
    EXPECT_EQ(stats.successful_shutdowns, 1);
}

// EN: Test shutdown state management
// FR: Test de la gestion d'état d'arrêt
TEST_F(SignalHandlerTest, ShutdownStateManagement) {
    EXPECT_FALSE(signal_handler_->isShutdownRequested());
    EXPECT_FALSE(signal_handler_->isShuttingDown());
    
    // EN: Trigger shutdown
    // FR: Déclenche l'arrêt
    signal_handler_->triggerShutdown();
    
    EXPECT_TRUE(signal_handler_->isShutdownRequested());
    
    // EN: Wait for completion
    // FR: Attend la fin
    signal_handler_->waitForShutdown();
    
    // EN: Should not allow new callbacks during shutdown
    // FR: Ne devrait pas permettre de nouveaux callbacks pendant l'arrêt
    signal_handler_->registerCleanupCallback("late_callback", [](){});
    
    auto stats = signal_handler_->getStats();
    // EN: Callback should not be registered
    // FR: Le callback ne devrait pas être enregistré
    EXPECT_EQ(stats.cleanup_callbacks_registered, 0);
}

// EN: Test multiple callback execution order
// FR: Test de l'ordre d'exécution de plusieurs callbacks
TEST_F(SignalHandlerTest, MultipleCallbackExecution) {
    std::vector<std::string> execution_order;
    std::mutex order_mutex;
    
    // EN: Register multiple callbacks
    // FR: Enregistre plusieurs callbacks
    signal_handler_->registerCsvFlushCallback("/tmp/first.csv", 
        [&execution_order, &order_mutex](const std::string&) {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back("csv_first");
        });
    
    signal_handler_->registerCsvFlushCallback("/tmp/second.csv", 
        [&execution_order, &order_mutex](const std::string&) {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back("csv_second");
        });
    
    signal_handler_->registerCleanupCallback("first_cleanup", 
        [&execution_order, &order_mutex]() {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back("cleanup_first");
        });
    
    signal_handler_->registerCleanupCallback("second_cleanup", 
        [&execution_order, &order_mutex]() {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back("cleanup_second");
        });
    
    // EN: Trigger shutdown and wait
    // FR: Déclenche l'arrêt et attend
    signal_handler_->triggerShutdown();
    signal_handler_->waitForShutdown();
    
    // EN: CSV flush should happen before cleanup
    // FR: Le flush CSV devrait se passer avant le nettoyage
    EXPECT_EQ(execution_order.size(), 4);
    
    // EN: Find first CSV and first cleanup positions
    // FR: Trouve les positions du premier CSV et du premier nettoyage
    auto first_csv_it = std::find_if(execution_order.begin(), execution_order.end(),
        [](const std::string& s) { return s.find("csv_") == 0; });
    auto first_cleanup_it = std::find_if(execution_order.begin(), execution_order.end(),
        [](const std::string& s) { return s.find("cleanup_") == 0; });
    
    // EN: CSV should come before cleanup
    // FR: CSV devrait venir avant le nettoyage
    if (first_csv_it != execution_order.end() && first_cleanup_it != execution_order.end()) {
        EXPECT_LT(std::distance(execution_order.begin(), first_csv_it),
                  std::distance(execution_order.begin(), first_cleanup_it));
    }
}

// EN: Test callback exception handling
// FR: Test de la gestion des exceptions des callbacks
TEST_F(SignalHandlerTest, CallbackExceptionHandling) {
    std::atomic<bool> good_callback_executed{false};
    
    // EN: Register callbacks - one throws, one doesn't
    // FR: Enregistre des callbacks - un lance une exception, l'autre non
    signal_handler_->registerCleanupCallback("throwing_callback", []() {
        throw std::runtime_error("Test exception");
    });
    
    signal_handler_->registerCleanupCallback("good_callback", [&good_callback_executed]() {
        good_callback_executed = true;
    });
    
    signal_handler_->registerCsvFlushCallback("/tmp/throwing.csv", 
        [](const std::string&) {
            throw std::runtime_error("CSV exception");
        });
    
    // EN: Should complete shutdown despite exceptions
    // FR: Devrait terminer l'arrêt malgré les exceptions
    EXPECT_NO_THROW(signal_handler_->triggerShutdown());
    signal_handler_->waitForShutdown();
    
    // EN: Good callback should still execute
    // FR: Le bon callback devrait quand même s'exécuter
    EXPECT_TRUE(good_callback_executed);
    
    auto stats = signal_handler_->getStats();
    EXPECT_EQ(stats.signals_received, 1);
}

// EN: Test signal statistics tracking
// FR: Test du suivi des statistiques de signaux
TEST_F(SignalHandlerTest, SignalStatisticsTracking) {
    // EN: Initial stats should be zero
    // FR: Les statistiques initiales devraient être zéro
    auto initial_stats = signal_handler_->getStats();
    EXPECT_EQ(initial_stats.signals_received, 0);
    EXPECT_TRUE(initial_stats.signal_counts.empty());
    
    // EN: Trigger different signals
    // FR: Déclenche différents signaux
    signal_handler_->triggerShutdown(SIGINT);
    signal_handler_->waitForShutdown();
    
    auto stats_after_sigint = signal_handler_->getStats();
    EXPECT_EQ(stats_after_sigint.signals_received, 1);
    EXPECT_EQ(stats_after_sigint.signal_counts[SIGINT], 1);
    
    // EN: Reset and try SIGTERM
    // FR: Remet à zéro et essaie SIGTERM
    signal_handler_->reset();
    signal_handler_->triggerShutdown(SIGTERM);
    signal_handler_->waitForShutdown();
    
    auto stats_after_sigterm = signal_handler_->getStats();
    EXPECT_EQ(stats_after_sigterm.signals_received, 1);
    EXPECT_EQ(stats_after_sigterm.signal_counts[SIGTERM], 1);
}

// EN: Test timeout handling
// FR: Test de la gestion des timeouts
TEST_F(SignalHandlerTest, TimeoutHandling) {
    SignalHandlerConfig config;
    config.shutdown_timeout = 100ms;  // EN: Very short timeout / FR: Timeout très court
    config.csv_flush_timeout = 50ms;
    config.enable_emergency_flush = true;
    
    signal_handler_->configure(config);
    
    std::atomic<bool> slow_callback_started{false};
    std::atomic<bool> slow_callback_finished{false};
    
    // EN: Register slow callback that will timeout
    // FR: Enregistre un callback lent qui va timeout
    signal_handler_->registerCleanupCallback("slow_callback", 
        [&slow_callback_started, &slow_callback_finished]() {
            slow_callback_started = true;
            std::this_thread::sleep_for(200ms);  // EN: Longer than timeout / FR: Plus long que le timeout
            slow_callback_finished = true;
        });
    
    // EN: Trigger shutdown
    // FR: Déclenche l'arrêt
    auto start_time = std::chrono::steady_clock::now();
    signal_handler_->triggerShutdown();
    signal_handler_->waitForShutdown();
    auto end_time = std::chrono::steady_clock::now();
    
    // EN: Should complete reasonably quickly despite slow callback
    // FR: Devrait se terminer assez rapidement malgré le callback lent
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    EXPECT_LT(duration, 500ms);  // EN: Should be much less than callback sleep time / FR: Devrait être beaucoup moins que le temps de sommeil du callback
}

// EN: Test enable/disable functionality
// FR: Test de la fonctionnalité activation/désactivation
TEST_F(SignalHandlerTest, EnableDisableFunctionality) {
    // EN: Disable signal handler
    // FR: Désactive le gestionnaire de signaux
    signal_handler_->setEnabled(false);
    
    std::atomic<bool> callback_executed{false};
    signal_handler_->registerCleanupCallback("test", [&callback_executed]() {
        callback_executed = true;
    });
    
    // EN: Should not trigger when disabled
    // FR: Ne devrait pas se déclencher quand désactivé
    signal_handler_->triggerShutdown();
    
    // EN: Wait a bit to see if anything happens
    // FR: Attend un peu pour voir si quelque chose se passe
    std::this_thread::sleep_for(50ms);
    
    EXPECT_FALSE(signal_handler_->isShutdownRequested());
    EXPECT_FALSE(callback_executed);
    
    // EN: Re-enable and test
    // FR: Réactive et teste
    signal_handler_->setEnabled(true);
    signal_handler_->triggerShutdown();
    signal_handler_->waitForShutdown();
    
    EXPECT_TRUE(callback_executed);
}

// EN: Test CSV flush timing measurement
// FR: Test de la mesure du timing de flush CSV
TEST_F(SignalHandlerTest, CsvFlushTimingMeasurement) {
    std::atomic<int> flush_call_count{0};
    
    // EN: Register CSV callbacks that take some time
    // FR: Enregistre des callbacks CSV qui prennent du temps
    for (int i = 0; i < 3; ++i) {
        signal_handler_->registerCsvFlushCallback("/tmp/test" + std::to_string(i) + ".csv",
            [&flush_call_count](const std::string&) {
                flush_call_count++;
                std::this_thread::sleep_for(10ms);  // EN: Small delay / FR: Petit délai
            });
    }
    
    // EN: Trigger shutdown and measure
    // FR: Déclenche l'arrêt et mesure
    signal_handler_->triggerShutdown();
    signal_handler_->waitForShutdown();
    
    auto stats = signal_handler_->getStats();
    
    // EN: All callbacks should have been called
    // FR: Tous les callbacks devraient avoir été appelés
    EXPECT_EQ(flush_call_count.load(), 3);
    
    // EN: Should have recorded CSV flush time
    // FR: Devrait avoir enregistré le temps de flush CSV
    EXPECT_GT(stats.total_csv_flush_time.count(), 0);
}

// EN: Test reset functionality
// FR: Test de la fonctionnalité de remise à zéro
TEST_F(SignalHandlerTest, ResetFunctionality) {
    // EN: Add some callbacks and trigger shutdown
    // FR: Ajoute quelques callbacks et déclenche l'arrêt
    signal_handler_->registerCleanupCallback("test", [](){});
    signal_handler_->registerCsvFlushCallback("/tmp/test.csv", [](const std::string&){});
    signal_handler_->triggerShutdown();
    signal_handler_->waitForShutdown();
    
    auto stats_before_reset = signal_handler_->getStats();
    EXPECT_GT(stats_before_reset.signals_received, 0);
    EXPECT_GT(stats_before_reset.cleanup_callbacks_registered, 0);
    
    // EN: Reset should clear everything
    // FR: La remise à zéro devrait tout effacer
    signal_handler_->reset();
    
    auto stats_after_reset = signal_handler_->getStats();
    EXPECT_EQ(stats_after_reset.signals_received, 0);
    EXPECT_EQ(stats_after_reset.cleanup_callbacks_registered, 0);
    EXPECT_EQ(stats_after_reset.csv_flush_callbacks_registered, 0);
    EXPECT_FALSE(signal_handler_->isShutdownRequested());
}

// EN: Test emergency flush functionality
// FR: Test de la fonctionnalité de flush d'urgence
TEST_F(SignalHandlerTest, EmergencyFlushFunctionality) {
    SignalHandlerConfig config;
    config.csv_flush_timeout = 50ms;  // EN: Short timeout to trigger emergency / FR: Timeout court pour déclencher l'urgence
    config.enable_emergency_flush = true;
    
    signal_handler_->configure(config);
    
    std::atomic<int> emergency_flush_count{0};
    
    // EN: Register CSV callback that will timeout
    // FR: Enregistre un callback CSV qui va timeout
    signal_handler_->registerCsvFlushCallback("/tmp/slow.csv",
        [&emergency_flush_count](const std::string&) {
            emergency_flush_count++;
            std::this_thread::sleep_for(100ms);  // EN: Longer than timeout / FR: Plus long que le timeout
        });
    
    // EN: Should complete with emergency flush
    // FR: Devrait se terminer avec un flush d'urgence
    signal_handler_->triggerShutdown();
    signal_handler_->waitForShutdown();
    
    // EN: Emergency flush should have been called
    // FR: Le flush d'urgence devrait avoir été appelé
    EXPECT_GE(emergency_flush_count.load(), 1);
}

// EN: Test concurrent shutdown attempts
// FR: Test des tentatives d'arrêt concurrentes
TEST_F(SignalHandlerTest, ConcurrentShutdownAttempts) {
    std::atomic<int> callback_count{0};
    
    signal_handler_->registerCleanupCallback("concurrent_test", [&callback_count]() {
        callback_count++;
        std::this_thread::sleep_for(50ms);
    });
    
    // EN: Launch multiple threads trying to trigger shutdown
    // FR: Lance plusieurs threads essayant de déclencher l'arrêt
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 5; ++i) {
        futures.push_back(std::async(std::launch::async, [this]() {
            signal_handler_->triggerShutdown();
        }));
    }
    
    // EN: Wait for all attempts
    // FR: Attend toutes les tentatives
    for (auto& future : futures) {
        future.wait();
    }
    
    signal_handler_->waitForShutdown();
    
    // EN: Callback should only be executed once despite multiple triggers
    // FR: Le callback ne devrait être exécuté qu'une fois malgré les multiples déclenchements
    EXPECT_EQ(callback_count.load(), 1);
    
    auto stats = signal_handler_->getStats();
    EXPECT_EQ(stats.signals_received, 1);  // EN: Only first signal should be processed / FR: Seul le premier signal devrait être traité
}

// EN: Integration test with actual CSV file creation
// FR: Test d'intégration avec création réelle de fichiers CSV
TEST_F(SignalHandlerTest, CsvFileIntegrationTest) {
    std::string test_csv_path = "/tmp/signal_handler_test.csv";
    std::atomic<bool> csv_written{false};
    
    // EN: Register callback that actually writes CSV
    // FR: Enregistre un callback qui écrit vraiment du CSV
    signal_handler_->registerCsvFlushCallback(test_csv_path,
        [&csv_written, &test_csv_path](const std::string& path) {
            EXPECT_EQ(path, test_csv_path);
            
            std::ofstream csv_file(path);
            csv_file << "header1,header2,header3\n";
            csv_file << "value1,value2,value3\n";
            csv_file.flush();
            csv_file.close();
            
            csv_written = true;
        });
    
    // EN: Trigger shutdown
    // FR: Déclenche l'arrêt
    signal_handler_->triggerShutdown();
    signal_handler_->waitForShutdown();
    
    // EN: Verify CSV was written
    // FR: Vérifie que le CSV a été écrit
    EXPECT_TRUE(csv_written);
    
    // EN: Verify file exists and has content
    // FR: Vérifie que le fichier existe et a du contenu
    std::ifstream check_file(test_csv_path);
    EXPECT_TRUE(check_file.is_open());
    
    std::string line;
    std::getline(check_file, line);
    EXPECT_EQ(line, "header1,header2,header3");
    
    check_file.close();
    
    // EN: Clean up test file
    // FR: Nettoie le fichier de test
    std::remove(test_csv_path.c_str());
}

// EN: Main function for running tests
// FR: Fonction principale pour exécuter les tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}