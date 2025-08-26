# Dry Run System - Système de Simulation

## English Documentation

### Overview

The Dry Run System is a comprehensive simulation framework designed for the BB-Pipeline project. It provides complete simulation capabilities without executing actual pipeline operations, enabling users to validate configurations, estimate resource usage, analyze performance profiles, and optimize execution plans before running real operations.

### Key Features

#### Multiple Simulation Modes

- **Validation Only**: Comprehensive configuration and dependency validation
- **Resource Estimation**: Detailed analysis of CPU, memory, disk, and network requirements  
- **Full Simulation**: Complete end-to-end simulation with performance profiling
- **Interactive Mode**: User-guided simulation with confirmation prompts
- **Performance Profiling**: In-depth performance analysis and bottleneck identification

#### Advanced Validation

- **Configuration Validation**: Pipeline configuration syntax and structure validation
- **Dependency Analysis**: Circular dependency detection and resolution order optimization
- **File System Validation**: Input/output file existence and accessibility checks
- **Resource Constraint Validation**: System resource availability and capacity planning
- **Network Simulation**: Simulated network operations with bandwidth estimation

#### Intelligent Resource Estimation

- **CPU Usage Prediction**: Processor utilization estimates based on stage complexity
- **Memory Footprint Analysis**: RAM usage prediction with peak consumption estimates  
- **Disk Space Requirements**: Storage needs assessment for input/output operations
- **Network Bandwidth Usage**: Data transfer requirements and network load analysis
- **Execution Time Estimation**: Duration predictions with confidence intervals
- **I/O Operations Counting**: Input/output operation frequency and patterns

#### Performance Profiling

- **Stage-by-Stage Analysis**: Individual pipeline stage performance characteristics
- **Bottleneck Identification**: System resource constraints and optimization opportunities
- **Parallelization Analysis**: Concurrent execution potential and dependency mapping
- **Critical Path Detection**: Longest execution path identification for optimization
- **Efficiency Scoring**: Performance metrics and improvement recommendations
- **Resource Utilization Patterns**: Usage patterns across different resource types

#### Report Generation

- **HTML Reports**: Rich, interactive web-based reports with charts and graphs
- **JSON Export**: Machine-readable data for integration with external tools
- **Custom Formats**: Extensible report generation with pluggable generators
- **Executive Summaries**: High-level overview for stakeholders and decision makers
- **Technical Details**: Comprehensive technical information for engineers

### Architecture

#### Core Components

1. **DryRunSystem**: Main orchestration class handling simulation execution
2. **DryRunSystemManager**: Singleton manager for global simulation operations
3. **ISimulationEngine**: Pluggable simulation engine interface with default implementation
4. **IReportGenerator**: Customizable report generation system
5. **AutoDryRunGuard**: RAII helper for automatic simulation management

#### Configuration System

```cpp
struct DryRunConfig {
    DryRunMode mode;                           // Simulation execution mode
    SimulationDetail detail_level;            // Level of simulation detail
    bool enable_resource_estimation;          // Enable resource analysis
    bool enable_performance_profiling;        // Enable performance profiling
    bool enable_dependency_validation;        // Enable dependency checking
    bool enable_file_validation;              // Enable file system validation
    bool enable_network_simulation;           // Enable network simulation
    bool show_progress;                       // Show progress during simulation
    bool interactive_mode;                    // Enable interactive confirmations
    bool generate_report;                     // Generate detailed report
    std::string report_output_path;           // Output path for reports
    std::chrono::seconds timeout;             // Maximum simulation duration
    std::unordered_set<std::string> excluded_stages; // Stages to skip
    std::map<std::string, std::string> custom_parameters; // Custom parameters
};
```

#### Simulation Stages

```cpp
struct SimulationStage {
    std::string stage_id;                     // Unique identifier
    std::string stage_name;                   // Human-readable name
    std::string description;                  // Stage description
    std::vector<std::string> dependencies;    // Stage dependencies
    std::chrono::milliseconds estimated_duration; // Execution time estimate
    std::map<ResourceType, double> resource_estimates; // Resource requirements
    std::vector<std::string> input_files;     // Expected input files
    std::vector<std::string> output_files;    // Expected output files
    bool is_optional;                         // Whether stage is optional
    bool can_run_parallel;                    // Supports parallel execution
    std::map<std::string, std::string> metadata; // Additional metadata
};
```

### Usage Examples

#### Basic Setup and Configuration

```cpp
#include "orchestrator/dry_run_system.hpp"

// Create default configuration
BBP::DryRunConfig config = BBP::DryRunUtils::createDefaultConfig();

// Customize configuration
config.mode = BBP::DryRunMode::FULL_SIMULATION;
config.detail_level = BBP::SimulationDetail::DETAILED;
config.enable_resource_estimation = true;
config.enable_performance_profiling = true;
config.generate_report = true;
config.report_output_path = "./simulation_report.html";
config.timeout = std::chrono::minutes(10);

// Create and initialize dry run system
BBP::DryRunSystem dry_run_system(config);
dry_run_system.initialize();
```

#### Pipeline Validation

```cpp
// Validate pipeline configuration only
std::string config_path = "/path/to/pipeline/config.yaml";
auto validation_issues = dry_run_system.validateConfiguration(config_path);

std::cout << "Validation Results:" << std::endl;
for (const auto& issue : validation_issues) {
    std::cout << "[" << BBP::DryRunUtils::severityToString(issue.severity) << "] "
              << issue.category << ": " << issue.message << std::endl;
    
    if (!issue.suggestion.empty()) {
        std::cout << "  Suggestion: " << issue.suggestion << std::endl;
    }
}

// Check if configuration is safe to execute
bool safe_to_execute = std::none_of(validation_issues.begin(), validation_issues.end(),
    [](const auto& issue) {
        return issue.severity == BBP::ValidationSeverity::CRITICAL || 
               issue.severity == BBP::ValidationSeverity::ERROR;
    });

if (safe_to_execute) {
    std::cout << "Configuration is valid and safe to execute." << std::endl;
} else {
    std::cout << "Configuration has critical issues that must be resolved." << std::endl;
}
```

#### Resource Estimation

```cpp
// Load pipeline stages from configuration
auto stages = BBP::DryRunUtils::loadStagesFromConfig(config_path);

// Estimate resource requirements
auto resource_estimates = dry_run_system.estimateResources(stages);

std::cout << "Resource Estimates:" << std::endl;
for (const auto& [resource_type, estimate] : resource_estimates) {
    std::cout << "  " << BBP::DryRunUtils::resourceTypeToString(resource_type) 
              << ": " << estimate.estimated_value << " " << estimate.unit
              << " (confidence: " << estimate.confidence_percentage << "%)" << std::endl;
    std::cout << "    Range: " << estimate.minimum_value 
              << " - " << estimate.maximum_value << " " << estimate.unit << std::endl;
}

// Check for resource constraints
auto memory_estimate = resource_estimates.find(BBP::ResourceType::MEMORY_USAGE);
if (memory_estimate != resource_estimates.end() && 
    memory_estimate->second.estimated_value > 1000.0) { // > 1GB
    std::cout << "Warning: High memory usage expected. Consider optimizing." << std::endl;
}
```

#### Full Simulation with Performance Profiling

```cpp
// Configure for full simulation
BBP::DryRunConfig full_config = BBP::DryRunUtils::createFullSimulationConfig();
dry_run_system.updateConfig(full_config);

// Set up progress monitoring
dry_run_system.setProgressCallback([](const std::string& task, double progress) {
    std::cout << "[" << std::fixed << std::setprecision(1) << progress << "%] " 
              << task << std::endl;
});

// Set up validation monitoring
dry_run_system.setValidationCallback([](const BBP::ValidationIssue& issue) {
    if (issue.severity == BBP::ValidationSeverity::ERROR || 
        issue.severity == BBP::ValidationSeverity::CRITICAL) {
        std::cout << "VALIDATION ERROR: " << issue.message 
                  << " (Stage: " << issue.stage_id << ")" << std::endl;
    }
});

// Set up stage completion monitoring
dry_run_system.setStageCallback([](const std::string& stage_id, 
                                   const BBP::PerformanceProfile& profile) {
    std::cout << "Stage " << stage_id << " simulated:" << std::endl;
    std::cout << "  Wall time: " << profile.wall_time.count() << "ms" << std::endl;
    std::cout << "  CPU usage: " << profile.cpu_utilization << "%" << std::endl;
    std::cout << "  Memory peak: " << profile.memory_peak_mb << "MB" << std::endl;
    std::cout << "  Efficiency: " << (profile.efficiency_score * 100) << "%" << std::endl;
    
    if (!profile.bottlenecks.empty()) {
        std::cout << "  Bottlenecks: ";
        for (size_t i = 0; i < profile.bottlenecks.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << profile.bottlenecks[i];
        }
        std::cout << std::endl;
    }
});

// Execute full simulation
auto results = dry_run_system.execute(stages);

if (results.success) {
    std::cout << "Simulation completed successfully in " 
              << results.simulation_duration.count() << "ms" << std::endl;
    
    // Analyze execution plan
    const auto& plan = results.execution_plan;
    std::cout << "Execution Plan Analysis:" << std::endl;
    std::cout << "  Total estimated time: " << plan.total_estimated_time.count() << "ms" << std::endl;
    std::cout << "  Parallelization factor: " << plan.parallelization_factor << std::endl;
    std::cout << "  Critical path: " << plan.critical_path << std::endl;
    
    // Show optimization recommendations
    if (!results.recommendations.empty()) {
        std::cout << "Optimization Recommendations:" << std::endl;
        for (const auto& recommendation : results.recommendations) {
            std::cout << "  - " << recommendation << std::endl;
        }
    }
} else {
    std::cout << "Simulation failed. Check validation issues." << std::endl;
}
```

#### Interactive Mode

```cpp
// Configure for interactive mode
BBP::DryRunConfig interactive_config = config;
interactive_config.mode = BBP::DryRunMode::INTERACTIVE;
interactive_config.interactive_mode = true;
dry_run_system.updateConfig(interactive_config);

// Generate execution plan first
auto execution_plan = dry_run_system.generateExecutionPlan(stages);

// Run interactive confirmation
bool user_approved = dry_run_system.runInteractiveMode(execution_plan);

if (user_approved) {
    std::cout << "User approved execution. Proceeding with actual pipeline..." << std::endl;
    // Here you would proceed with actual pipeline execution
} else {
    std::cout << "User cancelled execution." << std::endl;
}
```

#### Report Generation and Export

```cpp
// Execute simulation
auto results = dry_run_system.execute(stages);

// Generate HTML report
std::string html_report = dry_run_system.generateReport(results, "html");
std::cout << "Generated HTML report (" << html_report.length() << " characters)" << std::endl;

// Generate JSON report for programmatic access
std::string json_report = dry_run_system.generateReport(results, "json");

// Export reports to files
std::string html_file = "./simulation_report.html";
std::string json_file = "./simulation_report.json";

if (dry_run_system.exportReport(results, html_file, "html")) {
    std::cout << "HTML report exported to: " << html_file << std::endl;
}

if (dry_run_system.exportReport(results, json_file, "json")) {
    std::cout << "JSON report exported to: " << json_file << std::endl;
}

// Parse JSON report for further analysis
try {
    auto json_data = nlohmann::json::parse(json_report);
    auto execution_time = json_data["execution_plan"]["total_estimated_time_ms"].get<int>();
    auto success = json_data["success"].get<bool>();
    
    std::cout << "Parsed from JSON report:" << std::endl;
    std::cout << "  Success: " << (success ? "Yes" : "No") << std::endl;
    std::cout << "  Total time: " << execution_time << "ms" << std::endl;
} catch (const std::exception& e) {
    std::cout << "Error parsing JSON report: " << e.what() << std::endl;
}
```

#### Using AutoDryRunGuard

```cpp
{
    // RAII automatic dry run execution
    BBP::AutoDryRunGuard guard(config_path, BBP::DryRunMode::VALIDATE_ONLY);
    
    // Check if execution would be safe
    if (guard.isSafeToExecute()) {
        std::cout << "Pipeline is safe to execute." << std::endl;
        
        // Get detailed execution plan
        auto plan = guard.getExecutionPlan();
        std::cout << "Estimated execution time: " 
                  << plan.total_estimated_time.count() << "ms" << std::endl;
        
        // Proceed with actual execution (your pipeline code here)
        // executePipeline(config_path);
    } else {
        std::cout << "Pipeline has validation issues:" << std::endl;
        auto issues = guard.getValidationIssues();
        for (const auto& issue : issues) {
            if (issue.severity == BBP::ValidationSeverity::ERROR || 
                issue.severity == BBP::ValidationSeverity::CRITICAL) {
                std::cout << "  - " << issue.message << std::endl;
            }
        }
    }
    
    // Guard destructor automatically executes simulation if not done manually
}
```

#### Global Manager Usage

```cpp
// Initialize global manager
auto& manager = BBP::DryRunSystemManager::getInstance();
manager.initialize(config);

// Quick validation check
auto quick_issues = manager.quickValidate(config_path);
bool has_critical_issues = std::any_of(quick_issues.begin(), quick_issues.end(),
    [](const auto& issue) {
        return issue.severity == BBP::ValidationSeverity::CRITICAL;
    });

if (!has_critical_issues) {
    // Get resource estimates
    auto estimates = manager.getResourceEstimates(config_path);
    
    // Generate execution preview
    auto preview = manager.generatePreview(config_path);
    
    // Check overall system readiness
    bool system_ready = manager.checkSystemReadiness(config_path);
    
    std::cout << "System readiness check: " << (system_ready ? "READY" : "NOT READY") << std::endl;
}
```

#### Custom Simulation Engine

```cpp
// Implement custom simulation engine
class CustomSimulationEngine : public BBP::detail::ISimulationEngine {
public:
    bool initialize(const BBP::DryRunConfig& config) override {
        // Custom initialization logic
        return true;
    }
    
    BBP::PerformanceProfile simulateStage(const BBP::SimulationStage& stage) override {
        // Custom stage simulation logic
        BBP::PerformanceProfile profile;
        profile.stage_id = stage.stage_id;
        
        // Your custom performance modeling here
        profile.wall_time = std::chrono::milliseconds(stage.estimated_duration.count() * 1.2);
        profile.cpu_time = std::chrono::milliseconds(profile.wall_time.count() * 0.8);
        profile.cpu_utilization = 65.0;
        profile.memory_peak_mb = 256;
        profile.efficiency_score = 0.85;
        
        return profile;
    }
    
    // Implement other required methods...
};

// Register custom engine
auto custom_engine = std::make_unique<CustomSimulationEngine>();
dry_run_system.registerSimulationEngine(std::move(custom_engine));
```

#### Custom Report Generator

```cpp
// Implement custom report generator
class CustomReportGenerator : public BBP::detail::IReportGenerator {
public:
    std::string generateReport(const BBP::DryRunResults& results) override {
        std::stringstream report;
        report << "=== CUSTOM SIMULATION REPORT ===" << std::endl;
        report << "Success: " << (results.success ? "YES" : "NO") << std::endl;
        report << "Duration: " << results.simulation_duration.count() << "ms" << std::endl;
        report << "Stages: " << results.execution_plan.stages.size() << std::endl;
        
        // Add custom analysis and formatting
        
        return report.str();
    }
    
    bool exportToFile(const std::string& report, const std::string& file_path) override {
        std::ofstream file(file_path);
        if (file.is_open()) {
            file << report;
            return true;
        }
        return false;
    }
};

// Register custom generator
auto custom_generator = std::make_unique<CustomReportGenerator>();
dry_run_system.registerReportGenerator("custom", std::move(custom_generator));

// Use custom report format
std::string custom_report = dry_run_system.generateReport(results, "custom");
```

### Advanced Features

#### Dependency Analysis and Optimization

```cpp
// Analyze stage dependencies
auto dependency_graph = BBP::DryRunUtils::generateDependencyGraph(stages);

// Check for circular dependencies
auto circular_deps = BBP::DryRunUtils::findCircularDependencies(stages);
if (!circular_deps.empty()) {
    std::cout << "Warning: Circular dependencies detected:" << std::endl;
    for (const auto& cycle : circular_deps) {
        std::cout << "  Cycle: ";
        for (size_t i = 0; i < cycle.size(); ++i) {
            if (i > 0) std::cout << " -> ";
            std::cout << cycle[i];
        }
        std::cout << std::endl;
    }
}

// Optimize execution plan
auto original_plan = dry_run_system.generateExecutionPlan(stages);
auto optimized_plan = BBP::DryRunUtils::optimizeExecutionPlan(original_plan);

std::cout << "Optimization Results:" << std::endl;
std::cout << "  Original parallelization factor: " << original_plan.parallelization_factor << std::endl;
std::cout << "  Optimized parallelization factor: " << optimized_plan.parallelization_factor << std::endl;
```

#### Performance Benchmarking

```cpp
// Benchmark different configurations
std::vector<BBP::DryRunConfig> configs = {
    BBP::DryRunUtils::createValidationOnlyConfig(),
    BBP::DryRunUtils::createFullSimulationConfig(),
    BBP::DryRunUtils::createPerformanceProfilingConfig()
};

for (const auto& test_config : configs) {
    dry_run_system.updateConfig(test_config);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto results = dry_run_system.execute(stages);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto benchmark_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    std::cout << "Config mode " << static_cast<int>(test_config.mode) 
              << " took " << benchmark_duration.count() << "ms" << std::endl;
}
```

### Performance Considerations

#### Simulation Performance

- **Configuration Optimization**: Choose appropriate simulation detail levels
- **Resource Estimation Accuracy**: Balance between accuracy and computation time
- **Parallel Simulation**: Utilize multi-threading for stage simulation where possible
- **Caching**: Cache simulation results for repeated executions
- **Memory Management**: Efficient memory usage during large pipeline simulations

#### Accuracy Tuning

- **Estimation Models**: Calibrate resource estimation models based on historical data
- **Confidence Intervals**: Use confidence levels to communicate estimation uncertainty
- **Validation Feedback**: Incorporate actual execution results to improve predictions
- **Custom Engines**: Implement domain-specific simulation engines for better accuracy

#### Scalability

- **Large Pipelines**: Handle pipelines with hundreds of stages efficiently
- **Distributed Simulation**: Support for distributed simulation across multiple nodes
- **Stream Processing**: Process large configuration files without loading entirely into memory
- **Progress Monitoring**: Real-time progress updates for long-running simulations

---

## Documentation Française

### Aperçu

Le Système de Simulation est un framework de simulation complet conçu pour le projet BB-Pipeline. Il fournit des capacités de simulation complètes sans exécuter les opérations réelles du pipeline, permettant aux utilisateurs de valider les configurations, estimer l'utilisation des ressources, analyser les profils de performance et optimiser les plans d'exécution avant d'exécuter les opérations réelles.

### Fonctionnalités Principales

#### Modes de Simulation Multiples

- **Validation Uniquement**: Validation complète de configuration et dépendances
- **Estimation de Ressources**: Analyse détaillée des besoins CPU, mémoire, disque et réseau
- **Simulation Complète**: Simulation bout-en-bout complète avec profilage de performance
- **Mode Interactif**: Simulation guidée par l'utilisateur avec invites de confirmation
- **Profilage de Performance**: Analyse de performance approfondie et identification des goulots d'étranglement

#### Validation Avancée

- **Validation de Configuration**: Validation de syntaxe et structure de configuration de pipeline
- **Analyse de Dépendances**: Détection de dépendances circulaires et optimisation d'ordre de résolution
- **Validation Système de Fichiers**: Vérifications d'existence et accessibilité des fichiers d'entrée/sortie
- **Validation de Contraintes de Ressources**: Disponibilité des ressources système et planification de capacité
- **Simulation Réseau**: Opérations réseau simulées avec estimation de bande passante

#### Estimation de Ressources Intelligente

- **Prédiction Utilisation CPU**: Estimations d'utilisation processeur basées sur la complexité des étapes
- **Analyse Empreinte Mémoire**: Prédiction d'usage RAM avec estimations de consommation de pointe
- **Besoins Espace Disque**: Évaluation des besoins de stockage pour les opérations d'entrée/sortie
- **Usage Bande Passante Réseau**: Analyse des besoins de transfert de données et charge réseau
- **Estimation Temps d'Exécution**: Prédictions de durée avec intervalles de confiance
- **Comptage Opérations E/S**: Fréquence et patterns des opérations d'entrée/sortie

#### Profilage de Performance

- **Analyse Étape par Étape**: Caractéristiques de performance des étapes individuelles du pipeline
- **Identification des Goulots d'Étranglement**: Contraintes de ressources système et opportunités d'optimisation
- **Analyse de Parallélisation**: Potentiel d'exécution concurrente et cartographie des dépendances
- **Détection du Chemin Critique**: Identification du chemin d'exécution le plus long pour optimisation
- **Notation d'Efficacité**: Métriques de performance et recommandations d'amélioration
- **Patterns d'Utilisation de Ressources**: Patterns d'usage sur différents types de ressources

#### Génération de Rapports

- **Rapports HTML**: Rapports web riches et interactifs avec graphiques et diagrammes
- **Export JSON**: Données lisibles par machine pour intégration avec outils externes
- **Formats Personnalisés**: Génération de rapports extensible avec générateurs pluggables
- **Résumés Exécutifs**: Aperçu de haut niveau pour parties prenantes et décideurs
- **Détails Techniques**: Information technique complète pour les ingénieurs

### Architecture

#### Composants Principaux

1. **DryRunSystem**: Classe d'orchestration principale gérant l'exécution de simulation
2. **DryRunSystemManager**: Gestionnaire singleton pour opérations de simulation globales
3. **ISimulationEngine**: Interface de moteur de simulation pluggable avec implémentation par défaut
4. **IReportGenerator**: Système de génération de rapports personnalisable
5. **AutoDryRunGuard**: Helper RAII pour gestion automatique de simulation

### Exemples d'Usage

#### Configuration et Installation de Base

```cpp
#include "orchestrator/dry_run_system.hpp"

// Crée la configuration par défaut
BBP::DryRunConfig config = BBP::DryRunUtils::createDefaultConfig();

// Personnalise la configuration
config.mode = BBP::DryRunMode::FULL_SIMULATION;
config.detail_level = BBP::SimulationDetail::DETAILED;
config.enable_resource_estimation = true;
config.enable_performance_profiling = true;
config.generate_report = true;
config.report_output_path = "./rapport_simulation.html";
config.timeout = std::chrono::minutes(10);

// Crée et initialise le système de simulation
BBP::DryRunSystem dry_run_system(config);
dry_run_system.initialize();
```

#### Validation de Pipeline

```cpp
// Valide seulement la configuration du pipeline
std::string config_path = "/chemin/vers/config/pipeline.yaml";
auto problemes_validation = dry_run_system.validateConfiguration(config_path);

std::cout << "Résultats de Validation:" << std::endl;
for (const auto& probleme : problemes_validation) {
    std::cout << "[" << BBP::DryRunUtils::severityToString(probleme.severity) << "] "
              << probleme.category << ": " << probleme.message << std::endl;
    
    if (!probleme.suggestion.empty()) {
        std::cout << "  Suggestion: " << probleme.suggestion << std::endl;
    }
}

// Vérifie si la configuration est sûre à exécuter
bool sur_a_executer = std::none_of(problemes_validation.begin(), problemes_validation.end(),
    [](const auto& probleme) {
        return probleme.severity == BBP::ValidationSeverity::CRITICAL || 
               probleme.severity == BBP::ValidationSeverity::ERROR;
    });

if (sur_a_executer) {
    std::cout << "La configuration est valide et sûre à exécuter." << std::endl;
} else {
    std::cout << "La configuration a des problèmes critiques qui doivent être résolus." << std::endl;
}
```

#### Estimation de Ressources

```cpp
// Charge les étapes du pipeline depuis la configuration
auto stages = BBP::DryRunUtils::loadStagesFromConfig(config_path);

// Estime les besoins en ressources
auto estimations_ressources = dry_run_system.estimateResources(stages);

std::cout << "Estimations de Ressources:" << std::endl;
for (const auto& [type_ressource, estimation] : estimations_ressources) {
    std::cout << "  " << BBP::DryRunUtils::resourceTypeToString(type_ressource) 
              << ": " << estimation.estimated_value << " " << estimation.unit
              << " (confiance: " << estimation.confidence_percentage << "%)" << std::endl;
    std::cout << "    Plage: " << estimation.minimum_value 
              << " - " << estimation.maximum_value << " " << estimation.unit << std::endl;
}

// Vérifie les contraintes de ressources
auto estimation_memoire = estimations_ressources.find(BBP::ResourceType::MEMORY_USAGE);
if (estimation_memoire != estimations_ressources.end() && 
    estimation_memoire->second.estimated_value > 1000.0) { // > 1GB
    std::cout << "Attention: Utilisation mémoire élevée attendue. Considérez l'optimisation." << std::endl;
}
```

### API Reference

#### Classe DryRunSystem

**Constructeur**
```cpp
explicit DryRunSystem(const DryRunConfig& config = DryRunConfig{});
```

**Méthodes Principales**
```cpp
bool initialize();
void shutdown();
DryRunResults execute(const std::vector<SimulationStage>& stages);
DryRunResults executeForPipeline(const std::string& pipeline_config_path);
std::vector<ValidationIssue> validateConfiguration(const std::string& config_path);
std::map<ResourceType, ResourceEstimate> estimateResources(const std::vector<SimulationStage>& stages);
ExecutionPlan generateExecutionPlan(const std::vector<SimulationStage>& stages);
PerformanceProfile simulateStage(const SimulationStage& stage);
```

**Méthodes de Gestion**
```cpp
bool runInteractiveMode(const ExecutionPlan& plan);
std::string generateReport(const DryRunResults& results, const std::string& format = "html");
bool exportReport(const DryRunResults& results, const std::string& file_path, const std::string& format = "html");
void updateConfig(const DryRunConfig& config);
const DryRunConfig& getConfig() const;
```

**Callbacks**
```cpp
void setProgressCallback(std::function<void(const std::string&, double)> callback);
void setValidationCallback(std::function<void(const ValidationIssue&)> callback);
void setStageCallback(std::function<void(const std::string&, const PerformanceProfile&)> callback);
```

**Enregistrement d'Extensions**
```cpp
void registerSimulationEngine(std::unique_ptr<detail::ISimulationEngine> engine);
void registerReportGenerator(const std::string& format, std::unique_ptr<detail::IReportGenerator> generator);
```

#### Classe DryRunSystemManager

```cpp
static DryRunSystemManager& getInstance();
bool initialize(const DryRunConfig& config = DryRunConfig{});
void shutdown();
DryRunSystem& getDryRunSystem();
std::vector<ValidationIssue> quickValidate(const std::string& config_path);
std::map<ResourceType, ResourceEstimate> getResourceEstimates(const std::string& config_path);
ExecutionPlan generatePreview(const std::string& config_path);
bool checkSystemReadiness(const std::string& config_path);
```

#### Classe AutoDryRunGuard

```cpp
AutoDryRunGuard(const std::string& config_path, DryRunMode mode = DryRunMode::VALIDATE_ONLY);
AutoDryRunGuard(const std::vector<SimulationStage>& stages, DryRunMode mode = DryRunMode::VALIDATE_ONLY);
DryRunResults execute();
bool isSafeToExecute() const;
std::vector<ValidationIssue> getValidationIssues() const;
ExecutionPlan getExecutionPlan() const;
```

### Gestion d'Erreurs

Le système de simulation fournit une gestion d'erreurs complète à travers :

- **Sécurité d'Exception**: Toutes les opérations sont sûres aux exceptions avec nettoyage approprié des ressources
- **Validation**: Validation d'entrée pour toutes les méthodes publiques
- **Logging**: Logging détaillé des erreurs et tentatives de récupération
- **Dégradation Gracieuse**: Le système continue de fonctionner même si la simulation échoue
- **Vérification de Récupération**: Vérification automatique de l'intégrité de l'état récupéré

### Thread Safety

Le système de simulation est complètement thread-safe :

- **Accès Concurrent**: Plusieurs threads peuvent accéder en sécurité à la même instance DryRunSystem
- **Opérations Atomiques**: Tous les changements d'état sont atomiques
- **Chemins Sans Verrous**: Les opérations en lecture seule utilisent des implémentations sans verrous quand possible
- **Prévention de Deadlock**: Ordonnancement de verrous soigneux pour éviter les scénarios de deadlock

### Intégration avec BB-Pipeline

Le système de simulation s'intègre parfaitement avec le framework BB-Pipeline :

- **Intégration Pipeline Engine**: Intégration directe avec PipelineEngine pour simulation automatique
- **Compatibilité Modules CSV**: Préserve l'état de traitement CSV et résultats intermédiaires
- **Système de Configuration**: Utilise la gestion de configuration de BB-Pipeline
- **Intégration Logging**: Utilise le système de logging structuré de BB-Pipeline

### Considérations de Performance

#### Performance de Simulation

- **Optimisation Configuration**: Choisir des niveaux de détail de simulation appropriés
- **Précision Estimation Ressources**: Équilibre entre précision et temps de calcul
- **Simulation Parallèle**: Utilise le multi-threading pour simulation d'étapes quand possible
- **Mise en Cache**: Met en cache les résultats de simulation pour exécutions répétées
- **Gestion Mémoire**: Usage efficace de la mémoire pendant les simulations de gros pipelines

#### Réglage de Précision

- **Modèles d'Estimation**: Calibre les modèles d'estimation de ressources basés sur données historiques
- **Intervalles de Confiance**: Utilise les niveaux de confiance pour communiquer l'incertitude d'estimation
- **Feedback de Validation**: Incorpore les résultats d'exécution réels pour améliorer les prédictions
- **Moteurs Personnalisés**: Implémente des moteurs de simulation spécifiques au domaine pour meilleure précision

#### Évolutivité

- **Gros Pipelines**: Gère les pipelines avec des centaines d'étapes efficacement
- **Simulation Distribuée**: Support pour simulation distribuée sur plusieurs nœuds
- **Traitement de Flux**: Traite les gros fichiers de configuration sans charger entièrement en mémoire
- **Monitoring de Progression**: Mises à jour de progression en temps réel pour simulations de longue durée

### Troubleshooting

#### Problèmes Courants

**Échec de Validation de Configuration**
- Vérifiez la syntaxe YAML/JSON de la configuration
- Validez les chemins de fichiers et permissions
- Assurez-vous que toutes les dépendances requises sont spécifiées

**Estimations de Ressources Inexactes**
- Calibrez les paramètres du moteur de simulation
- Fournissez des métadonnées de complexité d'étape plus précises
- Utilisez des moteurs de simulation personnalisés pour domaines spécifiques

**Performance de Simulation Lente**
- Réduisez le niveau de détail de simulation
- Désactivez le profilage de performance pour validation simple
- Utilisez la mise en cache pour configurations répétées

**Problèmes de Génération de Rapport**
- Vérifiez les permissions d'écriture du répertoire de sortie
- Assurez-vous que l'espace disque suffisant est disponible
- Validez le format de rapport demandé

#### Information de Debug

Activez le logging détaillé pour obtenir une information de debug complète :

```cpp
dry_run_system.setDetailedLogging(true);
```

Vérifiez les statistiques pour des insights de performance :

```cpp
auto stats = dry_run_system.getSimulationStatistics();
for (const auto& [key, value] : stats) {
    std::cout << key << ": " << value << std::endl;
}
```