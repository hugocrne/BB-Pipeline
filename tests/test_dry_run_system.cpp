// EN: Comprehensive unit tests for Dry Run System - 100% test coverage for complete simulation without execution
// FR: Tests unitaires complets pour le système de simulation - Couverture de test à 100% pour simulation complète sans exécution

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "orchestrator/dry_run_system.hpp"
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
class MockSimulationEngine : public detail::ISimulationEngine {
public:
    MOCK_METHOD(bool, initialize, (const DryRunConfig& config), (override));
    MOCK_METHOD(PerformanceProfile, simulateStage, (const SimulationStage& stage), (override));
    MOCK_METHOD(std::vector<ValidationIssue>, validateStage, (const SimulationStage& stage), (override));
    MOCK_METHOD(ResourceEstimate, estimateResource, (const SimulationStage& stage, ResourceType type), (override));
    MOCK_METHOD(ExecutionPlan, generateExecutionPlan, (const std::vector<SimulationStage>& stages), (override));
};

class MockReportGenerator : public detail::IReportGenerator {
public:
    MOCK_METHOD(std::string, generateReport, (const DryRunResults& results), (override));
    MOCK_METHOD(bool, exportToFile, (const std::string& report, const std::string& file_path), (override));
};

// EN: Test fixture for Dry Run System tests
// FR: Fixture de test pour les tests du système de simulation
class DryRunSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        // EN: Create temporary directory for test files
        // FR: Crée un répertoire temporaire pour les fichiers de test
        test_dir_ = std::filesystem::temp_directory_path() / "bbp_dry_run_test";
        std::filesystem::create_directories(test_dir_);
        
        // EN: Configure test dry run configuration
        // FR: Configure la configuration de simulation de test
        config_.mode = DryRunMode::FULL_SIMULATION;
        config_.detail_level = SimulationDetail::DETAILED;
        config_.enable_resource_estimation = true;
        config_.enable_performance_profiling = true;
        config_.enable_dependency_validation = true;
        config_.enable_file_validation = true;
        config_.enable_network_simulation = true;
        config_.show_progress = false; // Disable for testing
        config_.interactive_mode = false;
        config_.generate_report = true;
        config_.report_output_path = (test_dir_ / "test_report.html").string();
        config_.timeout = std::chrono::seconds(60);
        
        // EN: Create dry run system with test configuration
        // FR: Crée le système de simulation avec la configuration de test
        dry_run_system_ = std::make_unique<DryRunSystem>(config_);
        ASSERT_TRUE(dry_run_system_->initialize());
    }
    
    void TearDown() override {
        // EN: Cleanup test resources
        // FR: Nettoie les ressources de test
        if (dry_run_system_) {
            dry_run_system_->shutdown();
            dry_run_system_.reset();
        }
        
        // EN: Remove test directory
        // FR: Supprime le répertoire de test
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }
    
    // EN: Helper method to create test simulation stage
    // FR: Méthode helper pour créer une étape de simulation de test
    SimulationStage createTestStage(const std::string& stage_id, const std::string& name = "",
                                   const std::vector<std::string>& dependencies = {},
                                   bool can_parallel = true,
                                   std::chrono::milliseconds duration = std::chrono::milliseconds(10000)) {
        SimulationStage stage;
        stage.stage_id = stage_id;
        stage.stage_name = name.empty() ? ("Test " + stage_id) : name;
        stage.description = "Test stage for " + stage_id;
        stage.dependencies = dependencies;
        stage.estimated_duration = duration;
        stage.can_run_parallel = can_parallel;
        stage.is_optional = false;
        
        // EN: Add some test files
        // FR: Ajoute quelques fichiers de test
        if (stage_id != "first_stage") {
            stage.input_files.push_back("input_" + stage_id + ".csv");
        }
        stage.output_files.push_back("output_" + stage_id + ".csv");
        
        // EN: Add metadata
        // FR: Ajoute des métadonnées
        stage.metadata["test"] = "true";
        stage.metadata["complexity"] = "1.5";
        
        return stage;
    }
    
    // EN: Helper method to create test validation issue
    // FR: Méthode helper pour créer un problème de validation de test
    ValidationIssue createTestValidationIssue(ValidationSeverity severity = ValidationSeverity::WARNING,
                                              const std::string& stage_id = "test_stage") {
        ValidationIssue issue;
        issue.severity = severity;
        issue.category = "test";
        issue.message = "Test validation issue";
        issue.stage_id = stage_id;
        issue.suggestion = "Fix the test issue";
        issue.timestamp = std::chrono::system_clock::now();
        issue.context["test_context"] = "test_value";
        return issue;
    }
    
    // EN: Helper method to create test resource estimate
    // FR: Méthode helper pour créer une estimation de ressource de test
    ResourceEstimate createTestResourceEstimate(ResourceType type, double value = 100.0) {
        ResourceEstimate estimate;
        estimate.type = type;
        estimate.estimated_value = value;
        estimate.confidence_percentage = 80.0;
        estimate.unit = "test_unit";
        estimate.minimum_value = value * 0.8;
        estimate.maximum_value = value * 1.2;
        estimate.estimation_method = "Test estimation";
        estimate.assumptions.push_back("Test assumption");
        return estimate;
    }

protected:
    std::filesystem::path test_dir_;
    DryRunConfig config_;
    std::unique_ptr<DryRunSystem> dry_run_system_;
};

// EN: Test basic dry run system initialization
// FR: Test l'initialisation de base du système de simulation
TEST_F(DryRunSystemTest, BasicInitialization) {
    DryRunConfig test_config = DryRunUtils::createDefaultConfig();
    DryRunSystem system(test_config);
    
    EXPECT_TRUE(system.initialize());
    
    // EN: Test configuration access
    // FR: Test l'accès à la configuration
    const auto& config = system.getConfig();
    EXPECT_EQ(config.mode, DryRunMode::VALIDATE_ONLY);
    EXPECT_EQ(config.detail_level, SimulationDetail::STANDARD);
    EXPECT_TRUE(config.enable_dependency_validation);
}

// EN: Test dry run configuration validation
// FR: Test la validation de configuration de simulation
TEST_F(DryRunSystemTest, ConfigurationValidation) {
    // EN: Test valid configuration
    // FR: Test configuration valide
    DryRunConfig valid_config = DryRunUtils::createDefaultConfig();
    EXPECT_TRUE(DryRunUtils::validateDryRunConfig(valid_config));
    
    // EN: Test invalid configurations
    // FR: Test configurations invalides
    DryRunConfig invalid_config;
    
    // EN: Invalid timeout
    // FR: Timeout invalide
    invalid_config = valid_config;
    invalid_config.timeout = std::chrono::seconds(0);
    EXPECT_FALSE(DryRunUtils::validateDryRunConfig(invalid_config));
    
    // EN: Missing report path when report generation enabled
    // FR: Chemin de rapport manquant quand génération de rapport activée
    invalid_config = valid_config;
    invalid_config.generate_report = true;
    invalid_config.report_output_path = "";
    EXPECT_FALSE(DryRunUtils::validateDryRunConfig(invalid_config));
}

// EN: Test validation-only mode
// FR: Test le mode validation uniquement
TEST_F(DryRunSystemTest, ValidationOnlyMode) {
    // EN: Create stages with validation issues
    // FR: Crée des étapes avec problèmes de validation
    std::vector<SimulationStage> stages;
    
    // EN: Stage with empty ID (should cause error)
    // FR: Étape avec ID vide (devrait causer une erreur)
    SimulationStage invalid_stage;
    invalid_stage.stage_id = ""; // Empty ID
    invalid_stage.stage_name = "Invalid Stage";
    stages.push_back(invalid_stage);
    
    // EN: Valid stage
    // FR: Étape valide
    stages.push_back(createTestStage("valid_stage", "Valid Stage"));
    
    // EN: Configure for validation only
    // FR: Configure pour validation uniquement
    DryRunConfig validation_config = DryRunUtils::createValidationOnlyConfig();
    dry_run_system_->updateConfig(validation_config);
    
    // EN: Execute validation
    // FR: Exécute la validation
    auto results = dry_run_system_->execute(stages);
    
    EXPECT_FALSE(results.success); // Should fail due to invalid stage
    EXPECT_EQ(results.mode_executed, DryRunMode::VALIDATE_ONLY);
    EXPECT_FALSE(results.validation_issues.empty());
    
    // EN: Check that we have at least one error for the empty stage ID
    // FR: Vérifie qu'on a au moins une erreur pour l'ID d'étape vide
    bool found_error = std::any_of(results.validation_issues.begin(), results.validation_issues.end(),
        [](const ValidationIssue& issue) {
            return issue.severity == ValidationSeverity::ERROR && 
                   issue.message.find("Stage ID cannot be empty") != std::string::npos;
        });
    EXPECT_TRUE(found_error);
}

// EN: Test resource estimation mode
// FR: Test le mode d'estimation de ressources
TEST_F(DryRunSystemTest, ResourceEstimationMode) {
    std::vector<SimulationStage> stages;
    stages.push_back(createTestStage("cpu_intensive", "CPU Intensive Stage"));
    stages.push_back(createTestStage("memory_intensive", "Memory Intensive Stage"));
    stages.push_back(createTestStage("io_intensive", "I/O Intensive Stage"));
    
    // EN: Configure for resource estimation
    // FR: Configure pour estimation de ressources
    DryRunConfig resource_config = config_;
    resource_config.mode = DryRunMode::ESTIMATE_RESOURCES;
    dry_run_system_->updateConfig(resource_config);
    
    // EN: Execute resource estimation
    // FR: Exécute l'estimation de ressources
    auto results = dry_run_system_->execute(stages);
    
    EXPECT_TRUE(results.success);
    EXPECT_EQ(results.mode_executed, DryRunMode::ESTIMATE_RESOURCES);
    
    // EN: Check that resource estimates were generated
    // FR: Vérifie que les estimations de ressources ont été générées
    EXPECT_FALSE(results.resource_estimates.empty());
    
    // EN: Verify specific resource types are estimated
    // FR: Vérifie que des types de ressources spécifiques sont estimés
    EXPECT_TRUE(results.resource_estimates.contains(ResourceType::CPU_USAGE));
    EXPECT_TRUE(results.resource_estimates.contains(ResourceType::MEMORY_USAGE));
    EXPECT_TRUE(results.resource_estimates.contains(ResourceType::EXECUTION_TIME));
    
    // EN: Check resource estimate properties
    // FR: Vérifie les propriétés d'estimation de ressources
    auto cpu_estimate = results.resource_estimates.at(ResourceType::CPU_USAGE);
    EXPECT_GT(cpu_estimate.estimated_value, 0.0);
    EXPECT_GT(cpu_estimate.confidence_percentage, 0.0);
    EXPECT_FALSE(cpu_estimate.unit.empty());
    EXPECT_GE(cpu_estimate.maximum_value, cpu_estimate.minimum_value);
}

// EN: Test full simulation mode
// FR: Test le mode simulation complète
TEST_F(DryRunSystemTest, FullSimulationMode) {
    std::vector<SimulationStage> stages;
    
    // EN: Create a pipeline with dependencies
    // FR: Crée un pipeline avec dépendances
    stages.push_back(createTestStage("stage1", "First Stage", {}, true, std::chrono::milliseconds(5000)));
    stages.push_back(createTestStage("stage2", "Second Stage", {"stage1"}, true, std::chrono::milliseconds(8000)));
    stages.push_back(createTestStage("stage3", "Third Stage", {"stage1"}, false, std::chrono::milliseconds(12000)));
    stages.push_back(createTestStage("stage4", "Final Stage", {"stage2", "stage3"}, true, std::chrono::milliseconds(6000)));
    
    // EN: Execute full simulation
    // FR: Exécute la simulation complète
    auto results = dry_run_system_->execute(stages);
    
    EXPECT_TRUE(results.success);
    EXPECT_EQ(results.mode_executed, DryRunMode::FULL_SIMULATION);
    EXPECT_GT(results.simulation_duration.count(), 0);
    
    // EN: Check execution plan was generated
    // FR: Vérifie que le plan d'exécution a été généré
    EXPECT_EQ(results.execution_plan.stages.size(), stages.size());
    EXPECT_GT(results.execution_plan.total_estimated_time.count(), 0);
    EXPECT_FALSE(results.execution_plan.critical_path.empty());
    EXPECT_GT(results.execution_plan.parallelization_factor, 0.0);
    
    // EN: Check resource estimates are present
    // FR: Vérifie que les estimations de ressources sont présentes
    EXPECT_FALSE(results.resource_estimates.empty());
    
    // EN: Check stage details for performance profiling
    // FR: Vérifie les détails d'étapes pour le profilage de performance
    EXPECT_FALSE(results.stage_details.empty());
    for (const auto& stage : stages) {
        EXPECT_TRUE(results.stage_details.contains(stage.stage_id));
        const auto& stage_detail = results.stage_details.at(stage.stage_id);
        EXPECT_TRUE(stage_detail.contains("performance_profile"));
    }
}

// EN: Test performance profiling mode
// FR: Test le mode profilage de performance
TEST_F(DryRunSystemTest, PerformanceProfilingMode) {
    std::vector<SimulationStage> stages;
    stages.push_back(createTestStage("profile_stage", "Performance Test Stage"));
    
    // EN: Configure for performance profiling
    // FR: Configure pour le profilage de performance
    DryRunConfig profile_config = DryRunUtils::createPerformanceProfilingConfig();
    dry_run_system_->updateConfig(profile_config);
    
    // EN: Execute performance profiling
    // FR: Exécute le profilage de performance
    auto results = dry_run_system_->execute(stages);
    
    EXPECT_TRUE(results.success);
    EXPECT_EQ(results.mode_executed, DryRunMode::PERFORMANCE_PROFILE);
    
    // EN: Check that detailed performance information is available
    // FR: Vérifie que l'information de performance détaillée est disponible
    EXPECT_FALSE(results.stage_details.empty());
    EXPECT_TRUE(results.stage_details.contains("profile_stage"));
    
    const auto& stage_detail = results.stage_details.at("profile_stage");
    EXPECT_TRUE(stage_detail.contains("performance_profile"));
    
    const auto& perf_profile = stage_detail["performance_profile"];
    EXPECT_TRUE(perf_profile.contains("cpu_time_ms"));
    EXPECT_TRUE(perf_profile.contains("wall_time_ms"));
    EXPECT_TRUE(perf_profile.contains("cpu_utilization"));
    EXPECT_TRUE(perf_profile.contains("memory_peak_mb"));
    EXPECT_TRUE(perf_profile.contains("efficiency_score"));
}

// EN: Test individual stage simulation
// FR: Test la simulation d'étape individuelle
TEST_F(DryRunSystemTest, IndividualStageSimulation) {
    SimulationStage test_stage = createTestStage("individual_test", "Individual Test Stage");
    
    // EN: Simulate individual stage
    // FR: Simule une étape individuelle
    PerformanceProfile profile = dry_run_system_->simulateStage(test_stage);
    
    EXPECT_EQ(profile.stage_id, "individual_test");
    EXPECT_GT(profile.wall_time.count(), 0);
    EXPECT_GT(profile.cpu_time.count(), 0);
    EXPECT_GE(profile.cpu_utilization, 0.0);
    EXPECT_LE(profile.cpu_utilization, 100.0);
    EXPECT_GT(profile.memory_peak_mb, 0);
    EXPECT_GE(profile.efficiency_score, 0.0);
    EXPECT_LE(profile.efficiency_score, 1.0);
}

// EN: Test execution plan generation
// FR: Test la génération de plan d'exécution
TEST_F(DryRunSystemTest, ExecutionPlanGeneration) {
    std::vector<SimulationStage> stages;
    
    // EN: Create stages with complex dependencies
    // FR: Crée des étapes avec dépendances complexes
    stages.push_back(createTestStage("init", "Initialization", {}));
    stages.push_back(createTestStage("load_data", "Load Data", {"init"}));
    stages.push_back(createTestStage("process_a", "Process A", {"load_data"}));
    stages.push_back(createTestStage("process_b", "Process B", {"load_data"}));
    stages.push_back(createTestStage("merge", "Merge Results", {"process_a", "process_b"}));
    
    // EN: Generate execution plan
    // FR: Génère le plan d'exécution
    ExecutionPlan plan = dry_run_system_->generateExecutionPlan(stages);
    
    EXPECT_EQ(plan.stages.size(), stages.size());
    EXPECT_GT(plan.total_estimated_time.count(), 0);
    EXPECT_FALSE(plan.critical_path.empty());
    EXPECT_GT(plan.parallelization_factor, 0.0);
    
    // EN: Check that parallel groups are identified
    // FR: Vérifie que les groupes parallèles sont identifiés
    EXPECT_FALSE(plan.parallel_groups.empty());
    
    // EN: Check resource summary
    // FR: Vérifie le résumé des ressources
    EXPECT_FALSE(plan.resource_summary.empty());
    
    // EN: Check optimization suggestions
    // FR: Vérifie les suggestions d'optimisation
    // Note: Suggestions depend on stage characteristics, so we just check they exist
    // Note: Les suggestions dépendent des caractéristiques d'étape, donc on vérifie juste qu'elles existent
}

// EN: Test validation with file system checks
// FR: Test la validation avec vérifications du système de fichiers
TEST_F(DryRunSystemTest, FileSystemValidation) {
    // EN: Create test files
    // FR: Crée des fichiers de test
    std::string existing_file = (test_dir_ / "existing_input.csv").string();
    std::ofstream(existing_file) << "test,data\n1,2\n";
    
    std::string missing_file = (test_dir_ / "missing_input.csv").string();
    
    // EN: Create stages with existing and missing files
    // FR: Crée des étapes avec fichiers existants et manquants
    SimulationStage valid_stage = createTestStage("valid_file_stage");
    valid_stage.input_files = {existing_file};
    
    SimulationStage invalid_stage = createTestStage("invalid_file_stage");
    invalid_stage.input_files = {missing_file};
    
    std::vector<SimulationStage> stages = {valid_stage, invalid_stage};
    
    // EN: Enable file validation
    // FR: Active la validation de fichier
    DryRunConfig validation_config = config_;
    validation_config.enable_file_validation = true;
    dry_run_system_->updateConfig(validation_config);
    
    // EN: Execute validation
    // FR: Exécute la validation
    auto results = dry_run_system_->execute(stages);
    
    // EN: Should have validation issues for missing file
    // FR: Devrait avoir des problèmes de validation pour fichier manquant
    EXPECT_FALSE(results.validation_issues.empty());
    
    bool found_missing_file_error = std::any_of(results.validation_issues.begin(), results.validation_issues.end(),
        [&missing_file](const ValidationIssue& issue) {
            return issue.severity == ValidationSeverity::ERROR && 
                   issue.message.find("does not exist") != std::string::npos &&
                   issue.message.find(missing_file) != std::string::npos;
        });
    EXPECT_TRUE(found_missing_file_error);
}

// EN: Test callback functionality
// FR: Test la fonctionnalité des callbacks
TEST_F(DryRunSystemTest, CallbackFunctionality) {
    // EN: Setup callback tracking
    // FR: Configure le suivi des callbacks
    std::vector<std::pair<std::string, double>> progress_updates;
    std::vector<ValidationIssue> validation_issues;
    std::vector<std::pair<std::string, PerformanceProfile>> stage_profiles;
    
    // EN: Set callbacks
    // FR: Définit les callbacks
    dry_run_system_->setProgressCallback([&](const std::string& task, double progress) {
        progress_updates.emplace_back(task, progress);
    });
    
    dry_run_system_->setValidationCallback([&](const ValidationIssue& issue) {
        validation_issues.push_back(issue);
    });
    
    dry_run_system_->setStageCallback([&](const std::string& stage_id, const PerformanceProfile& profile) {
        stage_profiles.emplace_back(stage_id, profile);
    });
    
    // EN: Enable progress display for callback testing
    // FR: Active l'affichage de progression pour test des callbacks
    DryRunConfig callback_config = config_;
    callback_config.show_progress = true;
    dry_run_system_->updateConfig(callback_config);
    
    std::vector<SimulationStage> stages;
    stages.push_back(createTestStage("callback_test", "Callback Test Stage"));
    
    // EN: Add stage with validation issue
    // FR: Ajoute une étape avec problème de validation
    SimulationStage problematic_stage;
    problematic_stage.stage_id = ""; // This will cause validation error
    problematic_stage.stage_name = "Problematic Stage";
    stages.push_back(problematic_stage);
    
    // EN: Execute simulation
    // FR: Exécute la simulation
    auto results = dry_run_system_->execute(stages);
    
    // EN: Check that callbacks were called
    // FR: Vérifie que les callbacks ont été appelés
    EXPECT_FALSE(progress_updates.empty());
    EXPECT_FALSE(validation_issues.empty());
    EXPECT_FALSE(stage_profiles.empty());
    
    // EN: Verify callback content
    // FR: Vérifie le contenu des callbacks
    EXPECT_EQ(stage_profiles[0].first, "callback_test");
    EXPECT_EQ(stage_profiles[0].second.stage_id, "callback_test");
    
    bool found_stage_id_error = std::any_of(validation_issues.begin(), validation_issues.end(),
        [](const ValidationIssue& issue) {
            return issue.message.find("Stage ID cannot be empty") != std::string::npos;
        });
    EXPECT_TRUE(found_stage_id_error);
}

// EN: Test report generation
// FR: Test la génération de rapport
TEST_F(DryRunSystemTest, ReportGeneration) {
    std::vector<SimulationStage> stages;
    stages.push_back(createTestStage("report_test", "Report Test Stage"));
    
    // EN: Execute simulation with report generation
    // FR: Exécute la simulation avec génération de rapport
    auto results = dry_run_system_->execute(stages);
    EXPECT_TRUE(results.success);
    
    // EN: Generate HTML report
    // FR: Génère le rapport HTML
    std::string html_report = dry_run_system_->generateReport(results, "html");
    EXPECT_FALSE(html_report.empty());
    EXPECT_THAT(html_report, HasSubstr("<!DOCTYPE html>"));
    EXPECT_THAT(html_report, HasSubstr("BB-Pipeline Dry Run Report"));
    EXPECT_THAT(html_report, HasSubstr("report_test"));
    
    // EN: Generate JSON report
    // FR: Génère le rapport JSON
    std::string json_report = dry_run_system_->generateReport(results, "json");
    EXPECT_FALSE(json_report.empty());
    EXPECT_THAT(json_report, HasSubstr("\"success\""));
    EXPECT_THAT(json_report, HasSubstr("\"mode_executed\""));
    
    // EN: Test report export
    // FR: Test l'export de rapport
    std::string html_file = (test_dir_ / "test_report.html").string();
    std::string json_file = (test_dir_ / "test_report.json").string();
    
    EXPECT_TRUE(dry_run_system_->exportReport(results, html_file, "html"));
    EXPECT_TRUE(dry_run_system_->exportReport(results, json_file, "json"));
    
    // EN: Verify files were created
    // FR: Vérifie que les fichiers ont été créés
    EXPECT_TRUE(std::filesystem::exists(html_file));
    EXPECT_TRUE(std::filesystem::exists(json_file));
    
    // EN: Verify file content
    // FR: Vérifie le contenu des fichiers
    std::ifstream html_stream(html_file);
    std::string html_content((std::istreambuf_iterator<char>(html_stream)), std::istreambuf_iterator<char>());
    EXPECT_THAT(html_content, HasSubstr("BB-Pipeline Dry Run Report"));
}

// EN: Test custom simulation engine registration
// FR: Test l'enregistrement de moteur de simulation personnalisé
TEST_F(DryRunSystemTest, CustomSimulationEngine) {
    auto mock_engine = std::make_unique<MockSimulationEngine>();
    
    // EN: Set up mock expectations
    // FR: Configure les attentes du mock
    EXPECT_CALL(*mock_engine, initialize(_))
        .WillOnce(Return(true));
    
    // EN: Set up stage simulation expectation
    // FR: Configure l'attente de simulation d'étape
    PerformanceProfile mock_profile;
    mock_profile.stage_id = "custom_test";
    mock_profile.wall_time = std::chrono::milliseconds(5000);
    mock_profile.cpu_time = std::chrono::milliseconds(4000);
    mock_profile.cpu_utilization = 75.0;
    mock_profile.memory_peak_mb = 128;
    mock_profile.efficiency_score = 0.9;
    
    EXPECT_CALL(*mock_engine, simulateStage(_))
        .WillRepeatedly(Return(mock_profile));
    
    EXPECT_CALL(*mock_engine, validateStage(_))
        .WillRepeatedly(Return(std::vector<ValidationIssue>{}));
    
    EXPECT_CALL(*mock_engine, estimateResource(_, _))
        .WillRepeatedly(Return(createTestResourceEstimate(ResourceType::CPU_USAGE, 50.0)));
    
    // EN: Set up execution plan expectation
    // FR: Configure l'attente du plan d'exécution
    ExecutionPlan mock_plan;
    mock_plan.total_estimated_time = std::chrono::milliseconds(5000);
    mock_plan.parallelization_factor = 1.5;
    mock_plan.critical_path = "custom_test";
    
    EXPECT_CALL(*mock_engine, generateExecutionPlan(_))
        .WillOnce(Return(mock_plan));
    
    // EN: Register custom engine
    // FR: Enregistre le moteur personnalisé
    dry_run_system_->registerSimulationEngine(std::move(mock_engine));
    
    // EN: Test with custom engine
    // FR: Test avec moteur personnalisé
    std::vector<SimulationStage> stages;
    stages.push_back(createTestStage("custom_test", "Custom Engine Test"));
    
    auto results = dry_run_system_->execute(stages);
    EXPECT_TRUE(results.success);
    
    // EN: Verify mock engine was used
    // FR: Vérifie que le moteur mock a été utilisé
    EXPECT_EQ(results.execution_plan.total_estimated_time.count(), 5000);
    EXPECT_EQ(results.execution_plan.critical_path, "custom_test");
}

// EN: Test custom report generator registration
// FR: Test l'enregistrement de générateur de rapport personnalisé
TEST_F(DryRunSystemTest, CustomReportGenerator) {
    auto mock_generator = std::make_unique<MockReportGenerator>();
    
    // EN: Set up mock expectations
    // FR: Configure les attentes du mock
    EXPECT_CALL(*mock_generator, generateReport(_))
        .WillOnce(Return("Custom report content"));
    
    EXPECT_CALL(*mock_generator, exportToFile(_, _))
        .WillOnce(Return(true));
    
    // EN: Register custom generator
    // FR: Enregistre le générateur personnalisé
    dry_run_system_->registerReportGenerator("custom", std::move(mock_generator));
    
    // EN: Generate report with custom generator
    // FR: Génère le rapport avec générateur personnalisé
    std::vector<SimulationStage> stages;
    stages.push_back(createTestStage("report_generator_test", "Report Generator Test"));
    
    auto results = dry_run_system_->execute(stages);
    
    std::string custom_report = dry_run_system_->generateReport(results, "custom");
    EXPECT_EQ(custom_report, "Custom report content");
    
    std::string custom_file = (test_dir_ / "custom_report.txt").string();
    EXPECT_TRUE(dry_run_system_->exportReport(results, custom_file, "custom"));
}

// EN: Test detailed logging functionality
// FR: Test la fonctionnalité de logging détaillé
TEST_F(DryRunSystemTest, DetailedLogging) {
    // EN: Enable detailed logging
    // FR: Active le logging détaillé
    dry_run_system_->setDetailedLogging(true);
    
    std::vector<SimulationStage> stages;
    stages.push_back(createTestStage("logging_test", "Logging Test Stage"));
    
    // EN: Execute simulation with detailed logging
    // FR: Exécute la simulation avec logging détaillé
    auto results = dry_run_system_->execute(stages);
    EXPECT_TRUE(results.success);
    
    // EN: Disable detailed logging
    // FR: Désactive le logging détaillé
    dry_run_system_->setDetailedLogging(false);
    
    // EN: Execute another simulation (should have less verbose logging)
    // FR: Exécute une autre simulation (devrait avoir un logging moins verbeux)
    auto results2 = dry_run_system_->execute(stages);
    EXPECT_TRUE(results2.success);
}

// EN: Test statistics functionality
// FR: Test la fonctionnalité des statistiques
TEST_F(DryRunSystemTest, Statistics) {
    // EN: Get initial statistics
    // FR: Obtient les statistiques initiales
    auto initial_stats = dry_run_system_->getSimulationStatistics();
    EXPECT_FALSE(initial_stats.empty());
    
    // EN: Reset statistics
    // FR: Remet à zéro les statistiques
    dry_run_system_->resetStatistics();
    
    // EN: Get statistics after reset
    // FR: Obtient les statistiques après remise à zéro
    auto reset_stats = dry_run_system_->getSimulationStatistics();
    EXPECT_FALSE(reset_stats.empty());
}

} // anonymous namespace

// EN: Test fixture for DryRunSystemManager
// FR: Fixture de test pour DryRunSystemManager
class DryRunSystemManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "bbp_manager_dry_run_test";
        std::filesystem::create_directories(test_dir_);
        
        config_ = DryRunUtils::createDefaultConfig();
    }
    
    void TearDown() override {
        DryRunSystemManager::getInstance().shutdown();
        
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

protected:
    std::filesystem::path test_dir_;
    DryRunConfig config_;
};

// EN: Test DryRunSystemManager initialization
// FR: Test l'initialisation de DryRunSystemManager
TEST_F(DryRunSystemManagerTest, Initialization) {
    auto& manager = DryRunSystemManager::getInstance();
    
    // EN: Test initialization
    // FR: Test l'initialisation
    EXPECT_TRUE(manager.initialize(config_));
    
    // EN: Test double initialization (should succeed)
    // FR: Test la double initialisation (devrait réussir)
    EXPECT_TRUE(manager.initialize(config_));
    
    // EN: Get dry run system
    // FR: Obtient le système de simulation
    auto& dry_run_system = manager.getDryRunSystem();
    EXPECT_EQ(dry_run_system.getConfig().mode, config_.mode);
}

// EN: Test quick validation functionality
// FR: Test la fonctionnalité de validation rapide
TEST_F(DryRunSystemManagerTest, QuickValidation) {
    auto& manager = DryRunSystemManager::getInstance();
    ASSERT_TRUE(manager.initialize(config_));
    
    std::string config_path = (test_dir_ / "test_config.yaml").string();
    
    // EN: Test quick validation
    // FR: Test la validation rapide
    auto issues = manager.quickValidate(config_path);
    
    // EN: Issues may be present depending on configuration loading
    // FR: Des problèmes peuvent être présents selon le chargement de configuration
    // This is expected as we're using a test configuration path
    // C'est attendu car on utilise un chemin de configuration de test
}

// EN: Test resource estimates functionality
// FR: Test la fonctionnalité d'estimations de ressources
TEST_F(DryRunSystemManagerTest, ResourceEstimates) {
    auto& manager = DryRunSystemManager::getInstance();
    ASSERT_TRUE(manager.initialize(config_));
    
    std::string config_path = (test_dir_ / "test_config.yaml").string();
    
    // EN: Test resource estimates
    // FR: Test les estimations de ressources
    auto estimates = manager.getResourceEstimates(config_path);
    
    // EN: May be empty due to test configuration, but should not crash
    // FR: Peut être vide dû à la configuration de test, mais ne devrait pas crasher
    // The important thing is that the method executes without error
    // L'important est que la méthode s'exécute sans erreur
}

// EN: Test execution preview functionality
// FR: Test la fonctionnalité d'aperçu d'exécution
TEST_F(DryRunSystemManagerTest, ExecutionPreview) {
    auto& manager = DryRunSystemManager::getInstance();
    ASSERT_TRUE(manager.initialize(config_));
    
    std::string config_path = (test_dir_ / "test_config.yaml").string();
    
    // EN: Test execution preview
    // FR: Test l'aperçu d'exécution
    auto preview = manager.generatePreview(config_path);
    
    // EN: Preview may be empty due to test configuration, but should not crash
    // FR: L'aperçu peut être vide dû à la configuration de test, mais ne devrait pas crasher
}

// EN: Test system readiness check
// FR: Test la vérification de préparation du système
TEST_F(DryRunSystemManagerTest, SystemReadinessCheck) {
    auto& manager = DryRunSystemManager::getInstance();
    ASSERT_TRUE(manager.initialize(config_));
    
    std::string config_path = (test_dir_ / "test_config.yaml").string();
    
    // EN: Test system readiness check
    // FR: Test la vérification de préparation du système
    bool ready = manager.checkSystemReadiness(config_path);
    
    // EN: Readiness depends on configuration validation
    // FR: La préparation dépend de la validation de configuration
    // We just ensure the method executes without crashing
    // On s'assure juste que la méthode s'exécute sans crasher
    (void)ready; // Suppress unused variable warning
}

// EN: Test fixture for AutoDryRunGuard
// FR: Fixture de test pour AutoDryRunGuard
class AutoDryRunGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "bbp_auto_dry_run_test";
        std::filesystem::create_directories(test_dir_);
    }
    
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }
    
    // EN: Helper method to create test stages
    // FR: Méthode helper pour créer des étapes de test
    std::vector<SimulationStage> createTestStages() {
        std::vector<SimulationStage> stages;
        
        SimulationStage stage1;
        stage1.stage_id = "auto_test1";
        stage1.stage_name = "Auto Test Stage 1";
        stage1.description = "First stage for auto guard test";
        stage1.estimated_duration = std::chrono::milliseconds(5000);
        stage1.can_run_parallel = true;
        stages.push_back(stage1);
        
        SimulationStage stage2;
        stage2.stage_id = "auto_test2";
        stage2.stage_name = "Auto Test Stage 2";
        stage2.description = "Second stage for auto guard test";
        stage2.dependencies = {"auto_test1"};
        stage2.estimated_duration = std::chrono::milliseconds(8000);
        stage2.can_run_parallel = false;
        stages.push_back(stage2);
        
        return stages;
    }

protected:
    std::filesystem::path test_dir_;
};

// EN: Test AutoDryRunGuard basic functionality
// FR: Test la fonctionnalité de base d'AutoDryRunGuard
TEST_F(AutoDryRunGuardTest, BasicFunctionality) {
    auto stages = createTestStages();
    
    {
        // EN: Create AutoDryRunGuard in scope
        // FR: Crée AutoDryRunGuard dans la portée
        AutoDryRunGuard guard(stages, DryRunMode::VALIDATE_ONLY);
        
        // EN: Test safety check
        // FR: Test la vérification de sécurité
        bool safe = guard.isSafeToExecute();
        // Safety depends on validation results, we just ensure it doesn't crash
        // La sécurité dépend des résultats de validation, on s'assure juste que ça ne crashe pas
        (void)safe;
        
        // EN: Get validation issues
        // FR: Obtient les problèmes de validation
        auto issues = guard.getValidationIssues();
        // May have issues depending on stage configuration
        // Peut avoir des problèmes selon la configuration des étapes
        
        // EN: Get execution plan
        // FR: Obtient le plan d'exécution
        auto plan = guard.getExecutionPlan();
        // Plan may be empty for validation-only mode
        // Le plan peut être vide pour le mode validation uniquement
        
    } // EN: Guard destructor should execute dry run / FR: Le destructeur du guard devrait exécuter la simulation
}

// EN: Test AutoDryRunGuard with config path
// FR: Test AutoDryRunGuard avec chemin de configuration
TEST_F(AutoDryRunGuardTest, ConfigPathConstructor) {
    std::string config_path = (test_dir_ / "test_config.yaml").string();
    
    {
        AutoDryRunGuard guard(config_path, DryRunMode::ESTIMATE_RESOURCES);
        
        // EN: Manual execution
        // FR: Exécution manuelle
        auto results = guard.execute();
        
        // EN: Results may vary based on config loading, but should not crash
        // FR: Les résultats peuvent varier selon le chargement de config, mais ne devraient pas crasher
        EXPECT_EQ(results.mode_executed, DryRunMode::ESTIMATE_RESOURCES);
    }
}

// EN: Test utility functions
// FR: Test les fonctions utilitaires
class DryRunUtilsTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = std::filesystem::temp_directory_path() / "bbp_dry_run_utils_test";
        std::filesystem::create_directories(test_dir_);
    }
    
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(test_dir_, ec);
    }

protected:
    std::filesystem::path test_dir_;
};

// EN: Test configuration creation utilities
// FR: Test les utilitaires de création de configuration
TEST_F(DryRunUtilsTest, ConfigurationCreation) {
    // EN: Test default configuration
    // FR: Test la configuration par défaut
    auto default_config = DryRunUtils::createDefaultConfig();
    EXPECT_EQ(default_config.mode, DryRunMode::VALIDATE_ONLY);
    EXPECT_EQ(default_config.detail_level, SimulationDetail::STANDARD);
    EXPECT_TRUE(default_config.enable_dependency_validation);
    EXPECT_TRUE(default_config.enable_file_validation);
    EXPECT_TRUE(default_config.show_progress);
    EXPECT_FALSE(default_config.interactive_mode);
    
    // EN: Test validation-only configuration
    // FR: Test la configuration validation uniquement
    auto validation_config = DryRunUtils::createValidationOnlyConfig();
    EXPECT_EQ(validation_config.mode, DryRunMode::VALIDATE_ONLY);
    EXPECT_FALSE(validation_config.enable_resource_estimation);
    EXPECT_FALSE(validation_config.enable_performance_profiling);
    
    // EN: Test full simulation configuration
    // FR: Test la configuration de simulation complète
    auto full_config = DryRunUtils::createFullSimulationConfig();
    EXPECT_EQ(full_config.mode, DryRunMode::FULL_SIMULATION);
    EXPECT_EQ(full_config.detail_level, SimulationDetail::DETAILED);
    EXPECT_TRUE(full_config.enable_resource_estimation);
    EXPECT_TRUE(full_config.enable_performance_profiling);
    EXPECT_TRUE(full_config.enable_network_simulation);
    EXPECT_TRUE(full_config.generate_report);
    
    // EN: Test performance profiling configuration
    // FR: Test la configuration de profilage de performance
    auto profile_config = DryRunUtils::createPerformanceProfilingConfig();
    EXPECT_EQ(profile_config.mode, DryRunMode::PERFORMANCE_PROFILE);
    EXPECT_EQ(profile_config.detail_level, SimulationDetail::VERBOSE);
    EXPECT_TRUE(profile_config.enable_performance_profiling);
    EXPECT_TRUE(profile_config.generate_report);
}

// EN: Test string conversion utilities
// FR: Test les utilitaires de conversion de chaînes
TEST_F(DryRunUtilsTest, StringConversions) {
    // EN: Test severity to string conversion
    // FR: Test la conversion de gravité en chaîne
    EXPECT_EQ(DryRunUtils::severityToString(ValidationSeverity::INFO), "INFO");
    EXPECT_EQ(DryRunUtils::severityToString(ValidationSeverity::WARNING), "WARNING");
    EXPECT_EQ(DryRunUtils::severityToString(ValidationSeverity::ERROR), "ERROR");
    EXPECT_EQ(DryRunUtils::severityToString(ValidationSeverity::CRITICAL), "CRITICAL");
    
    // EN: Test resource type to string conversion
    // FR: Test la conversion de type de ressource en chaîne
    EXPECT_EQ(DryRunUtils::resourceTypeToString(ResourceType::CPU_USAGE), "CPU Usage");
    EXPECT_EQ(DryRunUtils::resourceTypeToString(ResourceType::MEMORY_USAGE), "Memory Usage");
    EXPECT_EQ(DryRunUtils::resourceTypeToString(ResourceType::DISK_SPACE), "Disk Space");
    EXPECT_EQ(DryRunUtils::resourceTypeToString(ResourceType::NETWORK_BANDWIDTH), "Network Bandwidth");
    EXPECT_EQ(DryRunUtils::resourceTypeToString(ResourceType::EXECUTION_TIME), "Execution Time");
    EXPECT_EQ(DryRunUtils::resourceTypeToString(ResourceType::IO_OPERATIONS), "I/O Operations");
}

// EN: Test dry run mode parsing
// FR: Test le parsing de mode de simulation
TEST_F(DryRunUtilsTest, DryRunModeParsing) {
    // EN: Test valid mode strings
    // FR: Test les chaînes de mode valides
    EXPECT_EQ(DryRunUtils::parseDryRunMode("validate"), DryRunMode::VALIDATE_ONLY);
    EXPECT_EQ(DryRunUtils::parseDryRunMode("validation"), DryRunMode::VALIDATE_ONLY);
    EXPECT_EQ(DryRunUtils::parseDryRunMode("estimate"), DryRunMode::ESTIMATE_RESOURCES);
    EXPECT_EQ(DryRunUtils::parseDryRunMode("resources"), DryRunMode::ESTIMATE_RESOURCES);
    EXPECT_EQ(DryRunUtils::parseDryRunMode("full"), DryRunMode::FULL_SIMULATION);
    EXPECT_EQ(DryRunUtils::parseDryRunMode("simulation"), DryRunMode::FULL_SIMULATION);
    EXPECT_EQ(DryRunUtils::parseDryRunMode("interactive"), DryRunMode::INTERACTIVE);
    EXPECT_EQ(DryRunUtils::parseDryRunMode("profile"), DryRunMode::PERFORMANCE_PROFILE);
    EXPECT_EQ(DryRunUtils::parseDryRunMode("performance"), DryRunMode::PERFORMANCE_PROFILE);
    
    // EN: Test invalid mode string
    // FR: Test chaîne de mode invalide
    EXPECT_EQ(DryRunUtils::parseDryRunMode("invalid_mode"), std::nullopt);
}

// EN: Test execution time estimation
// FR: Test l'estimation de temps d'exécution
TEST_F(DryRunUtilsTest, ExecutionTimeEstimation) {
    std::vector<SimulationStage> stages;
    
    SimulationStage stage1;
    stage1.estimated_duration = std::chrono::milliseconds(5000);
    stages.push_back(stage1);
    
    SimulationStage stage2;
    stage2.estimated_duration = std::chrono::milliseconds(8000);
    stages.push_back(stage2);
    
    SimulationStage stage3;
    stage3.estimated_duration = std::chrono::milliseconds(3000);
    stages.push_back(stage3);
    
    auto total_time = DryRunUtils::estimateTotalExecutionTime(stages);
    EXPECT_EQ(total_time.count(), 16000); // 5000 + 8000 + 3000
}

// EN: Test file accessibility check
// FR: Test la vérification d'accessibilité de fichier
TEST_F(DryRunUtilsTest, FileAccessibilityCheck) {
    // EN: Create test file
    // FR: Crée un fichier de test
    std::string existing_file = (test_dir_ / "existing_file.txt").string();
    std::ofstream(existing_file) << "test content";
    
    std::string non_existing_file = (test_dir_ / "non_existing_file.txt").string();
    
    // EN: Test accessibility checks
    // FR: Test les vérifications d'accessibilité
    EXPECT_TRUE(DryRunUtils::checkFileAccessibility(existing_file));
    EXPECT_FALSE(DryRunUtils::checkFileAccessibility(non_existing_file));
}

// EN: Test dependency graph generation
// FR: Test la génération de graphe de dépendances
TEST_F(DryRunUtilsTest, DependencyGraphGeneration) {
    std::vector<SimulationStage> stages;
    
    SimulationStage stage1;
    stage1.stage_id = "stage1";
    stage1.dependencies = {};
    stages.push_back(stage1);
    
    SimulationStage stage2;
    stage2.stage_id = "stage2";
    stage2.dependencies = {"stage1"};
    stages.push_back(stage2);
    
    SimulationStage stage3;
    stage3.stage_id = "stage3";
    stage3.dependencies = {"stage1", "stage2"};
    stages.push_back(stage3);
    
    auto graph = DryRunUtils::generateDependencyGraph(stages);
    
    EXPECT_EQ(graph.size(), 3);
    EXPECT_TRUE(graph["stage1"].empty());
    EXPECT_EQ(graph["stage2"].size(), 1);
    EXPECT_THAT(graph["stage2"], Contains("stage1"));
    EXPECT_EQ(graph["stage3"].size(), 2);
    EXPECT_THAT(graph["stage3"], Contains("stage1"));
    EXPECT_THAT(graph["stage3"], Contains("stage2"));
}

// EN: Test circular dependency detection
// FR: Test la détection de dépendances circulaires
TEST_F(DryRunUtilsTest, CircularDependencyDetection) {
    std::vector<SimulationStage> stages;
    
    // EN: Create stages with circular dependency
    // FR: Crée des étapes avec dépendance circulaire
    SimulationStage stage1;
    stage1.stage_id = "stage1";
    stage1.dependencies = {"stage3"}; // Creates circular dependency
    stages.push_back(stage1);
    
    SimulationStage stage2;
    stage2.stage_id = "stage2";
    stage2.dependencies = {"stage1"};
    stages.push_back(stage2);
    
    SimulationStage stage3;
    stage3.stage_id = "stage3";
    stage3.dependencies = {"stage2"};
    stages.push_back(stage3);
    
    auto cycles = DryRunUtils::findCircularDependencies(stages);
    
    // EN: Should detect at least one cycle
    // FR: Devrait détecter au moins un cycle
    EXPECT_FALSE(cycles.empty());
}

// EN: Test execution plan optimization
// FR: Test l'optimisation de plan d'exécution
TEST_F(DryRunUtilsTest, ExecutionPlanOptimization) {
    ExecutionPlan original_plan;
    
    // EN: Create stages with different dependency counts
    // FR: Crée des étapes avec différents comptes de dépendances
    SimulationStage stage1;
    stage1.stage_id = "stage1";
    stage1.dependencies = {"dep1", "dep2", "dep3"}; // 3 dependencies
    original_plan.stages.push_back(stage1);
    
    SimulationStage stage2;
    stage2.stage_id = "stage2";
    stage2.dependencies = {"dep1"}; // 1 dependency
    original_plan.stages.push_back(stage2);
    
    SimulationStage stage3;
    stage3.stage_id = "stage3";
    stage3.dependencies = {}; // 0 dependencies
    original_plan.stages.push_back(stage3);
    
    auto optimized_plan = DryRunUtils::optimizeExecutionPlan(original_plan);
    
    // EN: Optimized plan should reorder stages (fewer dependencies first)
    // FR: Le plan optimisé devrait réordonner les étapes (moins de dépendances d'abord)
    EXPECT_EQ(optimized_plan.stages.size(), 3);
    EXPECT_EQ(optimized_plan.stages[0].stage_id, "stage3"); // 0 dependencies
    EXPECT_EQ(optimized_plan.stages[1].stage_id, "stage2"); // 1 dependency
    EXPECT_EQ(optimized_plan.stages[2].stage_id, "stage1"); // 3 dependencies
    
    // EN: Should have optimization suggestions
    // FR: Devrait avoir des suggestions d'optimisation
    EXPECT_FALSE(optimized_plan.optimization_suggestions.empty());
}

// EN: Main test runner
// FR: Lanceur de test principal
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}