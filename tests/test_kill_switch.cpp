// EN: Comprehensive unit tests for the Kill Switch system with 100% coverage.
// FR: Tests unitaires complets pour le système Kill Switch avec couverture à 100%.

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <future>

#include "orchestrator/kill_switch.hpp"
#include "infrastructure/logging/logger.hpp"

using namespace BBP::Orchestrator;
using namespace std::chrono_literals;

// EN: Mock classes for callback testing
// FR: Classes mock pour tester les callbacks
class MockStatePreservation {
public:
    MOCK_METHOD(std::optional<StateSnapshot>, preserveState, (const std::string& component_id), ());
};

class MockTaskTermination {
public:
    MOCK_METHOD(bool, terminateTask, (const std::string& task_id, std::chrono::milliseconds timeout), ());
};

class MockCleanupOperation {
public:
    MOCK_METHOD(void, cleanup, (const std::string& operation_name), ());
};

class MockNotificationHandler {
public:
    MOCK_METHOD(void, notify, (KillSwitchTrigger trigger, KillSwitchPhase phase, const std::string& details), ());
};

// EN: Test fixture for Kill Switch tests
// FR: Fixture de test pour les tests Kill Switch
class KillSwitchTest : public ::testing::Test {
protected:
    void SetUp() override {
        // EN: Reset Kill Switch to clean state
        // FR: Remettre le Kill Switch à un état propre
        kill_switch_ = &KillSwitch::getInstance();
        kill_switch_->reset();
        
        // EN: Create test state directory
        // FR: Créer le répertoire d'état de test
        test_state_dir_ = "./test_kill_switch_state";
        std::filesystem::create_directories(test_state_dir_);
        
        // EN: Configure with test settings
        // FR: Configurer avec les paramètres de test
        test_config_ = KillSwitchUtils::createDefaultConfig();
        test_config_.state_directory = test_state_dir_;
        test_config_.total_shutdown_timeout = 1000ms;
        test_config_.task_stop_timeout = 200ms;
        test_config_.state_save_timeout = 200ms;
        test_config_.cleanup_timeout = 200ms;
        
        BBP::Infrastructure::Logger::getInstance().info("TEST", "About to configure");
        kill_switch_->configure(test_config_);
        BBP::Infrastructure::Logger::getInstance().info("TEST", "About to initialize");
        kill_switch_->initialize();
        BBP::Infrastructure::Logger::getInstance().info("TEST", "About to call setEnabled(true)");
        kill_switch_->setEnabled(true);
        BBP::Infrastructure::Logger::getInstance().info("TEST", "Called setEnabled(true) successfully");
        
        // EN: Create mock objects
        // FR: Créer les objets mock
        mock_state_ = std::make_unique<MockStatePreservation>();
        mock_task_ = std::make_unique<MockTaskTermination>();
        mock_cleanup_ = std::make_unique<MockCleanupOperation>();
        mock_notification_ = std::make_unique<MockNotificationHandler>();
    }

    void TearDown() override {
        // EN: Ensure Kill Switch is reset (SetUp will handle re-enabling)
        // FR: S'assurer que le Kill Switch est remis à zéro (SetUp se chargera de la réactivation)
        if (kill_switch_) {
            kill_switch_->reset();
        }
        
        // EN: Clean up test directory
        // FR: Nettoyer le répertoire de test
        try {
            if (std::filesystem::exists(test_state_dir_)) {
                std::filesystem::remove_all(test_state_dir_);
            }
        } catch (...) {
            // EN: Ignore cleanup errors in tests
            // FR: Ignorer les erreurs de nettoyage dans les tests
        }
        
        mock_state_.reset();
        mock_task_.reset();
        mock_cleanup_.reset();
        mock_notification_.reset();
    }

    // EN: Helper to create a test state snapshot
    // FR: Aide pour créer un instantané d'état de test
    StateSnapshot createTestSnapshot(const std::string& component_id) {
        StateSnapshot snapshot;
        snapshot.component_id = component_id;
        snapshot.operation_id = "test_operation_123";
        snapshot.timestamp = std::chrono::system_clock::now();
        snapshot.state_type = "test_state";
        snapshot.state_data = R"({"test_key": "test_value", "timestamp": 1234567890})";
        snapshot.metadata["test_meta"] = "test_meta_value";
        snapshot.data_size = snapshot.state_data.size();
        snapshot.checksum = 0x12345678;
        snapshot.priority = 1;
        return snapshot;
    }

protected:
    KillSwitch* kill_switch_;
    KillSwitchConfig test_config_;
    std::string test_state_dir_;
    
    std::unique_ptr<MockStatePreservation> mock_state_;
    std::unique_ptr<MockTaskTermination> mock_task_;
    std::unique_ptr<MockCleanupOperation> mock_cleanup_;
    std::unique_ptr<MockNotificationHandler> mock_notification_;
};

// EN: Basic functionality tests
// FR: Tests de fonctionnalité de base

TEST_F(KillSwitchTest, Singleton_ShouldReturnSameInstance) {
    // EN: Test singleton pattern implementation
    // FR: Tester l'implémentation du pattern singleton
    
    auto& instance1 = KillSwitch::getInstance();
    auto& instance2 = KillSwitch::getInstance();
    
    EXPECT_EQ(&instance1, &instance2) << "Singleton should return same instance";
    EXPECT_EQ(&instance1, kill_switch_) << "Instance should match test fixture";
}

TEST_F(KillSwitchTest, Configuration_ShouldAcceptValidConfig) {
    // EN: Test configuration with valid settings
    // FR: Tester la configuration avec des paramètres valides
    
    KillSwitchConfig config = KillSwitchUtils::createDefaultConfig();
    config.total_shutdown_timeout = 5000ms;
    config.state_directory = "./custom_test_state";
    
    EXPECT_NO_THROW(kill_switch_->configure(config)) << "Valid configuration should be accepted";
    
    auto retrieved_config = kill_switch_->getConfig();
    EXPECT_EQ(retrieved_config.total_shutdown_timeout, 5000ms) << "Configuration should be applied";
    EXPECT_EQ(retrieved_config.state_directory, "./custom_test_state") << "State directory should be set";
}

TEST_F(KillSwitchTest, Configuration_ShouldRejectInvalidConfig) {
    // EN: Test configuration with invalid settings
    // FR: Tester la configuration avec des paramètres invalides
    
    KillSwitchConfig invalid_config;
    invalid_config.total_shutdown_timeout = -1000ms;  // Invalid: negative timeout
    invalid_config.state_directory = "";              // Invalid: empty directory
    
    EXPECT_THROW(kill_switch_->configure(invalid_config), std::invalid_argument) 
        << "Invalid configuration should be rejected";
}

TEST_F(KillSwitchTest, Initialization_ShouldCreateStateDirectory) {
    // EN: Test that initialization creates state directory
    // FR: Tester que l'initialisation crée le répertoire d'état
    
    std::string custom_dir = "./test_init_state_dir";
    KillSwitchConfig config = test_config_;
    config.state_directory = custom_dir;
    
    kill_switch_->reset();
    kill_switch_->configure(config);
    
    EXPECT_NO_THROW(kill_switch_->initialize()) << "Initialization should succeed";
    EXPECT_TRUE(std::filesystem::exists(custom_dir)) << "State directory should be created";
    
    // EN: Cleanup
    // FR: Nettoyage
    std::filesystem::remove_all(custom_dir);
}

TEST_F(KillSwitchTest, CallbackRegistration_ShouldAcceptValidCallbacks) {
    // EN: Test callback registration with valid parameters
    // FR: Tester l'enregistrement de callbacks avec paramètres valides
    
    EXPECT_NO_THROW(kill_switch_->registerStatePreservationCallback("test_component", 
        [this](const std::string& component_id) -> std::optional<StateSnapshot> {
            return mock_state_->preserveState(component_id);
        })) << "State preservation callback should be registered";
    
    EXPECT_NO_THROW(kill_switch_->registerTaskTerminationCallback("test_task",
        [this](const std::string& task_id, std::chrono::milliseconds timeout) -> bool {
            return mock_task_->terminateTask(task_id, timeout);
        })) << "Task termination callback should be registered";
    
    EXPECT_NO_THROW(kill_switch_->registerCleanupCallback("test_cleanup",
        [this](const std::string& operation_name) {
            mock_cleanup_->cleanup(operation_name);
        })) << "Cleanup callback should be registered";
    
    EXPECT_NO_THROW(kill_switch_->registerNotificationCallback("test_notification",
        [this](KillSwitchTrigger trigger, KillSwitchPhase phase, const std::string& details) {
            mock_notification_->notify(trigger, phase, details);
        })) << "Notification callback should be registered";
}

TEST_F(KillSwitchTest, CallbackRegistration_ShouldRejectEmptyIds) {
    // EN: Test that empty IDs are rejected in callback registration
    // FR: Tester que les IDs vides sont rejetés dans l'enregistrement de callbacks
    
    EXPECT_THROW(kill_switch_->registerStatePreservationCallback("", 
        [](const std::string&) -> std::optional<StateSnapshot> { return std::nullopt; }), 
        std::invalid_argument) << "Empty component ID should be rejected";
    
    EXPECT_THROW(kill_switch_->registerTaskTerminationCallback("",
        [](const std::string&, std::chrono::milliseconds) -> bool { return true; }),
        std::invalid_argument) << "Empty task type should be rejected";
    
    EXPECT_THROW(kill_switch_->registerCleanupCallback("",
        [](const std::string&) {}),
        std::invalid_argument) << "Empty operation name should be rejected";
    
    EXPECT_THROW(kill_switch_->registerNotificationCallback("",
        [](KillSwitchTrigger, KillSwitchPhase, const std::string&) {}),
        std::invalid_argument) << "Empty notification ID should be rejected";
}

// EN: Trigger and shutdown tests
// FR: Tests de déclenchement et d'arrêt

TEST_F(KillSwitchTest, Trigger_ShouldChangeStateCorrectly) {
    // EN: Test that triggering changes state appropriately
    // FR: Tester que le déclenchement change l'état de façon appropriée
    
    EXPECT_FALSE(kill_switch_->isTriggered()) << "Should not be triggered initially";
    EXPECT_FALSE(kill_switch_->isShuttingDown()) << "Should not be shutting down initially";
    EXPECT_EQ(kill_switch_->getCurrentPhase(), KillSwitchPhase::INACTIVE) << "Should be inactive initially";
    
    kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "Test trigger");
    
    EXPECT_TRUE(kill_switch_->isTriggered()) << "Should be triggered after trigger call";
    
    // EN: Wait a bit for shutdown to start
    // FR: Attendre un peu que l'arrêt commence
    std::this_thread::sleep_for(50ms);
    
    EXPECT_NE(kill_switch_->getCurrentPhase(), KillSwitchPhase::INACTIVE) << "Phase should have changed";
}

TEST_F(KillSwitchTest, Trigger_ShouldIgnoreDisabledState) {
    // EN: Test that triggering is ignored when Kill Switch is disabled
    // FR: Tester que le déclenchement est ignoré quand le Kill Switch est désactivé
    
    kill_switch_->setEnabled(false);
    
    kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "Should be ignored");
    
    EXPECT_FALSE(kill_switch_->isTriggered()) << "Should not be triggered when disabled";
    EXPECT_EQ(kill_switch_->getCurrentPhase(), KillSwitchPhase::INACTIVE) << "Should remain inactive";
    
    kill_switch_->setEnabled(true);
}

TEST_F(KillSwitchTest, Trigger_ShouldIgnoreDuplicateTriggers) {
    // EN: Test that duplicate triggers are ignored
    // FR: Tester que les déclencheurs dupliqués sont ignorés
    
    kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "First trigger");
    
    EXPECT_TRUE(kill_switch_->isTriggered()) << "Should be triggered after first call";
    
    kill_switch_->trigger(KillSwitchTrigger::TIMEOUT, "Second trigger (should be ignored)");
    
    // EN: Check that statistics only show one trigger
    // FR: Vérifier que les statistiques ne montrent qu'un déclencheur
    auto stats = kill_switch_->getStats();
    EXPECT_EQ(stats.total_triggers, 1) << "Should only count first trigger";
    EXPECT_EQ(stats.trigger_counts[KillSwitchTrigger::USER_REQUEST], 1) << "Should count USER_REQUEST trigger";
    EXPECT_EQ(stats.trigger_counts[KillSwitchTrigger::TIMEOUT], 0) << "Should not count duplicate TIMEOUT trigger";
}

TEST_F(KillSwitchTest, TriggerWithTimeout_ShouldUseCustomTimeout) {
    // EN: Test triggering with custom timeout
    // FR: Tester le déclenchement avec timeout personnalisé
    
    auto custom_timeout = 2000ms;
    
    kill_switch_->triggerWithTimeout(KillSwitchTrigger::TIMEOUT, custom_timeout, "Custom timeout test");
    
    EXPECT_TRUE(kill_switch_->isTriggered()) << "Should be triggered with custom timeout";
    
    // EN: Wait for shutdown and verify it uses custom timeout
    // FR: Attendre l'arrêt et vérifier qu'il utilise le timeout personnalisé
    bool completed = kill_switch_->waitForCompletion(custom_timeout + 500ms);
    EXPECT_TRUE(completed) << "Shutdown should complete within custom timeout + buffer";
}

TEST_F(KillSwitchTest, WaitForCompletion_ShouldReturnCorrectly) {
    // EN: Test waiting for completion with different scenarios
    // FR: Tester l'attente de completion avec différents scénarios
    
    // EN: Test waiting without trigger (should return immediately)
    // FR: Tester l'attente sans déclencheur (devrait retourner immédiatement)
    bool result = kill_switch_->waitForCompletion(100ms);
    EXPECT_TRUE(result) << "Should return true when not triggered";
    
    // EN: Test waiting with trigger
    // FR: Tester l'attente avec déclencheur
    kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "Wait test");
    
    result = kill_switch_->waitForCompletion(2000ms);
    EXPECT_TRUE(result) << "Should complete within reasonable time";
    
    EXPECT_EQ(kill_switch_->getCurrentPhase(), KillSwitchPhase::COMPLETED) << "Should reach completed phase";
}

TEST_F(KillSwitchTest, ForceImmediate_ShouldBypassGracefulShutdown) {
    // EN: Test force immediate shutdown
    // FR: Tester l'arrêt immédiat forcé
    
    kill_switch_->forceImmediate("Emergency force test");
    
    EXPECT_TRUE(kill_switch_->isTriggered()) << "Should be triggered by force immediate";
    
    auto config = kill_switch_->getConfig();
    EXPECT_TRUE(config.force_immediate_stop) << "Force immediate flag should be set";
    
    // EN: Wait for completion
    // FR: Attendre la completion
    bool completed = kill_switch_->waitForCompletion(1000ms);
    EXPECT_TRUE(completed) << "Force immediate should complete quickly";
    
    auto stats = kill_switch_->getStats();
    EXPECT_EQ(stats.forced_shutdowns, 1) << "Should record forced shutdown";
}

// EN: State preservation tests
// FR: Tests de préservation d'état

TEST_F(KillSwitchTest, StatePreservation_ShouldCallRegisteredCallbacks) {
    // EN: Test that state preservation calls registered callbacks
    // FR: Tester que la préservation d'état appelle les callbacks enregistrés
    
    using testing::Return;
    using testing::_;
    
    // EN: Set up mock expectations
    // FR: Configurer les attentes mock
    auto test_snapshot = createTestSnapshot("test_component");
    EXPECT_CALL(*mock_state_, preserveState("test_component"))
        .WillOnce(Return(test_snapshot));
    
    // EN: Register callback
    // FR: Enregistrer le callback
    kill_switch_->registerStatePreservationCallback("test_component", 
        [this](const std::string& component_id) -> std::optional<StateSnapshot> {
            return mock_state_->preserveState(component_id);
        });
    
    // EN: Trigger shutdown
    // FR: Déclencher l'arrêt
    kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "State preservation test");
    
    // EN: Wait for completion
    // FR: Attendre la completion
    bool completed = kill_switch_->waitForCompletion(2000ms);
    EXPECT_TRUE(completed) << "Shutdown should complete";
    
    // EN: Verify state preservation was called
    // FR: Vérifier que la préservation d'état a été appelée
    auto stats = kill_switch_->getStats();
    EXPECT_GT(stats.total_states_saved, 0) << "Should have saved at least one state";
}

TEST_F(KillSwitchTest, StatePreservation_ShouldHandleExceptions) {
    // EN: Test that state preservation handles callback exceptions
    // FR: Tester que la préservation d'état gère les exceptions de callback
    
    using testing::Throw;
    using testing::_;
    
    // EN: Set up mock to throw exception
    // FR: Configurer le mock pour lancer une exception
    EXPECT_CALL(*mock_state_, preserveState(_))
        .WillOnce(Throw(std::runtime_error("Test exception")));
    
    // EN: Register callback that throws
    // FR: Enregistrer un callback qui lance une exception
    kill_switch_->registerStatePreservationCallback("error_component", 
        [this](const std::string& component_id) -> std::optional<StateSnapshot> {
            return mock_state_->preserveState(component_id);
        });
    
    // EN: Trigger shutdown (should not crash)
    // FR: Déclencher l'arrêt (ne devrait pas planter)
    EXPECT_NO_THROW(kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "Exception test"));
    
    // EN: Wait for completion
    // FR: Attendre la completion
    bool completed = kill_switch_->waitForCompletion(2000ms);
    EXPECT_TRUE(completed) << "Shutdown should complete even with exceptions";
    
    auto stats = kill_switch_->getStats();
    EXPECT_GT(stats.state_save_failures, 0) << "Should record state save failures";
}

TEST_F(KillSwitchTest, StateFileOperations_ShouldSaveAndLoadCorrectly) {
    // EN: Test state file save and load operations
    // FR: Tester les opérations de sauvegarde et chargement de fichiers d'état
    
    // EN: Create test snapshot and trigger state preservation
    // FR: Créer un instantané de test et déclencher la préservation d'état
    auto test_snapshot = createTestSnapshot("file_test_component");
    
    kill_switch_->registerStatePreservationCallback("file_test_component", 
        [test_snapshot](const std::string& /*component_id*/) -> std::optional<StateSnapshot> {
            return test_snapshot;
        });
    
    kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "File operations test");
    bool completed = kill_switch_->waitForCompletion(2000ms);
    EXPECT_TRUE(completed) << "Shutdown should complete";
    
    // EN: Check that state files were created
    // FR: Vérifier que les fichiers d'état ont été créés
    bool found_state_file = false;
    for (const auto& entry : std::filesystem::directory_iterator(test_state_dir_)) {
        if (entry.path().extension() == ".json" && 
            entry.path().filename().string().find("bb_pipeline_state_") == 0) {
            found_state_file = true;
            break;
        }
    }
    EXPECT_TRUE(found_state_file) << "State file should be created";
    
    // EN: Reset Kill Switch and load preserved state
    // FR: Remettre à zéro le Kill Switch et charger l'état préservé
    kill_switch_->reset();
    kill_switch_->configure(test_config_);
    kill_switch_->initialize();
    
    auto loaded_snapshots = kill_switch_->loadPreservedState();
    EXPECT_FALSE(loaded_snapshots.empty()) << "Should load preserved snapshots";
    
    // EN: Verify loaded data matches original
    // FR: Vérifier que les données chargées correspondent à l'original
    bool found_matching_snapshot = false;
    for (const auto& snapshot : loaded_snapshots) {
        if (snapshot.component_id == test_snapshot.component_id &&
            snapshot.operation_id == test_snapshot.operation_id) {
            found_matching_snapshot = true;
            EXPECT_EQ(snapshot.state_data, test_snapshot.state_data) << "State data should match";
            break;
        }
    }
    EXPECT_TRUE(found_matching_snapshot) << "Should find matching loaded snapshot";
}

// EN: Task termination tests
// FR: Tests de terminaison de tâche

TEST_F(KillSwitchTest, TaskTermination_ShouldCallRegisteredCallbacks) {
    // EN: Test that task termination calls registered callbacks
    // FR: Tester que la terminaison de tâche appelle les callbacks enregistrés
    
    using testing::Return;
    using testing::_;
    using testing::Ge;
    
    // EN: Set up mock expectations
    // FR: Configurer les attentes mock
    EXPECT_CALL(*mock_task_, terminateTask("test_task", Ge(50ms)))
        .WillOnce(Return(true));
    
    // EN: Register callback
    // FR: Enregistrer le callback
    kill_switch_->registerTaskTerminationCallback("test_task", 
        [this](const std::string& task_id, std::chrono::milliseconds timeout) -> bool {
            return mock_task_->terminateTask(task_id, timeout);
        });
    
    // EN: Trigger shutdown
    // FR: Déclencher l'arrêt
    kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "Task termination test");
    
    // EN: Wait for completion
    // FR: Attendre la completion
    bool completed = kill_switch_->waitForCompletion(2000ms);
    EXPECT_TRUE(completed) << "Shutdown should complete";
}

// EN: Cleanup operation tests
// FR: Tests d'opération de nettoyage

TEST_F(KillSwitchTest, CleanupOperations_ShouldCallRegisteredCallbacks) {
    // EN: Test that cleanup operations call registered callbacks
    // FR: Tester que les opérations de nettoyage appellent les callbacks enregistrés
    
    using testing::_;
    
    // EN: Set up mock expectations
    // FR: Configurer les attentes mock
    EXPECT_CALL(*mock_cleanup_, cleanup("test_cleanup"))
        .Times(1);
    
    // EN: Register callback
    // FR: Enregistrer le callback
    kill_switch_->registerCleanupCallback("test_cleanup", 
        [this](const std::string& operation_name) {
            mock_cleanup_->cleanup(operation_name);
        });
    
    // EN: Trigger shutdown
    // FR: Déclencher l'arrêt
    kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "Cleanup test");
    
    // EN: Wait for completion
    // FR: Attendre la completion
    bool completed = kill_switch_->waitForCompletion(2000ms);
    EXPECT_TRUE(completed) << "Shutdown should complete";
}

// EN: Notification tests
// FR: Tests de notification

TEST_F(KillSwitchTest, Notifications_ShouldCallRegisteredCallbacks) {
    // EN: Test that notifications are sent to registered callbacks
    // FR: Tester que les notifications sont envoyées aux callbacks enregistrés
    
    using testing::_;
    using testing::AtLeast;
    
    // EN: Set up mock expectations (should receive multiple notifications during shutdown)
    // FR: Configurer les attentes mock (devrait recevoir plusieurs notifications pendant l'arrêt)
    EXPECT_CALL(*mock_notification_, notify(_, _, _))
        .Times(AtLeast(3));  // TRIGGERED, at least one phase, COMPLETED
    
    // EN: Register callback
    // FR: Enregistrer le callback
    kill_switch_->registerNotificationCallback("test_notification", 
        [this](KillSwitchTrigger trigger, KillSwitchPhase phase, const std::string& details) {
            mock_notification_->notify(trigger, phase, details);
        });
    
    // EN: Trigger shutdown
    // FR: Déclencher l'arrêt
    kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "Notification test");
    
    // EN: Wait for completion
    // FR: Attendre la completion
    bool completed = kill_switch_->waitForCompletion(2000ms);
    EXPECT_TRUE(completed) << "Shutdown should complete";
}

// EN: Statistics tests
// FR: Tests de statistiques

TEST_F(KillSwitchTest, Statistics_ShouldBeUpdatedCorrectly) {
    // EN: Test that statistics are updated during shutdown
    // FR: Tester que les statistiques sont mises à jour pendant l'arrêt
    
    auto initial_stats = kill_switch_->getStats();
    EXPECT_EQ(initial_stats.total_triggers, 0) << "Should start with no triggers";
    EXPECT_EQ(initial_stats.successful_shutdowns, 0) << "Should start with no successful shutdowns";
    
    kill_switch_->trigger(KillSwitchTrigger::CRITICAL_ERROR, "Statistics test");
    
    bool completed = kill_switch_->waitForCompletion(2000ms);
    EXPECT_TRUE(completed) << "Shutdown should complete";
    
    auto final_stats = kill_switch_->getStats();
    EXPECT_EQ(final_stats.total_triggers, 1) << "Should record one trigger";
    EXPECT_EQ(final_stats.successful_shutdowns, 1) << "Should record one successful shutdown";
    EXPECT_EQ(final_stats.trigger_counts[KillSwitchTrigger::CRITICAL_ERROR], 1) << "Should count CRITICAL_ERROR trigger";
    EXPECT_GT(final_stats.avg_shutdown_time.count(), 0) << "Should record shutdown time";
    EXPECT_FALSE(final_stats.recent_trigger_reasons.empty()) << "Should record trigger reason";
}

// EN: Utility function tests
// FR: Tests de fonctions utilitaires

TEST(KillSwitchUtilsTest, TriggerToString_ShouldReturnCorrectStrings) {
    // EN: Test trigger enum to string conversion
    // FR: Tester la conversion enum trigger vers chaîne
    
    EXPECT_EQ(KillSwitchUtils::triggerToString(KillSwitchTrigger::USER_REQUEST), "USER_REQUEST");
    EXPECT_EQ(KillSwitchUtils::triggerToString(KillSwitchTrigger::SYSTEM_SIGNAL), "SYSTEM_SIGNAL");
    EXPECT_EQ(KillSwitchUtils::triggerToString(KillSwitchTrigger::TIMEOUT), "TIMEOUT");
    EXPECT_EQ(KillSwitchUtils::triggerToString(KillSwitchTrigger::RESOURCE_EXHAUSTION), "RESOURCE_EXHAUSTION");
    EXPECT_EQ(KillSwitchUtils::triggerToString(KillSwitchTrigger::CRITICAL_ERROR), "CRITICAL_ERROR");
    EXPECT_EQ(KillSwitchUtils::triggerToString(KillSwitchTrigger::DEPENDENCY_FAILURE), "DEPENDENCY_FAILURE");
    EXPECT_EQ(KillSwitchUtils::triggerToString(KillSwitchTrigger::SECURITY_THREAT), "SECURITY_THREAT");
    EXPECT_EQ(KillSwitchUtils::triggerToString(KillSwitchTrigger::EXTERNAL_COMMAND), "EXTERNAL_COMMAND");
}

TEST(KillSwitchUtilsTest, PhaseToString_ShouldReturnCorrectStrings) {
    // EN: Test phase enum to string conversion
    // FR: Tester la conversion enum phase vers chaîne
    
    EXPECT_EQ(KillSwitchUtils::phaseToString(KillSwitchPhase::INACTIVE), "INACTIVE");
    EXPECT_EQ(KillSwitchUtils::phaseToString(KillSwitchPhase::TRIGGERED), "TRIGGERED");
    EXPECT_EQ(KillSwitchUtils::phaseToString(KillSwitchPhase::STOPPING_TASKS), "STOPPING_TASKS");
    EXPECT_EQ(KillSwitchUtils::phaseToString(KillSwitchPhase::SAVING_STATE), "SAVING_STATE");
    EXPECT_EQ(KillSwitchUtils::phaseToString(KillSwitchPhase::CLEANUP), "CLEANUP");
    EXPECT_EQ(KillSwitchUtils::phaseToString(KillSwitchPhase::FINALIZING), "FINALIZING");
    EXPECT_EQ(KillSwitchUtils::phaseToString(KillSwitchPhase::COMPLETED), "COMPLETED");
}

TEST(KillSwitchUtilsTest, ValidateConfig_ShouldDetectInvalidConfigs) {
    // EN: Test configuration validation
    // FR: Tester la validation de configuration
    
    KillSwitchConfig valid_config = KillSwitchUtils::createDefaultConfig();
    auto errors = KillSwitchUtils::validateConfig(valid_config);
    EXPECT_TRUE(errors.empty()) << "Valid config should have no errors";
    
    KillSwitchConfig invalid_config;
    invalid_config.trigger_timeout = -100ms;  // Invalid
    invalid_config.state_directory = "";      // Invalid
    invalid_config.max_state_files = 0;       // Invalid
    
    errors = KillSwitchUtils::validateConfig(invalid_config);
    EXPECT_FALSE(errors.empty()) << "Invalid config should have errors";
    EXPECT_GE(errors.size(), 3) << "Should detect multiple validation errors";
}

TEST(KillSwitchUtilsTest, EstimateShutdownTime_ShouldReturnReasonableEstimate) {
    // EN: Test shutdown time estimation
    // FR: Tester l'estimation du temps d'arrêt
    
    KillSwitchConfig config = KillSwitchUtils::createDefaultConfig();
    
    auto estimate1 = KillSwitchUtils::estimateShutdownTime(config, 0, 0);
    auto estimate2 = KillSwitchUtils::estimateShutdownTime(config, 10, 5);
    auto estimate3 = KillSwitchUtils::estimateShutdownTime(config, 100, 50);
    
    EXPECT_GT(estimate1.count(), 0) << "Should provide positive estimate";
    EXPECT_GT(estimate2, estimate1) << "More tasks should increase estimate";
    EXPECT_GT(estimate3, estimate2) << "Much more work should further increase estimate";
    EXPECT_LE(estimate3, config.total_shutdown_timeout) << "Should not exceed total timeout";
}

// EN: Edge case and stress tests
// FR: Tests de cas limites et de stress

TEST_F(KillSwitchTest, ConcurrentTriggers_ShouldBeHandledSafely) {
    // EN: Test concurrent triggers from multiple threads
    // FR: Tester les déclencheurs concurrents depuis plusieurs threads
    
    std::atomic<int> trigger_count{0};
    std::vector<std::thread> threads;
    
    // EN: Start multiple threads that try to trigger Kill Switch
    // FR: Démarrer plusieurs threads qui essaient de déclencher le Kill Switch
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this, &trigger_count, i]() {
            kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "Concurrent trigger " + std::to_string(i));
            trigger_count++;
        });
    }
    
    // EN: Wait for all threads to complete
    // FR: Attendre que tous les threads se terminent
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(trigger_count.load(), 10) << "All threads should have attempted trigger";
    
    // EN: Only one trigger should be effective
    // FR: Seulement un déclencheur devrait être effectif
    auto stats = kill_switch_->getStats();
    EXPECT_EQ(stats.total_triggers, 1) << "Should only count one effective trigger";
    
    bool completed = kill_switch_->waitForCompletion(2000ms);
    EXPECT_TRUE(completed) << "Shutdown should complete successfully";
}

TEST_F(KillSwitchTest, TimeoutScenarios_ShouldBeHandledGracefully) {
    // EN: Test scenarios with very short timeouts
    // FR: Tester les scénarios avec des timeouts très courts
    
    KillSwitchConfig timeout_config = test_config_;
    timeout_config.task_stop_timeout = 1ms;    // Very short timeout
    timeout_config.state_save_timeout = 1ms;   // Very short timeout
    timeout_config.cleanup_timeout = 1ms;      // Very short timeout
    
    kill_switch_->reset();
    kill_switch_->configure(timeout_config);
    kill_switch_->initialize();
    
    // EN: Register slow callbacks that will timeout
    // FR: Enregistrer des callbacks lents qui vont expirer
    kill_switch_->registerTaskTerminationCallback("slow_task",
        [](const std::string&, std::chrono::milliseconds) -> bool {
            std::this_thread::sleep_for(100ms);  // Longer than timeout
            return true;
        });
    
    kill_switch_->registerCleanupCallback("slow_cleanup",
        [](const std::string&) {
            std::this_thread::sleep_for(100ms);  // Longer than timeout
        });
    
    kill_switch_->trigger(KillSwitchTrigger::TIMEOUT, "Timeout test");
    
    bool completed = kill_switch_->waitForCompletion(1000ms);
    EXPECT_TRUE(completed) << "Should complete even with timeouts";
    
    auto stats = kill_switch_->getStats();
    EXPECT_EQ(stats.timeout_shutdowns, 1) << "Should record timeout shutdown";
}

TEST_F(KillSwitchTest, LargeStateData_ShouldBeHandledCorrectly) {
    // EN: Test handling of large state data
    // FR: Tester la gestion de grandes données d'état
    
    // EN: Create large state snapshot
    // FR: Créer un grand instantané d'état
    std::string large_data(10000, 'X');  // 10KB of data
    
    kill_switch_->registerStatePreservationCallback("large_component", 
        [large_data](const std::string& component_id) -> std::optional<StateSnapshot> {
            StateSnapshot snapshot;
            snapshot.component_id = component_id;
            snapshot.operation_id = "large_operation";
            snapshot.timestamp = std::chrono::system_clock::now();
            snapshot.state_type = "large_state";
            snapshot.state_data = large_data;
            snapshot.data_size = large_data.size();
            snapshot.priority = 0;
            return snapshot;
        });
    
    kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "Large state test");
    
    bool completed = kill_switch_->waitForCompletion(3000ms);
    EXPECT_TRUE(completed) << "Should complete with large state data";
    
    auto stats = kill_switch_->getStats();
    EXPECT_GT(stats.total_state_size_bytes, 10000) << "Should record large state size";
}

TEST_F(KillSwitchTest, Reset_ShouldRestoreCleanState) {
    // EN: Test that reset properly cleans up all state
    // FR: Tester que reset nettoie correctement tout l'état
    
    // EN: Trigger and complete a shutdown
    // FR: Déclencher et terminer un arrêt
    kill_switch_->trigger(KillSwitchTrigger::USER_REQUEST, "Reset test");
    kill_switch_->waitForCompletion(2000ms);
    
    auto stats_before_reset = kill_switch_->getStats();
    EXPECT_GT(stats_before_reset.total_triggers, 0) << "Should have recorded triggers before reset";
    
    // EN: Reset and verify clean state
    // FR: Remettre à zéro et vérifier l'état propre
    kill_switch_->reset();
    kill_switch_->configure(test_config_);
    kill_switch_->initialize();
    
    EXPECT_FALSE(kill_switch_->isTriggered()) << "Should not be triggered after reset";
    EXPECT_FALSE(kill_switch_->isShuttingDown()) << "Should not be shutting down after reset";
    EXPECT_EQ(kill_switch_->getCurrentPhase(), KillSwitchPhase::INACTIVE) << "Should be inactive after reset";
    
    auto stats_after_reset = kill_switch_->getStats();
    EXPECT_EQ(stats_after_reset.total_triggers, 0) << "Should have no triggers after reset";
    EXPECT_EQ(stats_after_reset.successful_shutdowns, 0) << "Should have no shutdowns after reset";
}

// EN: Main test function
// FR: Fonction de test principale
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}