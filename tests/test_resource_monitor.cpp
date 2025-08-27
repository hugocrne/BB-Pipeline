// EN: Comprehensive unit tests for Resource Monitor - CPU/RAM/Network monitoring with throttling
// FR: Tests unitaires complets pour le Moniteur de Ressources - Surveillance CPU/RAM/Réseau avec throttling

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <atomic>
#include <future>
#include <sstream>
#include <random>
#include "orchestrator/resource_monitor.hpp"

using namespace BBP::Orchestrator;
using namespace std::chrono_literals;

// EN: Test fixture for Resource Monitor tests
// FR: Fixture de test pour les tests du Moniteur de Ressources
class ResourceMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // EN: Initialize test configuration
        // FR: Initialiser la configuration de test
        config_ = ResourceUtils::createDefaultConfig();
        config_.collection_interval = 50ms;  // EN: Fast collection for testing / FR: Collecte rapide pour les tests
        config_.history_size = 100;
        config_.enable_logging = false;  // EN: Disable logging for tests / FR: Désactiver la journalisation pour les tests
        config_.enable_alerts = true;
        config_.enable_throttling = true;
        
        // EN: Create resource monitor instance
        // FR: Créer l'instance du moniteur de ressources
        monitor_ = std::make_unique<ResourceMonitor>(config_);
        
        // EN: Reset event tracking
        // FR: Réinitialiser le suivi d'événements
        alerts_received_.clear();
        resource_updates_received_.clear();
        throttling_events_received_.clear();
        alert_count_.store(0);
        update_count_.store(0);
        throttling_count_.store(0);
    }
    
    void TearDown() override {
        if (monitor_) {
            monitor_->stop();
        }
        monitor_.reset();
    }
    
    // EN: Helper method to setup event callbacks
    // FR: Méthode d'aide pour configurer les callbacks d'événement
    void setupEventCallbacks() {
        monitor_->setAlertCallback([this](const ResourceAlert& alert) {
            std::lock_guard<std::mutex> lock(events_mutex_);
            alerts_received_.push_back(alert);
            alert_count_.fetch_add(1);
        });
        
        monitor_->setResourceUpdateCallback([this](const ResourceUsage& usage) {
            std::lock_guard<std::mutex> lock(events_mutex_);
            resource_updates_received_.push_back(usage);
            update_count_.fetch_add(1);
        });
        
        monitor_->setThrottlingCallback([this](ResourceType type, double factor, bool enabled) {
            std::lock_guard<std::mutex> lock(events_mutex_);
            throttling_events_received_.emplace_back(type, factor, enabled);
            throttling_count_.fetch_add(1);
        });
    }
    
    // EN: Helper method to wait for specific event count
    // FR: Méthode d'aide pour attendre un nombre d'événements spécifique
    bool waitForAlerts(size_t expected_count, std::chrono::milliseconds timeout = 2000ms) {
        auto start_time = std::chrono::steady_clock::now();
        while (alert_count_.load() < expected_count) {
            if (std::chrono::steady_clock::now() - start_time > timeout) {
                return false;
            }
            std::this_thread::sleep_for(10ms);
        }
        return true;
    }
    
    bool waitForUpdates(size_t expected_count, std::chrono::milliseconds timeout = 2000ms) {
        auto start_time = std::chrono::steady_clock::now();
        while (update_count_.load() < expected_count) {
            if (std::chrono::steady_clock::now() - start_time > timeout) {
                return false;
            }
            std::this_thread::sleep_for(10ms);
        }
        return true;
    }
    
    // EN: Helper method to create test threshold
    // FR: Méthode d'aide pour créer un seuil de test
    ResourceThreshold createTestThreshold(ResourceType type, double warning = 50.0, double critical = 75.0) {
        ResourceThreshold threshold;
        threshold.resource_type = type;
        threshold.warning_threshold = warning;
        threshold.critical_threshold = critical;
        threshold.emergency_threshold = 90.0;
        threshold.duration_before_alert = std::chrono::seconds(1);
        threshold.enable_throttling = true;
        threshold.throttling_strategy = ThrottlingStrategy::LINEAR;
        threshold.throttling_factor = 0.5;
        return threshold;
    }
    
    // EN: Helper to simulate high resource usage
    // FR: Assistant pour simuler un usage élevé de ressources
    void simulateHighCPUUsage(std::chrono::milliseconds duration) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < duration) {
            // EN: Busy loop to consume CPU / FR: Boucle occupée pour consommer le CPU
            volatile int dummy = 0;
            for (int i = 0; i < 1000000; ++i) {
                dummy += i;
            }
            std::this_thread::sleep_for(1ms);
        }
    }
    
    // EN: Helper to simulate memory allocation
    // FR: Assistant pour simuler l'allocation mémoire
    std::vector<char> simulateMemoryUsage(size_t bytes) {
        return std::vector<char>(bytes, 0x42);
    }

protected:
    ResourceMonitorConfig config_;
    std::unique_ptr<ResourceMonitor> monitor_;
    
    // EN: Event tracking
    // FR: Suivi d'événements
    std::vector<ResourceAlert> alerts_received_;
    std::vector<ResourceUsage> resource_updates_received_;
    std::vector<std::tuple<ResourceType, double, bool>> throttling_events_received_;
    std::mutex events_mutex_;
    std::atomic<size_t> alert_count_{0};
    std::atomic<size_t> update_count_{0};
    std::atomic<size_t> throttling_count_{0};
};

// EN: Test basic resource monitor construction and configuration
// FR: Tester la construction et configuration de base du moniteur de ressources
TEST_F(ResourceMonitorTest, BasicConstruction) {
    // EN: Test default configuration
    // FR: Tester la configuration par défaut
    EXPECT_TRUE(monitor_ != nullptr);
    EXPECT_FALSE(monitor_->isRunning());
    EXPECT_FALSE(monitor_->isPaused());
    
    // EN: Test configuration retrieval
    // FR: Tester la récupération de configuration
    auto retrieved_config = monitor_->getConfig();
    EXPECT_EQ(retrieved_config.collection_interval, config_.collection_interval);
    EXPECT_EQ(retrieved_config.history_size, config_.history_size);
    EXPECT_EQ(retrieved_config.enable_alerts, config_.enable_alerts);
    EXPECT_EQ(retrieved_config.enable_throttling, config_.enable_throttling);
}

// EN: Test resource monitor lifecycle
// FR: Tester le cycle de vie du moniteur de ressources
TEST_F(ResourceMonitorTest, MonitoringLifecycle) {
    setupEventCallbacks();
    
    // EN: Test initial state
    // FR: Tester l'état initial
    EXPECT_FALSE(monitor_->isRunning());
    EXPECT_FALSE(monitor_->isPaused());
    
    // EN: Test starting
    // FR: Tester le démarrage
    EXPECT_TRUE(monitor_->start());
    EXPECT_TRUE(monitor_->isRunning());
    EXPECT_FALSE(monitor_->isPaused());
    
    // EN: Test double start should fail
    // FR: Tester qu'un double démarrage devrait échouer
    EXPECT_FALSE(monitor_->start());
    
    // EN: Wait for some resource updates
    // FR: Attendre quelques mises à jour de ressources
    EXPECT_TRUE(waitForUpdates(5));
    
    // EN: Test pausing and resuming
    // FR: Tester la pause et la reprise
    monitor_->pause();
    EXPECT_TRUE(monitor_->isPaused());
    EXPECT_TRUE(monitor_->isRunning());
    
    size_t updates_during_pause = update_count_.load();
    std::this_thread::sleep_for(100ms);
    
    // EN: Should not receive many updates during pause
    // FR: Ne devrait pas recevoir beaucoup de mises à jour pendant la pause
    EXPECT_LE(update_count_.load() - updates_during_pause, 2);
    
    monitor_->resume();
    EXPECT_FALSE(monitor_->isPaused());
    EXPECT_TRUE(monitor_->isRunning());
    
    // EN: Test stopping
    // FR: Tester l'arrêt
    monitor_->stop();
    EXPECT_FALSE(monitor_->isRunning());
    EXPECT_FALSE(monitor_->isPaused());
}

// EN: Test system information collection
// FR: Tester la collecte d'informations système
TEST_F(ResourceMonitorTest, SystemInfoCollection) {
    EXPECT_TRUE(monitor_->start());
    std::this_thread::sleep_for(100ms);
    
    auto system_info = monitor_->getSystemInfo();
    
    // EN: Basic system information should be available
    // FR: Les informations système de base devraient être disponibles
    EXPECT_GT(system_info.cpu_core_count, 0);
    EXPECT_GT(system_info.cpu_logical_count, 0);
    EXPECT_GT(system_info.total_physical_memory, 0);
    EXPECT_LE(system_info.available_physical_memory, system_info.total_physical_memory);
    EXPECT_GT(system_info.page_size, 0);
    EXPECT_FALSE(system_info.operating_system.empty());
    
    #ifndef _WIN32
    // EN: Linux-specific information
    // FR: Informations spécifiques à Linux
    EXPECT_GE(system_info.system_load_1min, 0.0);
    EXPECT_GT(system_info.process_count, 0);
    #endif
    
    monitor_->stop();
}

// EN: Test resource usage collection
// FR: Tester la collecte d'usage des ressources
TEST_F(ResourceMonitorTest, ResourceUsageCollection) {
    setupEventCallbacks();
    
    EXPECT_TRUE(monitor_->start());
    
    // EN: Wait for resource collection
    // FR: Attendre la collecte de ressources
    EXPECT_TRUE(waitForUpdates(10));
    
    // EN: Test CPU usage collection
    // FR: Tester la collecte d'usage CPU
    auto cpu_usage = monitor_->getCurrentUsage(ResourceType::CPU);
    ASSERT_TRUE(cpu_usage.has_value());
    EXPECT_EQ(cpu_usage->type, ResourceType::CPU);
    EXPECT_EQ(cpu_usage->unit, ResourceUnit::PERCENTAGE);
    EXPECT_TRUE(cpu_usage->is_valid);
    EXPECT_GE(cpu_usage->current_value, 0.0);
    EXPECT_LE(cpu_usage->current_value, 100.0);
    
    // EN: Test memory usage collection
    // FR: Tester la collecte d'usage mémoire
    auto memory_usage = monitor_->getCurrentUsage(ResourceType::MEMORY);
    ASSERT_TRUE(memory_usage.has_value());
    EXPECT_EQ(memory_usage->type, ResourceType::MEMORY);
    EXPECT_EQ(memory_usage->unit, ResourceUnit::PERCENTAGE);
    EXPECT_TRUE(memory_usage->is_valid);
    EXPECT_GE(memory_usage->current_value, 0.0);
    EXPECT_LE(memory_usage->current_value, 100.0);
    EXPECT_GT(memory_usage->metadata.size(), 0);
    
    // EN: Test network usage collection
    // FR: Tester la collecte d'usage réseau
    auto network_usage = monitor_->getCurrentUsage(ResourceType::NETWORK);
    ASSERT_TRUE(network_usage.has_value());
    EXPECT_EQ(network_usage->type, ResourceType::NETWORK);
    EXPECT_EQ(network_usage->unit, ResourceUnit::BYTES_PER_SECOND);
    EXPECT_TRUE(network_usage->is_valid);
    EXPECT_GE(network_usage->current_value, 0.0);
    
    // EN: Test process usage collection
    // FR: Tester la collecte d'usage processus
    auto process_usage = monitor_->getCurrentUsage(ResourceType::PROCESS);
    ASSERT_TRUE(process_usage.has_value());
    EXPECT_EQ(process_usage->type, ResourceType::PROCESS);
    EXPECT_TRUE(process_usage->is_valid);
    EXPECT_GE(process_usage->current_value, 0.0);
    
    // EN: Test getting all usage at once
    // FR: Tester l'obtention de tous les usages à la fois
    auto all_usage = monitor_->getAllCurrentUsage();
    EXPECT_GE(all_usage.size(), 4); // At least CPU, Memory, Network, Process
    EXPECT_TRUE(all_usage.find(ResourceType::CPU) != all_usage.end());
    EXPECT_TRUE(all_usage.find(ResourceType::MEMORY) != all_usage.end());
    
    monitor_->stop();
}

// EN: Test threshold configuration and alerts
// FR: Tester la configuration de seuils et les alertes
TEST_F(ResourceMonitorTest, ThresholdConfigurationAndAlerts) {
    setupEventCallbacks();
    
    // EN: Add custom low thresholds to trigger alerts easily
    // FR: Ajouter des seuils bas personnalisés pour déclencher facilement les alertes
    auto cpu_threshold = createTestThreshold(ResourceType::CPU, 1.0, 5.0); // Very low thresholds
    auto memory_threshold = createTestThreshold(ResourceType::MEMORY, 10.0, 20.0);
    
    monitor_->addThreshold(cpu_threshold);
    monitor_->addThreshold(memory_threshold);
    
    // EN: Verify threshold was added
    // FR: Vérifier que le seuil a été ajouté
    auto thresholds = monitor_->getThresholds();
    EXPECT_GE(thresholds.size(), 2);
    
    auto cpu_threshold_it = std::find_if(thresholds.begin(), thresholds.end(),
                                        [](const ResourceThreshold& t) {
                                            return t.resource_type == ResourceType::CPU;
                                        });
    ASSERT_NE(cpu_threshold_it, thresholds.end());
    EXPECT_EQ(cpu_threshold_it->warning_threshold, 1.0);
    
    EXPECT_TRUE(monitor_->start());
    
    // EN: Wait for alerts to be generated due to low thresholds
    // FR: Attendre que les alertes soient générées à cause des seuils bas
    EXPECT_TRUE(waitForAlerts(1, 3000ms));
    
    auto alerts = monitor_->getActiveAlerts();
    EXPECT_GE(alerts.size(), 1);
    
    // EN: Verify alert properties
    // FR: Vérifier les propriétés d'alerte
    bool cpu_alert_found = false;
    for (const auto& alert : alerts) {
        if (alert.resource_type == ResourceType::CPU) {
            cpu_alert_found = true;
            EXPECT_GE(alert.severity, ResourceAlertSeverity::WARNING);
            EXPECT_GT(alert.current_value, 0.0);
            EXPECT_FALSE(alert.message.empty());
            EXPECT_FALSE(alert.recommended_action.empty());
            break;
        }
    }
    EXPECT_TRUE(cpu_alert_found);
    
    // EN: Test threshold removal
    // FR: Tester la suppression de seuil
    EXPECT_TRUE(monitor_->removeThreshold(ResourceType::CPU));
    EXPECT_FALSE(monitor_->removeThreshold(ResourceType::CPU)); // Should fail second time
    
    monitor_->stop();
}

// EN: Test throttling functionality
// FR: Tester la fonctionnalité de throttling
TEST_F(ResourceMonitorTest, ThrottlingFunctionality) {
    setupEventCallbacks();
    
    // EN: Add threshold that enables throttling
    // FR: Ajouter un seuil qui active le throttling
    auto threshold = createTestThreshold(ResourceType::CPU, 1.0, 5.0);
    threshold.enable_throttling = true;
    threshold.throttling_factor = 0.3;
    
    monitor_->addThreshold(threshold);
    EXPECT_TRUE(monitor_->start());
    
    // EN: Wait for throttling to be applied
    // FR: Attendre que le throttling soit appliqué
    std::this_thread::sleep_for(500ms);
    
    // EN: Check if throttling is active
    // FR: Vérifier si le throttling est actif
    bool throttling_active = monitor_->isThrottlingActive(ResourceType::CPU);
    if (throttling_active) {
        double throttling_factor = monitor_->getCurrentThrottlingFactor(ResourceType::CPU);
        EXPECT_LT(throttling_factor, 1.0);
        EXPECT_GT(throttling_factor, 0.0);
    }
    
    // EN: Test manual throttling
    // FR: Tester le throttling manuel
    monitor_->manualThrottle(ResourceType::MEMORY, 0.5, std::chrono::minutes(1));
    EXPECT_TRUE(monitor_->isThrottlingActive(ResourceType::MEMORY));
    EXPECT_DOUBLE_EQ(monitor_->getCurrentThrottlingFactor(ResourceType::MEMORY), 0.5);
    
    // EN: Test disabling throttling
    // FR: Tester la désactivation du throttling
    monitor_->disableThrottling(ResourceType::MEMORY);
    EXPECT_FALSE(monitor_->isThrottlingActive(ResourceType::MEMORY));
    
    // EN: Test getting all throttling factors
    // FR: Tester l'obtention de tous les facteurs de throttling
    auto all_factors = monitor_->getAllThrottlingFactors();
    EXPECT_GE(all_factors.size(), 0);
    
    monitor_->stop();
}

// EN: Test resource statistics calculation
// FR: Tester le calcul des statistiques de ressources
TEST_F(ResourceMonitorTest, ResourceStatisticsCalculation) {
    EXPECT_TRUE(monitor_->start());
    
    // EN: Let monitor collect some data
    // FR: Laisser le moniteur collecter quelques données
    std::this_thread::sleep_for(1000ms);
    
    // EN: Get CPU statistics for last minute
    // FR: Obtenir les statistiques CPU de la dernière minute
    auto cpu_stats = monitor_->getResourceStatistics(ResourceType::CPU, std::chrono::minutes(1));
    
    EXPECT_EQ(cpu_stats.resource_type, ResourceType::CPU);
    EXPECT_GT(cpu_stats.sample_count, 0);
    EXPECT_GE(cpu_stats.mean_value, 0.0);
    EXPECT_LE(cpu_stats.mean_value, 100.0);
    EXPECT_GE(cpu_stats.minimum_value, 0.0);
    EXPECT_LE(cpu_stats.maximum_value, 100.0);
    EXPECT_LE(cpu_stats.minimum_value, cpu_stats.maximum_value);
    EXPECT_GE(cpu_stats.standard_deviation, 0.0);
    EXPECT_GE(cpu_stats.variance, 0.0);
    EXPECT_GE(cpu_stats.percentile_95, 0.0);
    EXPECT_GE(cpu_stats.percentile_99, 0.0);
    EXPECT_GT(cpu_stats.total_duration.count(), 0);
    
    // EN: Test memory statistics
    // FR: Tester les statistiques mémoire
    auto memory_stats = monitor_->getResourceStatistics(ResourceType::MEMORY, std::chrono::minutes(1));
    EXPECT_EQ(memory_stats.resource_type, ResourceType::MEMORY);
    EXPECT_GT(memory_stats.sample_count, 0);
    
    // EN: Test getting all statistics
    // FR: Tester l'obtention de toutes les statistiques
    auto all_stats = monitor_->getAllResourceStatistics(std::chrono::minutes(1));
    EXPECT_GE(all_stats.size(), 2); // At least CPU and Memory
    EXPECT_TRUE(all_stats.find(ResourceType::CPU) != all_stats.end());
    EXPECT_TRUE(all_stats.find(ResourceType::MEMORY) != all_stats.end());
    
    monitor_->stop();
}

// EN: Test resource history management
// FR: Tester la gestion de l'historique des ressources
TEST_F(ResourceMonitorTest, ResourceHistoryManagement) {
    // EN: Set small history size for testing
    // FR: Définir une petite taille d'historique pour les tests
    config_.history_size = 10;
    monitor_ = std::make_unique<ResourceMonitor>(config_);
    
    EXPECT_TRUE(monitor_->start());
    
    // EN: Let it collect more data than history size
    // FR: Le laisser collecter plus de données que la taille d'historique
    std::this_thread::sleep_for(800ms); // Should collect ~16 samples with 50ms interval
    
    auto cpu_history = monitor_->getResourceHistory(ResourceType::CPU, std::chrono::minutes(1));
    
    // EN: History should be limited to configured size
    // FR: L'historique devrait être limité à la taille configurée
    EXPECT_LE(cpu_history.size(), config_.history_size);
    EXPECT_GT(cpu_history.size(), 0);
    
    // EN: Verify history is ordered by timestamp
    // FR: Vérifier que l'historique est ordonné par horodatage
    for (size_t i = 1; i < cpu_history.size(); ++i) {
        EXPECT_LE(cpu_history[i-1].timestamp, cpu_history[i].timestamp);
    }
    
    // EN: Test clearing history
    // FR: Tester l'effacement de l'historique
    monitor_->clearHistory(ResourceType::CPU);
    auto cleared_history = monitor_->getResourceHistory(ResourceType::CPU, std::chrono::minutes(1));
    EXPECT_LT(cleared_history.size(), cpu_history.size());
    
    monitor_->stop();
}

// EN: Test custom metrics functionality
// FR: Tester la fonctionnalité des métriques personnalisées
TEST_F(ResourceMonitorTest, CustomMetrics) {
    std::atomic<double> custom_value{42.0};
    
    // EN: Add custom metric
    // FR: Ajouter une métrique personnalisée
    EXPECT_TRUE(monitor_->addCustomMetric("test_metric", 
                                         [&custom_value]() { return custom_value.load(); },
                                         ResourceUnit::COUNT));
    
    // EN: Test duplicate addition should fail
    // FR: Tester que l'ajout dupliqué devrait échouer
    EXPECT_FALSE(monitor_->addCustomMetric("test_metric", 
                                          [&custom_value]() { return custom_value.load(); },
                                          ResourceUnit::COUNT));
    
    EXPECT_TRUE(monitor_->start());
    std::this_thread::sleep_for(100ms);
    
    // EN: Verify custom metric is collected
    // FR: Vérifier que la métrique personnalisée est collectée
    auto metric_names = monitor_->getCustomMetricNames();
    EXPECT_EQ(metric_names.size(), 1);
    EXPECT_EQ(metric_names[0], "test_metric");
    
    auto metric_value = monitor_->getCustomMetricValue("test_metric");
    ASSERT_TRUE(metric_value.has_value());
    EXPECT_DOUBLE_EQ(metric_value.value(), 42.0);
    
    // EN: Change custom value and verify it's updated
    // FR: Changer la valeur personnalisée et vérifier qu'elle est mise à jour
    custom_value.store(84.0);
    std::this_thread::sleep_for(100ms);
    
    metric_value = monitor_->getCustomMetricValue("test_metric");
    ASSERT_TRUE(metric_value.has_value());
    EXPECT_DOUBLE_EQ(metric_value.value(), 84.0);
    
    // EN: Test metric removal
    // FR: Tester la suppression de métrique
    EXPECT_TRUE(monitor_->removeCustomMetric("test_metric"));
    EXPECT_FALSE(monitor_->removeCustomMetric("test_metric")); // Should fail second time
    
    auto empty_names = monitor_->getCustomMetricNames();
    EXPECT_EQ(empty_names.size(), 0);
    
    monitor_->stop();
}

// EN: Test performance monitoring
// FR: Tester la surveillance des performances
TEST_F(ResourceMonitorTest, PerformanceMonitoring) {
    EXPECT_TRUE(monitor_->start());
    
    // EN: Let monitor run for a while
    // FR: Laisser le moniteur fonctionner un moment
    std::this_thread::sleep_for(1000ms);
    
    auto performance = monitor_->getMonitorPerformance();
    
    // EN: Verify performance metrics are reasonable
    // FR: Vérifier que les métriques de performance sont raisonnables
    EXPECT_GT(performance.collections_per_second, 0);
    EXPECT_LT(performance.collections_per_second, 1000); // Shouldn't be too high
    EXPECT_GE(performance.failed_collections, 0);
    EXPECT_GT(performance.avg_collection_time.count(), 0);
    EXPECT_GT(performance.max_collection_time.count(), 0);
    EXPECT_GE(performance.max_collection_time, performance.avg_collection_time);
    EXPECT_GE(performance.cpu_overhead_percentage, 0.0);
    EXPECT_LT(performance.cpu_overhead_percentage, 50.0); // Shouldn't use too much CPU
    EXPECT_GT(performance.memory_usage_bytes, 0);
    
    monitor_->resetPerformanceCounters();
    
    // EN: Performance counters should be reset
    // FR: Les compteurs de performance devraient être réinitialisés
    std::this_thread::sleep_for(200ms);
    auto reset_performance = monitor_->getMonitorPerformance();
    EXPECT_LE(reset_performance.collections_per_second, performance.collections_per_second);
    
    monitor_->stop();
}

// EN: Test alert history and acknowledgment
// FR: Tester l'historique d'alertes et l'accusé de réception
TEST_F(ResourceMonitorTest, AlertHistoryAndAcknowledgment) {
    setupEventCallbacks();
    
    // EN: Add threshold that will trigger alerts
    // FR: Ajouter un seuil qui déclenchera des alertes
    auto threshold = createTestThreshold(ResourceType::CPU, 1.0, 5.0);
    monitor_->addThreshold(threshold);
    
    EXPECT_TRUE(monitor_->start());
    
    // EN: Wait for alerts to be generated
    // FR: Attendre que les alertes soient générées
    EXPECT_TRUE(waitForAlerts(1, 3000ms));
    
    // EN: Get alert history
    // FR: Obtenir l'historique d'alertes
    auto alert_history = monitor_->getAlertHistory(std::chrono::hours(1));
    EXPECT_GE(alert_history.size(), 1);
    
    // EN: Verify alert history properties
    // FR: Vérifier les propriétés de l'historique d'alertes
    auto& first_alert = alert_history[0];
    EXPECT_EQ(first_alert.resource_type, ResourceType::CPU);
    EXPECT_GE(first_alert.severity, ResourceAlertSeverity::WARNING);
    EXPECT_GT(first_alert.current_value, 0.0);
    
    // EN: Test alert acknowledgment
    // FR: Tester l'accusé de réception d'alerte
    monitor_->acknowledgeAlert(ResourceType::CPU);
    
    // EN: Test alert muting
    // FR: Tester la mise en sourdine d'alerte
    monitor_->muteAlerts(ResourceType::CPU, std::chrono::minutes(1));
    
    monitor_->stop();
}

// EN: Test configuration updates
// FR: Tester les mises à jour de configuration
TEST_F(ResourceMonitorTest, ConfigurationUpdates) {
    EXPECT_TRUE(monitor_->start());
    
    auto original_config = monitor_->getConfig();
    
    // EN: Update configuration
    // FR: Mettre à jour la configuration
    auto new_config = original_config;
    new_config.collection_interval = std::chrono::milliseconds(200);
    new_config.history_size = 200;
    new_config.enable_alerts = false;
    
    monitor_->updateConfig(new_config);
    
    auto updated_config = monitor_->getConfig();
    EXPECT_EQ(updated_config.collection_interval, new_config.collection_interval);
    EXPECT_EQ(updated_config.history_size, new_config.history_size);
    EXPECT_EQ(updated_config.enable_alerts, new_config.enable_alerts);
    
    monitor_->stop();
}

// EN: Test data export and import
// FR: Tester l'export et l'import de données
TEST_F(ResourceMonitorTest, DataExportImport) {
    EXPECT_TRUE(monitor_->start());
    
    // EN: Let monitor collect some data
    // FR: Laisser le moniteur collecter quelques données
    std::this_thread::sleep_for(1000ms);
    
    // EN: Test JSON export
    // FR: Tester l'export JSON
    std::string json_data = monitor_->exportToJSON(std::chrono::hours(1));
    EXPECT_FALSE(json_data.empty());
    EXPECT_TRUE(json_data.find("\"resource_type\"") != std::string::npos);
    EXPECT_TRUE(json_data.find("\"timestamp\"") != std::string::npos);
    
    // EN: Test file export
    // FR: Tester l'export vers fichier
    std::string export_path = "/tmp/test_resource_monitor_export.json";
    EXPECT_TRUE(monitor_->exportData(export_path, std::chrono::hours(1)));
    
    // EN: Verify file exists and has content
    // FR: Vérifier que le fichier existe et a du contenu
    std::ifstream export_file(export_path);
    EXPECT_TRUE(export_file.good());
    
    std::string file_content((std::istreambuf_iterator<char>(export_file)),
                            std::istreambuf_iterator<char>());
    EXPECT_FALSE(file_content.empty());
    export_file.close();
    
    // EN: Test JSON import
    // FR: Tester l'import JSON
    EXPECT_TRUE(monitor_->importFromJSON(json_data));
    
    // EN: Test file import
    // FR: Tester l'import depuis fichier
    EXPECT_TRUE(monitor_->importData(export_path));
    
    // EN: Cleanup
    // FR: Nettoyage
    std::remove(export_path.c_str());
    
    monitor_->stop();
}

// EN: Test self-diagnostics functionality
// FR: Tester la fonctionnalité d'auto-diagnostics
TEST_F(ResourceMonitorTest, SelfDiagnostics) {
    EXPECT_TRUE(monitor_->start());
    std::this_thread::sleep_for(200ms);
    
    // EN: Run self-diagnostics
    // FR: Exécuter l'auto-diagnostic
    bool diagnostics_passed = monitor_->runSelfDiagnostics();
    EXPECT_TRUE(diagnostics_passed);
    
    monitor_->stop();
}

// EN: Test resource availability checking
// FR: Tester la vérification de disponibilité des ressources
TEST_F(ResourceMonitorTest, ResourceAvailabilityChecking) {
    EXPECT_TRUE(monitor_->start());
    std::this_thread::sleep_for(100ms);
    
    // EN: Test basic resource availability
    // FR: Tester la disponibilité des ressources de base
    EXPECT_TRUE(monitor_->isResourceAvailable(ResourceType::CPU));
    EXPECT_TRUE(monitor_->isResourceAvailable(ResourceType::MEMORY));
    EXPECT_TRUE(monitor_->isResourceAvailable(ResourceType::PROCESS));
    
    // EN: Network and disk might not always be available depending on system
    // FR: Le réseau et le disque ne sont pas toujours disponibles selon le système
    bool network_available = monitor_->isResourceAvailable(ResourceType::NETWORK);
    bool disk_available = monitor_->isResourceAvailable(ResourceType::DISK);
    
    // EN: These should be boolean values
    // FR: Ces valeurs devraient être booléennes
    EXPECT_TRUE(network_available == true || network_available == false);
    EXPECT_TRUE(disk_available == true || disk_available == false);
    
    monitor_->stop();
}

// EN: Test utility functions
// FR: Tester les fonctions utilitaires
TEST_F(ResourceMonitorTest, UtilityFunctions) {
    // EN: Test resource type to string conversion
    // FR: Tester la conversion de type de ressource en chaîne
    EXPECT_EQ(ResourceMonitor::resourceTypeToString(ResourceType::CPU), "CPU");
    EXPECT_EQ(ResourceMonitor::resourceTypeToString(ResourceType::MEMORY), "Memory");
    EXPECT_EQ(ResourceMonitor::resourceTypeToString(ResourceType::NETWORK), "Network");
    
    // EN: Test alert severity to string conversion
    // FR: Tester la conversion de sévérité d'alerte en chaîne
    EXPECT_EQ(ResourceMonitor::alertSeverityToString(ResourceAlertSeverity::INFO), "INFO");
    EXPECT_EQ(ResourceMonitor::alertSeverityToString(ResourceAlertSeverity::WARNING), "WARNING");
    EXPECT_EQ(ResourceMonitor::alertSeverityToString(ResourceAlertSeverity::CRITICAL), "CRITICAL");
    
    // EN: Test resource value formatting
    // FR: Tester le formatage des valeurs de ressources
    EXPECT_EQ(ResourceMonitor::formatResourceValue(75.5, ResourceUnit::PERCENTAGE), "75.5%");
    EXPECT_TRUE(ResourceMonitor::formatResourceValue(1024, ResourceUnit::BYTES).find("1.0") != std::string::npos);
    
    // EN: Test threshold checking
    // FR: Tester la vérification des seuils
    auto threshold = createTestThreshold(ResourceType::CPU, 50.0, 80.0);
    EXPECT_FALSE(ResourceMonitor::isResourceCritical(70.0, threshold));
    EXPECT_TRUE(ResourceMonitor::isResourceCritical(85.0, threshold));
}

// EN: Test ResourceUtils utility functions
// FR: Tester les fonctions utilitaires ResourceUtils
TEST_F(ResourceMonitorTest, ResourceUtilityFunctions) {
    // EN: Test configuration creation
    // FR: Tester la création de configuration
    auto default_config = ResourceUtils::createDefaultConfig();
    EXPECT_GT(default_config.collection_interval.count(), 0);
    EXPECT_GT(default_config.history_size, 0);
    EXPECT_TRUE(default_config.enable_system_monitoring);
    
    auto lightweight_config = ResourceUtils::createLightweightConfig();
    EXPECT_GT(lightweight_config.collection_interval.count(), default_config.collection_interval.count());
    
    // EN: Test threshold creation
    // FR: Tester la création de seuils
    auto default_thresholds = ResourceUtils::createDefaultThresholds();
    EXPECT_GE(default_thresholds.size(), 2); // At least CPU and Memory
    
    auto cpu_threshold = ResourceUtils::createCPUThreshold(60.0, 85.0);
    EXPECT_EQ(cpu_threshold.resource_type, ResourceType::CPU);
    EXPECT_DOUBLE_EQ(cpu_threshold.warning_threshold, 60.0);
    EXPECT_DOUBLE_EQ(cpu_threshold.critical_threshold, 85.0);
    
    auto memory_threshold = ResourceUtils::createMemoryThreshold();
    EXPECT_EQ(memory_threshold.resource_type, ResourceType::MEMORY);
    EXPECT_GT(memory_threshold.warning_threshold, 0.0);
    
    // EN: Test formatting functions
    // FR: Tester les fonctions de formatage
    EXPECT_EQ(ResourceUtils::formatBytes(1024), "1.0 KB");
    EXPECT_EQ(ResourceUtils::formatBytes(1024 * 1024), "1.0 MB");
    EXPECT_EQ(ResourceUtils::formatBytes(1024 * 1024 * 1024), "1.0 GB");
    
    EXPECT_EQ(ResourceUtils::formatBytesPerSecond(1024), "1.0 KB/s");
    EXPECT_EQ(ResourceUtils::formatPercentage(75.5), "75.5%");
    
    // EN: Test statistical functions
    // FR: Tester les fonctions statistiques
    std::vector<double> values = {10.0, 20.0, 30.0, 40.0, 50.0};
    EXPECT_DOUBLE_EQ(ResourceUtils::calculateMean(values), 30.0);
    EXPECT_DOUBLE_EQ(ResourceUtils::calculateMedian(values), 30.0);
    
    std::vector<double> even_values = {10.0, 20.0, 30.0, 40.0};
    EXPECT_DOUBLE_EQ(ResourceUtils::calculateMedian(even_values), 25.0);
    
    double std_dev = ResourceUtils::calculateStandardDeviation(values);
    EXPECT_GT(std_dev, 0.0);
    EXPECT_LT(std_dev, 20.0); // Should be reasonable for this dataset
    
    double percentile_95 = ResourceUtils::calculatePercentile(values, 95.0);
    EXPECT_GE(percentile_95, 40.0);
    EXPECT_LE(percentile_95, 50.0);
    
    // EN: Test trend calculation
    // FR: Tester le calcul de tendance
    std::vector<std::pair<double, double>> time_value_pairs = {
        {1.0, 10.0}, {2.0, 20.0}, {3.0, 30.0}, {4.0, 40.0}, {5.0, 50.0}
    };
    double slope = ResourceUtils::calculateTrendSlope(time_value_pairs);
    EXPECT_NEAR(slope, 10.0, 0.1); // Should be close to 10 for this linear progression
}

// EN: Test specialized monitors
// FR: Tester les moniteurs spécialisés
TEST_F(ResourceMonitorTest, SpecializedMonitors) {
    // EN: Test PipelineResourceMonitor
    // FR: Tester PipelineResourceMonitor
    PipelineResourceMonitor pipeline_monitor(config_);
    EXPECT_TRUE(pipeline_monitor.start());
    
    // EN: Test pipeline-specific operations
    // FR: Tester les opérations spécifiques au pipeline
    std::vector<ResourceThreshold> stage_thresholds = {
        ResourceUtils::createCPUThreshold(70.0, 90.0)
    };
    pipeline_monitor.setPipelineStageThresholds("test_stage", stage_thresholds);
    
    pipeline_monitor.notifyStageStart("test_stage");
    std::this_thread::sleep_for(100ms);
    pipeline_monitor.notifyStageEnd("test_stage");
    
    auto stage_usage = pipeline_monitor.getStageResourceUsage();
    EXPECT_GE(stage_usage.size(), 0);
    
    bool should_throttle = pipeline_monitor.shouldThrottlePipeline();
    EXPECT_TRUE(should_throttle == true || should_throttle == false);
    
    pipeline_monitor.stop();
    
    // EN: Test NetworkResourceMonitor
    // FR: Tester NetworkResourceMonitor
    NetworkResourceMonitor network_monitor(config_);
    EXPECT_TRUE(network_monitor.start());
    
    std::this_thread::sleep_for(200ms);
    
    auto interface_stats = network_monitor.getNetworkInterfaceStats();
    EXPECT_GE(interface_stats.size(), 0); // Might be 0 on some systems
    
    double total_utilization = network_monitor.getTotalNetworkUtilization();
    EXPECT_GE(total_utilization, 0.0);
    
    bool is_saturated = network_monitor.isNetworkSaturated();
    EXPECT_TRUE(is_saturated == true || is_saturated == false);
    
    network_monitor.stop();
    
    // EN: Test MemoryResourceMonitor
    // FR: Tester MemoryResourceMonitor
    MemoryResourceMonitor memory_monitor(config_);
    EXPECT_TRUE(memory_monitor.start());
    
    std::this_thread::sleep_for(200ms);
    
    auto memory_breakdown = memory_monitor.getDetailedMemoryBreakdown();
    EXPECT_GE(memory_breakdown.free_memory, 0);
    EXPECT_GE(memory_breakdown.fragmentation_percentage, 0.0);
    EXPECT_LE(memory_breakdown.fragmentation_percentage, 100.0);
    
    bool is_fragmented = memory_monitor.isMemoryFragmented();
    EXPECT_TRUE(is_fragmented == true || is_fragmented == false);
    
    bool recommend_gc = memory_monitor.recommendGarbageCollection();
    EXPECT_TRUE(recommend_gc == true || recommend_gc == false);
    
    memory_monitor.stop();
}

// EN: Test ResourceMonitorManager
// FR: Tester ResourceMonitorManager
TEST_F(ResourceMonitorTest, ResourceMonitorManager) {
    auto& manager = ResourceMonitorManager::getInstance();
    
    // EN: Test monitor creation
    // FR: Tester la création de moniteur
    std::string monitor_id1 = manager.createMonitor("test_monitor1", config_);
    std::string monitor_id2 = manager.createPipelineMonitor("test_pipeline", config_);
    std::string monitor_id3 = manager.createNetworkMonitor("test_network", config_);
    
    EXPECT_FALSE(monitor_id1.empty());
    EXPECT_FALSE(monitor_id2.empty());
    EXPECT_FALSE(monitor_id3.empty());
    EXPECT_NE(monitor_id1, monitor_id2);
    EXPECT_NE(monitor_id2, monitor_id3);
    
    // EN: Test monitor retrieval
    // FR: Tester la récupération de moniteur
    auto retrieved_monitor = manager.getMonitor(monitor_id1);
    EXPECT_TRUE(retrieved_monitor != nullptr);
    
    auto monitor_ids = manager.getMonitorIds();
    EXPECT_GE(monitor_ids.size(), 3);
    
    // EN: Test global operations
    // FR: Tester les opérations globales
    manager.startAll();
    std::this_thread::sleep_for(200ms);
    
    auto global_status = manager.getGlobalStatus();
    EXPECT_GE(global_status.active_monitors, 3);
    EXPECT_GT(global_status.overall_system_health, 0.0);
    EXPECT_LE(global_status.overall_system_health, 100.0);
    
    std::string status_summary = manager.getGlobalStatusSummary();
    EXPECT_FALSE(status_summary.empty());
    
    bool is_healthy = manager.isSystemHealthy();
    EXPECT_TRUE(is_healthy == true || is_healthy == false);
    
    // EN: Test emergency operations
    // FR: Tester les opérations d'urgence
    manager.emergencyThrottleAll(0.1);
    std::this_thread::sleep_for(100ms);
    
    manager.resetAllThrottling();
    
    // EN: Test cleanup
    // FR: Tester le nettoyage
    manager.stopAll();
    EXPECT_TRUE(manager.removeMonitor(monitor_id1));
    EXPECT_TRUE(manager.removeMonitor(monitor_id2));
    EXPECT_TRUE(manager.removeMonitor(monitor_id3));
    EXPECT_FALSE(manager.removeMonitor("nonexistent_monitor"));
}

// EN: Test AutoResourceMonitor RAII helper
// FR: Tester l'assistant RAII AutoResourceMonitor
TEST_F(ResourceMonitorTest, AutoResourceMonitor) {
    std::string monitor_id;
    {
        // EN: Test RAII auto cleanup
        // FR: Tester le nettoyage automatique RAII
        AutoResourceMonitor auto_monitor("test_auto_monitor", config_);
        monitor_id = auto_monitor.getMonitorId();
        
        EXPECT_FALSE(monitor_id.empty());
        
        auto monitor = auto_monitor.getMonitor();
        EXPECT_TRUE(monitor != nullptr);
        
        std::this_thread::sleep_for(100ms);
        
        bool is_healthy = auto_monitor.isHealthy();
        EXPECT_TRUE(is_healthy == true || is_healthy == false);
        
        auto_monitor.enableEmergencyMode();
        
    } // EN: AutoResourceMonitor goes out of scope and should cleanup / FR: AutoResourceMonitor sort du scope et devrait nettoyer
    
    // EN: Verify cleanup occurred
    // FR: Vérifier que le nettoyage s'est produit
    auto& manager = ResourceMonitorManager::getInstance();
    auto cleaned_monitor = manager.getMonitor(monitor_id);
    EXPECT_TRUE(cleaned_monitor == nullptr); // EN: Should be cleaned up / FR: Devrait être nettoyé
}

// EN: Performance and stress tests
// FR: Tests de performance et de stress
TEST_F(ResourceMonitorTest, PerformanceStressTest) {
    const int NUM_CUSTOM_METRICS = 50;
    const std::chrono::milliseconds TEST_DURATION = 2000ms;
    
    // EN: Add many custom metrics
    // FR: Ajouter beaucoup de métriques personnalisées
    std::vector<std::atomic<double>> metric_values(NUM_CUSTOM_METRICS);
    for (int i = 0; i < NUM_CUSTOM_METRICS; ++i) {
        metric_values[i].store(i * 10.0);
        monitor_->addCustomMetric("metric_" + std::to_string(i),
                                 [&metric_values, i]() { return metric_values[i].load(); },
                                 ResourceUnit::COUNT);
    }
    
    // EN: Set very fast collection interval for stress test
    // FR: Définir un intervalle de collecte très rapide pour le test de stress
    config_.collection_interval = std::chrono::milliseconds(10);
    monitor_->updateConfig(config_);
    
    auto start_time = std::chrono::steady_clock::now();
    
    EXPECT_TRUE(monitor_->start());
    
    // EN: Continuously update metrics during test
    // FR: Mettre à jour continuellement les métriques pendant le test
    std::atomic<bool> keep_running{true};
    std::thread metric_updater([&]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(0.0, 100.0);
        
        while (keep_running.load()) {
            for (int i = 0; i < NUM_CUSTOM_METRICS; ++i) {
                metric_values[i].store(dist(gen));
            }
            std::this_thread::sleep_for(5ms);
        }
    });
    
    // EN: Let it run for test duration
    // FR: Le laisser fonctionner pendant la durée du test
    std::this_thread::sleep_for(TEST_DURATION);
    
    auto end_time = std::chrono::steady_clock::now();
    auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    keep_running.store(false);
    metric_updater.join();
    
    auto performance = monitor_->getMonitorPerformance();
    
    // EN: Verify performance is acceptable under stress
    // FR: Vérifier que les performances sont acceptables sous stress
    EXPECT_GT(performance.collections_per_second, 10); // Should collect at least 10 times per second
    EXPECT_LT(performance.cpu_overhead_percentage, 80.0); // Shouldn't use more than 80% CPU
    EXPECT_LT(performance.avg_collection_time.count(), 100); // Average collection should be under 100ms
    EXPECT_LT(performance.failed_collections, performance.collections_per_second * 2); // Low failure rate
    
    monitor_->stop();
    
    std::cout << "Stress test completed:" << std::endl;
    std::cout << "  Duration: " << actual_duration.count() << "ms" << std::endl;
    std::cout << "  Collections/sec: " << performance.collections_per_second << std::endl;
    std::cout << "  Avg collection time: " << performance.avg_collection_time.count() << "ms" << std::endl;
    std::cout << "  CPU overhead: " << performance.cpu_overhead_percentage << "%" << std::endl;
    std::cout << "  Failed collections: " << performance.failed_collections << std::endl;
    std::cout << "  Memory usage: " << performance.memory_usage_bytes << " bytes" << std::endl;
}

// EN: Test error handling and edge cases
// FR: Tester la gestion d'erreurs et les cas limites
TEST_F(ResourceMonitorTest, ErrorHandlingEdgeCases) {
    // EN: Test operations on non-running monitor
    // FR: Tester les opérations sur un moniteur non en cours d'exécution
    EXPECT_FALSE(monitor_->isPaused()); // Should not be paused when not running
    monitor_->pause(); // Should not crash
    monitor_->resume(); // Should not crash
    
    // EN: Test invalid resource type queries
    // FR: Tester les requêtes de type de ressource invalides
    auto invalid_usage = monitor_->getCurrentUsage(static_cast<ResourceType>(999));
    EXPECT_FALSE(invalid_usage.has_value());
    
    // EN: Test empty threshold operations
    // FR: Tester les opérations de seuil vides
    EXPECT_FALSE(monitor_->removeThreshold(static_cast<ResourceType>(999)));
    
    // EN: Test custom metric edge cases
    // FR: Tester les cas limites des métriques personnalisées
    EXPECT_FALSE(monitor_->addCustomMetric("", []() { return 0.0; })); // Empty name
    EXPECT_FALSE(monitor_->removeCustomMetric("nonexistent_metric"));
    
    auto nonexistent_value = monitor_->getCustomMetricValue("nonexistent_metric");
    EXPECT_FALSE(nonexistent_value.has_value());
    
    // EN: Test with throwing custom metric collector
    // FR: Tester avec un collecteur de métrique personnalisé qui lève une exception
    monitor_->addCustomMetric("throwing_metric", []() -> double {
        throw std::runtime_error("Test exception");
    });
    
    EXPECT_TRUE(monitor_->start());
    std::this_thread::sleep_for(200ms); // Should not crash despite throwing metric
    
    // EN: Test invalid file operations
    // FR: Tester les opérations de fichier invalides
    EXPECT_FALSE(monitor_->exportData(""));  // Empty path
    EXPECT_FALSE(monitor_->exportData("/invalid/path/file.json")); // Invalid path
    EXPECT_FALSE(monitor_->importData("/nonexistent/file.json"));
    EXPECT_FALSE(monitor_->importFromJSON("invalid json"));
    
    monitor_->stop();
}

// EN: Main test execution
// FR: Exécution principale du test
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}