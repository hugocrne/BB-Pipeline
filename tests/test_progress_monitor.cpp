// EN: Comprehensive unit tests for Progress Monitor - Real-time progress tracking with ETA
// FR: Tests unitaires complets pour le Moniteur de Progression - Suivi de progression temps réel avec ETA

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <fstream>
#include <atomic>
#include <future>
#include <sstream>
#include "orchestrator/progress_monitor.hpp"

using namespace BBP::Orchestrator;
using namespace std::chrono_literals;

// EN: Test fixture for Progress Monitor tests
// FR: Fixture de test pour les tests du Moniteur de Progression
class ProgressMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // EN: Initialize test configuration
        // FR: Initialiser la configuration de test
        config_ = ProgressUtils::createDefaultConfig();
        config_.update_interval = 10ms;  // EN: Fast updates for testing / FR: Mises à jour rapides pour les tests
        config_.refresh_interval = 5ms;
        config_.enable_colors = false;   // EN: Disable colors for consistent testing / FR: Désactiver les couleurs pour des tests cohérents
        config_.output_stream = &test_output_stream_;
        
        // EN: Create progress monitor instance
        // FR: Créer l'instance du moniteur de progression
        monitor_ = std::make_unique<ProgressMonitor>(config_);
        
        // EN: Reset event tracking
        // FR: Réinitialiser le suivi d'événements
        events_received_.clear();
        event_count_.store(0);
    }
    
    void TearDown() override {
        if (monitor_) {
            monitor_->stop();
        }
        monitor_.reset();
    }
    
    // EN: Helper method to create a simple task configuration
    // FR: Méthode d'aide pour créer une configuration de tâche simple
    ProgressTaskConfig createTestTask(const std::string& id, size_t total_units = 100, double weight = 1.0) {
        ProgressTaskConfig task;
        task.id = id;
        task.name = "Test Task " + id;
        task.description = "Test task for unit testing";
        task.total_units = total_units;
        task.weight = weight;
        task.estimated_duration = 1000ms;
        return task;
    }
    
    // EN: Helper method to setup event callback
    // FR: Méthode d'aide pour configurer le callback d'événement
    void setupEventCallback() {
        monitor_->setEventCallback([this](const ProgressEvent& event) {
            std::lock_guard<std::mutex> lock(events_mutex_);
            events_received_.push_back(event);
            event_count_.fetch_add(1);
        });
    }
    
    // EN: Helper method to wait for specific event count
    // FR: Méthode d'aide pour attendre un nombre d'événements spécifique
    bool waitForEvents(size_t expected_count, std::chrono::milliseconds timeout = 1000ms) {
        auto start_time = std::chrono::steady_clock::now();
        while (event_count_.load() < expected_count) {
            if (std::chrono::steady_clock::now() - start_time > timeout) {
                return false;
            }
            std::this_thread::sleep_for(10ms);
        }
        return true;
    }
    
    // EN: Helper method to get specific events by type
    // FR: Méthode d'aide pour obtenir des événements spécifiques par type
    std::vector<ProgressEvent> getEventsByType(ProgressEventType type) {
        std::lock_guard<std::mutex> lock(events_mutex_);
        std::vector<ProgressEvent> filtered_events;
        for (const auto& event : events_received_) {
            if (event.type == type) {
                filtered_events.push_back(event);
            }
        }
        return filtered_events;
    }
    
    // EN: Helper method to simulate gradual progress
    // FR: Méthode d'aide pour simuler une progression graduelle
    void simulateGradualProgress(const std::string& task_id, size_t total_units, 
                                std::chrono::milliseconds step_delay = 10ms) {
        for (size_t i = 0; i <= total_units; i += 10) {
            monitor_->updateProgress(task_id, std::min(i, total_units));
            std::this_thread::sleep_for(step_delay);
        }
    }
    
protected:
    ProgressMonitorConfig config_;
    std::unique_ptr<ProgressMonitor> monitor_;
    std::ostringstream test_output_stream_;
    
    // EN: Event tracking
    // FR: Suivi d'événements
    std::vector<ProgressEvent> events_received_;
    std::mutex events_mutex_;
    std::atomic<size_t> event_count_{0};
};

// EN: Test basic progress monitor construction and configuration
// FR: Tester la construction et configuration de base du moniteur de progression
TEST_F(ProgressMonitorTest, BasicConstruction) {
    // EN: Test default configuration
    // FR: Tester la configuration par défaut
    EXPECT_TRUE(monitor_ != nullptr);
    
    // EN: Test configuration retrieval
    // FR: Tester la récupération de configuration
    auto retrieved_config = monitor_->getConfig();
    EXPECT_EQ(retrieved_config.update_interval, config_.update_interval);
    EXPECT_EQ(retrieved_config.display_mode, config_.display_mode);
    EXPECT_EQ(retrieved_config.eta_strategy, config_.eta_strategy);
}

// EN: Test task management operations
// FR: Tester les opérations de gestion des tâches
TEST_F(ProgressMonitorTest, TaskManagement) {
    // EN: Test adding tasks
    // FR: Tester l'ajout de tâches
    auto task1 = createTestTask("task1", 100);
    auto task2 = createTestTask("task2", 200, 2.0);
    
    EXPECT_TRUE(monitor_->addTask(task1));
    EXPECT_TRUE(monitor_->addTask(task2));
    
    // EN: Test duplicate task rejection
    // FR: Tester le rejet de tâche dupliquée
    EXPECT_FALSE(monitor_->addTask(task1));
    
    // EN: Test task retrieval
    // FR: Tester la récupération de tâche
    auto task_ids = monitor_->getTaskIds();
    EXPECT_EQ(task_ids.size(), 2);
    EXPECT_TRUE(std::find(task_ids.begin(), task_ids.end(), "task1") != task_ids.end());
    EXPECT_TRUE(std::find(task_ids.begin(), task_ids.end(), "task2") != task_ids.end());
    
    auto retrieved_task = monitor_->getTask("task1");
    ASSERT_TRUE(retrieved_task.has_value());
    EXPECT_EQ(retrieved_task->id, "task1");
    EXPECT_EQ(retrieved_task->total_units, 100);
    
    // EN: Test task removal
    // FR: Tester la suppression de tâche
    EXPECT_TRUE(monitor_->removeTask("task1"));
    EXPECT_FALSE(monitor_->removeTask("task1")); // EN: Should fail second time / FR: Devrait échouer la deuxième fois
    
    task_ids = monitor_->getTaskIds();
    EXPECT_EQ(task_ids.size(), 1);
    EXPECT_EQ(task_ids[0], "task2");
}

// EN: Test progress monitoring lifecycle
// FR: Tester le cycle de vie de la surveillance de progression
TEST_F(ProgressMonitorTest, MonitoringLifecycle) {
    setupEventCallback();
    
    auto task = createTestTask("lifecycle_task", 100);
    EXPECT_TRUE(monitor_->addTask(task));
    
    // EN: Test initial state
    // FR: Tester l'état initial
    EXPECT_FALSE(monitor_->isRunning());
    EXPECT_FALSE(monitor_->isPaused());
    
    // EN: Test starting
    // FR: Tester le démarrage
    EXPECT_TRUE(monitor_->start());
    EXPECT_TRUE(monitor_->isRunning());
    EXPECT_FALSE(monitor_->isPaused());
    
    // EN: Wait for start event
    // FR: Attendre l'événement de démarrage
    EXPECT_TRUE(waitForEvents(1));
    auto start_events = getEventsByType(ProgressEventType::STARTED);
    EXPECT_EQ(start_events.size(), 1);
    
    // EN: Test pausing and resuming
    // FR: Tester la pause et la reprise
    monitor_->pause();
    EXPECT_TRUE(monitor_->isPaused());
    
    monitor_->resume();
    EXPECT_FALSE(monitor_->isPaused());
    
    // EN: Test stopping
    // FR: Tester l'arrêt
    monitor_->stop();
    EXPECT_FALSE(monitor_->isRunning());
    EXPECT_FALSE(monitor_->isPaused());
}

// EN: Test progress updates and statistics calculation
// FR: Tester les mises à jour de progression et le calcul des statistiques
TEST_F(ProgressMonitorTest, ProgressUpdates) {
    setupEventCallback();
    
    auto task = createTestTask("progress_task", 100);
    EXPECT_TRUE(monitor_->addTask(task));
    EXPECT_TRUE(monitor_->start());
    
    // EN: Test initial statistics
    // FR: Tester les statistiques initiales
    auto stats = monitor_->getOverallStatistics();
    EXPECT_EQ(stats.total_units, 100);
    EXPECT_EQ(stats.completed_units, 0);
    EXPECT_DOUBLE_EQ(stats.current_progress, 0.0);
    
    // EN: Test progress updates
    // FR: Tester les mises à jour de progression
    monitor_->updateProgress("progress_task", 25.0);
    std::this_thread::sleep_for(20ms); // EN: Allow processing / FR: Permettre le traitement
    
    stats = monitor_->getOverallStatistics();
    EXPECT_EQ(stats.completed_units, 25);
    EXPECT_DOUBLE_EQ(stats.current_progress, 25.0);
    
    // EN: Test percentage-based updates
    // FR: Tester les mises à jour basées sur le pourcentage
    monitor_->updateProgress("progress_task", 50.0);
    std::this_thread::sleep_for(20ms);
    
    stats = monitor_->getOverallStatistics();
    EXPECT_EQ(stats.completed_units, 50);
    EXPECT_DOUBLE_EQ(stats.current_progress, 50.0);
    
    // EN: Test increment operations
    // FR: Tester les opérations d'incrémentation
    monitor_->incrementProgress("progress_task", 10);
    std::this_thread::sleep_for(20ms);
    
    stats = monitor_->getOverallStatistics();
    EXPECT_EQ(stats.completed_units, 60);
    EXPECT_DOUBLE_EQ(stats.current_progress, 60.0);
    
    monitor_->stop();
}

// EN: Test ETA calculation strategies
// FR: Tester les stratégies de calcul ETA
TEST_F(ProgressMonitorTest, ETACalculation) {
    // EN: Test linear ETA calculation
    // FR: Tester le calcul ETA linéaire
    config_.eta_strategy = ETACalculationStrategy::LINEAR;
    monitor_ = std::make_unique<ProgressMonitor>(config_);
    
    auto task = createTestTask("eta_task", 100);
    EXPECT_TRUE(monitor_->addTask(task));
    EXPECT_TRUE(monitor_->start());
    
    // EN: Simulate some progress
    // FR: Simuler une certaine progression
    monitor_->updateProgress("eta_task", 25.0);
    std::this_thread::sleep_for(100ms); // EN: Let some time pass / FR: Laisser passer du temps
    
    auto stats = monitor_->getOverallStatistics();
    EXPECT_GT(stats.estimated_remaining_time.count(), 0);
    
    // EN: Test moving average ETA
    // FR: Tester l'ETA par moyenne mobile
    config_.eta_strategy = ETACalculationStrategy::MOVING_AVERAGE;
    monitor_ = std::make_unique<ProgressMonitor>(config_);
    
    EXPECT_TRUE(monitor_->addTask(task));
    EXPECT_TRUE(monitor_->start());
    
    // EN: Create progress history
    // FR: Créer un historique de progression
    for (int i = 10; i <= 50; i += 10) {
        monitor_->updateProgress("eta_task", static_cast<double>(i));
        std::this_thread::sleep_for(20ms);
    }
    
    stats = monitor_->getOverallStatistics();
    EXPECT_GT(stats.estimated_remaining_time.count(), 0);
    EXPECT_GT(stats.confidence_level, 0.0);
    
    monitor_->stop();
}

// EN: Test multiple task coordination
// FR: Tester la coordination de tâches multiples
TEST_F(ProgressMonitorTest, MultipleTaskCoordination) {
    setupEventCallback();
    
    // EN: Create multiple tasks with different weights
    // FR: Créer plusieurs tâches avec des poids différents
    auto task1 = createTestTask("multi_task1", 100, 1.0);
    auto task2 = createTestTask("multi_task2", 200, 2.0);
    auto task3 = createTestTask("multi_task3", 150, 1.5);
    
    EXPECT_TRUE(monitor_->addTask(task1));
    EXPECT_TRUE(monitor_->addTask(task2));
    EXPECT_TRUE(monitor_->addTask(task3));
    EXPECT_TRUE(monitor_->start());
    
    // EN: Update progress on multiple tasks
    // FR: Mettre à jour la progression sur plusieurs tâches
    monitor_->updateProgress("multi_task1", 50.0);
    monitor_->updateProgress("multi_task2", 100.0);
    monitor_->updateProgress("multi_task3", 75.0);
    
    std::this_thread::sleep_for(50ms);
    
    auto stats = monitor_->getOverallStatistics();
    EXPECT_EQ(stats.total_units, 450); // 100 + 200 + 150
    EXPECT_EQ(stats.completed_units, 225); // 50 + 100 + 75
    
    // EN: Calculate weighted progress
    // FR: Calculer la progression pondérée
    double expected_weighted_progress = (50*1.0 + 100*2.0 + 75*1.5) / (100*1.0 + 200*2.0 + 150*1.5) * 100.0;
    EXPECT_NEAR(stats.current_progress, expected_weighted_progress, 0.1);
    
    // EN: Test individual task statistics
    // FR: Tester les statistiques de tâches individuelles
    auto task1_stats = monitor_->getTaskStatistics("multi_task1");
    EXPECT_EQ(task1_stats.completed_units, 50);
    EXPECT_DOUBLE_EQ(task1_stats.current_progress, 50.0);
    
    monitor_->stop();
}

// EN: Test task completion and failure handling
// FR: Tester la gestion de completion et d'échec des tâches
TEST_F(ProgressMonitorTest, TaskCompletionAndFailure) {
    setupEventCallback();
    
    auto task1 = createTestTask("complete_task", 100);
    auto task2 = createTestTask("fail_task", 100);
    
    EXPECT_TRUE(monitor_->addTask(task1));
    EXPECT_TRUE(monitor_->addTask(task2));
    EXPECT_TRUE(monitor_->start());
    
    // EN: Test task completion
    // FR: Tester la completion de tâche
    monitor_->updateProgress("complete_task", 100.0);
    monitor_->setTaskCompleted("complete_task");
    
    std::this_thread::sleep_for(50ms);
    
    auto complete_events = getEventsByType(ProgressEventType::STAGE_COMPLETED);
    EXPECT_GE(complete_events.size(), 1);
    
    auto task1_stats = monitor_->getTaskStatistics("complete_task");
    EXPECT_EQ(task1_stats.completed_units, task1_stats.total_units);
    EXPECT_TRUE(task1_stats.isComplete());
    
    // EN: Test task failure
    // FR: Tester l'échec de tâche
    monitor_->updateProgress("fail_task", 50.0);
    monitor_->setTaskFailed("fail_task", "Test error message");
    
    std::this_thread::sleep_for(50ms);
    
    auto fail_events = getEventsByType(ProgressEventType::STAGE_FAILED);
    EXPECT_GE(fail_events.size(), 1);
    EXPECT_EQ(fail_events[0].message, "Test error message");
    
    auto task2_stats = monitor_->getTaskStatistics("fail_task");
    EXPECT_TRUE(task2_stats.hasErrors());
    
    monitor_->stop();
}

// EN: Test batch operations
// FR: Tester les opérations par lot
TEST_F(ProgressMonitorTest, BatchOperations) {
    setupEventCallback();
    
    // EN: Create multiple tasks
    // FR: Créer plusieurs tâches
    std::vector<ProgressTaskConfig> tasks;
    for (int i = 1; i <= 5; ++i) {
        tasks.push_back(createTestTask("batch_task" + std::to_string(i), 100));
    }
    
    EXPECT_TRUE(monitor_->start(tasks));
    
    // EN: Test batch progress updates
    // FR: Tester les mises à jour de progression par lot
    std::map<std::string, size_t> progress_updates;
    progress_updates["batch_task1"] = 50;
    progress_updates["batch_task2"] = 75;
    progress_updates["batch_task3"] = 100;
    
    monitor_->updateMultipleProgress(progress_updates);
    std::this_thread::sleep_for(50ms);
    
    auto stats = monitor_->getOverallStatistics();
    EXPECT_EQ(stats.completed_units, 225); // 50 + 75 + 100
    
    // EN: Test batch completion
    // FR: Tester la completion par lot
    std::vector<std::string> completed_tasks = {"batch_task4", "batch_task5"};
    monitor_->setMultipleCompleted(completed_tasks);
    
    std::this_thread::sleep_for(50ms);
    
    stats = monitor_->getOverallStatistics();
    EXPECT_EQ(stats.completed_units, 425); // Previous 225 + 200 from completed tasks
    
    monitor_->stop();
}

// EN: Test display modes and formatting
// FR: Tester les modes d'affichage et le formatage
TEST_F(ProgressMonitorTest, DisplayModes) {
    auto task = createTestTask("display_task", 100);
    EXPECT_TRUE(monitor_->addTask(task));
    EXPECT_TRUE(monitor_->start());
    
    monitor_->updateProgress("display_task", 50.0);
    std::this_thread::sleep_for(20ms);
    
    // EN: Test simple progress bar
    // FR: Tester la barre de progression simple
    config_.display_mode = ProgressDisplayMode::SIMPLE_BAR;
    monitor_->updateConfig(config_);
    
    std::string display = monitor_->getCurrentDisplayString();
    EXPECT_TRUE(display.find("[") != std::string::npos);
    EXPECT_TRUE(display.find("]") != std::string::npos);
    EXPECT_TRUE(display.find("50%") != std::string::npos);
    
    // EN: Test percentage mode
    // FR: Tester le mode pourcentage
    config_.display_mode = ProgressDisplayMode::PERCENTAGE;
    monitor_->updateConfig(config_);
    
    display = monitor_->getCurrentDisplayString();
    EXPECT_TRUE(display.find("50") != std::string::npos);
    EXPECT_TRUE(display.find("%") != std::string::npos);
    
    // EN: Test JSON mode
    // FR: Tester le mode JSON
    config_.display_mode = ProgressDisplayMode::JSON;
    monitor_->updateConfig(config_);
    
    display = monitor_->getCurrentDisplayString();
    EXPECT_TRUE(display.find("\"progress_percentage\"") != std::string::npos);
    EXPECT_TRUE(display.find("50") != std::string::npos);
    
    monitor_->stop();
}

// EN: Test custom formatter functionality
// FR: Tester la fonctionnalité de formateur personnalisé
TEST_F(ProgressMonitorTest, CustomFormatter) {
    auto task = createTestTask("custom_task", 100);
    EXPECT_TRUE(monitor_->addTask(task));
    
    // EN: Set custom formatter
    // FR: Définir un formateur personnalisé
    monitor_->setCustomFormatter([](const ProgressStatistics& stats, const ProgressMonitorConfig&) {
        return "Custom: " + std::to_string(static_cast<int>(stats.current_progress)) + "% complete";
    });
    
    config_.display_mode = ProgressDisplayMode::CUSTOM;
    monitor_->updateConfig(config_);
    monitor_->start();
    
    monitor_->updateProgress("custom_task", 75.0);
    std::this_thread::sleep_for(20ms);
    
    std::string display = monitor_->getCurrentDisplayString();
    EXPECT_TRUE(display.find("Custom: 75% complete") != std::string::npos);
    
    monitor_->stop();
}

// EN: Test state serialization and persistence
// FR: Tester la sérialisation d'état et la persistance
TEST_F(ProgressMonitorTest, StatePersistence) {
    auto task1 = createTestTask("persist_task1", 100);
    auto task2 = createTestTask("persist_task2", 200);
    
    EXPECT_TRUE(monitor_->addTask(task1));
    EXPECT_TRUE(monitor_->addTask(task2));
    EXPECT_TRUE(monitor_->start());
    
    monitor_->updateProgress("persist_task1", 50.0);
    monitor_->updateProgress("persist_task2", 150.0);
    
    // EN: Test state saving
    // FR: Tester la sauvegarde d'état
    std::string state_file = "/tmp/test_progress_state.json";
    EXPECT_TRUE(monitor_->saveState(state_file));
    
    // EN: Verify file exists and has content
    // FR: Vérifier que le fichier existe et a du contenu
    std::ifstream file(state_file);
    EXPECT_TRUE(file.good());
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    EXPECT_TRUE(content.find("persist_task1") != std::string::npos);
    EXPECT_TRUE(content.find("persist_task2") != std::string::npos);
    
    file.close();
    
    // EN: Test state loading
    // FR: Tester le chargement d'état
    auto new_monitor = std::make_unique<ProgressMonitor>(config_);
    EXPECT_TRUE(new_monitor->loadState(state_file));
    
    // EN: Cleanup
    // FR: Nettoyage
    std::remove(state_file.c_str());
    monitor_->stop();
}

// EN: Test dependency management
// FR: Tester la gestion des dépendances
TEST_F(ProgressMonitorTest, DependencyManagement) {
    auto task1 = createTestTask("dep_task1", 100);
    auto task2 = createTestTask("dep_task2", 100);
    auto task3 = createTestTask("dep_task3", 100);
    
    EXPECT_TRUE(monitor_->addTask(task1));
    EXPECT_TRUE(monitor_->addTask(task2));
    EXPECT_TRUE(monitor_->addTask(task3));
    
    // EN: Set up dependencies: task2 depends on task1, task3 depends on task2
    // FR: Configurer les dépendances: task2 dépend de task1, task3 dépend de task2
    monitor_->addDependency("dep_task2", "dep_task1");
    monitor_->addDependency("dep_task3", "dep_task2");
    
    EXPECT_TRUE(monitor_->start());
    
    // EN: Test ready tasks (only task1 should be ready initially)
    // FR: Tester les tâches prêtes (seule task1 devrait être prête initialement)
    auto ready_tasks = monitor_->getReadyTasks();
    EXPECT_EQ(ready_tasks.size(), 1);
    EXPECT_EQ(ready_tasks[0], "dep_task1");
    
    // EN: Complete task1, now task2 should be ready
    // FR: Terminer task1, maintenant task2 devrait être prête
    monitor_->setTaskCompleted("dep_task1");
    std::this_thread::sleep_for(20ms);
    
    ready_tasks = monitor_->getReadyTasks();
    EXPECT_TRUE(std::find(ready_tasks.begin(), ready_tasks.end(), "dep_task2") != ready_tasks.end());
    EXPECT_FALSE(monitor_->canExecuteTask("dep_task3"));
    
    // EN: Complete task2, now task3 should be ready
    // FR: Terminer task2, maintenant task3 devrait être prête
    monitor_->setTaskCompleted("dep_task2");
    std::this_thread::sleep_for(20ms);
    
    EXPECT_TRUE(monitor_->canExecuteTask("dep_task3"));
    
    monitor_->stop();
}

// EN: Test specialized progress monitors
// FR: Tester les moniteurs de progression spécialisés
TEST_F(ProgressMonitorTest, SpecializedMonitors) {
    // EN: Test file transfer progress monitor
    // FR: Tester le moniteur de progression de transfert de fichier
    FileTransferProgressMonitor file_monitor(config_);
    
    file_monitor.startTransfer("test_file.txt", 1024000); // 1MB
    file_monitor.updateTransferred(512000); // 50%
    file_monitor.setTransferRate(102400); // 100KB/s
    
    std::string transfer_info = file_monitor.getCurrentTransferInfo();
    EXPECT_TRUE(transfer_info.find("test_file.txt") != std::string::npos);
    
    // EN: Test network progress monitor
    // FR: Tester le moniteur de progression réseau
    NetworkProgressMonitor network_monitor(config_);
    
    network_monitor.startNetworkOperation("API Scan", 1000);
    network_monitor.updateCompletedRequests(500);
    network_monitor.updateNetworkStats(150.0, 10.5); // 150ms latency, 10.5 requests/s
    
    std::string network_summary = network_monitor.getNetworkSummary();
    EXPECT_TRUE(network_summary.find("API Scan") != std::string::npos);
    
    // EN: Test batch processing monitor
    // FR: Tester le moniteur de traitement par lot
    BatchProcessingProgressMonitor batch_monitor(config_);
    
    batch_monitor.startBatch("Data Processing", 10000);
    batch_monitor.updateBatchProgress(7500, 250); // 7500 processed, 250 failed
    
    std::map<std::string, size_t> categories;
    categories["success"] = 7250;
    categories["failed"] = 250;
    batch_monitor.reportBatchStats(categories);
    
    std::string batch_summary = batch_monitor.getBatchSummary();
    EXPECT_TRUE(batch_summary.find("Data Processing") != std::string::npos);
}

// EN: Test progress monitor manager
// FR: Tester le gestionnaire de moniteur de progression
TEST_F(ProgressMonitorTest, ProgressMonitorManager) {
    auto& manager = ProgressMonitorManager::getInstance();
    
    // EN: Test monitor creation
    // FR: Tester la création de moniteur
    std::string monitor_id1 = manager.createMonitor("test_monitor1", config_);
    std::string monitor_id2 = manager.createMonitor("test_monitor2", config_);
    
    EXPECT_FALSE(monitor_id1.empty());
    EXPECT_FALSE(monitor_id2.empty());
    EXPECT_NE(monitor_id1, monitor_id2);
    
    // EN: Test monitor retrieval
    // FR: Tester la récupération de moniteur
    auto retrieved_monitor = manager.getMonitor(monitor_id1);
    EXPECT_TRUE(retrieved_monitor != nullptr);
    
    auto monitor_ids = manager.getMonitorIds();
    EXPECT_GE(monitor_ids.size(), 2);
    
    // EN: Test global operations
    // FR: Tester les opérations globales
    auto task1 = createTestTask("manager_task1", 100);
    auto task2 = createTestTask("manager_task2", 200);
    
    retrieved_monitor->addTask(task1);
    retrieved_monitor->start();
    
    auto another_monitor = manager.getMonitor(monitor_id2);
    another_monitor->addTask(task2);
    another_monitor->start();
    
    // EN: Test global statistics
    // FR: Tester les statistiques globales
    auto global_stats = manager.getGlobalStatistics();
    EXPECT_GE(global_stats.total_monitors, 2);
    EXPECT_GE(global_stats.active_monitors, 2);
    EXPECT_EQ(global_stats.total_tasks, 2);
    
    // EN: Test global operations
    // FR: Tester les opérations globales
    manager.pauseAll();
    manager.resumeAll();
    
    // EN: Cleanup
    // FR: Nettoyage
    EXPECT_TRUE(manager.removeMonitor(monitor_id1));
    EXPECT_TRUE(manager.removeMonitor(monitor_id2));
    EXPECT_FALSE(manager.removeMonitor("nonexistent_monitor"));
}

// EN: Test Auto Progress Monitor RAII helper
// FR: Tester l'assistant RAII Auto Progress Monitor
TEST_F(ProgressMonitorTest, AutoProgressMonitor) {
    std::vector<ProgressTaskConfig> tasks;
    tasks.push_back(createTestTask("auto_task1", 100));
    tasks.push_back(createTestTask("auto_task2", 200));
    
    std::string monitor_id;
    {
        // EN: Test RAII auto cleanup
        // FR: Tester le nettoyage automatique RAII
        AutoProgressMonitor auto_monitor("test_auto_monitor", tasks, config_);
        monitor_id = auto_monitor.getMonitorId();
        
        EXPECT_FALSE(monitor_id.empty());
        
        auto monitor = auto_monitor.getMonitor();
        EXPECT_TRUE(monitor != nullptr);
        EXPECT_TRUE(monitor->isRunning());
        
        // EN: Test progress operations through RAII wrapper
        // FR: Tester les opérations de progression via l'enveloppe RAII
        auto_monitor.updateProgress("auto_task1", 50);
        auto_monitor.incrementProgress("auto_task2", 25);
        auto_monitor.setTaskCompleted("auto_task1");
        
        std::this_thread::sleep_for(50ms);
        
        auto stats = monitor->getOverallStatistics();
        EXPECT_GT(stats.completed_units, 0);
        
    } // EN: AutoProgressMonitor goes out of scope and should cleanup / FR: AutoProgressMonitor sort du scope et devrait nettoyer
    
    // EN: Verify cleanup occurred
    // FR: Vérifier que le nettoyage s'est produit
    auto& manager = ProgressMonitorManager::getInstance();
    auto cleaned_monitor = manager.getMonitor(monitor_id);
    EXPECT_TRUE(cleaned_monitor == nullptr); // EN: Should be cleaned up / FR: Devrait être nettoyé
}

// EN: Test utility functions
// FR: Tester les fonctions utilitaires
TEST_F(ProgressMonitorTest, UtilityFunctions) {
    // EN: Test configuration helpers
    // FR: Tester les assistants de configuration
    auto default_config = ProgressUtils::createDefaultConfig();
    EXPECT_EQ(default_config.display_mode, ProgressDisplayMode::DETAILED_BAR);
    EXPECT_TRUE(default_config.show_eta);
    EXPECT_TRUE(default_config.show_speed);
    
    auto quiet_config = ProgressUtils::createQuietConfig();
    EXPECT_EQ(quiet_config.display_mode, ProgressDisplayMode::PERCENTAGE);
    EXPECT_FALSE(quiet_config.show_eta);
    EXPECT_FALSE(quiet_config.enable_colors);
    
    auto verbose_config = ProgressUtils::createVerboseConfig();
    EXPECT_EQ(verbose_config.display_mode, ProgressDisplayMode::VERBOSE);
    
    // EN: Test task generation helpers
    // FR: Tester les assistants de génération de tâches
    std::vector<std::string> filenames = {"file1.txt", "file2.txt", "file3.txt"};
    auto file_tasks = ProgressUtils::createTasksFromFileList(filenames);
    EXPECT_EQ(file_tasks.size(), 3);
    EXPECT_EQ(file_tasks[0].name, "file1.txt");
    
    auto range_tasks = ProgressUtils::createTasksFromRange("batch_item", 5);
    EXPECT_EQ(range_tasks.size(), 5);
    EXPECT_EQ(range_tasks[0].name, "batch_item_1");
    EXPECT_EQ(range_tasks[4].name, "batch_item_5");
    
    auto simple_task = ProgressUtils::createSimpleTask("simple", 150);
    EXPECT_EQ(simple_task.name, "simple");
    EXPECT_EQ(simple_task.total_units, 150);
    
    // EN: Test display utilities
    // FR: Tester les utilitaires d'affichage
    std::string progress_bar = ProgressUtils::createProgressBar(75.0, 20, '#', '-');
    EXPECT_EQ(progress_bar.length(), 20);
    EXPECT_TRUE(progress_bar.find('#') != std::string::npos);
    EXPECT_TRUE(progress_bar.find('-') != std::string::npos);
    
    std::string colored_bar = ProgressUtils::createColoredProgressBar(50.0, 10);
    EXPECT_EQ(colored_bar.length(), 10);
    
    std::string byte_format = ProgressUtils::formatBytes(1536); // 1.5 KB
    EXPECT_TRUE(byte_format.find("1.5") != std::string::npos);
    EXPECT_TRUE(byte_format.find("KB") != std::string::npos);
    
    std::string rate_format = ProgressUtils::formatRate(25.7, "items");
    EXPECT_TRUE(rate_format.find("25.7") != std::string::npos);
    EXPECT_TRUE(rate_format.find("items") != std::string::npos);
    
    // EN: Test ETA utilities
    // FR: Tester les utilitaires ETA
    auto linear_eta = ProgressUtils::calculateLinearETA(25.0, 1000ms);
    EXPECT_GT(linear_eta.count(), 0);
    
    std::vector<double> progress_history = {10.0, 20.0, 30.0, 40.0};
    std::vector<std::chrono::milliseconds> time_history = {100ms, 200ms, 300ms, 400ms};
    auto moving_avg_eta = ProgressUtils::calculateMovingAverageETA(progress_history, time_history);
    EXPECT_GT(moving_avg_eta.count(), 0);
    
    std::vector<double> eta_errors = {0.1, 0.15, 0.08, 0.12, 0.09};
    double confidence = ProgressUtils::calculateETAConfidence(eta_errors);
    EXPECT_GE(confidence, 0.0);
    EXPECT_LE(confidence, 1.0);
}

// EN: Performance and stress tests
// FR: Tests de performance et de stress
TEST_F(ProgressMonitorTest, PerformanceTest) {
    const size_t NUM_TASKS = 100;
    const size_t UPDATES_PER_TASK = 100;
    
    // EN: Create many tasks
    // FR: Créer de nombreuses tâches
    std::vector<ProgressTaskConfig> tasks;
    for (size_t i = 0; i < NUM_TASKS; ++i) {
        tasks.push_back(createTestTask("perf_task_" + std::to_string(i), UPDATES_PER_TASK));
    }
    
    config_.update_mode = ProgressUpdateMode::THROTTLED;
    config_.update_interval = 10ms;
    monitor_ = std::make_unique<ProgressMonitor>(config_);
    
    auto start_time = std::chrono::steady_clock::now();
    
    EXPECT_TRUE(monitor_->start(tasks));
    
    // EN: Perform many updates concurrently
    // FR: Effectuer de nombreuses mises à jour simultanément
    std::vector<std::future<void>> futures;
    for (size_t i = 0; i < NUM_TASKS; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            std::string task_id = "perf_task_" + std::to_string(i);
            for (size_t j = 0; j <= UPDATES_PER_TASK; j += 10) {
                monitor_->updateProgress(task_id, j);
                // EN: Small delay to simulate real work / FR: Petit délai pour simuler un vrai travail
                std::this_thread::sleep_for(1ms);
            }
        }));
    }
    
    // EN: Wait for all updates to complete
    // FR: Attendre que toutes les mises à jour se terminent
    for (auto& future : futures) {
        future.get();
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // EN: Verify final state
    // FR: Vérifier l'état final
    auto stats = monitor_->getOverallStatistics();
    EXPECT_EQ(stats.total_units, NUM_TASKS * UPDATES_PER_TASK);
    
    // EN: Performance should be reasonable
    // FR: La performance devrait être raisonnable
    EXPECT_LT(total_time.count(), 10000); // EN: Should complete within 10 seconds / FR: Devrait se terminer en moins de 10 secondes
    
    monitor_->stop();
    
    std::cout << "Performance test completed in " << total_time.count() << "ms" << std::endl;
    std::cout << "Processed " << NUM_TASKS * UPDATES_PER_TASK << " total progress updates" << std::endl;
    std::cout << "Average: " << static_cast<double>(NUM_TASKS * UPDATES_PER_TASK) / total_time.count() 
              << " updates/ms" << std::endl;
}

// EN: Test error handling and edge cases
// FR: Tester la gestion d'erreurs et les cas limites
TEST_F(ProgressMonitorTest, ErrorHandling) {
    // EN: Test invalid task operations
    // FR: Tester les opérations de tâches invalides
    EXPECT_FALSE(monitor_->removeTask("nonexistent_task"));
    EXPECT_FALSE(monitor_->getTask("nonexistent_task").has_value());
    
    // EN: Test operations on non-running monitor
    // FR: Tester les opérations sur un moniteur non en cours d'exécution
    monitor_->updateProgress("nonexistent_task", 50.0); // EN: Should not crash / FR: Ne devrait pas planter
    monitor_->setTaskCompleted("nonexistent_task"); // EN: Should not crash / FR: Ne devrait pas planter
    
    // EN: Test edge case values
    // FR: Tester les valeurs de cas limites
    auto task = createTestTask("edge_task", 100);
    EXPECT_TRUE(monitor_->addTask(task));
    EXPECT_TRUE(monitor_->start());
    
    // EN: Test progress beyond bounds
    // FR: Tester la progression au-delà des limites
    monitor_->updateProgress("edge_task", 150.0); // EN: Should be clamped to 100 / FR: Devrait être limité à 100
    auto stats = monitor_->getTaskStatistics("edge_task");
    EXPECT_EQ(stats.completed_units, 100);
    
    // EN: Test negative progress (via percentage)
    // FR: Tester la progression négative (via pourcentage)
    monitor_->updateProgress("edge_task", -10.0); // EN: Should be handled gracefully / FR: Devrait être géré avec élégance
    
    // EN: Test invalid configurations
    // FR: Tester les configurations invalides
    ProgressMonitorConfig invalid_config;
    invalid_config.update_interval = std::chrono::milliseconds(0); // EN: Invalid interval / FR: Intervalle invalide
    invalid_config.progress_bar_width = 0; // EN: Invalid width / FR: Largeur invalide
    
    // EN: Monitor should still work with reasonable defaults / FR: Le moniteur devrait toujours fonctionner avec des défauts raisonnables
    auto edge_monitor = std::make_unique<ProgressMonitor>(invalid_config);
    EXPECT_TRUE(edge_monitor->addTask(createTestTask("invalid_config_task", 10)));
    
    monitor_->stop();
}

// EN: Main test execution
// FR: Exécution principale du test
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}