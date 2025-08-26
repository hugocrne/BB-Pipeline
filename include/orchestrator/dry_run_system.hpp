// EN: Dry Run System for BB-Pipeline - Complete simulation without real execution
// FR: Système de Simulation pour BB-Pipeline - Simulation complète sans exécution réelle

#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <nlohmann/json.hpp>

namespace BBP {

// EN: Forward declarations for dependency injection
// FR: Déclarations forward pour l'injection de dépendances
class Logger;
class ThreadPool;
class PipelineEngine;
class ConfigManager;

// EN: Dry run execution modes
// FR: Modes d'exécution de simulation
enum class DryRunMode {
    VALIDATE_ONLY,      // EN: Only validate configuration and dependencies / FR: Valide seulement la configuration et dépendances
    ESTIMATE_RESOURCES, // EN: Estimate resource usage and execution time / FR: Estime l'usage des ressources et temps d'exécution
    FULL_SIMULATION,    // EN: Complete simulation with detailed logging / FR: Simulation complète avec logging détaillé
    INTERACTIVE,        // EN: Interactive mode with user confirmations / FR: Mode interactif avec confirmations utilisateur
    PERFORMANCE_PROFILE // EN: Performance profiling simulation / FR: Simulation de profilage performance
};

// EN: Simulation detail levels
// FR: Niveaux de détail de simulation
enum class SimulationDetail {
    MINIMAL = 0,    // EN: Basic validation and summary / FR: Validation de base et résumé
    STANDARD = 1,   // EN: Standard simulation with key metrics / FR: Simulation standard avec métriques clés
    DETAILED = 2,   // EN: Detailed simulation with stage-by-stage analysis / FR: Simulation détaillée avec analyse étape par étape
    VERBOSE = 3     // EN: Full verbose simulation with all possible information / FR: Simulation complète avec toute l'information possible
};

// EN: Resource estimation types
// FR: Types d'estimation de ressources
enum class ResourceType {
    CPU_USAGE,          // EN: CPU utilization estimation / FR: Estimation utilisation CPU
    MEMORY_USAGE,       // EN: Memory consumption estimation / FR: Estimation consommation mémoire
    DISK_SPACE,         // EN: Disk space requirements / FR: Besoins espace disque
    NETWORK_BANDWIDTH,  // EN: Network bandwidth usage / FR: Usage bande passante réseau
    EXECUTION_TIME,     // EN: Total execution time estimate / FR: Estimation temps d'exécution total
    IO_OPERATIONS      // EN: Input/output operations count / FR: Nombre d'opérations entrée/sortie
};

// EN: Validation result severity levels
// FR: Niveaux de gravité des résultats de validation
enum class ValidationSeverity {
    INFO,       // EN: Informational message / FR: Message informatif
    WARNING,    // EN: Warning that might affect execution / FR: Avertissement qui pourrait affecter l'exécution
    ERROR,      // EN: Error that prevents execution / FR: Erreur qui empêche l'exécution
    CRITICAL    // EN: Critical error requiring immediate attention / FR: Erreur critique nécessitant attention immédiate
};

// EN: Simulation stage information
// FR: Informations d'étape de simulation
struct SimulationStage {
    std::string stage_id;                               // EN: Unique stage identifier / FR: Identifiant unique d'étape
    std::string stage_name;                             // EN: Human-readable stage name / FR: Nom d'étape lisible
    std::string description;                            // EN: Stage description / FR: Description de l'étape
    std::vector<std::string> dependencies;              // EN: Stage dependencies / FR: Dépendances d'étape
    std::chrono::milliseconds estimated_duration;       // EN: Estimated execution time / FR: Temps d'exécution estimé
    std::map<ResourceType, double> resource_estimates;  // EN: Resource usage estimates / FR: Estimations d'usage des ressources
    std::vector<std::string> input_files;               // EN: Expected input files / FR: Fichiers d'entrée attendus
    std::vector<std::string> output_files;              // EN: Expected output files / FR: Fichiers de sortie attendus
    bool is_optional;                                   // EN: Whether stage is optional / FR: Si l'étape est optionnelle
    bool can_run_parallel;                              // EN: Whether stage supports parallel execution / FR: Si l'étape supporte l'exécution parallèle
    std::map<std::string, std::string> metadata;        // EN: Additional stage metadata / FR: Métadonnées additionnelles d'étape
};

// EN: Validation issue information
// FR: Informations de problème de validation
struct ValidationIssue {
    ValidationSeverity severity;                        // EN: Issue severity level / FR: Niveau de gravité du problème
    std::string category;                              // EN: Issue category / FR: Catégorie du problème
    std::string message;                               // EN: Issue description / FR: Description du problème
    std::string stage_id;                              // EN: Related stage (if applicable) / FR: Étape liée (si applicable)
    std::string suggestion;                            // EN: Suggested resolution / FR: Résolution suggérée
    std::chrono::system_clock::time_point timestamp;   // EN: Issue detection time / FR: Heure de détection du problème
    std::map<std::string, std::string> context;        // EN: Additional context information / FR: Informations de contexte additionnelles
};

// EN: Resource estimation results
// FR: Résultats d'estimation de ressources
struct ResourceEstimate {
    ResourceType type;                                  // EN: Resource type / FR: Type de ressource
    double estimated_value;                            // EN: Estimated resource usage / FR: Usage estimé de la ressource
    double confidence_percentage;                       // EN: Confidence in the estimate / FR: Confiance dans l'estimation
    std::string unit;                                  // EN: Value unit (MB, seconds, etc.) / FR: Unité de valeur (MB, secondes, etc.)
    double minimum_value;                              // EN: Minimum expected value / FR: Valeur minimale attendue
    double maximum_value;                              // EN: Maximum expected value / FR: Valeur maximale attendue
    std::string estimation_method;                     // EN: How the estimate was calculated / FR: Comment l'estimation a été calculée
    std::vector<std::string> assumptions;              // EN: Assumptions made in estimation / FR: Hypothèses faites dans l'estimation
};

// EN: Execution plan information
// FR: Informations du plan d'exécution
struct ExecutionPlan {
    std::vector<SimulationStage> stages;               // EN: Ordered list of execution stages / FR: Liste ordonnée des étapes d'exécution
    std::vector<std::vector<std::string>> parallel_groups; // EN: Groups of stages that can run in parallel / FR: Groupes d'étapes pouvant s'exécuter en parallèle
    std::chrono::milliseconds total_estimated_time;    // EN: Total estimated execution time / FR: Temps d'exécution total estimé
    std::map<ResourceType, ResourceEstimate> resource_summary; // EN: Summary of resource requirements / FR: Résumé des besoins en ressources
    std::string critical_path;                         // EN: Critical path through the pipeline / FR: Chemin critique à travers le pipeline
    double parallelization_factor;                     // EN: Potential speedup from parallelization / FR: Accélération potentielle de la parallélisation
    std::vector<std::string> optimization_suggestions; // EN: Suggestions for optimization / FR: Suggestions d'optimisation
};

// EN: Dry run configuration
// FR: Configuration de simulation
struct DryRunConfig {
    DryRunMode mode;                                   // EN: Dry run execution mode / FR: Mode d'exécution de simulation
    SimulationDetail detail_level;                    // EN: Level of simulation detail / FR: Niveau de détail de simulation
    bool enable_resource_estimation;                  // EN: Enable resource usage estimation / FR: Active l'estimation d'usage des ressources
    bool enable_performance_profiling;                // EN: Enable performance profiling / FR: Active le profilage de performance
    bool enable_dependency_validation;                // EN: Enable dependency validation / FR: Active la validation des dépendances
    bool enable_file_validation;                      // EN: Enable input/output file validation / FR: Active la validation des fichiers entrée/sortie
    bool enable_network_simulation;                   // EN: Enable network operation simulation / FR: Active la simulation des opérations réseau
    bool show_progress;                               // EN: Show progress during simulation / FR: Affiche la progression pendant simulation
    bool interactive_mode;                            // EN: Enable interactive confirmations / FR: Active les confirmations interactives
    bool generate_report;                             // EN: Generate detailed report / FR: Génère un rapport détaillé
    std::string report_output_path;                   // EN: Output path for generated report / FR: Chemin de sortie pour le rapport généré
    std::chrono::seconds timeout;                     // EN: Maximum simulation time / FR: Temps maximum de simulation
    std::unordered_set<std::string> excluded_stages;  // EN: Stages to exclude from simulation / FR: Étapes à exclure de la simulation
    std::map<std::string, std::string> custom_parameters; // EN: Custom simulation parameters / FR: Paramètres de simulation personnalisés
};

// EN: Dry run execution results
// FR: Résultats d'exécution de simulation
struct DryRunResults {
    bool success;                                      // EN: Whether simulation completed successfully / FR: Si la simulation s'est terminée avec succès
    DryRunMode mode_executed;                          // EN: Mode that was executed / FR: Mode qui a été exécuté
    std::chrono::system_clock::time_point start_time;  // EN: Simulation start time / FR: Heure de début de simulation
    std::chrono::system_clock::time_point end_time;    // EN: Simulation end time / FR: Heure de fin de simulation
    std::chrono::milliseconds simulation_duration;     // EN: Time taken for simulation / FR: Temps pris pour la simulation
    ExecutionPlan execution_plan;                      // EN: Generated execution plan / FR: Plan d'exécution généré
    std::vector<ValidationIssue> validation_issues;    // EN: Issues found during validation / FR: Problèmes trouvés pendant la validation
    std::map<ResourceType, ResourceEstimate> resource_estimates; // EN: Resource usage estimates / FR: Estimations d'usage des ressources
    std::map<std::string, nlohmann::json> stage_details; // EN: Detailed information per stage / FR: Informations détaillées par étape
    std::vector<std::string> warnings;                // EN: General warnings / FR: Avertissements généraux
    std::vector<std::string> recommendations;          // EN: Optimization recommendations / FR: Recommandations d'optimisation
    std::string report_path;                          // EN: Path to generated report / FR: Chemin vers le rapport généré
};

// EN: Performance profile information
// FR: Informations de profil de performance
struct PerformanceProfile {
    std::string stage_id;                              // EN: Stage identifier / FR: Identifiant d'étape
    std::chrono::milliseconds cpu_time;               // EN: CPU time estimate / FR: Estimation temps CPU
    std::chrono::milliseconds wall_time;              // EN: Wall clock time estimate / FR: Estimation temps réel
    double cpu_utilization;                           // EN: CPU utilization percentage / FR: Pourcentage d'utilisation CPU
    size_t memory_peak_mb;                            // EN: Peak memory usage in MB / FR: Pic d'utilisation mémoire en MB
    size_t disk_reads_mb;                             // EN: Disk reads in MB / FR: Lectures disque en MB
    size_t disk_writes_mb;                            // EN: Disk writes in MB / FR: Écritures disque en MB
    size_t network_bytes;                             // EN: Network bytes transferred / FR: Octets réseau transférés
    double efficiency_score;                          // EN: Stage efficiency score (0-1) / FR: Score d'efficacité d'étape (0-1)
    std::vector<std::string> bottlenecks;             // EN: Identified bottlenecks / FR: Goulots d'étranglement identifiés
};

namespace detail {
    // EN: Internal simulation engine interface
    // FR: Interface interne du moteur de simulation
    class ISimulationEngine {
    public:
        virtual ~ISimulationEngine() = default;
        
        // EN: Initialize simulation engine
        // FR: Initialise le moteur de simulation
        virtual bool initialize(const DryRunConfig& config) = 0;
        
        // EN: Simulate stage execution
        // FR: Simule l'exécution d'étape
        virtual PerformanceProfile simulateStage(const SimulationStage& stage) = 0;
        
        // EN: Validate stage configuration
        // FR: Valide la configuration d'étape
        virtual std::vector<ValidationIssue> validateStage(const SimulationStage& stage) = 0;
        
        // EN: Estimate resource usage
        // FR: Estime l'usage des ressources
        virtual ResourceEstimate estimateResource(const SimulationStage& stage, ResourceType type) = 0;
        
        // EN: Generate execution plan
        // FR: Génère le plan d'exécution
        virtual ExecutionPlan generateExecutionPlan(const std::vector<SimulationStage>& stages) = 0;
    };
    
    // EN: Default simulation engine implementation
    // FR: Implémentation par défaut du moteur de simulation
    class DefaultSimulationEngine : public ISimulationEngine {
    public:
        bool initialize(const DryRunConfig& config) override;
        PerformanceProfile simulateStage(const SimulationStage& stage) override;
        std::vector<ValidationIssue> validateStage(const SimulationStage& stage) override;
        ResourceEstimate estimateResource(const SimulationStage& stage, ResourceType type) override;
        ExecutionPlan generateExecutionPlan(const std::vector<SimulationStage>& stages) override;
        
    private:
        DryRunConfig config_;
        std::mt19937 random_generator_;
        
        // EN: Helper methods for estimation
        // FR: Méthodes helper pour estimation
        double estimateStageComplexity(const SimulationStage& stage);
        std::chrono::milliseconds estimateExecutionTime(const SimulationStage& stage);
        size_t estimateMemoryUsage(const SimulationStage& stage);
        std::vector<std::string> findOptimizations(const std::vector<SimulationStage>& stages);
    };
    
    // EN: Report generator interface
    // FR: Interface de générateur de rapport
    class IReportGenerator {
    public:
        virtual ~IReportGenerator() = default;
        
        // EN: Generate simulation report
        // FR: Génère le rapport de simulation
        virtual std::string generateReport(const DryRunResults& results) = 0;
        
        // EN: Export report to file
        // FR: Exporte le rapport vers un fichier
        virtual bool exportToFile(const std::string& report, const std::string& file_path) = 0;
    };
    
    // EN: HTML report generator
    // FR: Générateur de rapport HTML
    class HtmlReportGenerator : public IReportGenerator {
    public:
        std::string generateReport(const DryRunResults& results) override;
        bool exportToFile(const std::string& report, const std::string& file_path) override;
        
    private:
        std::string generateHtmlHeader();
        std::string generateExecutionPlanSection(const ExecutionPlan& plan);
        std::string generateValidationSection(const std::vector<ValidationIssue>& issues);
        std::string generateResourceSection(const std::map<ResourceType, ResourceEstimate>& estimates);
        std::string generateRecommendationsSection(const std::vector<std::string>& recommendations);
        std::string generateHtmlFooter();
    };
    
    // EN: JSON report generator
    // FR: Générateur de rapport JSON
    class JsonReportGenerator : public IReportGenerator {
    public:
        std::string generateReport(const DryRunResults& results) override;
        bool exportToFile(const std::string& report, const std::string& file_path) override;
        
    private:
        nlohmann::json convertResultsToJson(const DryRunResults& results);
    };
}

// EN: Main Dry Run System class - Handles complete simulation without execution
// FR: Classe principale du système de simulation - Gère la simulation complète sans exécution
class DryRunSystem {
public:
    // EN: Constructor with configuration
    // FR: Constructeur avec configuration
    explicit DryRunSystem(const DryRunConfig& config = DryRunConfig{});
    
    // EN: Destructor
    // FR: Destructeur
    ~DryRunSystem();
    
    // EN: Non-copyable and non-movable
    // FR: Non-copiable et non-déplaçable
    DryRunSystem(const DryRunSystem&) = delete;
    DryRunSystem& operator=(const DryRunSystem&) = delete;
    DryRunSystem(DryRunSystem&&) = delete;
    DryRunSystem& operator=(DryRunSystem&&) = delete;
    
    // EN: Initialize dry run system
    // FR: Initialise le système de simulation
    bool initialize();
    
    // EN: Shutdown dry run system
    // FR: Arrête le système de simulation
    void shutdown();
    
    // EN: Execute dry run simulation
    // FR: Exécute la simulation
    DryRunResults execute(const std::vector<SimulationStage>& stages);
    
    // EN: Execute dry run for pipeline configuration
    // FR: Exécute la simulation pour configuration de pipeline
    DryRunResults executeForPipeline(const std::string& pipeline_config_path);
    
    // EN: Validate pipeline configuration only
    // FR: Valide seulement la configuration du pipeline
    std::vector<ValidationIssue> validateConfiguration(const std::string& config_path);
    
    // EN: Estimate resources for pipeline
    // FR: Estime les ressources pour le pipeline
    std::map<ResourceType, ResourceEstimate> estimateResources(const std::vector<SimulationStage>& stages);
    
    // EN: Generate execution plan
    // FR: Génère le plan d'exécution
    ExecutionPlan generateExecutionPlan(const std::vector<SimulationStage>& stages);
    
    // EN: Simulate single stage
    // FR: Simule une seule étape
    PerformanceProfile simulateStage(const SimulationStage& stage);
    
    // EN: Interactive mode - ask user for confirmations
    // FR: Mode interactif - demande confirmations à l'utilisateur
    bool runInteractiveMode(const ExecutionPlan& plan);
    
    // EN: Generate detailed report
    // FR: Génère un rapport détaillé
    std::string generateReport(const DryRunResults& results, const std::string& format = "html");
    
    // EN: Export report to file
    // FR: Exporte le rapport vers un fichier
    bool exportReport(const DryRunResults& results, const std::string& file_path, const std::string& format = "html");
    
    // EN: Update configuration
    // FR: Met à jour la configuration
    void updateConfig(const DryRunConfig& config);
    
    // EN: Get current configuration
    // FR: Obtient la configuration actuelle
    const DryRunConfig& getConfig() const { return config_; }
    
    // EN: Set progress callback
    // FR: Définit le callback de progression
    void setProgressCallback(std::function<void(const std::string&, double)> callback);
    
    // EN: Set validation callback
    // FR: Définit le callback de validation
    void setValidationCallback(std::function<void(const ValidationIssue&)> callback);
    
    // EN: Set stage completion callback
    // FR: Définit le callback de completion d'étape
    void setStageCallback(std::function<void(const std::string&, const PerformanceProfile&)> callback);
    
    // EN: Register custom simulation engine
    // FR: Enregistre un moteur de simulation personnalisé
    void registerSimulationEngine(std::unique_ptr<detail::ISimulationEngine> engine);
    
    // EN: Register custom report generator
    // FR: Enregistre un générateur de rapport personnalisé
    void registerReportGenerator(const std::string& format, std::unique_ptr<detail::IReportGenerator> generator);
    
    // EN: Enable/disable detailed logging
    // FR: Active/désactive le logging détaillé
    void setDetailedLogging(bool enabled);
    
    // EN: Get simulation statistics
    // FR: Obtient les statistiques de simulation
    std::map<std::string, double> getSimulationStatistics() const;
    
    // EN: Reset simulation statistics
    // FR: Remet à zéro les statistiques de simulation
    void resetStatistics();

private:
    // EN: PIMPL to hide implementation details
    // FR: PIMPL pour cacher les détails d'implémentation
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    
    // EN: Configuration
    // FR: Configuration
    DryRunConfig config_;
    
    // EN: Thread safety
    // FR: Thread safety
    mutable std::mutex mutex_;
};

// EN: Dry Run System Manager - Singleton for global simulation management
// FR: Gestionnaire du système de simulation - Singleton pour la gestion globale de simulation
class DryRunSystemManager {
public:
    // EN: Get singleton instance
    // FR: Obtient l'instance singleton
    static DryRunSystemManager& getInstance();
    
    // EN: Initialize with configuration
    // FR: Initialise avec la configuration
    bool initialize(const DryRunConfig& config = DryRunConfig{});
    
    // EN: Shutdown manager
    // FR: Arrête le gestionnaire
    void shutdown();
    
    // EN: Get dry run system
    // FR: Obtient le système de simulation
    DryRunSystem& getDryRunSystem();
    
    // EN: Execute quick validation
    // FR: Exécute une validation rapide
    std::vector<ValidationIssue> quickValidate(const std::string& config_path);
    
    // EN: Get resource estimates
    // FR: Obtient les estimations de ressources
    std::map<ResourceType, ResourceEstimate> getResourceEstimates(const std::string& config_path);
    
    // EN: Generate execution preview
    // FR: Génère un aperçu d'exécution
    ExecutionPlan generatePreview(const std::string& config_path);
    
    // EN: Check system readiness for execution
    // FR: Vérifie la préparation du système pour l'exécution
    bool checkSystemReadiness(const std::string& config_path);

private:
    // EN: Private constructor for singleton
    // FR: Constructeur privé pour singleton
    DryRunSystemManager() = default;
    
    // EN: Non-copyable and non-movable
    // FR: Non-copiable et non-déplaçable
    DryRunSystemManager(const DryRunSystemManager&) = delete;
    DryRunSystemManager& operator=(const DryRunSystemManager&) = delete;
    DryRunSystemManager(DryRunSystemManager&&) = delete;
    DryRunSystemManager& operator=(DryRunSystemManager&&) = delete;
    
    std::unique_ptr<DryRunSystem> dry_run_system_;
    mutable std::mutex manager_mutex_;
    bool initialized_ = false;
};

// EN: RAII helper for automatic dry run execution
// FR: Helper RAII pour l'exécution automatique de simulation
class AutoDryRunGuard {
public:
    // EN: Constructor with pipeline configuration
    // FR: Constructeur avec configuration de pipeline
    explicit AutoDryRunGuard(const std::string& config_path, DryRunMode mode = DryRunMode::VALIDATE_ONLY);
    
    // EN: Constructor with stages
    // FR: Constructeur avec étapes
    AutoDryRunGuard(const std::vector<SimulationStage>& stages, DryRunMode mode = DryRunMode::VALIDATE_ONLY);
    
    // EN: Destructor - executes dry run if not already done
    // FR: Destructeur - exécute la simulation si pas encore fait
    ~AutoDryRunGuard();
    
    // EN: Execute dry run manually
    // FR: Exécute la simulation manuellement
    DryRunResults execute();
    
    // EN: Check if execution would be safe
    // FR: Vérifie si l'exécution serait sûre
    bool isSafeToExecute() const;
    
    // EN: Get validation issues
    // FR: Obtient les problèmes de validation
    std::vector<ValidationIssue> getValidationIssues() const;
    
    // EN: Get execution plan
    // FR: Obtient le plan d'exécution
    ExecutionPlan getExecutionPlan() const;

private:
    DryRunSystem dry_run_system_;
    std::string config_path_;
    std::vector<SimulationStage> stages_;
    DryRunMode mode_;
    mutable std::optional<DryRunResults> cached_results_;
    bool executed_;
};

// EN: Utility functions for dry run operations
// FR: Fonctions utilitaires pour les opérations de simulation
namespace DryRunUtils {
    
    // EN: Create default dry run configuration
    // FR: Crée la configuration de simulation par défaut
    DryRunConfig createDefaultConfig();
    
    // EN: Create validation-only configuration
    // FR: Crée la configuration validation uniquement
    DryRunConfig createValidationOnlyConfig();
    
    // EN: Create full simulation configuration
    // FR: Crée la configuration de simulation complète
    DryRunConfig createFullSimulationConfig();
    
    // EN: Create performance profiling configuration
    // FR: Crée la configuration de profilage de performance
    DryRunConfig createPerformanceProfilingConfig();
    
    // EN: Load stages from pipeline configuration
    // FR: Charge les étapes depuis la configuration de pipeline
    std::vector<SimulationStage> loadStagesFromConfig(const std::string& config_path);
    
    // EN: Convert validation severity to string
    // FR: Convertit la gravité de validation en chaîne
    std::string severityToString(ValidationSeverity severity);
    
    // EN: Convert resource type to string
    // FR: Convertit le type de ressource en chaîne
    std::string resourceTypeToString(ResourceType type);
    
    // EN: Parse dry run mode from string
    // FR: Parse le mode de simulation depuis une chaîne
    std::optional<DryRunMode> parseDryRunMode(const std::string& mode_str);
    
    // EN: Validate dry run configuration
    // FR: Valide la configuration de simulation
    bool validateDryRunConfig(const DryRunConfig& config);
    
    // EN: Estimate total execution time
    // FR: Estime le temps d'exécution total
    std::chrono::milliseconds estimateTotalExecutionTime(const std::vector<SimulationStage>& stages);
    
    // EN: Check file accessibility
    // FR: Vérifie l'accessibilité des fichiers
    bool checkFileAccessibility(const std::string& file_path);
    
    // EN: Generate stage dependency graph
    // FR: Génère le graphe de dépendances d'étapes
    std::map<std::string, std::vector<std::string>> generateDependencyGraph(const std::vector<SimulationStage>& stages);
    
    // EN: Find circular dependencies
    // FR: Trouve les dépendances circulaires
    std::vector<std::vector<std::string>> findCircularDependencies(const std::vector<SimulationStage>& stages);
    
    // EN: Optimize execution plan
    // FR: Optimise le plan d'exécution
    ExecutionPlan optimizeExecutionPlan(const ExecutionPlan& original_plan);
    
} // namespace DryRunUtils

// EN: Convenience macros for dry run operations
// FR: Macros de convenance pour les opérations de simulation

#define DRY_RUN_VALIDATE(config_path) \
    DryRunSystemManager::getInstance().quickValidate(config_path)

#define DRY_RUN_ESTIMATE_RESOURCES(config_path) \
    DryRunSystemManager::getInstance().getResourceEstimates(config_path)

#define DRY_RUN_PREVIEW(config_path) \
    DryRunSystemManager::getInstance().generatePreview(config_path)

#define DRY_RUN_AUTO_GUARD(config_path, mode) \
    AutoDryRunGuard _auto_dry_run_guard(config_path, mode)

#define DRY_RUN_CHECK_READY(config_path) \
    DryRunSystemManager::getInstance().checkSystemReadiness(config_path)

} // namespace BBP