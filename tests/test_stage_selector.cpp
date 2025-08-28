// EN: Comprehensive Unit Tests for Stage Selector - Individual module execution with validation
// FR: Tests Unitaires Complets pour Sélecteur d'Étapes - Exécution de modules individuels avec validation

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "orchestrator/stage_selector.hpp"
#include "orchestrator/pipeline_engine.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

using namespace BBP::Orchestrator;
using namespace testing;

// EN: Test fixture for Stage Selector tests
// FR: Fixture de test pour les tests du Sélecteur d'Étapes
class StageSelectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // EN: Create selector with test configuration
        // FR: Créer le sélecteur avec configuration de test
        config_.enable_caching = true;
        config_.enable_statistics = true;
        config_.cache_ttl = std::chrono::seconds(60);
        config_.max_cache_entries = 100;
        config_.default_selection_timeout = std::chrono::seconds(30);
        
        selector_ = std::make_unique<StageSelector>(config_);
        
        // EN: Setup test stages
        // FR: Configurer les étapes de test
        setupTestStages();
    }
    
    void TearDown() override {
        selector_.reset();
        
        // EN: Clean up any test files
        // FR: Nettoyer tous les fichiers de test
        std::error_code ec;
        std::filesystem::remove_all("test_output", ec);
    }
    
    void setupTestStages() {
        // EN: Create a set of test stages with various configurations
        // FR: Créer un ensemble d'étapes de test avec diverses configurations
        
        // EN: Stage 1: Basic subdomain enumeration
        // FR: Étape 1: Énumération de base des sous-domaines
        PipelineStageConfig stage1;
        stage1.id = "subhunter";
        stage1.name = "Subdomain Hunter";
        stage1.description = "Enumerate subdomains using passive and active techniques";
        stage1.executable = "bbp-subhunter";
        stage1.arguments = {"--scope", "data/scope.csv", "--out", "out/01_subdomains.csv"};
        stage1.priority = PipelineStagePriority::HIGH;
        stage1.timeout = std::chrono::seconds(600);
        stage1.max_retries = 2;
        stage1.metadata["tags"] = "reconnaissance,passive";
        stage1.metadata["category"] = "enumeration";
        test_stages_.push_back(stage1);
        
        // EN: Stage 2: HTTP probing (depends on stage 1)
        // FR: Étape 2: Sondage HTTP (dépend de l'étape 1)
        PipelineStageConfig stage2;
        stage2.id = "httpxpp";
        stage2.name = "HTTP Prober";
        stage2.description = "Probe discovered subdomains for HTTP services";
        stage2.executable = "bbp-httpxpp";
        stage2.arguments = {"--in", "out/01_subdomains.csv", "--out", "out/02_probe.csv"};
        stage2.dependencies = {"subhunter"};
        stage2.priority = PipelineStagePriority::NORMAL;
        stage2.timeout = std::chrono::seconds(300);
        stage2.max_retries = 1;
        stage2.metadata["tags"] = "reconnaissance,active";
        stage2.metadata["category"] = "probing";
        test_stages_.push_back(stage2);
        
        // EN: Stage 3: Directory bruteforcing (depends on stage 2)
        // FR: Étape 3: Force brute de répertoires (dépend de l'étape 2)
        PipelineStageConfig stage3;
        stage3.id = "dirbff";
        stage3.name = "Directory Brute Forcer";
        stage3.description = "Brute force directories and files";
        stage3.executable = "bbp-dirbff";
        stage3.arguments = {"--in", "out/02_probe.csv", "--out", "out/04_discovery.csv"};
        stage3.dependencies = {"httpxpp"};
        stage3.priority = PipelineStagePriority::NORMAL;
        stage3.timeout = std::chrono::seconds(1200);
        stage3.max_retries = 0;
        stage3.allow_failure = true;
        stage3.metadata["tags"] = "discovery,active";
        stage3.metadata["category"] = "bruteforce";
        test_stages_.push_back(stage3);
        
        // EN: Stage 4: JavaScript intelligence (depends on stage 2)
        // FR: Étape 4: Intelligence JavaScript (dépend de l'étape 2)
        PipelineStageConfig stage4;
        stage4.id = "jsintel";
        stage4.name = "JavaScript Intelligence";
        stage4.description = "Analyze JavaScript files for endpoints and secrets";
        stage4.executable = "bbp-jsintel";
        stage4.arguments = {"--in", "out/02_probe.csv", "--out", "out/05_jsintel.csv"};
        stage4.dependencies = {"httpxpp"};
        stage4.priority = PipelineStagePriority::LOW;
        stage4.timeout = std::chrono::seconds(900);
        stage4.metadata["tags"] = "analysis,passive";
        stage4.metadata["category"] = "intelligence";
        test_stages_.push_back(stage4);
        
        // EN: Stage 5: API testing (depends on stages 3 and 4)
        // FR: Étape 5: Test d'API (dépend des étapes 3 et 4)
        PipelineStageConfig stage5;
        stage5.id = "apitester";
        stage5.name = "API Tester";
        stage5.description = "Test discovered API endpoints";
        stage5.executable = "bbp-apitester";
        stage5.arguments = {"--discovery", "out/04_discovery.csv", "--jsintel", "out/05_jsintel.csv", "--out", "out/07_api_findings.csv"};
        stage5.dependencies = {"dirbff", "jsintel"};
        stage5.priority = PipelineStagePriority::CRITICAL;
        stage5.timeout = std::chrono::seconds(1800);
        stage5.metadata["tags"] = "testing,active";
        stage5.metadata["category"] = "security";
        test_stages_.push_back(stage5);
        
        // EN: Stage 6: Aggregator (depends on all previous stages)
        // FR: Étape 6: Agrégateur (dépend de toutes les étapes précédentes)
        PipelineStageConfig stage6;
        stage6.id = "aggregator";
        stage6.name = "Results Aggregator";
        stage6.description = "Aggregate and rank all findings";
        stage6.executable = "bbp-aggregator";
        stage6.arguments = {"--inputs", "out/", "--out", "out/99_final_ranked.csv"};
        stage6.dependencies = {"subhunter", "httpxpp", "dirbff", "jsintel", "apitester"};
        stage6.priority = PipelineStagePriority::HIGH;
        stage6.timeout = std::chrono::seconds(300);
        stage6.metadata["tags"] = "aggregation,final";
        stage6.metadata["category"] = "reporting";
        test_stages_.push_back(stage6);
        
        // EN: Stage 7: Independent monitoring stage (no dependencies)
        // FR: Étape 7: Étape de surveillance indépendante (pas de dépendances)
        PipelineStageConfig stage7;
        stage7.id = "monitor";
        stage7.name = "Change Monitor";
        stage7.description = "Monitor for changes in target infrastructure";
        stage7.executable = "bbp-changes";
        stage7.arguments = {"--scope", "data/scope.csv", "--out", "out/09_changes.csv"};
        stage7.priority = PipelineStagePriority::LOW;
        stage7.timeout = std::chrono::seconds(600);
        stage7.allow_failure = true;
        stage7.metadata["tags"] = "monitoring,passive";
        stage7.metadata["category"] = "continuous";
        test_stages_.push_back(stage7);
    }
    
    // EN: Helper functions for tests
    // FR: Fonctions d'aide pour les tests
    StageSelectionConfig createBasicSelectionConfig() {
        StageSelectionConfig config;
        config.validation_level = StageValidationLevel::DEPENDENCIES;
        config.include_dependencies = true;
        config.resolve_conflicts = true;
        config.optimize_execution_order = true;
        return config;
    }
    
    StageSelectionFilter createIdFilter(const std::string& id) {
        return StageSelectorUtils::createIdFilter(id, true);
    }
    
    StageSelectionFilter createTagFilter(const std::set<std::string>& tags) {
        return StageSelectorUtils::createTagFilter(tags);
    }
    
    StageSelectionFilter createPriorityFilter(PipelineStagePriority min_priority) {
        return StageSelectorUtils::createPriorityFilter(min_priority, PipelineStagePriority::CRITICAL);
    }

protected:
    StageSelectorConfig config_;
    std::unique_ptr<StageSelector> selector_;
    std::vector<PipelineStageConfig> test_stages_;
};

// EN: Test stage selection by ID
// FR: Tester la sélection d'étapes par ID
TEST_F(StageSelectorTest, SelectStagesByIds) {
    // EN: Test selecting specific stages by ID
    // FR: Tester la sélection d'étapes spécifiques par ID
    std::vector<std::string> stage_ids = {"subhunter", "httpxpp"};
    
    auto result = selector_->selectStagesByIds(test_stages_, stage_ids, StageValidationLevel::BASIC);
    
    EXPECT_EQ(result.status, StageSelectionStatus::SUCCESS);
    EXPECT_EQ(result.selected_stage_ids.size(), 2);
    EXPECT_THAT(result.selected_stage_ids, UnorderedElementsAre("subhunter", "httpxpp"));
    EXPECT_GT(result.selection_time.count(), 0);
    EXPECT_EQ(result.total_available_stages, test_stages_.size());
}

// EN: Test stage selection by pattern
// FR: Tester la sélection d'étapes par motif
TEST_F(StageSelectorTest, SelectStagesByPattern) {
    // EN: Test pattern-based selection
    // FR: Tester la sélection basée sur des motifs
    auto result = selector_->selectStagesByPattern(test_stages_, ".*hunter.*", false);
    
    EXPECT_EQ(result.status, StageSelectionStatus::SUCCESS);
    EXPECT_EQ(result.selected_stage_ids.size(), 1);
    EXPECT_EQ(result.selected_stage_ids[0], "subhunter");
    
    // EN: Test with regex that matches multiple stages
    // FR: Tester avec regex qui correspond à plusieurs étapes
    auto result2 = selector_->selectStagesByPattern(test_stages_, "bbp-.*", true);
    
    EXPECT_EQ(result2.status, StageSelectionStatus::SUCCESS);
    EXPECT_GT(result2.selected_stage_ids.size(), 1);
}

// EN: Test dependency resolution
// FR: Tester la résolution des dépendances
TEST_F(StageSelectorTest, DependencyResolution) {
    // EN: Select a stage with dependencies and check if dependencies are included
    // FR: Sélectionner une étape avec dépendances et vérifier si les dépendances sont incluses
    std::vector<std::string> stage_ids = {"apitester"};
    
    auto result = selector_->selectStagesByIds(test_stages_, stage_ids, StageValidationLevel::DEPENDENCIES);
    
    EXPECT_EQ(result.status, StageSelectionStatus::SUCCESS);
    
    // EN: Should include all dependencies transitively
    // FR: Devrait inclure toutes les dépendances de manière transitive
    EXPECT_GE(result.selected_stage_ids.size(), 5); // apitester + its dependencies
    EXPECT_THAT(result.selected_stage_ids, Contains("subhunter"));
    EXPECT_THAT(result.selected_stage_ids, Contains("httpxpp"));
    EXPECT_THAT(result.selected_stage_ids, Contains("dirbff"));
    EXPECT_THAT(result.selected_stage_ids, Contains("jsintel"));
    EXPECT_THAT(result.selected_stage_ids, Contains("apitester"));
    
    // EN: Check execution order is valid
    // FR: Vérifier que l'ordre d'exécution est valide
    EXPECT_FALSE(result.execution_order.empty());
    
    // EN: subhunter should come before httpxpp
    // FR: subhunter devrait venir avant httpxpp
    auto subhunter_pos = std::find(result.execution_order.begin(), result.execution_order.end(), "subhunter");
    auto httpxpp_pos = std::find(result.execution_order.begin(), result.execution_order.end(), "httpxpp");
    EXPECT_LT(subhunter_pos, httpxpp_pos);
}

// EN: Test filtering by tags
// FR: Tester le filtrage par tags
TEST_F(StageSelectorTest, FilterByTags) {
    StageSelectionConfig config = createBasicSelectionConfig();
    config.filters.push_back(createTagFilter({"reconnaissance"}));
    
    auto result = selector_->selectStages(test_stages_, config);
    
    EXPECT_EQ(result.status, StageSelectionStatus::SUCCESS);
    EXPECT_GE(result.selected_stage_ids.size(), 2); // subhunter, httpxpp have reconnaissance tag
    
    // EN: All selected stages should have the reconnaissance tag
    // FR: Toutes les étapes sélectionnées devraient avoir le tag reconnaissance
    for (const auto& stage : result.selected_stages) {
        auto tags_it = stage.metadata.find("tags");
        ASSERT_NE(tags_it, stage.metadata.end());
        EXPECT_THAT(tags_it->second, HasSubstr("reconnaissance"));
    }
}

// EN: Test filtering by priority
// FR: Tester le filtrage par priorité
TEST_F(StageSelectorTest, FilterByPriority) {
    StageSelectionConfig config = createBasicSelectionConfig();
    config.filters.push_back(createPriorityFilter(PipelineStagePriority::HIGH));
    config.include_dependencies = false; // Don't auto-include dependencies for this test
    
    auto result = selector_->selectStages(test_stages_, config);
    
    EXPECT_EQ(result.status, StageSelectionStatus::SUCCESS);
    
    // EN: Should select stages with HIGH or CRITICAL priority
    // FR: Devrait sélectionner les étapes avec priorité HIGH ou CRITICAL
    for (const auto& stage : result.selected_stages) {
        EXPECT_GE(stage.priority, PipelineStagePriority::HIGH);
    }
}

// EN: Test circular dependency detection
// FR: Tester la détection de dépendances circulaires
TEST_F(StageSelectorTest, CircularDependencyDetection) {
    // EN: Create stages with circular dependency
    // FR: Créer des étapes avec dépendance circulaire
    std::vector<PipelineStageConfig> circular_stages;
    
    PipelineStageConfig stageA;
    stageA.id = "stageA";
    stageA.name = "Stage A";
    stageA.executable = "test-a";
    stageA.dependencies = {"stageB"};
    circular_stages.push_back(stageA);
    
    PipelineStageConfig stageB;
    stageB.id = "stageB";
    stageB.name = "Stage B";
    stageB.executable = "test-b";
    stageB.dependencies = {"stageA"};
    circular_stages.push_back(stageB);
    
    auto cycles = selector_->detectCircularDependencies(circular_stages);
    EXPECT_FALSE(cycles.empty());
    
    // EN: Test selection should fail due to circular dependency
    // FR: La sélection de test devrait échouer à cause de la dépendance circulaire
    StageSelectionConfig config = createBasicSelectionConfig();
    config.filters.push_back(createIdFilter("stageA"));
    
    auto result = selector_->selectStages(circular_stages, config);
    EXPECT_EQ(result.status, StageSelectionStatus::CIRCULAR_DEPENDENCY);
    EXPECT_FALSE(result.errors.empty());
}

// EN: Test stage validation
// FR: Tester la validation d'étapes
TEST_F(StageSelectorTest, StageValidation) {
    // EN: Test basic validation
    // FR: Tester la validation de base
    EXPECT_TRUE(selector_->validateStageSelection(test_stages_, StageValidationLevel::BASIC));
    EXPECT_TRUE(selector_->validateStageSelection(test_stages_, StageValidationLevel::DEPENDENCIES));
    
    // EN: Test validation with invalid stage
    // FR: Tester la validation avec une étape invalide
    std::vector<PipelineStageConfig> invalid_stages = test_stages_;
    invalid_stages[0].id = ""; // Invalid empty ID
    
    EXPECT_FALSE(selector_->validateStageSelection(invalid_stages, StageValidationLevel::BASIC));
}

// EN: Test compatibility analysis
// FR: Tester l'analyse de compatibilité
TEST_F(StageSelectorTest, CompatibilityAnalysis) {
    auto compatibility = selector_->analyzeStageCompatibility(test_stages_);
    
    EXPECT_TRUE(compatibility.are_compatible);
    EXPECT_GE(compatibility.compatibility_score, 0.8); // High compatibility expected
    EXPECT_EQ(compatibility.compatible_stages.size(), test_stages_.size());
    EXPECT_TRUE(compatibility.incompatible_stages.empty());
}

// EN: Test execution plan creation
// FR: Tester la création de plan d'exécution
TEST_F(StageSelectorTest, ExecutionPlanCreation) {
    PipelineExecutionConfig exec_config;
    exec_config.execution_mode = PipelineExecutionMode::HYBRID;
    
    auto plan = selector_->createExecutionPlan(test_stages_, exec_config);
    
    EXPECT_FALSE(plan.plan_id.empty());
    EXPECT_TRUE(plan.is_valid);
    EXPECT_EQ(plan.stages.size(), test_stages_.size());
    EXPECT_EQ(plan.execution_order.size(), test_stages_.size());
    EXPECT_GT(plan.estimated_total_time.count(), 0);
    EXPECT_FALSE(plan.critical_path.empty());
    
    // EN: Verify execution order respects dependencies
    // FR: Vérifier que l'ordre d'exécution respecte les dépendances
    std::map<std::string, size_t> stage_positions;
    for (size_t i = 0; i < plan.execution_order.size(); ++i) {
        stage_positions[plan.execution_order[i]] = i;
    }
    
    for (const auto& stage : plan.stages) {
        for (const auto& dep : stage.dependencies) {
            EXPECT_LT(stage_positions[dep], stage_positions[stage.id])
                << "Dependency " << dep << " should come before " << stage.id;
        }
    }
}

// EN: Test parallel execution groups
// FR: Tester les groupes d'exécution parallèle
TEST_F(StageSelectorTest, ParallelExecutionGroups) {
    auto parallel_groups = selector_->identifyParallelExecutionGroups(test_stages_);
    
    EXPECT_FALSE(parallel_groups.empty());
    
    // EN: First group should contain only subhunter (no dependencies)
    // FR: Le premier groupe devrait contenir seulement subhunter (pas de dépendances)
    EXPECT_THAT(parallel_groups[0], Contains("subhunter"));
    
    // EN: Verify no stage appears in multiple groups
    // FR: Vérifier qu'aucune étape n'apparaît dans plusieurs groupes
    std::set<std::string> all_stages;
    for (const auto& group : parallel_groups) {
        for (const auto& stage_id : group) {
            EXPECT_TRUE(all_stages.insert(stage_id).second)
                << "Stage " << stage_id << " appears in multiple parallel groups";
        }
    }
}

// EN: Test constraint checking
// FR: Tester la vérification des contraintes
TEST_F(StageSelectorTest, ConstraintChecking) {
    auto& stage = test_stages_[0]; // subhunter
    
    // EN: Test allowed constraints
    // FR: Tester les contraintes autorisées
    std::vector<StageExecutionConstraint> allowed = {
        StageExecutionConstraint::NETWORK_DEPENDENT,
        StageExecutionConstraint::FILESYSTEM_DEPENDENT,
        StageExecutionConstraint::PARALLEL_SAFE
    };
    
    EXPECT_TRUE(selector_->checkStageConstraints(stage, allowed, {}));
    
    // EN: Test forbidden constraints
    // FR: Tester les contraintes interdites
    std::vector<StageExecutionConstraint> forbidden = {
        StageExecutionConstraint::EXCLUSIVE_ACCESS
    };
    
    EXPECT_TRUE(selector_->checkStageConstraints(stage, {}, forbidden));
    
    // EN: Test constraint inference
    // FR: Tester l'inférence de contraintes
    auto inferred = selector_->inferStageConstraints(stage);
    EXPECT_FALSE(inferred.empty());
}

// EN: Test resource usage estimation
// FR: Tester l'estimation d'utilisation des ressources
TEST_F(StageSelectorTest, ResourceUsageEstimation) {
    auto& stage = test_stages_[0]; // subhunter with HIGH priority
    
    auto cpu_usage = selector_->estimateStageResourceUsage(stage, "cpu");
    EXPECT_GT(cpu_usage, 0.0);
    
    auto memory_usage = selector_->estimateStageResourceUsage(stage, "memory");
    EXPECT_GT(memory_usage, 0.0);
    
    auto network_usage = selector_->estimateStageResourceUsage(stage, "network");
    EXPECT_GE(network_usage, 0.0); // May be 0 for non-network stages
    
    auto disk_usage = selector_->estimateStageResourceUsage(stage, "disk");
    EXPECT_GE(disk_usage, 0.0);
}

// EN: Test execution time estimation
// FR: Tester l'estimation de temps d'exécution
TEST_F(StageSelectorTest, ExecutionTimeEstimation) {
    auto& stage = test_stages_[0]; // subhunter with 600s timeout
    
    auto estimated_time = selector_->estimateStageExecutionTime(stage);
    EXPECT_EQ(estimated_time, std::chrono::milliseconds(600000)); // 600 seconds in ms
}

// EN: Test metadata extraction
// FR: Tester l'extraction de métadonnées
TEST_F(StageSelectorTest, MetadataExtraction) {
    auto& stage = test_stages_[0]; // subhunter
    
    auto metadata = selector_->extractStageMetadata(stage);
    
    EXPECT_FALSE(metadata.empty());
    EXPECT_EQ(metadata["estimated_duration"], "600s");
    EXPECT_EQ(metadata["priority"], std::to_string(static_cast<int>(PipelineStagePriority::HIGH)));
    EXPECT_EQ(metadata["dependencies_count"], "0");
    EXPECT_EQ(metadata["has_retries"], "true");
    EXPECT_THAT(metadata, Contains(Key("tags")));
}

// EN: Test caching functionality
// FR: Tester la fonctionnalité de cache
TEST_F(StageSelectorTest, CachingFunctionality) {
    StageSelectionConfig config = createBasicSelectionConfig();
    config.enable_caching = true;
    config.filters.push_back(createIdFilter("subhunter"));
    
    // EN: First selection should not be cached
    // FR: La première sélection ne devrait pas être en cache
    auto result1 = selector_->selectStages(test_stages_, config);
    EXPECT_EQ(result1.status, StageSelectionStatus::SUCCESS);
    
    // EN: Second identical selection should be faster (cached)
    // FR: La seconde sélection identique devrait être plus rapide (en cache)
    auto start = std::chrono::high_resolution_clock::now();
    auto result2 = selector_->selectStages(test_stages_, config);
    auto end = std::chrono::high_resolution_clock::now();
    [[maybe_unused]] auto cached_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_EQ(result2.status, StageSelectionStatus::SUCCESS);
    EXPECT_EQ(result1.selected_stage_ids, result2.selected_stage_ids);
    
    // EN: Clear cache and test
    // FR: Vider le cache et tester
    selector_->clearCache();
    
    // EN: Should work after cache clear
    // FR: Devrait fonctionner après vidage du cache
    auto result3 = selector_->selectStages(test_stages_, config);
    EXPECT_EQ(result3.status, StageSelectionStatus::SUCCESS);
}

// EN: Test statistics collection
// FR: Tester la collecte de statistiques
TEST_F(StageSelectorTest, StatisticsCollection) {
    // EN: Get initial statistics
    // FR: Obtenir les statistiques initiales
    auto initial_stats = selector_->getStatistics();
    EXPECT_EQ(initial_stats.total_selections, 0);
    
    // EN: Perform some selections
    // FR: Effectuer quelques sélections
    StageSelectionConfig config = createBasicSelectionConfig();
    config.filters.push_back(createIdFilter("subhunter"));
    
    selector_->selectStages(test_stages_, config);
    selector_->selectStages(test_stages_, config); // Second call should hit cache
    
    // EN: Check updated statistics
    // FR: Vérifier les statistiques mises à jour
    auto updated_stats = selector_->getStatistics();
    EXPECT_EQ(updated_stats.total_selections, 2);
    EXPECT_EQ(updated_stats.successful_selections, 2);
    EXPECT_EQ(updated_stats.failed_selections, 0);
    EXPECT_GE(updated_stats.cached_selections, 1); // At least one cached
    EXPECT_GT(updated_stats.avg_selection_time.count(), 0);
    
    // EN: Test statistics reset
    // FR: Tester la remise à zéro des statistiques
    selector_->resetStatistics();
    auto reset_stats = selector_->getStatistics();
    EXPECT_EQ(reset_stats.total_selections, 0);
}

// EN: Test event handling
// FR: Tester la gestion d'événements
TEST_F(StageSelectorTest, EventHandling) {
    std::vector<StageSelectorEvent> received_events;
    
    // EN: Set up event callback
    // FR: Configurer le callback d'événement
    selector_->setEventCallback([&received_events](const StageSelectorEvent& event) {
        received_events.push_back(event);
    });
    
    // EN: Perform selection to trigger events
    // FR: Effectuer une sélection pour déclencher des événements
    StageSelectionConfig config = createBasicSelectionConfig();
    config.filters.push_back(createIdFilter("subhunter"));
    
    selector_->selectStages(test_stages_, config);
    
    // EN: Check that events were received
    // FR: Vérifier que des événements ont été reçus
    EXPECT_FALSE(received_events.empty());
    
    // EN: Should have at least SELECTION_STARTED and SELECTION_COMPLETED events
    // FR: Devrait avoir au moins les événements SELECTION_STARTED et SELECTION_COMPLETED
    bool has_started = false, has_completed = false;
    for (const auto& event : received_events) {
        if (event.type == StageSelectorEventType::SELECTION_STARTED) has_started = true;
        if (event.type == StageSelectorEventType::SELECTION_COMPLETED) has_completed = true;
    }
    
    EXPECT_TRUE(has_started);
    EXPECT_TRUE(has_completed);
    
    // EN: Remove event callback
    // FR: Supprimer le callback d'événement
    selector_->removeEventCallback();
}

// EN: Test import/export functionality
// FR: Tester la fonctionnalité d'import/export
TEST_F(StageSelectorTest, ImportExportFunctionality) {
    // EN: Create test output directory
    // FR: Créer le répertoire de sortie de test
    std::filesystem::create_directories("test_output");
    
    // EN: Perform selection and export result
    // FR: Effectuer une sélection et exporter le résultat
    StageSelectionConfig config = createBasicSelectionConfig();
    config.filters.push_back(createIdFilter("subhunter"));
    
    auto result = selector_->selectStages(test_stages_, config);
    EXPECT_EQ(result.status, StageSelectionStatus::SUCCESS);
    
    std::string result_file = "test_output/selection_result.json";
    EXPECT_TRUE(selector_->exportSelectionResult(result, result_file));
    EXPECT_TRUE(std::filesystem::exists(result_file));
    
    // EN: Import the result back
    // FR: Réimporter le résultat
    auto imported_result = selector_->importSelectionResult(result_file);
    ASSERT_TRUE(imported_result.has_value());
    EXPECT_EQ(imported_result->status, result.status);
    EXPECT_EQ(imported_result->selected_stage_ids, result.selected_stage_ids);
    
    // EN: Test execution plan export/import
    // FR: Tester l'export/import de plan d'exécution
    auto plan = selector_->createExecutionPlan(test_stages_);
    
    std::string plan_file = "test_output/execution_plan.json";
    EXPECT_TRUE(selector_->exportExecutionPlan(plan, plan_file));
    EXPECT_TRUE(std::filesystem::exists(plan_file));
    
    auto imported_plan = selector_->importExecutionPlan(plan_file);
    ASSERT_TRUE(imported_plan.has_value());
    EXPECT_EQ(imported_plan->plan_id, plan.plan_id);
    EXPECT_EQ(imported_plan->execution_order, plan.execution_order);
    EXPECT_EQ(imported_plan->is_valid, plan.is_valid);
}

// EN: Test async selection operations
// FR: Tester les opérations de sélection asynchrones
TEST_F(StageSelectorTest, AsyncSelectionOperations) {
    StageSelectionConfig config = createBasicSelectionConfig();
    config.filters.push_back(createIdFilter("subhunter"));
    
    // EN: Test async selection
    // FR: Tester la sélection asynchrone
    auto future_result = selector_->selectStagesAsync(test_stages_, config);
    
    // EN: Wait for completion
    // FR: Attendre la completion
    auto result = future_result.get();
    EXPECT_EQ(result.status, StageSelectionStatus::SUCCESS);
    EXPECT_EQ(result.selected_stage_ids.size(), 1);
    EXPECT_EQ(result.selected_stage_ids[0], "subhunter");
}

// EN: Test empty selection scenarios
// FR: Tester les scénarios de sélection vide
TEST_F(StageSelectorTest, EmptySelectionScenarios) {
    StageSelectionConfig config = createBasicSelectionConfig();
    
    // EN: Filter that matches nothing
    // FR: Filtre qui ne correspond à rien
    config.filters.push_back(createIdFilter("nonexistent_stage"));
    
    auto result = selector_->selectStages(test_stages_, config);
    EXPECT_EQ(result.status, StageSelectionStatus::EMPTY_SELECTION);
    EXPECT_TRUE(result.selected_stage_ids.empty());
    EXPECT_FALSE(result.warnings.empty());
}

// EN: Test configuration validation
// FR: Tester la validation de configuration
TEST_F(StageSelectorTest, ConfigurationValidation) {
    // EN: Test invalid configuration
    // FR: Tester une configuration invalide
    StageSelectionConfig invalid_config;
    invalid_config.max_selected_stages = 0; // Invalid
    invalid_config.selection_timeout = std::chrono::seconds(-1); // Invalid
    
    auto validation_errors = StageSelectorUtils::validateSelectionConfig(invalid_config);
    EXPECT_FALSE(validation_errors.empty());
    EXPECT_THAT(validation_errors, Contains(HasSubstr("max_selected_stages cannot be zero")));
    EXPECT_THAT(validation_errors, Contains(HasSubstr("selection_timeout must be positive")));
}

// EN: Test utility functions
// FR: Tester les fonctions utilitaires
TEST_F(StageSelectorTest, UtilityFunctions) {
    // EN: Test enum string conversions
    // FR: Tester les conversions de chaînes d'enum
    EXPECT_EQ(StageSelectorUtils::criteriaToString(StageSelectionCriteria::BY_ID), "BY_ID");
    EXPECT_EQ(StageSelectorUtils::stringToCriteria("BY_NAME"), StageSelectionCriteria::BY_NAME);
    
    EXPECT_EQ(StageSelectorUtils::constraintToString(StageExecutionConstraint::PARALLEL_SAFE), "PARALLEL_SAFE");
    EXPECT_EQ(StageSelectorUtils::stringToConstraint("CPU_INTENSIVE"), StageExecutionConstraint::CPU_INTENSIVE);
    
    EXPECT_EQ(StageSelectorUtils::selectionStatusToString(StageSelectionStatus::SUCCESS), "SUCCESS");
    
    // EN: Test validation utilities
    // FR: Tester les utilitaires de validation
    EXPECT_TRUE(StageSelectorUtils::isValidStageId("valid_stage_id"));
    EXPECT_FALSE(StageSelectorUtils::isValidStageId(""));
    EXPECT_FALSE(StageSelectorUtils::isValidStageId("invalid stage id")); // spaces not allowed
    
    EXPECT_TRUE(StageSelectorUtils::isValidPattern(".*test.*"));
    EXPECT_FALSE(StageSelectorUtils::isValidPattern("[(invalid)"));
    
    // EN: Test constraint compatibility
    // FR: Tester la compatibilité des contraintes
    EXPECT_TRUE(StageSelectorUtils::areConstraintsCompatible(
        StageExecutionConstraint::CPU_INTENSIVE,
        StageExecutionConstraint::MEMORY_INTENSIVE
    ));
    
    EXPECT_FALSE(StageSelectorUtils::areConstraintsCompatible(
        StageExecutionConstraint::SEQUENTIAL_ONLY,
        StageExecutionConstraint::PARALLEL_SAFE
    ));
    
    // EN: Test filter creation utilities
    // FR: Tester les utilitaires de création de filtres
    auto id_filter = StageSelectorUtils::createIdFilter("test_id");
    EXPECT_EQ(id_filter.criteria, StageSelectionCriteria::BY_ID);
    EXPECT_EQ(id_filter.value, "test_id");
    EXPECT_TRUE(id_filter.exact_match);
    
    auto tag_filter = StageSelectorUtils::createTagFilter({"tag1", "tag2"});
    EXPECT_EQ(tag_filter.criteria, StageSelectionCriteria::BY_TAG);
    EXPECT_EQ(tag_filter.tags.size(), 2);
    
    auto priority_filter = StageSelectorUtils::createPriorityFilter(PipelineStagePriority::HIGH);
    EXPECT_EQ(priority_filter.criteria, StageSelectionCriteria::BY_PRIORITY);
    EXPECT_EQ(priority_filter.min_priority, PipelineStagePriority::HIGH);
}

// EN: Test performance and bottleneck identification
// FR: Tester l'identification des performances et goulots d'étranglement
TEST_F(StageSelectorTest, PerformanceAnalysis) {
    // EN: Test bottleneck identification
    // FR: Tester l'identification des goulots d'étranglement
    auto bottlenecks = StageSelectorUtils::identifyBottleneckStages(test_stages_);
    EXPECT_FALSE(bottlenecks.empty());
    
    // EN: Should identify stages with long execution times or many dependencies
    // FR: Devrait identifier les étapes avec des temps d'exécution longs ou beaucoup de dépendances
    bool found_timeout_bottleneck = false;
    bool found_dependency_bottleneck = false;
    
    for (const auto& bottleneck : bottlenecks) {
        if (bottleneck.find("long execution time") != std::string::npos) {
            found_timeout_bottleneck = true;
        }
        if (bottleneck.find("many dependencies") != std::string::npos) {
            found_dependency_bottleneck = true;
        }
    }
    
    // EN: At least one type of bottleneck should be found
    // FR: Au moins un type de goulot d'étranglement devrait être trouvé
    EXPECT_TRUE(found_timeout_bottleneck || found_dependency_bottleneck);
}

// EN: Test report generation
// FR: Tester la génération de rapports
TEST_F(StageSelectorTest, ReportGeneration) {
    StageSelectionConfig config = createBasicSelectionConfig();
    config.filters.push_back(createIdFilter("subhunter"));
    
    auto result = selector_->selectStages(test_stages_, config);
    EXPECT_EQ(result.status, StageSelectionStatus::SUCCESS);
    
    // EN: Test selection report generation
    // FR: Tester la génération de rapport de sélection
    auto report = StageSelectorUtils::generateSelectionReport(result);
    EXPECT_FALSE(report.empty());
    EXPECT_THAT(report, HasSubstr("Stage Selection Report"));
    EXPECT_THAT(report, HasSubstr("Status: SUCCESS"));
    EXPECT_THAT(report, HasSubstr("Selected Stages:"));
    EXPECT_THAT(report, HasSubstr("subhunter"));
    
    // EN: Test execution plan report generation
    // FR: Tester la génération de rapport de plan d'exécution
    auto plan = selector_->createExecutionPlan(test_stages_);
    auto plan_report = StageSelectorUtils::generateExecutionPlanReport(plan);
    EXPECT_FALSE(plan_report.empty());
    EXPECT_THAT(plan_report, HasSubstr("Execution Plan Report"));
    EXPECT_THAT(plan_report, HasSubstr("Plan ID:"));
    EXPECT_THAT(plan_report, HasSubstr("Execution Order:"));
}

// EN: Test health and status monitoring
// FR: Tester la surveillance de santé et statut
TEST_F(StageSelectorTest, HealthAndStatusMonitoring) {
    EXPECT_TRUE(selector_->isHealthy());
    
    auto status = selector_->getStatus();
    EXPECT_FALSE(status.empty());
    EXPECT_THAT(status, HasSubstr("StageSelector:"));
    EXPECT_THAT(status, HasSubstr("selections"));
}

// EN: Test concurrent selections
// FR: Tester les sélections concurrentes
TEST_F(StageSelectorTest, ConcurrentSelections) {
    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::vector<StageSelectionResult> results(num_threads);
    
    StageSelectionConfig config = createBasicSelectionConfig();
    config.filters.push_back(createIdFilter("subhunter"));
    
    // EN: Launch concurrent selections
    // FR: Lancer des sélections concurrentes
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, &config, &results, i]() {
            results[i] = selector_->selectStages(test_stages_, config);
        });
    }
    
    // EN: Wait for all threads to complete
    // FR: Attendre que tous les threads se terminent
    for (auto& thread : threads) {
        thread.join();
    }
    
    // EN: Verify all selections succeeded
    // FR: Vérifier que toutes les sélections ont réussi
    for (const auto& result : results) {
        EXPECT_EQ(result.status, StageSelectionStatus::SUCCESS);
        EXPECT_EQ(result.selected_stage_ids.size(), 1);
        EXPECT_EQ(result.selected_stage_ids[0], "subhunter");
    }
    
    // EN: Check that statistics reflect concurrent operations
    // FR: Vérifier que les statistiques reflètent les opérations concurrentes
    auto stats = selector_->getStatistics();
    EXPECT_GE(stats.total_selections, num_threads);
}

// EN: Test error handling and edge cases
// FR: Tester la gestion d'erreurs et les cas limites
TEST_F(StageSelectorTest, ErrorHandlingAndEdgeCases) {
    // EN: Test with empty stage list
    // FR: Tester avec une liste d'étapes vide
    std::vector<PipelineStageConfig> empty_stages;
    StageSelectionConfig config = createBasicSelectionConfig();
    
    auto result = selector_->selectStages(empty_stages, config);
    EXPECT_EQ(result.status, StageSelectionStatus::EMPTY_SELECTION);
    
    // EN: Test with invalid regex pattern
    // FR: Tester avec un motif regex invalide
    auto invalid_result = selector_->selectStagesByPattern(test_stages_, "[(invalid", false);
    EXPECT_EQ(invalid_result.status, StageSelectionStatus::CONFIGURATION_ERROR);
    EXPECT_FALSE(invalid_result.errors.empty());
    
    // EN: Test with conflicting filters
    // FR: Tester avec des filtres conflictuels
    StageSelectionConfig conflict_config;
    conflict_config.filters.push_back(createIdFilter("stage1"));
    
    StageSelectionFilter exclude_filter;
    exclude_filter.criteria = StageSelectionCriteria::BY_ID;
    exclude_filter.mode = StageFilterMode::EXCLUDE;
    exclude_filter.value = "stage1";
    conflict_config.filters.push_back(exclude_filter);
    
    auto conflict_result = selector_->selectStages(test_stages_, conflict_config);
    // EN: Should handle conflicting filters gracefully
    // FR: Devrait gérer les filtres conflictuels avec grâce
}

// EN: Main function for running tests
// FR: Fonction principale pour exécuter les tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}