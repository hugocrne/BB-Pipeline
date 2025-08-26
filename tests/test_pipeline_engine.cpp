#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>

#include "orchestrator/pipeline_engine.hpp"

using namespace BBP::Orchestrator;

// EN: Test fixture for PipelineEngine tests
// FR: Fixture de test pour les tests de PipelineEngine
class PipelineEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // EN: Create pipeline engine with test configuration
        // FR: Créer le moteur de pipeline avec la configuration de test
        PipelineEngine::Config config;
        config.thread_pool_size = 4;
        config.enable_metrics = true;
        config.enable_logging = false; // EN: Disable logging for tests / FR: Désactiver la journalisation pour les tests
        config.max_pipeline_history = 10;
        
        engine = std::make_unique<PipelineEngine>(config);
        
        // EN: Create test directory
        // FR: Créer le répertoire de test
        test_dir = std::filesystem::temp_directory_path() / "pipeline_engine_test";
        std::filesystem::create_directories(test_dir);
    }
    
    void TearDown() override {
        engine->shutdown();
        engine.reset();
        
        // EN: Cleanup test directory
        // FR: Nettoyer le répertoire de test
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }
    
    // EN: Helper method to create a simple test stage
    // FR: Méthode d'aide pour créer une étape de test simple
    PipelineStageConfig createTestStage(const std::string& id, 
                                       const std::string& executable = "echo",
                                       const std::vector<std::string>& args = {"hello"},
                                       const std::vector<std::string>& deps = {}) {
        PipelineStageConfig stage;
        stage.id = id;
        stage.name = "Test Stage " + id;
        stage.description = "Test stage for " + id;
        stage.executable = executable;
        stage.arguments = args;
        stage.dependencies = deps;
        stage.timeout = std::chrono::seconds(10);
        stage.max_retries = 0;
        stage.allow_failure = false;
        
        return stage;
    }
    
    // EN: Helper method to create stages with dependencies
    // FR: Méthode d'aide pour créer des étapes avec des dépendances
    std::vector<PipelineStageConfig> createDependentStages() {
        std::vector<PipelineStageConfig> stages;
        
        stages.push_back(createTestStage("stage1", "echo", {"stage1"}));
        stages.push_back(createTestStage("stage2", "echo", {"stage2"}, {"stage1"}));
        stages.push_back(createTestStage("stage3", "echo", {"stage3"}, {"stage2"}));
        stages.push_back(createTestStage("stage4", "echo", {"stage4"}, {"stage1"}));
        stages.push_back(createTestStage("stage5", "echo", {"stage5"}, {"stage3", "stage4"}));
        
        return stages;
    }
    
    std::unique_ptr<PipelineEngine> engine;
    std::filesystem::path test_dir;
};

// EN: Test PipelineEngine configuration
// FR: Tester la configuration de PipelineEngine
TEST_F(PipelineEngineTest, ConfigurationTest) {
    // EN: Test default configuration
    // FR: Tester la configuration par défaut
    auto config = engine->getConfig();
    EXPECT_GT(config.thread_pool_size, 0);
    EXPECT_TRUE(config.enable_metrics);
    EXPECT_EQ(config.max_pipeline_history, 10);
    
    // EN: Update configuration
    // FR: Mettre à jour la configuration
    config.thread_pool_size = 8;
    config.enable_metrics = false;
    engine->updateConfig(config);
    
    auto updated_config = engine->getConfig();
    EXPECT_EQ(updated_config.thread_pool_size, 8);
    EXPECT_FALSE(updated_config.enable_metrics);
}

// EN: Test pipeline creation and management
// FR: Tester la création et la gestion de pipeline
TEST_F(PipelineEngineTest, PipelineManagement) {
    // EN: Test pipeline creation
    // FR: Tester la création de pipeline
    auto stages = createDependentStages();
    std::string pipeline_id = engine->createPipeline(stages);
    
    EXPECT_FALSE(pipeline_id.empty());
    
    // EN: Test getting pipeline IDs
    // FR: Tester l'obtention des IDs de pipeline
    auto pipeline_ids = engine->getPipelineIds();
    EXPECT_EQ(pipeline_ids.size(), 1);
    EXPECT_EQ(pipeline_ids[0], pipeline_id);
    
    // EN: Test getting pipeline stages
    // FR: Tester l'obtention des étapes de pipeline
    auto retrieved_stages = engine->getPipelineStages(pipeline_id);
    ASSERT_TRUE(retrieved_stages.has_value());
    EXPECT_EQ(retrieved_stages->size(), 5);
    
    // EN: Test adding a stage
    // FR: Tester l'ajout d'une étape
    auto new_stage = createTestStage("stage6", "echo", {"stage6"}, {"stage5"});
    EXPECT_TRUE(engine->addStage(pipeline_id, new_stage));
    
    retrieved_stages = engine->getPipelineStages(pipeline_id);
    ASSERT_TRUE(retrieved_stages.has_value());
    EXPECT_EQ(retrieved_stages->size(), 6);
    
    // EN: Test updating a stage
    // FR: Tester la mise à jour d'une étape
    new_stage.description = "Updated description";
    EXPECT_TRUE(engine->updateStage(pipeline_id, new_stage));
    
    // EN: Test removing a stage
    // FR: Tester la suppression d'une étape
    EXPECT_TRUE(engine->removeStage(pipeline_id, "stage6"));
    
    retrieved_stages = engine->getPipelineStages(pipeline_id);
    ASSERT_TRUE(retrieved_stages.has_value());
    EXPECT_EQ(retrieved_stages->size(), 5);
    
    // EN: Test with invalid pipeline ID
    // FR: Tester avec un ID de pipeline invalide
    EXPECT_FALSE(engine->addStage("invalid_id", new_stage));
    EXPECT_FALSE(engine->removeStage("invalid_id", "stage1"));
    EXPECT_FALSE(engine->updateStage("invalid_id", new_stage));
}

// EN: Test dependency resolution
// FR: Tester la résolution des dépendances
TEST_F(PipelineEngineTest, DependencyResolution) {
    auto stages = createDependentStages();
    std::string pipeline_id = engine->createPipeline(stages);
    
    // EN: Test execution order
    // FR: Tester l'ordre d'exécution
    auto execution_order = engine->getExecutionOrder(pipeline_id);
    EXPECT_EQ(execution_order.size(), 5);
    
    // EN: Verify that stage1 comes before stage2
    // FR: Vérifier que stage1 vient avant stage2
    auto stage1_pos = std::find(execution_order.begin(), execution_order.end(), "stage1");
    auto stage2_pos = std::find(execution_order.begin(), execution_order.end(), "stage2");
    EXPECT_LT(stage1_pos, stage2_pos);
    
    // EN: Verify that stage5 comes last (depends on stage3 and stage4)
    // FR: Vérifier que stage5 vient en dernier (dépend de stage3 et stage4)
    EXPECT_EQ(execution_order.back(), "stage5");
    
    // EN: Test dependency validation
    // FR: Tester la validation des dépendances
    EXPECT_TRUE(engine->validateDependencies(pipeline_id));
    
    // EN: Test circular dependency detection
    // FR: Tester la détection de dépendances circulaires
    auto circular_deps = engine->detectCircularDependencies(pipeline_id);
    EXPECT_TRUE(circular_deps.empty());
}

// EN: Test circular dependency detection
// FR: Tester la détection de dépendances circulaires
TEST_F(PipelineEngineTest, CircularDependencyDetection) {
    std::vector<PipelineStageConfig> circular_stages;
    
    // EN: Create stages with circular dependency
    // FR: Créer des étapes avec dépendance circulaire
    circular_stages.push_back(createTestStage("a", "echo", {"a"}, {"b"}));
    circular_stages.push_back(createTestStage("b", "echo", {"b"}, {"c"}));
    circular_stages.push_back(createTestStage("c", "echo", {"c"}, {"a"}));
    
    std::string pipeline_id = engine->createPipeline(circular_stages);
    
    // EN: Test circular dependency detection
    // FR: Tester la détection de dépendances circulaires
    EXPECT_FALSE(engine->validateDependencies(pipeline_id));
    
    auto circular_deps = engine->detectCircularDependencies(pipeline_id);
    EXPECT_FALSE(circular_deps.empty());
}

// EN: Test pipeline validation
// FR: Tester la validation de pipeline
TEST_F(PipelineEngineTest, PipelineValidation) {
    auto stages = createDependentStages();
    std::string pipeline_id = engine->createPipeline(stages);
    
    // EN: Test valid pipeline
    // FR: Tester un pipeline valide
    auto validation_result = engine->validatePipeline(pipeline_id);
    EXPECT_TRUE(validation_result.is_valid);
    EXPECT_TRUE(validation_result.errors.empty());
    
    // EN: Test invalid pipeline with missing dependency
    // FR: Tester un pipeline invalide avec dépendance manquante
    std::vector<PipelineStageConfig> invalid_stages;
    invalid_stages.push_back(createTestStage("stage1", "echo", {"stage1"}, {"missing_stage"}));
    
    validation_result = engine->validateStages(invalid_stages);
    EXPECT_FALSE(validation_result.is_valid);
    EXPECT_FALSE(validation_result.errors.empty());
}

// EN: Test pipeline execution modes
// FR: Tester les modes d'exécution de pipeline
TEST_F(PipelineEngineTest, ExecutionModes) {
    auto stages = createDependentStages();
    std::string pipeline_id = engine->createPipeline(stages);
    
    // EN: Test sequential execution
    // FR: Tester l'exécution séquentielle
    PipelineExecutionConfig config;
    config.execution_mode = PipelineExecutionMode::SEQUENTIAL;
    config.dry_run = true; // EN: Use dry run for faster testing / FR: Utiliser l'exécution à blanc pour un test plus rapide
    
    auto stats = engine->executePipeline(pipeline_id, config);
    EXPECT_EQ(stats.total_stages_executed, 5);
    EXPECT_EQ(stats.successful_stages, 5);
    EXPECT_EQ(stats.success_rate, 1.0);
    
    // EN: Test parallel execution
    // FR: Tester l'exécution parallèle
    config.execution_mode = PipelineExecutionMode::PARALLEL;
    stats = engine->executePipeline(pipeline_id, config);
    EXPECT_EQ(stats.total_stages_executed, 5);
    EXPECT_EQ(stats.successful_stages, 5);
    
    // EN: Test hybrid execution
    // FR: Tester l'exécution hybride
    config.execution_mode = PipelineExecutionMode::HYBRID;
    stats = engine->executePipeline(pipeline_id, config);
    EXPECT_EQ(stats.total_stages_executed, 5);
    EXPECT_EQ(stats.successful_stages, 5);
}

// EN: Test async pipeline execution
// FR: Tester l'exécution asynchrone de pipeline
TEST_F(PipelineEngineTest, AsyncExecution) {
    auto stages = createDependentStages();
    std::string pipeline_id = engine->createPipeline(stages);
    
    PipelineExecutionConfig config;
    config.dry_run = true;
    
    // EN: Test async execution
    // FR: Tester l'exécution asynchrone
    auto future = engine->executePipelineAsync(pipeline_id, config);
    
    // EN: Wait for completion
    // FR: Attendre la completion
    auto stats = future.get();
    
    EXPECT_EQ(stats.total_stages_executed, 5);
    EXPECT_EQ(stats.successful_stages, 5);
    EXPECT_EQ(stats.success_rate, 1.0);
}

// EN: Test pipeline control operations
// FR: Tester les opérations de contrôle de pipeline
TEST_F(PipelineEngineTest, PipelineControl) {
    auto stages = createDependentStages();
    std::string pipeline_id = engine->createPipeline(stages);
    
    // EN: Test pause/resume (these are currently no-ops but should not fail)
    // FR: Tester pause/reprise (ce sont actuellement des no-ops mais ne devraient pas échouer)
    EXPECT_FALSE(engine->pausePipeline(pipeline_id));  // EN: No active execution / FR: Pas d'exécution active
    EXPECT_FALSE(engine->resumePipeline(pipeline_id)); // EN: No active execution / FR: Pas d'exécution active
    EXPECT_FALSE(engine->cancelPipeline(pipeline_id)); // EN: No active execution / FR: Pas d'exécution active
    
    // EN: Test retry failed stages
    // FR: Tester la reprise des étapes échouées
    EXPECT_TRUE(engine->retryFailedStages(pipeline_id));
    
    // EN: Test with invalid pipeline ID
    // FR: Tester avec un ID de pipeline invalide
    EXPECT_FALSE(engine->pausePipeline("invalid_id"));
    EXPECT_FALSE(engine->resumePipeline("invalid_id"));
    EXPECT_FALSE(engine->cancelPipeline("invalid_id"));
}

// EN: Test progress monitoring
// FR: Tester la surveillance de la progression
TEST_F(PipelineEngineTest, ProgressMonitoring) {
    auto stages = createDependentStages();
    std::string pipeline_id = engine->createPipeline(stages);
    
    // EN: Test progress monitoring with no active execution
    // FR: Tester la surveillance de progression sans exécution active
    auto progress = engine->getPipelineProgress(pipeline_id);
    EXPECT_FALSE(progress.has_value());
    
    // EN: Test with invalid pipeline ID
    // FR: Tester avec un ID de pipeline invalide
    progress = engine->getPipelineProgress("invalid_id");
    EXPECT_FALSE(progress.has_value());
}

// EN: Test stage result management
// FR: Tester la gestion des résultats d'étapes
TEST_F(PipelineEngineTest, StageResultManagement) {
    auto stages = createDependentStages();
    std::string pipeline_id = engine->createPipeline(stages);
    
    // EN: Test getting stage results with no execution
    // FR: Tester l'obtention des résultats d'étapes sans exécution
    auto result = engine->getStageResult(pipeline_id, "stage1");
    EXPECT_FALSE(result.has_value());
    
    auto all_results = engine->getAllStageResults(pipeline_id);
    EXPECT_TRUE(all_results.empty());
    
    // EN: Test with invalid pipeline ID
    // FR: Tester avec un ID de pipeline invalide
    result = engine->getStageResult("invalid_id", "stage1");
    EXPECT_FALSE(result.has_value());
    
    all_results = engine->getAllStageResults("invalid_id");
    EXPECT_TRUE(all_results.empty());
}

// EN: Test pipeline state management
// FR: Tester la gestion d'état de pipeline
TEST_F(PipelineEngineTest, StateManagement) {
    auto stages = createDependentStages();
    std::string pipeline_id = engine->createPipeline(stages);
    
    auto filepath = test_dir / "pipeline_state.json";
    
    // EN: Test saving pipeline state
    // FR: Tester la sauvegarde de l'état du pipeline
    EXPECT_TRUE(engine->savePipelineState(pipeline_id, filepath.string()));
    EXPECT_TRUE(std::filesystem::exists(filepath));
    
    // EN: Test loading pipeline state
    // FR: Tester le chargement de l'état du pipeline
    std::string new_pipeline_id = "loaded_pipeline";
    EXPECT_TRUE(engine->loadPipelineState(new_pipeline_id, filepath.string()));
    
    auto loaded_stages = engine->getPipelineStages(new_pipeline_id);
    ASSERT_TRUE(loaded_stages.has_value());
    EXPECT_EQ(loaded_stages->size(), 5);
    
    // EN: Test clearing pipeline state
    // FR: Tester l'effacement de l'état du pipeline
    engine->clearPipelineState(pipeline_id);
    auto cleared_stages = engine->getPipelineStages(pipeline_id);
    EXPECT_FALSE(cleared_stages.has_value());
    
    // EN: Test with invalid file
    // FR: Tester avec un fichier invalide
    EXPECT_FALSE(engine->loadPipelineState("test", "/invalid/path/file.json"));
}

// EN: Test statistics and monitoring
// FR: Tester les statistiques et la surveillance
TEST_F(PipelineEngineTest, StatisticsAndMonitoring) {
    auto stages = createDependentStages();
    std::string pipeline_id = engine->createPipeline(stages);
    
    // EN: Test getting statistics with no execution
    // FR: Tester l'obtention de statistiques sans exécution
    auto stats = engine->getPipelineStatistics(pipeline_id);
    EXPECT_FALSE(stats.has_value());
    
    auto all_stats = engine->getAllPipelineStatistics();
    EXPECT_TRUE(all_stats.empty());
    
    // EN: Execute pipeline to generate statistics
    // FR: Exécuter le pipeline pour générer des statistiques
    PipelineExecutionConfig config;
    config.dry_run = true;
    engine->executePipeline(pipeline_id, config);
    
    // EN: Test statistics after execution
    // FR: Tester les statistiques après exécution
    stats = engine->getPipelineStatistics(pipeline_id);
    EXPECT_TRUE(stats.has_value());
    
    all_stats = engine->getAllPipelineStatistics();
    EXPECT_EQ(all_stats.size(), 1);
    
    // EN: Test engine statistics
    // FR: Tester les statistiques du moteur
    auto engine_stats = engine->getEngineStatistics();
    EXPECT_GE(engine_stats.total_pipelines_executed, 1);
    
    // EN: Test clearing statistics
    // FR: Tester l'effacement des statistiques
    engine->clearStatistics();
    all_stats = engine->getAllPipelineStatistics();
    EXPECT_TRUE(all_stats.empty());
}

// EN: Test event callbacks
// FR: Tester les callbacks d'événements
TEST_F(PipelineEngineTest, EventCallbacks) {
    // EN: Event tracking variables
    // FR: Variables de suivi d'événements
    std::vector<PipelineEvent> received_events;
    std::mutex events_mutex;
    
    // EN: Register event callback
    // FR: Enregistrer le callback d'événement
    engine->registerEventCallback([&](const PipelineEvent& event) {
        std::lock_guard<std::mutex> lock(events_mutex);
        received_events.push_back(event);
    });
    
    auto stages = createDependentStages();
    std::string pipeline_id = engine->createPipeline(stages);
    
    PipelineExecutionConfig config;
    config.dry_run = true;
    engine->executePipeline(pipeline_id, config);
    
    // EN: Give some time for events to be processed
    // FR: Donner du temps pour que les événements soient traités
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // EN: Check that events were received
    // FR: Vérifier que les événements ont été reçus
    {
        std::lock_guard<std::mutex> lock(events_mutex);
        EXPECT_GT(received_events.size(), 0);
    }
    
    // EN: Unregister callback
    // FR: Désinscrire le callback
    engine->unregisterEventCallback();
    
    // EN: Clear events and execute again
    // FR: Effacer les événements et réexécuter
    {
        std::lock_guard<std::mutex> lock(events_mutex);
        received_events.clear();
    }
    
    engine->executePipeline(pipeline_id, config);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // EN: Should not receive new events
    // FR: Ne devrait pas recevoir de nouveaux événements
    {
        std::lock_guard<std::mutex> lock(events_mutex);
        EXPECT_EQ(received_events.size(), 0);
    }
}

// EN: Test health and status
// FR: Tester la santé et le statut
TEST_F(PipelineEngineTest, HealthAndStatus) {
    // EN: Test engine health
    // FR: Tester la santé du moteur
    EXPECT_TRUE(engine->isHealthy());
    
    // EN: Test engine status
    // FR: Tester le statut du moteur
    std::string status = engine->getStatus();
    EXPECT_FALSE(status.empty());
    EXPECT_NE(status.find("PipelineEngine Status"), std::string::npos);
    
    // EN: Test shutdown
    // FR: Tester l'arrêt
    engine->shutdown();
    
    // EN: Health should be affected after shutdown
    // FR: La santé devrait être affectée après l'arrêt
    EXPECT_FALSE(engine->isHealthy());
}

// EN: Test fixture for PipelineTask tests
// FR: Fixture de test pour les tests de PipelineTask
class PipelineTaskTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = std::make_unique<PipelineExecutionConfig>();
        context = std::make_unique<PipelineExecutionContext>("test_pipeline", *config);
    }
    
    void TearDown() override {
        context.reset();
        config.reset();
    }
    
    std::unique_ptr<PipelineExecutionConfig> config;
    std::unique_ptr<PipelineExecutionContext> context;
};

// EN: Test PipelineTask basic functionality
// FR: Tester les fonctionnalités de base de PipelineTask
TEST_F(PipelineTaskTest, BasicFunctionality) {
    PipelineStageConfig stage_config;
    stage_config.id = "test_stage";
    stage_config.name = "Test Stage";
    stage_config.executable = "echo";
    stage_config.arguments = {"hello", "world"};
    stage_config.timeout = std::chrono::seconds(5);
    
    PipelineTask task(stage_config, context.get());
    
    // EN: Test basic properties
    // FR: Tester les propriétés de base
    EXPECT_EQ(task.getId(), "test_stage");
    EXPECT_EQ(task.getConfig().id, "test_stage");
    EXPECT_EQ(task.getStatus(), PipelineStageStatus::PENDING);
    EXPECT_FALSE(task.isCancelled());
    
    // EN: Test dependency management
    // FR: Tester la gestion des dépendances
    EXPECT_TRUE(task.areDependenciesMet()); // EN: No dependencies / FR: Pas de dépendances
    
    task.addDependency("dep1");
    EXPECT_FALSE(task.areDependenciesMet()); // EN: Dependency not met / FR: Dépendance non satisfaite
    
    task.removeDependency("dep1");
    EXPECT_TRUE(task.areDependenciesMet());
}

// EN: Test PipelineTask execution
// FR: Tester l'exécution de PipelineTask
TEST_F(PipelineTaskTest, TaskExecution) {
    PipelineStageConfig stage_config;
    stage_config.id = "test_stage";
    stage_config.executable = "echo";
    stage_config.arguments = {"test"};
    stage_config.timeout = std::chrono::seconds(5);
    
    PipelineTask task(stage_config, context.get());
    
    // EN: Execute task
    // FR: Exécuter la tâche
    auto result = task.execute();
    
    // EN: Verify result
    // FR: Vérifier le résultat
    EXPECT_EQ(result.stage_id, "test_stage");
    EXPECT_EQ(result.status, PipelineStageStatus::COMPLETED);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_GT(result.execution_time.count(), 0);
}

// EN: Test PipelineTask cancellation
// FR: Tester l'annulation de PipelineTask
TEST_F(PipelineTaskTest, TaskCancellation) {
    PipelineStageConfig stage_config;
    stage_config.id = "test_stage";
    stage_config.executable = "sleep";
    stage_config.arguments = {"1"};
    stage_config.timeout = std::chrono::seconds(5);
    
    PipelineTask task(stage_config, context.get());
    
    // EN: Cancel task before execution
    // FR: Annuler la tâche avant l'exécution
    task.cancel();
    EXPECT_TRUE(task.isCancelled());
    EXPECT_EQ(task.getStatus(), PipelineStageStatus::CANCELLED);
    
    // EN: Execute cancelled task
    // FR: Exécuter la tâche annulée
    auto result = task.execute();
    
    EXPECT_EQ(result.status, PipelineStageStatus::CANCELLED);
    EXPECT_FALSE(result.error_message.empty());
}

// EN: Test fixture for PipelineDependencyResolver tests
// FR: Fixture de test pour les tests de PipelineDependencyResolver
class PipelineDependencyResolverTest : public ::testing::Test {
protected:
    std::vector<PipelineStageConfig> createTestStages() {
        std::vector<PipelineStageConfig> stages;
        
        PipelineStageConfig stage1;
        stage1.id = "stage1";
        stage1.dependencies = {};
        stages.push_back(stage1);
        
        PipelineStageConfig stage2;
        stage2.id = "stage2";
        stage2.dependencies = {"stage1"};
        stages.push_back(stage2);
        
        PipelineStageConfig stage3;
        stage3.id = "stage3";
        stage3.dependencies = {"stage1"};
        stages.push_back(stage3);
        
        PipelineStageConfig stage4;
        stage4.id = "stage4";
        stage4.dependencies = {"stage2", "stage3"};
        stages.push_back(stage4);
        
        return stages;
    }
};

// EN: Test dependency resolution
// FR: Tester la résolution des dépendances
TEST_F(PipelineDependencyResolverTest, DependencyResolution) {
    auto stages = createTestStages();
    PipelineDependencyResolver resolver(stages);
    
    // EN: Test execution order
    // FR: Tester l'ordre d'exécution
    auto execution_order = resolver.getExecutionOrder();
    EXPECT_EQ(execution_order.size(), 4);
    
    // EN: stage1 should be first
    // FR: stage1 devrait être en premier
    EXPECT_EQ(execution_order[0], "stage1");
    
    // EN: stage4 should be last
    // FR: stage4 devrait être en dernier
    EXPECT_EQ(execution_order[3], "stage4");
    
    // EN: Test execution levels
    // FR: Tester les niveaux d'exécution
    auto levels = resolver.getExecutionLevels();
    EXPECT_GE(levels.size(), 3); // EN: At least 3 levels / FR: Au moins 3 niveaux
    
    // EN: First level should contain only stage1
    // FR: Le premier niveau devrait contenir seulement stage1
    EXPECT_EQ(levels[0].size(), 1);
    EXPECT_EQ(levels[0][0], "stage1");
    
    // EN: Test dependency queries
    // FR: Tester les requêtes de dépendances
    auto dependencies = resolver.getDependencies("stage4");
    EXPECT_EQ(dependencies.size(), 2);
    EXPECT_TRUE(std::find(dependencies.begin(), dependencies.end(), "stage2") != dependencies.end());
    EXPECT_TRUE(std::find(dependencies.begin(), dependencies.end(), "stage3") != dependencies.end());
    
    auto dependents = resolver.getDependents("stage1");
    EXPECT_EQ(dependents.size(), 2);
    
    // EN: Test execution conditions
    // FR: Tester les conditions d'exécution
    std::set<std::string> completed = {"stage1"};
    EXPECT_TRUE(resolver.canExecute("stage2", completed));
    EXPECT_TRUE(resolver.canExecute("stage3", completed));
    EXPECT_FALSE(resolver.canExecute("stage4", completed));
    
    completed.insert("stage2");
    completed.insert("stage3");
    EXPECT_TRUE(resolver.canExecute("stage4", completed));
}

// EN: Test circular dependency detection
// FR: Tester la détection de dépendances circulaires
TEST_F(PipelineDependencyResolverTest, CircularDependencyDetection) {
    std::vector<PipelineStageConfig> circular_stages;
    
    PipelineStageConfig stage1;
    stage1.id = "stage1";
    stage1.dependencies = {"stage3"};
    circular_stages.push_back(stage1);
    
    PipelineStageConfig stage2;
    stage2.id = "stage2";
    stage2.dependencies = {"stage1"};
    circular_stages.push_back(stage2);
    
    PipelineStageConfig stage3;
    stage3.id = "stage3";
    stage3.dependencies = {"stage2"};
    circular_stages.push_back(stage3);
    
    PipelineDependencyResolver resolver(circular_stages);
    
    // EN: Should detect circular dependency
    // FR: Devrait détecter la dépendance circulaire
    EXPECT_TRUE(resolver.hasCircularDependency());
    
    auto circular_deps = resolver.getCircularDependencies();
    EXPECT_FALSE(circular_deps.empty());
}

// EN: Test fixture for PipelineExecutionContext tests
// FR: Fixture de test pour les tests de PipelineExecutionContext
class PipelineExecutionContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        config = PipelineExecutionConfig{};
        context = std::make_unique<PipelineExecutionContext>("test_pipeline", config);
    }
    
    void TearDown() override {
        context.reset();
    }
    
    PipelineExecutionConfig config;
    std::unique_ptr<PipelineExecutionContext> context;
};

// EN: Test execution context basic functionality
// FR: Tester les fonctionnalités de base du contexte d'exécution
TEST_F(PipelineExecutionContextTest, BasicFunctionality) {
    // EN: Test basic properties
    // FR: Tester les propriétés de base
    EXPECT_EQ(context->getPipelineId(), "test_pipeline");
    EXPECT_FALSE(context->isCancelled());
    EXPECT_TRUE(context->shouldContinue());
    
    // EN: Test getting results with empty context
    // FR: Tester l'obtention de résultats avec un contexte vide
    auto result = context->getStageResult("stage1");
    EXPECT_FALSE(result.has_value());
    
    auto all_results = context->getAllStageResults();
    EXPECT_TRUE(all_results.empty());
    
    // EN: Test progress with empty context
    // FR: Tester la progression avec un contexte vide
    auto progress = context->getCurrentProgress();
    EXPECT_EQ(progress.total_stages, 0);
    EXPECT_EQ(progress.completed_stages, 0);
    EXPECT_EQ(progress.completion_percentage, 0.0);
}

// EN: Test stage result management
// FR: Tester la gestion des résultats d'étapes
TEST_F(PipelineExecutionContextTest, StageResultManagement) {
    // EN: Create test stage result
    // FR: Créer un résultat d'étape de test
    PipelineStageResult result;
    result.stage_id = "stage1";
    result.status = PipelineStageStatus::COMPLETED;
    result.execution_time = std::chrono::milliseconds(100);
    result.exit_code = 0;
    
    // EN: Update stage result
    // FR: Mettre à jour le résultat d'étape
    context->updateStageResult("stage1", result);
    
    // EN: Retrieve stage result
    // FR: Récupérer le résultat d'étape
    auto retrieved_result = context->getStageResult("stage1");
    ASSERT_TRUE(retrieved_result.has_value());
    EXPECT_EQ(retrieved_result->stage_id, "stage1");
    EXPECT_EQ(retrieved_result->status, PipelineStageStatus::COMPLETED);
    EXPECT_EQ(retrieved_result->execution_time.count(), 100);
    
    // EN: Test getting all results
    // FR: Tester l'obtention de tous les résultats
    auto all_results = context->getAllStageResults();
    EXPECT_EQ(all_results.size(), 1);
    EXPECT_EQ(all_results[0].stage_id, "stage1");
}

// EN: Test cancellation
// FR: Tester l'annulation
TEST_F(PipelineExecutionContextTest, Cancellation) {
    // EN: Initially should not be cancelled
    // FR: Initialement ne devrait pas être annulé
    EXPECT_FALSE(context->isCancelled());
    EXPECT_TRUE(context->shouldContinue());
    
    // EN: Request cancellation
    // FR: Demander l'annulation
    context->requestCancellation();
    
    // EN: Should now be cancelled
    // FR: Devrait maintenant être annulé
    EXPECT_TRUE(context->isCancelled());
    EXPECT_FALSE(context->shouldContinue());
}

// EN: Test event handling
// FR: Tester la gestion d'événements
TEST_F(PipelineExecutionContextTest, EventHandling) {
    std::vector<PipelineEvent> received_events;
    std::mutex events_mutex;
    
    // EN: Set event callback
    // FR: Définir le callback d'événement
    context->setEventCallback([&](const PipelineEvent& event) {
        std::lock_guard<std::mutex> lock(events_mutex);
        received_events.push_back(event);
    });
    
    // EN: Notify stage started
    // FR: Notifier le démarrage d'étape
    context->notifyStageStarted("stage1");
    
    // EN: Create completed stage result
    // FR: Créer un résultat d'étape complété
    PipelineStageResult result;
    result.stage_id = "stage1";
    result.status = PipelineStageStatus::COMPLETED;
    context->notifyStageCompleted("stage1", result);
    
    // EN: Give some time for events to be processed
    // FR: Donner du temps pour que les événements soient traités
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // EN: Check received events
    // FR: Vérifier les événements reçus
    {
        std::lock_guard<std::mutex> lock(events_mutex);
        EXPECT_GE(received_events.size(), 2);
        
        // EN: Find stage started event
        // FR: Trouver l'événement de démarrage d'étape
        auto started_event = std::find_if(received_events.begin(), received_events.end(),
            [](const PipelineEvent& e) { return e.type == PipelineEventType::STAGE_STARTED; });
        EXPECT_NE(started_event, received_events.end());
        
        // EN: Find stage completed event
        // FR: Trouver l'événement de completion d'étape
        auto completed_event = std::find_if(received_events.begin(), received_events.end(),
            [](const PipelineEvent& e) { return e.type == PipelineEventType::STAGE_COMPLETED; });
        EXPECT_NE(completed_event, received_events.end());
    }
}

// EN: Test fixture for PipelineUtils tests
// FR: Fixture de test pour les tests de PipelineUtils
class PipelineUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir = std::filesystem::temp_directory_path() / "pipeline_utils_test";
        std::filesystem::create_directories(test_dir);
    }
    
    void TearDown() override {
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }
    
    std::filesystem::path test_dir;
};

// EN: Test validation utilities
// FR: Tester les utilitaires de validation
TEST_F(PipelineUtilsTest, ValidationUtilities) {
    // EN: Test valid stage ID
    // FR: Tester un ID d'étape valide
    EXPECT_TRUE(PipelineUtils::isValidStageId("valid_stage_id"));
    EXPECT_TRUE(PipelineUtils::isValidStageId("stage-1"));
    EXPECT_TRUE(PipelineUtils::isValidStageId("Stage123"));
    
    // EN: Test invalid stage IDs
    // FR: Tester des IDs d'étapes invalides
    EXPECT_FALSE(PipelineUtils::isValidStageId(""));
    EXPECT_FALSE(PipelineUtils::isValidStageId("invalid stage"));
    EXPECT_FALSE(PipelineUtils::isValidStageId("stage@123"));
    
    // EN: Test executable validation
    // FR: Tester la validation d'exécutable
    EXPECT_TRUE(PipelineUtils::isValidExecutable("echo"));  // EN: Should exist in PATH / FR: Devrait exister dans PATH
    EXPECT_FALSE(PipelineUtils::isValidExecutable("nonexistent_executable_12345"));
    
    // EN: Test stage config validation
    // FR: Tester la validation de configuration d'étape
    PipelineStageConfig valid_config;
    valid_config.id = "test_stage";
    valid_config.executable = "echo";
    valid_config.timeout = std::chrono::seconds(10);
    valid_config.max_retries = 0;
    
    auto errors = PipelineUtils::validateStageConfig(valid_config);
    EXPECT_TRUE(errors.empty());
    
    // EN: Test invalid config
    // FR: Tester une configuration invalide
    PipelineStageConfig invalid_config;
    invalid_config.id = "";
    invalid_config.executable = "";
    invalid_config.timeout = std::chrono::seconds(-1);
    invalid_config.max_retries = -1;
    
    errors = PipelineUtils::validateStageConfig(invalid_config);
    EXPECT_FALSE(errors.empty());
}

// EN: Test dependency utilities
// FR: Tester les utilitaires de dépendances
TEST_F(PipelineUtilsTest, DependencyUtilities) {
    std::vector<PipelineStageConfig> stages;
    
    PipelineStageConfig stage1;
    stage1.id = "stage1";
    stage1.dependencies = {};
    stages.push_back(stage1);
    
    PipelineStageConfig stage2;
    stage2.id = "stage2";
    stage2.dependencies = {"stage1", "missing_stage"};
    stages.push_back(stage2);
    
    // EN: Test missing dependencies detection
    // FR: Tester la détection de dépendances manquantes
    auto missing = PipelineUtils::findMissingDependencies(stages);
    EXPECT_EQ(missing.size(), 1);
    EXPECT_EQ(missing[0], "missing_stage");
    
    // EN: Test cyclic dependency detection
    // FR: Tester la détection de dépendances cycliques
    EXPECT_FALSE(PipelineUtils::hasCyclicDependency(stages));
    
    // EN: Create cyclic dependency
    // FR: Créer une dépendance cyclique
    std::vector<PipelineStageConfig> cyclic_stages;
    
    PipelineStageConfig cyclic1;
    cyclic1.id = "cyclic1";
    cyclic1.dependencies = {"cyclic2"};
    cyclic_stages.push_back(cyclic1);
    
    PipelineStageConfig cyclic2;
    cyclic2.id = "cyclic2";
    cyclic2.dependencies = {"cyclic1"};
    cyclic_stages.push_back(cyclic2);
    
    EXPECT_TRUE(PipelineUtils::hasCyclicDependency(cyclic_stages));
}

// EN: Test format utilities
// FR: Tester les utilitaires de formatage
TEST_F(PipelineUtilsTest, FormatUtilities) {
    // EN: Test duration formatting
    // FR: Tester le formatage de durée
    auto duration = std::chrono::milliseconds(65432);
    std::string formatted = PipelineUtils::formatDuration(duration);
    EXPECT_FALSE(formatted.empty());
    EXPECT_NE(formatted.find("65.432s"), std::string::npos);
    
    // EN: Test hours formatting
    // FR: Tester le formatage des heures
    auto long_duration = std::chrono::milliseconds(3665000); // EN: 1h 1m 5s / FR: 1h 1m 5s
    formatted = PipelineUtils::formatDuration(long_duration);
    EXPECT_NE(formatted.find("1h"), std::string::npos);
    EXPECT_NE(formatted.find("1m"), std::string::npos);
    EXPECT_NE(formatted.find("5s"), std::string::npos);
    
    // EN: Test timestamp formatting
    // FR: Tester le formatage d'horodatage
    auto timestamp = std::chrono::system_clock::now();
    std::string timestamp_str = PipelineUtils::formatTimestamp(timestamp);
    EXPECT_FALSE(timestamp_str.empty());
    
    // EN: Test status string conversion
    // FR: Tester la conversion de chaîne de statut
    EXPECT_EQ(PipelineUtils::statusToString(PipelineStageStatus::PENDING), "PENDING");
    EXPECT_EQ(PipelineUtils::statusToString(PipelineStageStatus::COMPLETED), "COMPLETED");
    EXPECT_EQ(PipelineUtils::statusToString(PipelineStageStatus::FAILED), "FAILED");
    
    // EN: Test execution mode conversion
    // FR: Tester la conversion de mode d'exécution
    EXPECT_EQ(PipelineUtils::executionModeToString(PipelineExecutionMode::SEQUENTIAL), "SEQUENTIAL");
    EXPECT_EQ(PipelineUtils::executionModeToString(PipelineExecutionMode::PARALLEL), "PARALLEL");
    EXPECT_EQ(PipelineUtils::executionModeToString(PipelineExecutionMode::HYBRID), "HYBRID");
    
    // EN: Test error strategy conversion
    // FR: Tester la conversion de stratégie d'erreur
    EXPECT_EQ(PipelineUtils::errorStrategyToString(PipelineErrorStrategy::FAIL_FAST), "FAIL_FAST");
    EXPECT_EQ(PipelineUtils::errorStrategyToString(PipelineErrorStrategy::CONTINUE), "CONTINUE");
    EXPECT_EQ(PipelineUtils::errorStrategyToString(PipelineErrorStrategy::RETRY), "RETRY");
    EXPECT_EQ(PipelineUtils::errorStrategyToString(PipelineErrorStrategy::SKIP), "SKIP");
}

// EN: Test file I/O utilities
// FR: Tester les utilitaires d'E/S de fichier
TEST_F(PipelineUtilsTest, FileIOUtilities) {
    std::vector<PipelineStageConfig> stages;
    
    PipelineStageConfig stage1;
    stage1.id = "stage1";
    stage1.name = "Test Stage 1";
    stage1.executable = "echo";
    stage1.arguments = {"hello"};
    stage1.timeout = std::chrono::seconds(30);
    stages.push_back(stage1);
    
    PipelineStageConfig stage2;
    stage2.id = "stage2";
    stage2.name = "Test Stage 2";
    stage2.executable = "ls";
    stage2.dependencies = {"stage1"};
    stage2.environment["TEST_VAR"] = "test_value";
    stages.push_back(stage2);
    
    // EN: Test JSON save/load
    // FR: Tester la sauvegarde/chargement JSON
    auto json_file = test_dir / "pipeline.json";
    EXPECT_TRUE(PipelineUtils::savePipelineToJSON(json_file.string(), stages));
    EXPECT_TRUE(std::filesystem::exists(json_file));
    
    std::vector<PipelineStageConfig> loaded_stages;
    EXPECT_TRUE(PipelineUtils::loadPipelineFromJSON(json_file.string(), loaded_stages));
    EXPECT_EQ(loaded_stages.size(), 2);
    EXPECT_EQ(loaded_stages[0].id, "stage1");
    EXPECT_EQ(loaded_stages[1].id, "stage2");
    EXPECT_EQ(loaded_stages[1].dependencies[0], "stage1");
    
    // EN: Test YAML save/load
    // FR: Tester la sauvegarde/chargement YAML
    auto yaml_file = test_dir / "pipeline.yaml";
    EXPECT_TRUE(PipelineUtils::savePipelineToYAML(yaml_file.string(), stages));
    EXPECT_TRUE(std::filesystem::exists(yaml_file));
    
    loaded_stages.clear();
    EXPECT_TRUE(PipelineUtils::loadPipelineFromYAML(yaml_file.string(), loaded_stages));
    EXPECT_EQ(loaded_stages.size(), 2);
    EXPECT_EQ(loaded_stages[0].id, "stage1");
    EXPECT_EQ(loaded_stages[1].id, "stage2");
    
    // EN: Test with invalid files
    // FR: Tester avec des fichiers invalides
    EXPECT_FALSE(PipelineUtils::loadPipelineFromJSON("/invalid/path.json", loaded_stages));
    EXPECT_FALSE(PipelineUtils::loadPipelineFromYAML("/invalid/path.yaml", loaded_stages));
}