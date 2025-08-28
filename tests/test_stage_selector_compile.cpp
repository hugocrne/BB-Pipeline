#include "orchestrator/stage_selector.hpp"
#include <iostream>

using namespace BBP::Orchestrator;

int main() {
    std::cout << "🧪 Testing Stage Selector - Compilation Test" << std::endl;
    
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
        
        // EN: Test utility functions only
        // FR: Tester seulement les fonctions utilitaires
        std::cout << "\nTesting utility functions..." << std::endl;
        
        auto criteria_str = StageSelectorUtils::criteriaToString(StageSelectionCriteria::BY_ID);
        std::cout << "✓ Criteria to string: " << criteria_str << std::endl;
        
        auto filter_mode_str = StageSelectorUtils::filterModeToString(StageFilterMode::INCLUDE);
        std::cout << "✓ Filter mode to string: " << filter_mode_str << std::endl;
        
        auto status_str = StageSelectorUtils::selectionStatusToString(StageSelectionStatus::SUCCESS);
        std::cout << "✓ Status to string: " << status_str << std::endl;
        
        std::cout << "\n🎉 Stage Selector compilation test passed!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}