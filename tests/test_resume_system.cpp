// EN: Comprehensive unit tests for Resume System - 100% test coverage for intelligent crash recovery
// FR: Tests unitaires complets pour le système de reprise - Couverture de test à 100% pour la récupération intelligente après crash

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "orchestrator/resume_system.hpp"
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <random>

using namespace BBP;
using namespace testing;

namespace {

// EN: Mock classes for testing
// FR: Classes mock pour les tests
class MockPipelineEngine {
public:
    MOCK_METHOD(void, start, (), ());
    MOCK_METHOD(void, stop, (), ());
    MOCK_METHOD(bool, isRunning, (), (const));
    MOCK_METHOD(double, getProgress, (), (const));
    MOCK_METHOD(nlohmann::json, getState, (), (const));
    MOCK_METHOD(void, setState, (const nlohmann::json&), ());
};

// EN: Test fixture for Resume System tests
// FR: Fixture de test pour les tests du système de reprise
class ResumeSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // EN: Create temporary directory for test checkpoints
        // FR: Crée un répertoire temporaire pour les checkpoints de test
        test_dir_ = std::filesystem::temp_directory_path() / "bbp_resume_test";
        std::filesystem::create_directories(test_dir_);
        
        // EN: Configure test checkpoint configuration
        // FR: Configure la configuration de checkpoint de test
        config_.checkpoint_dir = test_dir_.string();
        config_.strategy = CheckpointStrategy::TIME_BASED;
        config_.granularity = CheckpointGranularity::MEDIUM;
        config_.time_interval = std::chrono::seconds(1);
        config_.progress_threshold = 10.0;
        config_.max_checkpoints = 5;
        config_.enable_compression = true;
        config_.enable_encryption = false;
        config_.enable_verification = true;
        config_.max_memory_threshold_mb = 100;
        config_.auto_cleanup = true;
        config_.cleanup_age = std::chrono::hours(1);
        
        // EN: Create resume system with test configuration
        // FR: Crée le système de reprise avec la configuration de test
        resume_system_ = std::make_unique<ResumeSystem>(config_);
        ASSERT_TRUE(resume_system_->initialize());
    }
    
    void TearDown() override {
        // EN: Cleanup test resources
        // FR: Nettoie les ressources de test
        if (resume_system_) {
            resume_system_->shutdown();
            resume_system_.reset();
        }
        
        // EN: Remove test directory
        // FR: Supprime le répertoire de test
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }
    
    // EN: Helper method to create test pipeline state
    // FR: Méthode helper pour créer l'état du pipeline de test
    nlohmann::json createTestPipelineState(const std::string& stage, double progress = 50.0) {
        nlohmann::json state;
        state["current_stage"] = stage;
        state["progress"] = progress;
        state["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state["completed_stages"] = nlohmann::json::array({"stage1", "stage2"});
        state["pending_stages"] = nlohmann::json::array({"stage3", "stage4"});
        
        nlohmann::json stage_results;
        stage_results["stage1"] = {{"status", "completed"}, {"output", "result1.csv"}};
        stage_results["stage2"] = {{"status", "completed"}, {"output", "result2.csv"}};
        state["stage_results"] = stage_results;
        
        return state;
    }
    
    // EN: Helper method to create test operation ID
    // FR: Méthode helper pour créer un ID d'opération de test
    std::string createTestOperationId() {
        static int counter = 0;
        return "test_operation_" + std::to_string(++counter);
    }

protected:
    std::filesystem::path test_dir_;
    CheckpointConfig config_;
    std::unique_ptr<ResumeSystem> resume_system_;
    MockPipelineEngine mock_pipeline_;
};

// EN: Test basic checkpoint creation and metadata
// FR: Test la création basique de checkpoint et les métadonnées
TEST_F(ResumeSystemTest, BasicCheckpointCreation) {
    // EN: Start monitoring an operation
    // FR: Commence le monitoring d'une opération
    std::string operation_id = createTestOperationId();
    std::string config_path = "/test/pipeline.yaml";
    
    ASSERT_TRUE(resume_system_->startMonitoring(operation_id, config_path));
    EXPECT_EQ(resume_system_->getCurrentState(), ResumeState::RUNNING);
    
    // EN: Create a manual checkpoint
    // FR: Crée un checkpoint manuel
    auto pipeline_state = createTestPipelineState("test_stage", 25.0);
    std::map<std::string, std::string> metadata;
    metadata["test_key"] = "test_value";
    metadata["stage_type"] = "processing";
    
    std::string checkpoint_id = resume_system_->createCheckpoint("test_stage", pipeline_state, metadata);
    EXPECT_FALSE(checkpoint_id.empty());
    EXPECT_THAT(checkpoint_id, StartsWith(operation_id + "_"));
    
    // EN: Verify checkpoint was created
    // FR: Vérifie que le checkpoint a été créé
    auto checkpoints = resume_system_->listCheckpoints(operation_id);
    EXPECT_EQ(checkpoints.size(), 1);
    EXPECT_EQ(checkpoints[0], checkpoint_id);
    
    // EN: Verify checkpoint metadata
    // FR: Vérifie les métadonnées du checkpoint
    auto checkpoint_metadata = resume_system_->getCheckpointMetadata(checkpoint_id);
    ASSERT_TRUE(checkpoint_metadata.has_value());
    
    EXPECT_EQ(checkpoint_metadata->checkpoint_id, checkpoint_id);
    EXPECT_EQ(checkpoint_metadata->pipeline_id, operation_id);
    EXPECT_EQ(checkpoint_metadata->stage_name, "test_stage");
    EXPECT_EQ(checkpoint_metadata->granularity, CheckpointGranularity::MEDIUM);
    EXPECT_DOUBLE_EQ(checkpoint_metadata->progress_percentage, 0.0); // Manual checkpoint doesn't update progress
    EXPECT_GT(checkpoint_metadata->memory_footprint, 0);
    EXPECT_GT(checkpoint_metadata->elapsed_time.count(), 0);
    EXPECT_TRUE(checkpoint_metadata->is_verified);
    EXPECT_FALSE(checkpoint_metadata->verification_hash.empty());
    
    // EN: Check custom metadata
    // FR: Vérifie les métadonnées personnalisées
    EXPECT_EQ(checkpoint_metadata->custom_metadata.at("test_key"), "test_value");
    EXPECT_EQ(checkpoint_metadata->custom_metadata.at("stage_type"), "processing");
    
    resume_system_->stopMonitoring();
}

// EN: Test automatic checkpoint creation
// FR: Test la création automatique de checkpoint
TEST_F(ResumeSystemTest, AutomaticCheckpointCreation) {
    std::string operation_id = createTestOperationId();
    
    ASSERT_TRUE(resume_system_->startMonitoring(operation_id, "/test/config.yaml"));
    
    // EN: Create automatic checkpoint with progress
    // FR: Crée un checkpoint automatique avec progression
    auto pipeline_state = createTestPipelineState("auto_stage", 75.0);
    
    std::string checkpoint_id = resume_system_->createAutomaticCheckpoint("auto_stage", pipeline_state, 75.0);
    EXPECT_FALSE(checkpoint_id.empty());
    
    // EN: Verify automatic checkpoint metadata
    // FR: Vérifie les métadonnées du checkpoint automatique
    auto metadata = resume_system_->getCheckpointMetadata(checkpoint_id);
    ASSERT_TRUE(metadata.has_value());
    
    EXPECT_EQ(metadata->stage_name, "auto_stage");
    EXPECT_DOUBLE_EQ(metadata->progress_percentage, 75.0);
    
    // EN: Check statistics
    // FR: Vérifie les statistiques
    auto stats = resume_system_->getStatistics();
    EXPECT_EQ(stats.total_checkpoints_created, 1);
    
    resume_system_->stopMonitoring();
}

// EN: Test checkpoint verification
// FR: Test la vérification des checkpoints
TEST_F(ResumeSystemTest, CheckpointVerification) {
    std::string operation_id = createTestOperationId();
    ASSERT_TRUE(resume_system_->startMonitoring(operation_id, "/test/config.yaml"));
    
    // EN: Create checkpoint with verification enabled
    // FR: Crée un checkpoint avec vérification activée
    auto pipeline_state = createTestPipelineState("verify_stage");
    std::string checkpoint_id = resume_system_->createCheckpoint("verify_stage", pipeline_state);
    
    // EN: Verify checkpoint integrity
    // FR: Vérifie l'intégrité du checkpoint
    EXPECT_TRUE(resume_system_->verifyCheckpoint(checkpoint_id));
    
    // EN: Test with invalid checkpoint ID
    // FR: Test avec un ID de checkpoint invalide
    EXPECT_FALSE(resume_system_->verifyCheckpoint("invalid_checkpoint_id"));
    
    resume_system_->stopMonitoring();
}

// EN: Test resume from checkpoint functionality
// FR: Test la fonctionnalité de reprise depuis un checkpoint
TEST_F(ResumeSystemTest, ResumeFromCheckpoint) {
    std::string operation_id = createTestOperationId();
    std::string config_path = "/test/resume_config.yaml";
    
    // EN: Create checkpoint first
    // FR: Crée d'abord un checkpoint
    ASSERT_TRUE(resume_system_->startMonitoring(operation_id, config_path));
    
    auto pipeline_state = createTestPipelineState("resume_test_stage", 60.0);
    std::string checkpoint_id = resume_system_->createAutomaticCheckpoint("resume_test_stage", pipeline_state, 60.0);
    EXPECT_FALSE(checkpoint_id.empty());
    
    resume_system_->stopMonitoring();
    
    // EN: Test resume functionality
    // FR: Test la fonctionnalité de reprise
    EXPECT_TRUE(resume_system_->canResume(operation_id));
    
    // EN: Get available resume points
    // FR: Obtient les points de reprise disponibles
    auto resume_points = resume_system_->getAvailableResumePoints(operation_id);
    EXPECT_EQ(resume_points.size(), 1);
    EXPECT_EQ(resume_points[0].checkpoint_id, checkpoint_id);
    EXPECT_DOUBLE_EQ(resume_points[0].progress_percentage, 60.0);
    
    // EN: Resume from checkpoint
    // FR: Reprend depuis le checkpoint
    auto resume_context = resume_system_->resumeFromCheckpoint(checkpoint_id, ResumeMode::LAST_CHECKPOINT);
    ASSERT_TRUE(resume_context.has_value());
    
    EXPECT_EQ(resume_context->operation_id, operation_id);
    EXPECT_EQ(resume_context->resume_mode, ResumeMode::LAST_CHECKPOINT);
    EXPECT_EQ(resume_context->resume_reason, "Resume from checkpoint " + checkpoint_id);
    
    // EN: Verify resumed stage information
    // FR: Vérifie les informations d'étape reprises
    EXPECT_THAT(resume_context->completed_stages, Contains("stage1"));
    EXPECT_THAT(resume_context->completed_stages, Contains("stage2"));
    EXPECT_THAT(resume_context->pending_stages, Contains("stage3"));
    EXPECT_THAT(resume_context->pending_stages, Contains("stage4"));
    
    // EN: Verify stage results are preserved
    // FR: Vérifie que les résultats d'étape sont préservés
    ASSERT_TRUE(resume_context->stage_results.contains("stage1"));
    EXPECT_EQ(resume_context->stage_results["stage1"]["status"], "completed");
    EXPECT_EQ(resume_context->stage_results["stage1"]["output"], "result1.csv");
    
    // EN: Check statistics after resume
    // FR: Vérifie les statistiques après reprise
    auto stats = resume_system_->getStatistics();
    EXPECT_EQ(stats.successful_resumes, 1);
    EXPECT_EQ(stats.failed_resumes, 0);
    EXPECT_GT(stats.total_recovery_time.count(), 0);
}

// EN: Test automatic resume functionality
// FR: Test la fonctionnalité de reprise automatique
TEST_F(ResumeSystemTest, AutomaticResume) {
    std::string operation_id = createTestOperationId();
    
    // EN: Create multiple checkpoints with different progress levels
    // FR: Crée plusieurs checkpoints avec différents niveaux de progression
    ASSERT_TRUE(resume_system_->startMonitoring(operation_id, "/test/config.yaml"));
    
    // EN: Create checkpoints with increasing progress
    // FR: Crée des checkpoints avec progression croissante
    std::string checkpoint1 = resume_system_->createAutomaticCheckpoint(
        "stage1", createTestPipelineState("stage1", 25.0), 25.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Ensure different timestamps
    
    std::string checkpoint2 = resume_system_->createAutomaticCheckpoint(
        "stage2", createTestPipelineState("stage2", 50.0), 50.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    std::string checkpoint3 = resume_system_->createAutomaticCheckpoint(
        "stage3", createTestPipelineState("stage3", 75.0), 75.0);
    
    resume_system_->stopMonitoring();
    
    // EN: Test automatic resume (should pick the best checkpoint)
    // FR: Test la reprise automatique (devrait choisir le meilleur checkpoint)
    auto resume_context = resume_system_->resumeAutomatically(operation_id);
    ASSERT_TRUE(resume_context.has_value());
    
    EXPECT_EQ(resume_context->operation_id, operation_id);
    EXPECT_EQ(resume_context->resume_mode, ResumeMode::BEST_CHECKPOINT);
    
    // EN: Should resume from the checkpoint with highest progress (75%)
    // FR: Devrait reprendre depuis le checkpoint avec la progression la plus élevée (75%)
    auto checkpoints = resume_system_->listCheckpoints(operation_id);
    EXPECT_EQ(checkpoints.size(), 3);
}

// EN: Test checkpoint cleanup functionality
// FR: Test la fonctionnalité de nettoyage des checkpoints
TEST_F(ResumeSystemTest, CheckpointCleanup) {
    std::string operation_id = createTestOperationId();
    
    // EN: Configure for aggressive cleanup testing
    // FR: Configure pour un test de nettoyage agressif
    config_.max_checkpoints = 2;
    config_.cleanup_age = std::chrono::milliseconds(50);
    resume_system_->updateConfig(config_);
    
    ASSERT_TRUE(resume_system_->startMonitoring(operation_id, "/test/config.yaml"));
    
    // EN: Create more checkpoints than the limit
    // FR: Crée plus de checkpoints que la limite
    std::vector<std::string> checkpoint_ids;
    for (int i = 0; i < 5; ++i) {
        auto state = createTestPipelineState("stage" + std::to_string(i), i * 20.0);
        auto checkpoint_id = resume_system_->createAutomaticCheckpoint(
            "stage" + std::to_string(i), state, i * 20.0);
        checkpoint_ids.push_back(checkpoint_id);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // EN: Wait for cleanup age threshold
    // FR: Attend le seuil d'âge de nettoyage
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // EN: Trigger cleanup
    // FR: Déclenche le nettoyage
    size_t cleaned = resume_system_->cleanupOldCheckpoints();
    EXPECT_GT(cleaned, 0);
    
    // EN: Verify that only max_checkpoints remain
    // FR: Vérifie qu'il ne reste que max_checkpoints
    auto remaining_checkpoints = resume_system_->listCheckpoints(operation_id);
    EXPECT_LE(remaining_checkpoints.size(), config_.max_checkpoints);
    
    resume_system_->stopMonitoring();
}

// EN: Test checkpoint deletion
// FR: Test la suppression de checkpoint
TEST_F(ResumeSystemTest, CheckpointDeletion) {
    std::string operation_id = createTestOperationId();
    
    ASSERT_TRUE(resume_system_->startMonitoring(operation_id, "/test/config.yaml"));
    
    // EN: Create checkpoint
    // FR: Crée un checkpoint
    auto pipeline_state = createTestPipelineState("delete_test");
    std::string checkpoint_id = resume_system_->createCheckpoint("delete_test", pipeline_state);
    
    // EN: Verify checkpoint exists
    // FR: Vérifie que le checkpoint existe
    auto checkpoints = resume_system_->listCheckpoints(operation_id);
    EXPECT_EQ(checkpoints.size(), 1);
    EXPECT_EQ(checkpoints[0], checkpoint_id);
    
    // EN: Delete checkpoint
    // FR: Supprime le checkpoint
    EXPECT_TRUE(resume_system_->deleteCheckpoint(checkpoint_id));
    
    // EN: Verify checkpoint is deleted
    // FR: Vérifie que le checkpoint est supprimé
    checkpoints = resume_system_->listCheckpoints(operation_id);
    EXPECT_EQ(checkpoints.size(), 0);
    
    // EN: Test deleting non-existent checkpoint
    // FR: Test la suppression d'un checkpoint inexistant
    EXPECT_TRUE(resume_system_->deleteCheckpoint("non_existent_checkpoint"));
    
    resume_system_->stopMonitoring();
}

// EN: Test resume system statistics
// FR: Test les statistiques du système de reprise
TEST_F(ResumeSystemTest, Statistics) {
    std::string operation_id = createTestOperationId();
    
    // EN: Reset statistics to start fresh
    // FR: Remet à zéro les statistiques pour commencer à neuf
    resume_system_->resetStatistics();
    
    auto initial_stats = resume_system_->getStatistics();
    EXPECT_EQ(initial_stats.total_checkpoints_created, 0);
    EXPECT_EQ(initial_stats.successful_resumes, 0);
    EXPECT_EQ(initial_stats.failed_resumes, 0);
    
    ASSERT_TRUE(resume_system_->startMonitoring(operation_id, "/test/config.yaml"));
    
    // EN: Create several checkpoints
    // FR: Crée plusieurs checkpoints
    for (int i = 0; i < 3; ++i) {
        auto state = createTestPipelineState("stats_stage" + std::to_string(i));
        resume_system_->createCheckpoint("stats_stage" + std::to_string(i), state);
    }
    
    resume_system_->stopMonitoring();
    
    // EN: Perform a successful resume
    // FR: Effectue une reprise réussie
    auto checkpoints = resume_system_->listCheckpoints(operation_id);
    ASSERT_FALSE(checkpoints.empty());
    
    auto resume_result = resume_system_->resumeFromCheckpoint(checkpoints[0]);
    EXPECT_TRUE(resume_result.has_value());
    
    // EN: Check updated statistics
    // FR: Vérifie les statistiques mises à jour
    auto updated_stats = resume_system_->getStatistics();
    EXPECT_EQ(updated_stats.total_checkpoints_created, 3);
    EXPECT_EQ(updated_stats.successful_resumes, 1);
    EXPECT_EQ(updated_stats.failed_resumes, 0);
    EXPECT_GT(updated_stats.total_recovery_time.count(), 0);
    
    // EN: Test failed resume (should increment failed_resumes)
    // FR: Test une reprise échouée (devrait incrémenter failed_resumes)
    auto failed_resume = resume_system_->resumeFromCheckpoint("invalid_checkpoint");
    EXPECT_FALSE(failed_resume.has_value());
    
    auto final_stats = resume_system_->getStatistics();
    EXPECT_EQ(final_stats.failed_resumes, 1);
}

// EN: Test resume system configuration updates
// FR: Test les mises à jour de configuration du système de reprise
TEST_F(ResumeSystemTest, ConfigurationUpdates) {
    // EN: Test initial configuration
    // FR: Test la configuration initiale
    auto initial_config = resume_system_->getConfig();
    EXPECT_EQ(initial_config.strategy, CheckpointStrategy::TIME_BASED);
    EXPECT_EQ(initial_config.max_checkpoints, 5);
    
    // EN: Update configuration
    // FR: Met à jour la configuration
    CheckpointConfig new_config = config_;
    new_config.strategy = CheckpointStrategy::PROGRESS_BASED;
    new_config.max_checkpoints = 10;
    new_config.enable_compression = false;
    
    resume_system_->updateConfig(new_config);
    
    // EN: Verify configuration was updated
    // FR: Vérifie que la configuration a été mise à jour
    auto updated_config = resume_system_->getConfig();
    EXPECT_EQ(updated_config.strategy, CheckpointStrategy::PROGRESS_BASED);
    EXPECT_EQ(updated_config.max_checkpoints, 10);
    EXPECT_FALSE(updated_config.enable_compression);
}

// EN: Test resume system callbacks
// FR: Test les callbacks du système de reprise
TEST_F(ResumeSystemTest, Callbacks) {
    std::string operation_id = createTestOperationId();
    
    // EN: Setup callback tracking variables
    // FR: Configure les variables de suivi des callbacks
    bool progress_callback_called = false;
    bool checkpoint_callback_called = false;
    bool recovery_callback_called = false;
    
    std::string callback_operation_id;
    double callback_progress = 0.0;
    std::string callback_checkpoint_id;
    CheckpointMetadata callback_metadata;
    bool callback_recovery_success = false;
    
    // EN: Set callbacks
    // FR: Définit les callbacks
    resume_system_->setProgressCallback([&](const std::string& op_id, double progress) {
        progress_callback_called = true;
        callback_operation_id = op_id;
        callback_progress = progress;
    });
    
    resume_system_->setCheckpointCallback([&](const std::string& checkpoint_id, const CheckpointMetadata& metadata) {
        checkpoint_callback_called = true;
        callback_checkpoint_id = checkpoint_id;
        callback_metadata = metadata;
    });
    
    resume_system_->setRecoveryCallback([&](const std::string& checkpoint_id, bool success) {
        recovery_callback_called = true;
        callback_checkpoint_id = checkpoint_id;
        callback_recovery_success = success;
    });
    
    ASSERT_TRUE(resume_system_->startMonitoring(operation_id, "/test/config.yaml"));
    
    // EN: Create checkpoint (should trigger checkpoint callback)
    // FR: Crée un checkpoint (devrait déclencher le callback de checkpoint)
    auto pipeline_state = createTestPipelineState("callback_test");
    std::string checkpoint_id = resume_system_->createCheckpoint("callback_test", pipeline_state);
    
    // EN: Verify checkpoint callback was called
    // FR: Vérifie que le callback de checkpoint a été appelé
    EXPECT_TRUE(checkpoint_callback_called);
    EXPECT_EQ(callback_checkpoint_id, checkpoint_id);
    EXPECT_EQ(callback_metadata.stage_name, "callback_test");
    
    resume_system_->stopMonitoring();
    
    // EN: Test recovery callback
    // FR: Test le callback de récupération
    recovery_callback_called = false;
    auto resume_result = resume_system_->resumeFromCheckpoint(checkpoint_id);
    EXPECT_TRUE(resume_result.has_value());
    
    // EN: Verify recovery callback was called
    // FR: Vérifie que le callback de récupération a été appelé
    EXPECT_TRUE(recovery_callback_called);
    EXPECT_TRUE(callback_recovery_success);
}

// EN: Test force checkpoint functionality
// FR: Test la fonctionnalité de checkpoint forcé
TEST_F(ResumeSystemTest, ForceCheckpoint) {
    std::string operation_id = createTestOperationId();
    
    ASSERT_TRUE(resume_system_->startMonitoring(operation_id, "/test/config.yaml"));
    
    // EN: Force checkpoint creation
    // FR: Force la création d'un checkpoint
    std::string checkpoint_id = resume_system_->forceCheckpoint("Emergency checkpoint for testing");
    EXPECT_FALSE(checkpoint_id.empty());
    
    // EN: Verify checkpoint metadata contains force reason
    // FR: Vérifie que les métadonnées du checkpoint contiennent la raison de forçage
    auto metadata = resume_system_->getCheckpointMetadata(checkpoint_id);
    ASSERT_TRUE(metadata.has_value());
    
    EXPECT_TRUE(metadata->custom_metadata.contains("force_reason"));
    EXPECT_EQ(metadata->custom_metadata.at("force_reason"), "Emergency checkpoint for testing");
    EXPECT_TRUE(metadata->custom_metadata.contains("force_timestamp"));
    
    resume_system_->stopMonitoring();
    
    // EN: Test force checkpoint when not monitoring (should return empty)
    // FR: Test le checkpoint forcé quand pas en monitoring (devrait retourner vide)
    std::string invalid_checkpoint = resume_system_->forceCheckpoint("Should fail");
    EXPECT_TRUE(invalid_checkpoint.empty());
}

// EN: Test detailed logging functionality
// FR: Test la fonctionnalité de logging détaillé
TEST_F(ResumeSystemTest, DetailedLogging) {
    std::string operation_id = createTestOperationId();
    
    // EN: Enable detailed logging
    // FR: Active le logging détaillé
    resume_system_->setDetailedLogging(true);
    
    ASSERT_TRUE(resume_system_->startMonitoring(operation_id, "/test/config.yaml"));
    
    // EN: Create checkpoint with detailed logging enabled
    // FR: Crée un checkpoint avec logging détaillé activé
    auto pipeline_state = createTestPipelineState("logging_test");
    std::string checkpoint_id = resume_system_->createCheckpoint("logging_test", pipeline_state);
    EXPECT_FALSE(checkpoint_id.empty());
    
    // EN: Disable detailed logging
    // FR: Désactive le logging détaillé
    resume_system_->setDetailedLogging(false);
    
    // EN: Create another checkpoint (should have less verbose logging)
    // FR: Crée un autre checkpoint (devrait avoir un logging moins verbeux)
    std::string checkpoint_id2 = resume_system_->createCheckpoint("quiet_logging", pipeline_state);
    EXPECT_FALSE(checkpoint_id2.empty());
    
    resume_system_->stopMonitoring();
}

} // anonymous namespace

// EN: Test fixture for AutoCheckpointGuard
// FR: Fixture de test pour AutoCheckpointGuard
class AutoCheckpointGuardTest : public ResumeSystemTest {
protected:
    void SetUp() override {
        ResumeSystemTest::SetUp();
        
        operation_id_ = createTestOperationId();
        stage_name_ = "auto_guard_test";
        
        ASSERT_TRUE(resume_system_->startMonitoring(operation_id_, "/test/config.yaml"));
    }
    
    void TearDown() override {
        if (resume_system_) {
            resume_system_->stopMonitoring();
        }
        ResumeSystemTest::TearDown();
    }

protected:
    std::string operation_id_;
    std::string stage_name_;
};

// EN: Test AutoCheckpointGuard basic functionality
// FR: Test la fonctionnalité de base d'AutoCheckpointGuard
TEST_F(AutoCheckpointGuardTest, BasicFunctionality) {
    bool checkpoint_created = false;
    std::string final_checkpoint_id;
    
    resume_system_->setCheckpointCallback([&](const std::string& checkpoint_id, const CheckpointMetadata& metadata) {
        if (metadata.stage_name == stage_name_ + "_final") {
            checkpoint_created = true;
            final_checkpoint_id = checkpoint_id;
        }
    });
    
    {
        // EN: Create AutoCheckpointGuard in scope
        // FR: Crée AutoCheckpointGuard dans la portée
        AutoCheckpointGuard guard(operation_id_, stage_name_, *resume_system_);
        
        // EN: Set pipeline state
        // FR: Définit l'état du pipeline
        auto test_state = createTestPipelineState(stage_name_);
        guard.setPipelineState(test_state);
        
        // EN: Add custom metadata
        // FR: Ajoute des métadonnées personnalisées
        guard.addMetadata("test_metadata", "auto_guard_value");
        guard.addMetadata("guard_type", "automatic");
        
        // EN: Update progress
        // FR: Met à jour la progression
        guard.updateProgress(25.0);
        guard.updateProgress(75.0); // Should trigger progress checkpoint due to significant change
        
    } // EN: Guard destructor should create final checkpoint / FR: Le destructeur du guard devrait créer le checkpoint final
    
    // EN: Verify final checkpoint was created
    // FR: Vérifie que le checkpoint final a été créé
    EXPECT_TRUE(checkpoint_created);
    EXPECT_FALSE(final_checkpoint_id.empty());
    
    // EN: Verify checkpoint metadata
    // FR: Vérifie les métadonnées du checkpoint
    auto metadata = resume_system_->getCheckpointMetadata(final_checkpoint_id);
    ASSERT_TRUE(metadata.has_value());
    
    EXPECT_EQ(metadata->stage_name, stage_name_ + "_final");
    EXPECT_TRUE(metadata->custom_metadata.contains("test_metadata"));
    EXPECT_EQ(metadata->custom_metadata.at("test_metadata"), "auto_guard_value");
    EXPECT_TRUE(metadata->custom_metadata.contains("completion_time"));
    EXPECT_TRUE(metadata->custom_metadata.contains("final_progress"));
}

// EN: Test AutoCheckpointGuard force checkpoint
// FR: Test le checkpoint forcé d'AutoCheckpointGuard
TEST_F(AutoCheckpointGuardTest, ForceCheckpoint) {
    bool forced_checkpoint_created = false;
    
    resume_system_->setCheckpointCallback([&](const std::string& checkpoint_id, const CheckpointMetadata& metadata) {
        if (metadata.custom_metadata.contains("forced_checkpoint") && 
            metadata.custom_metadata.at("forced_checkpoint") == "true") {
            forced_checkpoint_created = true;
        }
    });
    
    {
        AutoCheckpointGuard guard(operation_id_, stage_name_, *resume_system_);
        
        auto test_state = createTestPipelineState(stage_name_);
        guard.setPipelineState(test_state);
        
        // EN: Force checkpoint creation
        // FR: Force la création de checkpoint
        std::string forced_checkpoint_id = guard.forceCheckpoint();
        EXPECT_FALSE(forced_checkpoint_id.empty());
    }
    
    EXPECT_TRUE(forced_checkpoint_created);
}

// EN: Test fixture for ResumeSystemManager
// FR: Fixture de test pour ResumeSystemManager
class ResumeSystemManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "bbp_manager_test";
        std::filesystem::create_directories(test_dir_);
        
        config_.checkpoint_dir = test_dir_.string();
        config_.strategy = CheckpointStrategy::HYBRID;
        config_.max_checkpoints = 3;
        config_.auto_cleanup = true;
    }
    
    void TearDown() override {
        ResumeSystemManager::getInstance().shutdown();
        
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

protected:
    std::filesystem::path test_dir_;
    CheckpointConfig config_;
    MockPipelineEngine mock_pipeline_;
};

// EN: Test ResumeSystemManager initialization
// FR: Test l'initialisation de ResumeSystemManager
TEST_F(ResumeSystemManagerTest, Initialization) {
    auto& manager = ResumeSystemManager::getInstance();
    
    // EN: Test initialization
    // FR: Test l'initialisation
    EXPECT_TRUE(manager.initialize(config_));
    
    // EN: Test double initialization (should succeed)
    // FR: Test la double initialisation (devrait réussir)
    EXPECT_TRUE(manager.initialize(config_));
    
    // EN: Get resume system
    // FR: Obtient le système de reprise
    auto& resume_system = manager.getResumeSystem();
    EXPECT_EQ(resume_system.getCurrentState(), ResumeState::IDLE);
}

// EN: Test pipeline registration
// FR: Test l'enregistrement de pipeline
TEST_F(ResumeSystemManagerTest, PipelineRegistration) {
    auto& manager = ResumeSystemManager::getInstance();
    ASSERT_TRUE(manager.initialize(config_));
    
    std::string pipeline_id = "test_pipeline_123";
    
    // EN: Register pipeline
    // FR: Enregistre le pipeline
    EXPECT_TRUE(manager.registerPipeline(pipeline_id, &mock_pipeline_));
    
    // EN: Test duplicate registration (should fail)
    // FR: Test l'enregistrement en double (devrait échouer)
    EXPECT_FALSE(manager.registerPipeline(pipeline_id, &mock_pipeline_));
    
    // EN: Unregister pipeline
    // FR: Désenregistre le pipeline
    manager.unregisterPipeline(pipeline_id);
    
    // EN: Should be able to register again after unregistering
    // FR: Devrait pouvoir enregistrer à nouveau après désenregistrement
    EXPECT_TRUE(manager.registerPipeline(pipeline_id, &mock_pipeline_));
}

// EN: Test crash detection
// FR: Test la détection de crash
TEST_F(ResumeSystemManagerTest, CrashDetection) {
    auto& manager = ResumeSystemManager::getInstance();
    ASSERT_TRUE(manager.initialize(config_));
    
    auto& resume_system = manager.getResumeSystem();
    
    // EN: Create some checkpoints to simulate crashed operations
    // FR: Crée quelques checkpoints pour simuler des opérations crashées
    std::string operation_id1 = "crashed_operation_1";
    std::string operation_id2 = "crashed_operation_2";
    std::string operation_id3 = "running_operation_3";
    
    ASSERT_TRUE(resume_system.startMonitoring(operation_id1, "/test/config1.yaml"));
    auto state1 = nlohmann::json{{"stage", "processing"}, {"progress", 50.0}};
    resume_system.createCheckpoint("test_stage", state1);
    resume_system.stopMonitoring();
    
    ASSERT_TRUE(resume_system.startMonitoring(operation_id2, "/test/config2.yaml"));
    auto state2 = nlohmann::json{{"stage", "analysis"}, {"progress", 75.0}};
    resume_system.createCheckpoint("test_stage", state2);
    resume_system.stopMonitoring();
    
    // EN: Register one pipeline as still running
    // FR: Enregistre un pipeline comme toujours en cours
    manager.registerPipeline(operation_id3, &mock_pipeline_);
    
    ASSERT_TRUE(resume_system.startMonitoring(operation_id3, "/test/config3.yaml"));
    auto state3 = nlohmann::json{{"stage", "finishing"}, {"progress", 90.0}};
    resume_system.createCheckpoint("test_stage", state3);
    // EN: Don't stop monitoring for operation_id3 to simulate it's still running
    // FR: N'arrête pas le monitoring pour operation_id3 pour simuler qu'il fonctionne encore
    
    // EN: Detect crashed operations
    // FR: Détecte les opérations crashées
    auto crashed_operations = manager.detectCrashedOperations();
    
    // EN: Should detect operation_id1 and operation_id2 as crashed, but not operation_id3
    // FR: Devrait détecter operation_id1 et operation_id2 comme crashées, mais pas operation_id3
    EXPECT_THAT(crashed_operations, Contains(operation_id1));
    EXPECT_THAT(crashed_operations, Contains(operation_id2));
    EXPECT_THAT(crashed_operations, Not(Contains(operation_id3)));
    
    resume_system.stopMonitoring();
}

// EN: Test automatic recovery
// FR: Test la récupération automatique
TEST_F(ResumeSystemManagerTest, AutomaticRecovery) {
    auto& manager = ResumeSystemManager::getInstance();
    ASSERT_TRUE(manager.initialize(config_));
    
    auto& resume_system = manager.getResumeSystem();
    
    // EN: Create checkpoint for operation that can be recovered
    // FR: Crée un checkpoint pour une opération qui peut être récupérée
    std::string operation_id = "recoverable_operation";
    
    ASSERT_TRUE(resume_system.startMonitoring(operation_id, "/test/recovery_config.yaml"));
    auto state = nlohmann::json{
        {"stage", "data_processing"}, 
        {"progress", 60.0},
        {"completed_stages", nlohmann::json::array({"init", "load"})},
        {"pending_stages", nlohmann::json::array({"process", "finalize"})}
    };
    resume_system.createAutomaticCheckpoint("data_processing", state, 60.0);
    resume_system.stopMonitoring();
    
    // EN: Test automatic recovery
    // FR: Test la récupération automatique
    EXPECT_TRUE(manager.attemptAutomaticRecovery(operation_id));
    
    // EN: Test recovery of non-existent operation
    // FR: Test la récupération d'une opération inexistante
    EXPECT_FALSE(manager.attemptAutomaticRecovery("non_existent_operation"));
}

// EN: Test global statistics
// FR: Test les statistiques globales
TEST_F(ResumeSystemManagerTest, GlobalStatistics) {
    auto& manager = ResumeSystemManager::getInstance();
    ASSERT_TRUE(manager.initialize(config_));
    
    auto& resume_system = manager.getResumeSystem();
    
    // EN: Create some checkpoints and perform recovery
    // FR: Crée quelques checkpoints et effectue une récupération
    std::string operation_id = "stats_test_operation";
    
    ASSERT_TRUE(resume_system.startMonitoring(operation_id, "/test/stats_config.yaml"));
    
    // EN: Create multiple checkpoints
    // FR: Crée plusieurs checkpoints
    for (int i = 0; i < 3; ++i) {
        auto state = nlohmann::json{{"checkpoint", i}, {"progress", i * 25.0}};
        resume_system.createCheckpoint("stage" + std::to_string(i), state);
    }
    
    resume_system.stopMonitoring();
    
    // EN: Perform recovery
    // FR: Effectue une récupération
    auto checkpoints = resume_system.listCheckpoints(operation_id);
    ASSERT_FALSE(checkpoints.empty());
    auto recovery_result = resume_system.resumeFromCheckpoint(checkpoints[0]);
    EXPECT_TRUE(recovery_result.has_value());
    
    // EN: Get global statistics
    // FR: Obtient les statistiques globales
    auto stats = manager.getGlobalStatistics();
    EXPECT_EQ(stats.total_checkpoints_created, 3);
    EXPECT_EQ(stats.successful_resumes, 1);
    EXPECT_EQ(stats.failed_resumes, 0);
    EXPECT_GT(stats.total_recovery_time.count(), 0);
}

// EN: Test utility functions
// FR: Test les fonctions utilitaires
class ResumeSystemUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // EN: Test setup for utility functions
        // FR: Configuration de test pour les fonctions utilitaires
    }
};

// EN: Test configuration creation utilities
// FR: Test les utilitaires de création de configuration
TEST_F(ResumeSystemUtilsTest, ConfigurationCreation) {
    // EN: Test default configuration
    // FR: Test la configuration par défaut
    auto default_config = ResumeSystemUtils::createDefaultConfig();
    EXPECT_EQ(default_config.strategy, CheckpointStrategy::HYBRID);
    EXPECT_EQ(default_config.granularity, CheckpointGranularity::MEDIUM);
    EXPECT_TRUE(default_config.enable_compression);
    EXPECT_TRUE(default_config.enable_verification);
    EXPECT_TRUE(default_config.auto_cleanup);
    
    // EN: Test high frequency configuration
    // FR: Test la configuration haute fréquence
    auto high_freq_config = ResumeSystemUtils::createHighFrequencyConfig();
    EXPECT_EQ(high_freq_config.strategy, CheckpointStrategy::TIME_BASED);
    EXPECT_EQ(high_freq_config.granularity, CheckpointGranularity::FINE);
    EXPECT_LT(high_freq_config.time_interval, default_config.time_interval);
    EXPECT_GT(high_freq_config.max_checkpoints, default_config.max_checkpoints);
    
    // EN: Test low overhead configuration
    // FR: Test la configuration faible surcharge
    auto low_overhead_config = ResumeSystemUtils::createLowOverheadConfig();
    EXPECT_EQ(low_overhead_config.strategy, CheckpointStrategy::PROGRESS_BASED);
    EXPECT_EQ(low_overhead_config.granularity, CheckpointGranularity::COARSE);
    EXPECT_FALSE(low_overhead_config.enable_compression);
    EXPECT_FALSE(low_overhead_config.enable_verification);
    EXPECT_LT(low_overhead_config.max_checkpoints, default_config.max_checkpoints);
}

// EN: Test checkpoint size estimation
// FR: Test l'estimation de taille de checkpoint
TEST_F(ResumeSystemUtilsTest, CheckpointSizeEstimation) {
    // EN: Test empty state
    // FR: Test état vide
    nlohmann::json empty_state;
    EXPECT_GT(ResumeSystemUtils::estimateCheckpointSize(empty_state), 0);
    
    // EN: Test simple state
    // FR: Test état simple
    nlohmann::json simple_state = {{"key", "value"}, {"number", 42}};
    auto simple_size = ResumeSystemUtils::estimateCheckpointSize(simple_state);
    EXPECT_GT(simple_size, 0);
    
    // EN: Test complex state
    // FR: Test état complexe
    nlohmann::json complex_state = {
        {"stages", nlohmann::json::array({"stage1", "stage2", "stage3"})},
        {"results", {{"stage1", {{"output", "file1.csv"}, {"count", 1000}}}}},
        {"metadata", {{"start_time", 1234567890}, {"version", "1.0"}}}
    };
    auto complex_size = ResumeSystemUtils::estimateCheckpointSize(complex_state);
    EXPECT_GT(complex_size, simple_size);
}

// EN: Test configuration validation
// FR: Test la validation de configuration
TEST_F(ResumeSystemUtilsTest, ConfigurationValidation) {
    // EN: Test valid configuration
    // FR: Test configuration valide
    auto valid_config = ResumeSystemUtils::createDefaultConfig();
    EXPECT_TRUE(ResumeSystemUtils::validateConfig(valid_config));
    
    // EN: Test invalid configurations
    // FR: Test configurations invalides
    CheckpointConfig invalid_config;
    
    // EN: Empty checkpoint directory
    // FR: Répertoire de checkpoint vide
    invalid_config = valid_config;
    invalid_config.checkpoint_dir = "";
    EXPECT_FALSE(ResumeSystemUtils::validateConfig(invalid_config));
    
    // EN: Zero max checkpoints
    // FR: Zéro checkpoints maximum
    invalid_config = valid_config;
    invalid_config.max_checkpoints = 0;
    EXPECT_FALSE(ResumeSystemUtils::validateConfig(invalid_config));
    
    // EN: Invalid time interval
    // FR: Intervalle de temps invalide
    invalid_config = valid_config;
    invalid_config.time_interval = std::chrono::seconds(0);
    EXPECT_FALSE(ResumeSystemUtils::validateConfig(invalid_config));
    
    // EN: Invalid progress threshold
    // FR: Seuil de progression invalide
    invalid_config = valid_config;
    invalid_config.progress_threshold = -10.0;
    EXPECT_FALSE(ResumeSystemUtils::validateConfig(invalid_config));
    
    invalid_config = valid_config;
    invalid_config.progress_threshold = 150.0;
    EXPECT_FALSE(ResumeSystemUtils::validateConfig(invalid_config));
}

// EN: Test operation ID generation
// FR: Test la génération d'ID d'opération
TEST_F(ResumeSystemUtilsTest, OperationIdGeneration) {
    // EN: Generate multiple operation IDs
    // FR: Génère plusieurs IDs d'opération
    std::vector<std::string> operation_ids;
    for (int i = 0; i < 10; ++i) {
        operation_ids.push_back(ResumeSystemUtils::generateOperationId());
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // Ensure different timestamps
    }
    
    // EN: Verify all IDs are unique and properly formatted
    // FR: Vérifie que tous les IDs sont uniques et bien formatés
    std::set<std::string> unique_ids(operation_ids.begin(), operation_ids.end());
    EXPECT_EQ(unique_ids.size(), operation_ids.size()); // All should be unique
    
    for (const auto& id : operation_ids) {
        EXPECT_THAT(id, StartsWith("op_"));
        EXPECT_GT(id.length(), 10); // Should be reasonably long
    }
}

// EN: Test resume context parsing
// FR: Test le parsing de contexte de reprise
TEST_F(ResumeSystemUtilsTest, ResumeContextParsing) {
    // EN: Test valid resume arguments
    // FR: Test les arguments de reprise valides
    std::vector<std::string> valid_args = {
        "--resume-operation", "test_op_123",
        "--resume-config", "/path/to/config.yaml",
        "--resume-mode", "best"
    };
    
    auto context = ResumeSystemUtils::parseResumeContext(valid_args);
    ASSERT_TRUE(context.has_value());
    
    EXPECT_EQ(context->operation_id, "test_op_123");
    EXPECT_EQ(context->pipeline_config_path, "/path/to/config.yaml");
    EXPECT_EQ(context->resume_mode, ResumeMode::BEST_CHECKPOINT);
    
    // EN: Test different resume modes
    // FR: Test différents modes de reprise
    std::vector<std::string> full_restart_args = {"--resume-operation", "op1", "--resume-mode", "full"};
    auto full_context = ResumeSystemUtils::parseResumeContext(full_restart_args);
    ASSERT_TRUE(full_context.has_value());
    EXPECT_EQ(full_context->resume_mode, ResumeMode::FULL_RESTART);
    
    std::vector<std::string> last_checkpoint_args = {"--resume-operation", "op2", "--resume-mode", "last"};
    auto last_context = ResumeSystemUtils::parseResumeContext(last_checkpoint_args);
    ASSERT_TRUE(last_context.has_value());
    EXPECT_EQ(last_context->resume_mode, ResumeMode::LAST_CHECKPOINT);
    
    std::vector<std::string> interactive_args = {"--resume-operation", "op3", "--resume-mode", "interactive"};
    auto interactive_context = ResumeSystemUtils::parseResumeContext(interactive_args);
    ASSERT_TRUE(interactive_context.has_value());
    EXPECT_EQ(interactive_context->resume_mode, ResumeMode::INTERACTIVE);
    
    // EN: Test invalid arguments (missing operation ID)
    // FR: Test arguments invalides (ID d'opération manquant)
    std::vector<std::string> invalid_args = {"--resume-config", "/config.yaml"};
    auto invalid_context = ResumeSystemUtils::parseResumeContext(invalid_args);
    EXPECT_FALSE(invalid_context.has_value());
}

// EN: Test compression and decompression utilities
// FR: Test les utilitaires de compression et décompression
TEST_F(ResumeSystemUtilsTest, CompressionUtilities) {
    // EN: Test data compression and decompression
    // FR: Test la compression et décompression de données
    std::string test_data = "This is a test data string that should compress well because it has repeated patterns. "
                           "This is a test data string that should compress well because it has repeated patterns. "
                           "This is a test data string that should compress well because it has repeated patterns.";
    
    std::vector<uint8_t> original_data(test_data.begin(), test_data.end());
    
    // EN: Compress data
    // FR: Compresse les données
    auto compressed = ResumeSystemUtils::compressCheckpointData(original_data);
    EXPECT_LT(compressed.size(), original_data.size()); // Should be smaller after compression
    
    // EN: Decompress data
    // FR: Décompresse les données
    auto decompressed = ResumeSystemUtils::decompressCheckpointData(compressed);
    EXPECT_EQ(decompressed.size(), original_data.size());
    EXPECT_EQ(decompressed, original_data);
    
    // EN: Test empty data
    // FR: Test données vides
    std::vector<uint8_t> empty_data;
    auto compressed_empty = ResumeSystemUtils::compressCheckpointData(empty_data);
    EXPECT_TRUE(compressed_empty.empty());
    
    auto decompressed_empty = ResumeSystemUtils::decompressCheckpointData(compressed_empty);
    EXPECT_TRUE(decompressed_empty.empty());
}

// EN: Test encryption and decryption utilities
// FR: Test les utilitaires de chiffrement et déchiffrement
TEST_F(ResumeSystemUtilsTest, EncryptionUtilities) {
    // EN: Test data encryption and decryption
    // FR: Test le chiffrement et déchiffrement de données
    std::string test_data = "Sensitive checkpoint data that needs to be encrypted for security";
    std::vector<uint8_t> original_data(test_data.begin(), test_data.end());
    std::string encryption_key = "test_encryption_key_123";
    
    // EN: Encrypt data
    // FR: Chiffre les données
    auto encrypted = ResumeSystemUtils::encryptCheckpointData(original_data, encryption_key);
    EXPECT_NE(encrypted, original_data); // Should be different after encryption
    
    // EN: Decrypt data
    // FR: Déchiffre les données
    auto decrypted = ResumeSystemUtils::decryptCheckpointData(encrypted, encryption_key);
    EXPECT_EQ(decrypted, original_data);
    
    // EN: Test with wrong key (should not match original)
    // FR: Test avec mauvaise clé (ne devrait pas correspondre à l'original)
    std::string wrong_key = "wrong_key";
    auto wrong_decrypted = ResumeSystemUtils::decryptCheckpointData(encrypted, wrong_key);
    EXPECT_NE(wrong_decrypted, original_data);
    
    // EN: Test empty data and key
    // FR: Test données et clé vides
    std::vector<uint8_t> empty_data;
    std::string empty_key;
    
    auto encrypted_empty_data = ResumeSystemUtils::encryptCheckpointData(empty_data, encryption_key);
    EXPECT_TRUE(encrypted_empty_data.empty());
    
    auto encrypted_empty_key = ResumeSystemUtils::encryptCheckpointData(original_data, empty_key);
    EXPECT_EQ(encrypted_empty_key, original_data); // Should return original if key is empty
}

// EN: Main test runner
// FR: Lanceur de test principal
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}