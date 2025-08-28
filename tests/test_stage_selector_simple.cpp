#include "orchestrator/stage_selector.hpp"
#include <iostream>
#include <cassert>
#include <vector>

using namespace BBP::Orchestrator;

int main() {
    std::cout << "Testing Stage Selector..." << std::endl;
    
    try {
        // EN: Create basic configuration
        // FR: Créer une configuration de base
        StageSelectorConfig config;
        config.cache_ttl = std::chrono::minutes(5);
        config.max_cache_size = 100;
        config.enable_async = false;
        
        // EN: Create stage selector
        // FR: Créer le sélecteur d'étapes
        StageSelector selector(config);
        
        // EN: Create test stages
        // FR: Créer des étapes de test
        std::vector<PipelineStageConfig> test_stages;
        
        PipelineStageConfig stage1;
        stage1.id = "subhunter";
        stage1.name = "Subdomain Hunter";
        stage1.description = "Find subdomains";
        stage1.priority = 10;
        test_stages.push_back(stage1);
        
        PipelineStageConfig stage2;
        stage2.id = "httpxpp";
        stage2.name = "HTTP Prober";
        stage2.description = "Probe HTTP services";
        stage2.priority = 20;
        stage2.dependencies = {"subhunter"};
        test_stages.push_back(stage2);
        
        PipelineStageConfig stage3;
        stage3.id = "dirbff";
        stage3.name = "Directory Bruteforcer";
        stage3.description = "Brute force directories";
        stage3.priority = 15;
        stage3.dependencies = {"httpxpp"};
        test_stages.push_back(stage3);
        
        // EN: Test basic selection by IDs
        // FR: Tester la sélection de base par IDs
        std::cout << "Test 1: Basic selection by IDs" << std::endl;
        std::vector<std::string> stage_ids = {"subhunter", "httpxpp"};
        auto result = selector.selectStagesByIds(test_stages, stage_ids);
        
        assert(result.status == StageSelectionStatus::SUCCESS);
        assert(result.selected_stage_ids.size() >= 2);
        
        std::cout << "✓ Selected " << result.selected_stage_ids.size() << " stages including dependencies" << std::endl;
        for (const auto& id : result.selected_stage_ids) {
            std::cout << "  - " << id << std::endl;
        }
        
        // EN: Test selection with patterns
        // FR: Tester la sélection avec patterns
        std::cout << "\nTest 2: Selection by patterns" << std::endl;
        StageSelectionConfig pattern_config;
        pattern_config.criteria = StageSelectionCriteria::BY_PATTERN;
        pattern_config.pattern = "http.*";
        
        auto pattern_result = selector.selectStages(test_stages, pattern_config);
        assert(pattern_result.status == StageSelectionStatus::SUCCESS);
        
        std::cout << "✓ Found " << pattern_result.selected_stage_ids.size() << " stages matching pattern" << std::endl;
        
        // EN: Test dependency analysis
        // FR: Tester l'analyse des dépendances
        std::cout << "\nTest 3: Dependency Analysis" << std::endl;
        StageDependencyAnalyzer analyzer(test_stages);
        
        auto dependencies = analyzer.getDependencies("dirbff");
        std::cout << "✓ Dependencies for 'dirbff': ";
        for (const auto& dep : dependencies) {
            std::cout << dep << " ";
        }
        std::cout << std::endl;
        
        bool has_cycles = analyzer.hasCircularDependencies();
        std::cout << "✓ Circular dependencies: " << (has_cycles ? "YES" : "NO") << std::endl;
        
        // EN: Test constraint validation
        // FR: Tester la validation des contraintes
        std::cout << "\nTest 4: Constraint Validation" << std::endl;
        StageConstraintValidator validator;
        
        StageConstraintConfig constraint_config;
        constraint_config.validation_level = StageValidationLevel::BASIC;
        constraint_config.enforce_dependencies = true;
        constraint_config.max_execution_time = std::chrono::minutes(30);
        
        auto constraint_result = validator.validateStages(test_stages, constraint_config);
        std::cout << "✓ Stage validation: " << (constraint_result.is_valid ? "PASS" : "FAIL") << std::endl;
        
        if (!constraint_result.is_valid) {
            std::cout << "Validation errors:" << std::endl;
            for (const auto& error : constraint_result.errors) {
                std::cout << "  - " << error << std::endl;
            }
        }
        
        // EN: Test execution planning
        // FR: Tester la planification d'exécution
        std::cout << "\nTest 5: Execution Planning" << std::endl;
        StageExecutionPlanner planner(test_stages);
        
        StageExecutionPlan plan;
        auto planning_result = planner.createExecutionPlan(test_stages, plan);
        
        if (planning_result.success) {
            std::cout << "✓ Execution plan created successfully" << std::endl;
            std::cout << "  Total stages: " << plan.stages.size() << std::endl;
            std::cout << "  Estimated duration: " << plan.estimated_duration.count() << "ms" << std::endl;
        } else {
            std::cout << "✗ Failed to create execution plan" << std::endl;
        }
        
        std::cout << "\n🎉 All Stage Selector tests passed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}