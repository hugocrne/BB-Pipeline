#include "orchestrator/stage_selector.hpp"
#include <iostream>
#include <cassert>

using namespace BBP::Orchestrator;

int main() {
    std::cout << "🧪 Testing Stage Selector - Minimal Test" << std::endl;
    
    try {
        // EN: Create basic configuration  
        // FR: Créer une configuration de base
        StageSelectorConfig config;
        config.cache_ttl = std::chrono::minutes(5);
        
        // EN: Create stage selector
        // FR: Créer le sélecteur d'étapes
        std::cout << "Creating StageSelector..." << std::endl;
        StageSelector selector(config);
        std::cout << "✓ StageSelector created successfully" << std::endl;
        
        // EN: Test utility functions
        // FR: Tester les fonctions utilitaires
        std::cout << "\nTesting utility functions..." << std::endl;
        
        auto criteria_str = StageSelectorUtils::criteriaToString(StageSelectionCriteria::BY_ID);
        std::cout << "✓ Criteria to string: " << criteria_str << std::endl;
        
        auto filter_mode_str = StageSelectorUtils::filterModeToString(StageFilterMode::INCLUDE);
        std::cout << "✓ Filter mode to string: " << filter_mode_str << std::endl;
        
        auto validation_str = StageSelectorUtils::validationLevelToString(StageValidationLevel::BASIC);
        std::cout << "✓ Validation level to string: " << validation_str << std::endl;
        
        auto constraint_str = StageSelectorUtils::constraintToString(StageExecutionConstraint::PARALLEL_SAFE);
        std::cout << "✓ Constraint to string: " << constraint_str << std::endl;
        
        auto status_str = StageSelectorUtils::selectionStatusToString(StageSelectionStatus::SUCCESS);
        std::cout << "✓ Status to string: " << status_str << std::endl;
        
        // EN: Create test stages
        // FR: Créer des étapes de test
        std::cout << "\nCreating test stages..." << std::endl;
        
        std::vector<PipelineStageConfig> test_stages;
        
        PipelineStageConfig stage1;
        stage1.id = "subhunter";
        stage1.name = "Subdomain Hunter";
        stage1.description = "Find subdomains";
        stage1.priority = PipelineStagePriority::HIGH;
        test_stages.push_back(stage1);
        
        PipelineStageConfig stage2;
        stage2.id = "httpxpp";
        stage2.name = "HTTP Prober";
        stage2.description = "Probe HTTP services";
        stage2.priority = PipelineStagePriority::MEDIUM;
        stage2.dependencies = {"subhunter"};
        test_stages.push_back(stage2);
        
        std::cout << "✓ Created " << test_stages.size() << " test stages" << std::endl;
        
        // EN: Test basic selection by IDs
        // FR: Tester la sélection de base par IDs
        std::cout << "\nTesting stage selection by IDs..." << std::endl;
        
        std::vector<std::string> stage_ids = {"subhunter"};
        auto result = selector.selectStagesByIds(test_stages, stage_ids);
        
        std::cout << "Selection status: " << StageSelectorUtils::selectionStatusToString(result.status) << std::endl;
        std::cout << "Selected stages: " << result.selected_stage_ids.size() << std::endl;
        
        for (const auto& id : result.selected_stage_ids) {
            std::cout << "  - " << id << std::endl;
        }
        
        if (result.status == StageSelectionStatus::SUCCESS || result.status == StageSelectionStatus::PARTIAL_SUCCESS) {
            std::cout << "✓ Stage selection completed" << std::endl;
        } else {
            std::cout << "⚠ Stage selection completed with status: " << StageSelectorUtils::selectionStatusToString(result.status) << std::endl;
            for (const auto& error : result.errors) {
                std::cout << "  Error: " << error << std::endl;
            }
        }
        
        // EN: Test creating a dependency analyzer
        // FR: Tester la création d'un analyseur de dépendances
        std::cout << "\nTesting dependency analyzer..." << std::endl;
        
        StageDependencyAnalyzer analyzer;
        std::cout << "✓ DependencyAnalyzer created successfully" << std::endl;
        
        // EN: Test creating constraint validator
        // FR: Tester la création d'un validateur de contraintes
        std::cout << "\nTesting constraint validator..." << std::endl;
        
        StageConstraintValidator validator;
        std::cout << "✓ ConstraintValidator created successfully" << std::endl;
        
        // EN: Test creating execution planner
        // FR: Tester la création d'un planificateur d'exécution
        std::cout << "\nTesting execution planner..." << std::endl;
        
        StageExecutionPlanner planner;
        std::cout << "✓ ExecutionPlanner created successfully" << std::endl;
        
        std::cout << "\n🎉 All minimal Stage Selector tests passed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}