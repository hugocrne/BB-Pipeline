// EN: Stage Selector for BB-Pipeline - Individual module execution with validation
// FR: Sélecteur d'Étapes pour BB-Pipeline - Exécution de modules individuels avec validation

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <variant>
#include <nlohmann/json.hpp>

#include "orchestrator/pipeline_engine.hpp"
#include "infrastructure/logging/logger.hpp"
#include "infrastructure/config/config_manager.hpp"

namespace BBP {
namespace Orchestrator {

// EN: Forward declarations
// FR: Déclarations avancées
class StageSelector;
class StageDependencyAnalyzer;
class StageConstraintValidator;
class StageExecutionPlanner;

// EN: Stage selection criteria types
// FR: Types de critères de sélection d'étapes
enum class StageSelectionCriteria {
    BY_ID = 0,              // EN: Select by exact stage ID / FR: Sélection par ID exact d'étape
    BY_NAME = 1,            // EN: Select by stage name / FR: Sélection par nom d'étape
    BY_PATTERN = 2,         // EN: Select by regex pattern / FR: Sélection par motif regex
    BY_TAG = 3,             // EN: Select by stage tags / FR: Sélection par tags d'étape
    BY_PRIORITY = 4,        // EN: Select by priority level / FR: Sélection par niveau de priorité
    BY_DEPENDENCY = 5,      // EN: Select by dependency relationship / FR: Sélection par relation de dépendance
    BY_EXECUTION_TIME = 6,  // EN: Select by estimated execution time / FR: Sélection par temps d'exécution estimé
    BY_RESOURCE_USAGE = 7,  // EN: Select by resource requirements / FR: Sélection par besoins en ressources
    BY_SUCCESS_RATE = 8,    // EN: Select by historical success rate / FR: Sélection par taux de succès historique
    BY_CUSTOM = 9           // EN: Select by custom filter function / FR: Sélection par fonction de filtre personnalisée
};

// EN: Stage filtering modes
// FR: Modes de filtrage d'étapes
enum class StageFilterMode {
    INCLUDE = 0,    // EN: Include stages matching criteria / FR: Inclure les étapes correspondant aux critères
    EXCLUDE = 1,    // EN: Exclude stages matching criteria / FR: Exclure les étapes correspondant aux critères
    REQUIRE = 2     // EN: Require all criteria to be met / FR: Exiger que tous les critères soient satisfaits
};

// EN: Stage validation levels
// FR: Niveaux de validation d'étapes
enum class StageValidationLevel {
    NONE = 0,           // EN: No validation performed / FR: Aucune validation effectuée
    BASIC = 1,          // EN: Basic syntax and configuration validation / FR: Validation de syntaxe et configuration de base
    DEPENDENCIES = 2,   // EN: Validate dependencies and constraints / FR: Valider les dépendances et contraintes
    RESOURCES = 3,      // EN: Validate resource requirements / FR: Valider les exigences de ressources
    COMPATIBILITY = 4,  // EN: Validate stage compatibility / FR: Valider la compatibilité des étapes
    COMPREHENSIVE = 5   // EN: Full validation with all checks / FR: Validation complète avec toutes les vérifications
};

// EN: Stage execution constraints
// FR: Contraintes d'exécution d'étapes
enum class StageExecutionConstraint {
    NONE = 0,                   // EN: No constraints / FR: Aucune contrainte
    SEQUENTIAL_ONLY = 1,        // EN: Must execute sequentially / FR: Doit s'exécuter séquentiellement
    PARALLEL_SAFE = 2,          // EN: Safe for parallel execution / FR: Sûr pour l'exécution parallèle
    RESOURCE_INTENSIVE = 3,     // EN: Requires dedicated resources / FR: Nécessite des ressources dédiées
    NETWORK_DEPENDENT = 4,      // EN: Requires network connectivity / FR: Nécessite une connectivité réseau
    FILESYSTEM_DEPENDENT = 5,   // EN: Requires filesystem access / FR: Nécessite l'accès au système de fichiers
    MEMORY_INTENSIVE = 6,       // EN: High memory usage / FR: Usage mémoire élevé
    CPU_INTENSIVE = 7,          // EN: High CPU usage / FR: Usage CPU élevé
    EXCLUSIVE_ACCESS = 8,       // EN: Requires exclusive resource access / FR: Nécessite un accès exclusif aux ressources
    TIME_SENSITIVE = 9,         // EN: Timing constraints / FR: Contraintes de timing
    STATEFUL = 10              // EN: Maintains state between executions / FR: Maintient l'état entre les exécutions
};

// EN: Stage selection result status
// FR: Statut de résultat de sélection d'étapes
enum class StageSelectionStatus {
    SUCCESS = 0,                // EN: Selection completed successfully / FR: Sélection terminée avec succès
    PARTIAL_SUCCESS = 1,        // EN: Some stages selected with warnings / FR: Certaines étapes sélectionnées avec avertissements
    VALIDATION_FAILED = 2,      // EN: Validation failed for selected stages / FR: Validation échouée pour les étapes sélectionnées
    DEPENDENCY_ERROR = 3,       // EN: Dependency resolution failed / FR: Résolution des dépendances échouée
    CONSTRAINT_VIOLATION = 4,   // EN: Stage constraints violated / FR: Contraintes d'étapes violées
    RESOURCE_UNAVAILABLE = 5,   // EN: Required resources not available / FR: Ressources requises non disponibles
    CONFIGURATION_ERROR = 6,    // EN: Configuration error / FR: Erreur de configuration
    EMPTY_SELECTION = 7,        // EN: No stages selected / FR: Aucune étape sélectionnée
    CIRCULAR_DEPENDENCY = 8,    // EN: Circular dependency detected / FR: Dépendance circulaire détectée
    INCOMPATIBLE_STAGES = 9     // EN: Selected stages are incompatible / FR: Étapes sélectionnées incompatibles
};

// EN: Stage selection filter definition
// FR: Définition de filtre de sélection d'étapes
struct StageSelectionFilter {
    StageSelectionCriteria criteria;                        // EN: Selection criteria type / FR: Type de critères de sélection
    StageFilterMode mode = StageFilterMode::INCLUDE;        // EN: Filter mode / FR: Mode de filtre
    std::string value;                                       // EN: Filter value / FR: Valeur de filtre
    std::optional<std::regex> pattern;                      // EN: Regex pattern for pattern-based selection / FR: Motif regex pour sélection par motif
    std::set<std::string> tags;                             // EN: Tags for tag-based selection / FR: Tags pour sélection par tags
    PipelineStagePriority min_priority = PipelineStagePriority::LOW;    // EN: Minimum priority / FR: Priorité minimum
    PipelineStagePriority max_priority = PipelineStagePriority::CRITICAL; // EN: Maximum priority / FR: Priorité maximum
    std::chrono::seconds min_execution_time{0};             // EN: Minimum execution time / FR: Temps d'exécution minimum
    std::chrono::seconds max_execution_time{3600};          // EN: Maximum execution time / FR: Temps d'exécution maximum
    double min_success_rate = 0.0;                          // EN: Minimum success rate / FR: Taux de succès minimum
    double max_resource_usage = 100.0;                      // EN: Maximum resource usage / FR: Usage maximum des ressources
    std::function<bool(const PipelineStageConfig&)> custom_filter; // EN: Custom filter function / FR: Fonction de filtre personnalisée
    std::map<std::string, std::string> metadata_filters;    // EN: Metadata-based filters / FR: Filtres basés sur les métadonnées
    bool case_sensitive = false;                            // EN: Case sensitive matching / FR: Correspondance sensible à la casse
    bool exact_match = false;                               // EN: Exact match required / FR: Correspondance exacte requise
};

// EN: Stage selection configuration
// FR: Configuration de sélection d'étapes
struct StageSelectionConfig {
    std::vector<StageSelectionFilter> filters;                     // EN: Selection filters / FR: Filtres de sélection
    StageValidationLevel validation_level = StageValidationLevel::DEPENDENCIES; // EN: Validation level / FR: Niveau de validation
    bool include_dependencies = true;                              // EN: Include stage dependencies / FR: Inclure les dépendances d'étapes
    bool include_dependents = false;                               // EN: Include dependent stages / FR: Inclure les étapes dépendantes
    bool resolve_conflicts = true;                                 // EN: Automatically resolve conflicts / FR: Résoudre automatiquement les conflits
    bool optimize_execution_order = true;                          // EN: Optimize execution order / FR: Optimiser l'ordre d'exécution
    bool allow_partial_selection = false;                          // EN: Allow partial selection on errors / FR: Permettre sélection partielle sur erreurs
    std::set<StageExecutionConstraint> allowed_constraints;        // EN: Allowed execution constraints / FR: Contraintes d'exécution autorisées
    std::set<StageExecutionConstraint> forbidden_constraints;      // EN: Forbidden execution constraints / FR: Contraintes d'exécution interdites
    size_t max_selected_stages = 100;                             // EN: Maximum number of selected stages / FR: Nombre maximum d'étapes sélectionnées
    std::chrono::seconds selection_timeout{30};                    // EN: Selection operation timeout / FR: Délai d'expiration de sélection
    bool enable_caching = true;                                    // EN: Enable selection result caching / FR: Activer la mise en cache des résultats
    std::string cache_key_prefix = "stage_selection";              // EN: Cache key prefix / FR: Préfixe de clé de cache
    std::map<std::string, std::string> custom_properties;          // EN: Custom configuration properties / FR: Propriétés de configuration personnalisées
};

// EN: Stage execution constraint definition
// FR: Définition de contrainte d'exécution d'étapes
struct StageConstraintDefinition {
    StageExecutionConstraint constraint;                    // EN: Constraint type / FR: Type de contrainte
    std::string description;                                // EN: Human-readable description / FR: Description lisible
    bool is_mandatory = false;                              // EN: Mandatory constraint / FR: Contrainte obligatoire
    std::vector<StageExecutionConstraint> conflicts;        // EN: Conflicting constraints / FR: Contraintes conflictuelles
    std::vector<StageExecutionConstraint> dependencies;     // EN: Dependent constraints / FR: Contraintes dépendantes
    std::function<bool(const PipelineStageConfig&)> validator; // EN: Constraint validator function / FR: Fonction de validation de contrainte
    double resource_multiplier = 1.0;                      // EN: Resource usage multiplier / FR: Multiplicateur d'usage des ressources
    std::chrono::milliseconds overhead_time{0};            // EN: Execution overhead time / FR: Temps de surcharge d'exécution
    std::map<std::string, std::variant<int, double, std::string, bool>> parameters; // EN: Constraint parameters / FR: Paramètres de contrainte
};

// EN: Stage compatibility analysis result
// FR: Résultat d'analyse de compatibilité d'étapes
struct StageCompatibilityResult {
    bool are_compatible = true;                             // EN: Overall compatibility / FR: Compatibilité globale
    std::vector<std::string> compatible_stages;             // EN: List of compatible stages / FR: Liste des étapes compatibles
    std::vector<std::string> incompatible_stages;           // EN: List of incompatible stages / FR: Liste des étapes incompatibles
    std::map<std::string, std::vector<std::string>> conflicts; // EN: Conflicts between stages / FR: Conflits entre étapes
    std::vector<std::string> warnings;                     // EN: Compatibility warnings / FR: Avertissements de compatibilité
    std::vector<std::string> recommendations;              // EN: Recommendations for resolution / FR: Recommandations pour résolution
    double compatibility_score = 1.0;                      // EN: Overall compatibility score / FR: Score de compatibilité global
    std::map<std::string, double> stage_compatibility_scores; // EN: Per-stage compatibility scores / FR: Scores de compatibilité par étape
};

// EN: Stage selection result
// FR: Résultat de sélection d'étapes
struct StageSelectionResult {
    StageSelectionStatus status;                            // EN: Selection status / FR: Statut de sélection
    std::vector<std::string> selected_stage_ids;           // EN: Selected stage IDs / FR: IDs des étapes sélectionnées
    std::vector<PipelineStageConfig> selected_stages;      // EN: Selected stage configurations / FR: Configurations des étapes sélectionnées
    std::vector<std::string> execution_order;              // EN: Recommended execution order / FR: Ordre d'exécution recommandé
    std::vector<std::vector<std::string>> execution_levels; // EN: Parallel execution levels / FR: Niveaux d'exécution parallèle
    std::vector<std::string> dependency_chain;             // EN: Complete dependency chain / FR: Chaîne de dépendance complète
    std::vector<std::string> errors;                       // EN: Selection errors / FR: Erreurs de sélection
    std::vector<std::string> warnings;                     // EN: Selection warnings / FR: Avertissements de sélection
    std::vector<std::string> information;                  // EN: Information messages / FR: Messages d'information
    StageCompatibilityResult compatibility;                // EN: Stage compatibility analysis / FR: Analyse de compatibilité des étapes
    std::chrono::milliseconds selection_time{0};           // EN: Time taken for selection / FR: Temps pris pour la sélection
    std::chrono::milliseconds estimated_execution_time{0}; // EN: Estimated total execution time / FR: Temps d'exécution total estimé
    std::map<std::string, std::chrono::milliseconds> stage_execution_estimates; // EN: Per-stage execution estimates / FR: Estimations d'exécution par étape
    std::map<std::string, double> resource_estimates;      // EN: Resource usage estimates / FR: Estimations d'usage des ressources
    size_t total_available_stages = 0;                     // EN: Total stages available for selection / FR: Total d'étapes disponibles pour sélection
    size_t filtered_stages = 0;                            // EN: Stages after filtering / FR: Étapes après filtrage
    double selection_ratio = 0.0;                          // EN: Ratio of selected to available stages / FR: Ratio d'étapes sélectionnées sur disponibles
    std::map<std::string, std::string> metadata;           // EN: Additional result metadata / FR: Métadonnées supplémentaires de résultat
    std::chrono::system_clock::time_point selection_timestamp; // EN: Selection timestamp / FR: Horodatage de sélection
    std::string cache_key;                                  // EN: Cache key for this result / FR: Clé de cache pour ce résultat
};

// EN: Stage execution plan
// FR: Plan d'exécution d'étapes
struct StageExecutionPlan {
    std::string plan_id;                                    // EN: Unique plan identifier / FR: Identifiant unique du plan
    std::vector<PipelineStageConfig> stages;                // EN: Stages to execute / FR: Étapes à exécuter
    std::vector<std::string> execution_order;              // EN: Sequential execution order / FR: Ordre d'exécution séquentiel
    std::vector<std::vector<std::string>> parallel_groups;  // EN: Parallel execution groups / FR: Groupes d'exécution parallèle
    std::map<std::string, std::set<std::string>> dependencies; // EN: Stage dependencies / FR: Dépendances d'étapes
    std::map<std::string, StageConstraintDefinition> constraints; // EN: Stage constraints / FR: Contraintes d'étapes
    std::chrono::milliseconds estimated_total_time{0};     // EN: Estimated total execution time / FR: Temps d'exécution total estimé
    std::chrono::milliseconds estimated_parallel_time{0};  // EN: Estimated parallel execution time / FR: Temps d'exécution parallèle estimé
    std::map<std::string, double> resource_requirements;   // EN: Resource requirements by type / FR: Exigences de ressources par type
    double peak_resource_usage = 0.0;                      // EN: Peak resource usage / FR: Usage maximal des ressources
    std::vector<std::string> critical_path;                // EN: Critical execution path / FR: Chemin d'exécution critique
    std::vector<std::string> optimization_suggestions;     // EN: Optimization suggestions / FR: Suggestions d'optimisation
    PipelineExecutionConfig execution_config;              // EN: Execution configuration / FR: Configuration d'exécution
    bool is_valid = false;                                  // EN: Plan validity / FR: Validité du plan
    std::chrono::system_clock::time_point created_at;      // EN: Plan creation time / FR: Heure de création du plan
    std::map<std::string, std::string> plan_metadata;      // EN: Plan metadata / FR: Métadonnées du plan
};

// EN: Stage selector statistics
// FR: Statistiques du sélecteur d'étapes
struct StageSelectorStatistics {
    size_t total_selections = 0;                           // EN: Total number of selections performed / FR: Nombre total de sélections effectuées
    size_t successful_selections = 0;                      // EN: Successful selections / FR: Sélections réussies
    size_t failed_selections = 0;                          // EN: Failed selections / FR: Sélections échouées
    size_t cached_selections = 0;                          // EN: Selections served from cache / FR: Sélections servies depuis le cache
    std::chrono::milliseconds total_selection_time{0};     // EN: Total time spent on selections / FR: Temps total passé sur les sélections
    std::chrono::milliseconds avg_selection_time{0};       // EN: Average selection time / FR: Temps de sélection moyen
    std::chrono::milliseconds min_selection_time{0};       // EN: Minimum selection time / FR: Temps de sélection minimum
    std::chrono::milliseconds max_selection_time{0};       // EN: Maximum selection time / FR: Temps de sélection maximum
    size_t total_stages_evaluated = 0;                     // EN: Total stages evaluated / FR: Total d'étapes évaluées
    size_t total_stages_selected = 0;                      // EN: Total stages selected / FR: Total d'étapes sélectionnées
    double avg_selection_ratio = 0.0;                      // EN: Average selection ratio / FR: Ratio de sélection moyen
    std::map<StageSelectionCriteria, size_t> criteria_usage; // EN: Usage count by criteria / FR: Nombre d'utilisations par critère
    std::map<StageValidationLevel, size_t> validation_level_usage; // EN: Usage count by validation level / FR: Nombre d'utilisations par niveau de validation
    std::vector<std::string> most_selected_stages;         // EN: Most frequently selected stages / FR: Étapes les plus fréquemment sélectionnées
    std::chrono::system_clock::time_point last_reset;      // EN: Last statistics reset time / FR: Dernière heure de remise à zéro des statistiques
    std::map<std::string, size_t> error_counts;            // EN: Error counts by type / FR: Nombre d'erreurs par type
};

// EN: Stage selector configuration
// FR: Configuration du sélecteur d'étapes
struct StageSelectorConfig {
    size_t max_concurrent_selections = 4;                  // EN: Maximum concurrent selection operations / FR: Nombre maximum d'opérations de sélection simultanées
    bool enable_caching = true;                            // EN: Enable result caching / FR: Activer la mise en cache des résultats
    std::chrono::seconds cache_ttl{300};                   // EN: Cache time-to-live / FR: Durée de vie du cache
    size_t max_cache_entries = 1000;                       // EN: Maximum cache entries / FR: Nombre maximum d'entrées de cache
    bool enable_statistics = true;                         // EN: Enable statistics collection / FR: Activer la collecte de statistiques
    bool enable_detailed_logging = false;                  // EN: Enable detailed operation logging / FR: Activer la journalisation détaillée des opérations
    std::chrono::seconds default_selection_timeout{60};    // EN: Default selection timeout / FR: Délai d'expiration de sélection par défaut
    size_t max_dependency_depth = 10;                      // EN: Maximum dependency resolution depth / FR: Profondeur maximale de résolution de dépendances
    bool auto_include_dependencies = true;                 // EN: Automatically include dependencies / FR: Inclure automatiquement les dépendances
    bool auto_resolve_conflicts = true;                    // EN: Automatically resolve conflicts / FR: Résoudre automatiquement les conflits
    double compatibility_threshold = 0.8;                  // EN: Compatibility score threshold / FR: Seuil de score de compatibilité
    std::string default_log_level = "INFO";                // EN: Default logging level / FR: Niveau de journalisation par défaut
    std::map<std::string, std::string> custom_settings;    // EN: Custom configuration settings / FR: Paramètres de configuration personnalisés
};

// EN: Stage selector event types
// FR: Types d'événements du sélecteur d'étapes
enum class StageSelectorEventType {
    SELECTION_STARTED,      // EN: Selection process started / FR: Processus de sélection démarré
    SELECTION_COMPLETED,    // EN: Selection process completed / FR: Processus de sélection terminé
    SELECTION_FAILED,       // EN: Selection process failed / FR: Processus de sélection échoué
    VALIDATION_STARTED,     // EN: Validation started / FR: Validation démarrée
    VALIDATION_COMPLETED,   // EN: Validation completed / FR: Validation terminée
    DEPENDENCY_RESOLVED,    // EN: Dependency resolved / FR: Dépendance résolue
    CONSTRAINT_CHECKED,     // EN: Constraint checked / FR: Contrainte vérifiée
    STAGE_FILTERED,         // EN: Stage filtered / FR: Étape filtrée
    CACHE_HIT,             // EN: Cache hit occurred / FR: Succès de cache survenu
    CACHE_MISS             // EN: Cache miss occurred / FR: Échec de cache survenu
};

// EN: Stage selector event data
// FR: Données d'événement du sélecteur d'étapes
struct StageSelectorEvent {
    StageSelectorEventType type;                            // EN: Event type / FR: Type d'événement
    std::chrono::system_clock::time_point timestamp;       // EN: Event timestamp / FR: Horodatage de l'événement
    std::string selection_id;                              // EN: Selection operation ID / FR: ID d'opération de sélection
    std::string stage_id;                                  // EN: Related stage ID / FR: ID d'étape associé
    std::string message;                                   // EN: Event message / FR: Message d'événement
    std::map<std::string, std::string> metadata;          // EN: Event metadata / FR: Métadonnées d'événement
    std::chrono::milliseconds duration{0};                // EN: Operation duration / FR: Durée de l'opération
    bool success = true;                                   // EN: Operation success status / FR: Statut de succès de l'opération
};

// EN: Event callback function type
// FR: Type de fonction de rappel d'événement
using StageSelectorEventCallback = std::function<void(const StageSelectorEvent&)>;

// EN: Main stage selector class for individual module execution
// FR: Classe principale de sélecteur d'étapes pour l'exécution de modules individuels
class StageSelector {
public:
    // EN: Constructor with configuration
    // FR: Constructeur avec configuration
    explicit StageSelector(const StageSelectorConfig& config = StageSelectorConfig{});
    
    // EN: Destructor
    // FR: Destructeur
    ~StageSelector();

    // EN: Non-copyable and non-movable
    // FR: Non-copiable et non-déplaçable
    StageSelector(const StageSelector&) = delete;
    StageSelector& operator=(const StageSelector&) = delete;
    StageSelector(StageSelector&&) = delete;
    StageSelector& operator=(StageSelector&&) = delete;

    // EN: Stage selection operations
    // FR: Opérations de sélection d'étapes
    StageSelectionResult selectStages(
        const std::vector<PipelineStageConfig>& available_stages,
        const StageSelectionConfig& selection_config
    );
    
    std::future<StageSelectionResult> selectStagesAsync(
        const std::vector<PipelineStageConfig>& available_stages,
        const StageSelectionConfig& selection_config
    );
    
    StageSelectionResult selectStagesByIds(
        const std::vector<PipelineStageConfig>& available_stages,
        const std::vector<std::string>& stage_ids,
        StageValidationLevel validation_level = StageValidationLevel::DEPENDENCIES
    );
    
    StageSelectionResult selectStagesByPattern(
        const std::vector<PipelineStageConfig>& available_stages,
        const std::string& pattern,
        bool include_dependencies = true
    );

    // EN: Stage filtering and validation
    // FR: Filtrage et validation d'étapes
    std::vector<PipelineStageConfig> filterStages(
        const std::vector<PipelineStageConfig>& stages,
        const std::vector<StageSelectionFilter>& filters
    );
    
    bool validateStageSelection(
        const std::vector<PipelineStageConfig>& stages,
        StageValidationLevel level = StageValidationLevel::COMPREHENSIVE
    );
    
    StageCompatibilityResult analyzeStageCompatibility(
        const std::vector<PipelineStageConfig>& stages
    );

    // EN: Dependency analysis and resolution
    // FR: Analyse et résolution des dépendances
    std::vector<std::string> resolveDependencies(
        const std::vector<PipelineStageConfig>& all_stages,
        const std::vector<std::string>& selected_stage_ids,
        bool include_transitive = true
    );
    
    std::vector<std::string> findDependents(
        const std::vector<PipelineStageConfig>& all_stages,
        const std::vector<std::string>& selected_stage_ids
    );
    
    std::vector<std::string> detectCircularDependencies(
        const std::vector<PipelineStageConfig>& stages
    );

    // EN: Execution planning
    // FR: Planification d'exécution
    StageExecutionPlan createExecutionPlan(
        const std::vector<PipelineStageConfig>& stages,
        const PipelineExecutionConfig& execution_config = {}
    );
    
    std::vector<std::string> optimizeExecutionOrder(
        const std::vector<PipelineStageConfig>& stages
    );
    
    std::vector<std::vector<std::string>> identifyParallelExecutionGroups(
        const std::vector<PipelineStageConfig>& stages
    );

    // EN: Constraint management
    // FR: Gestion des contraintes
    bool checkStageConstraints(
        const PipelineStageConfig& stage,
        const std::vector<StageExecutionConstraint>& allowed_constraints,
        const std::vector<StageExecutionConstraint>& forbidden_constraints = {}
    );
    
    std::vector<StageExecutionConstraint> inferStageConstraints(
        const PipelineStageConfig& stage
    );
    
    void registerConstraintValidator(
        StageExecutionConstraint constraint,
        std::function<bool(const PipelineStageConfig&)> validator
    );

    // EN: Stage metadata and analysis
    // FR: Métadonnées et analyse d'étapes
    std::chrono::milliseconds estimateStageExecutionTime(
        const PipelineStageConfig& stage
    );
    
    double estimateStageResourceUsage(
        const PipelineStageConfig& stage,
        const std::string& resource_type = "cpu"
    );
    
    double calculateStageSuccessRate(
        const std::string& stage_id
    );
    
    std::map<std::string, std::string> extractStageMetadata(
        const PipelineStageConfig& stage
    );

    // EN: Caching management
    // FR: Gestion de la mise en cache
    void enableCaching(bool enable = true);
    void clearCache();
    void setCacheTTL(std::chrono::seconds ttl);
    size_t getCacheSize() const;
    double getCacheHitRatio() const;

    // EN: Event handling
    // FR: Gestion d'événements
    void setEventCallback(StageSelectorEventCallback callback);
    void removeEventCallback();
    void emitEvent(StageSelectorEventType type, const std::string& selection_id = "",
                   const std::string& stage_id = "", const std::string& message = "");

    // EN: Statistics and monitoring
    // FR: Statistiques et surveillance
    StageSelectorStatistics getStatistics() const;
    void resetStatistics();
    bool isHealthy() const;
    std::string getStatus() const;

    // EN: Configuration management
    // FR: Gestion de configuration
    void updateConfig(const StageSelectorConfig& new_config);
    StageSelectorConfig getConfig() const;
    
    // EN: Import/Export functionality
    // FR: Fonctionnalité d'import/export
    bool exportSelectionResult(const StageSelectionResult& result, const std::string& filepath) const;
    std::optional<StageSelectionResult> importSelectionResult(const std::string& filepath) const;
    bool exportExecutionPlan(const StageExecutionPlan& plan, const std::string& filepath) const;
    std::optional<StageExecutionPlan> importExecutionPlan(const std::string& filepath) const;

private:
    // EN: Internal implementation using PIMPL pattern
    // FR: Implémentation interne utilisant le pattern PIMPL
    class StageSelectorImpl;
    std::unique_ptr<StageSelectorImpl> impl_;
};

// EN: Utility functions for stage selection
// FR: Fonctions utilitaires pour la sélection d'étapes
namespace StageSelectorUtils {
    
    // EN: Stage selection criteria utilities
    // FR: Utilitaires de critères de sélection d'étapes
    std::string criteriaToString(StageSelectionCriteria criteria);
    StageSelectionCriteria stringToCriteria(const std::string& str);
    
    std::string filterModeToString(StageFilterMode mode);
    StageFilterMode stringToFilterMode(const std::string& str);
    
    std::string validationLevelToString(StageValidationLevel level);
    StageValidationLevel stringToValidationLevel(const std::string& str);
    
    std::string constraintToString(StageExecutionConstraint constraint);
    StageExecutionConstraint stringToConstraint(const std::string& str);
    
    std::string selectionStatusToString(StageSelectionStatus status);
    
    // EN: Filter creation utilities
    // FR: Utilitaires de création de filtres
    StageSelectionFilter createIdFilter(const std::string& stage_id, bool exact_match = true);
    StageSelectionFilter createNameFilter(const std::string& name, bool case_sensitive = false);
    StageSelectionFilter createPatternFilter(const std::string& pattern);
    StageSelectionFilter createTagFilter(const std::set<std::string>& tags);
    StageSelectionFilter createPriorityFilter(PipelineStagePriority min_priority, 
                                              PipelineStagePriority max_priority = PipelineStagePriority::CRITICAL);
    
    // EN: Validation utilities
    // FR: Utilitaires de validation
    bool isValidStageId(const std::string& id);
    bool isValidPattern(const std::string& pattern);
    std::vector<std::string> validateSelectionConfig(const StageSelectionConfig& config);
    std::vector<std::string> validateExecutionPlan(const StageExecutionPlan& plan);
    
    // EN: Constraint utilities
    // FR: Utilitaires de contraintes
    bool areConstraintsCompatible(StageExecutionConstraint c1, StageExecutionConstraint c2);
    std::vector<StageExecutionConstraint> getConflictingConstraints(StageExecutionConstraint constraint);
    std::vector<StageExecutionConstraint> getDependentConstraints(StageExecutionConstraint constraint);
    
    // EN: JSON serialization utilities
    // FR: Utilitaires de sérialisation JSON
    nlohmann::json selectionResultToJson(const StageSelectionResult& result);
    StageSelectionResult selectionResultFromJson(const nlohmann::json& json);
    nlohmann::json executionPlanToJson(const StageExecutionPlan& plan);
    StageExecutionPlan executionPlanFromJson(const nlohmann::json& json);
    nlohmann::json configToJson(const StageSelectorConfig& config);
    StageSelectorConfig configFromJson(const nlohmann::json& json);
    
    // EN: Performance utilities
    // FR: Utilitaires de performance
    std::chrono::milliseconds measureSelectionTime(
        std::function<StageSelectionResult()> selection_func
    );
    
    double calculateSelectionEfficiency(const StageSelectionResult& result);
    std::vector<std::string> identifyBottleneckStages(const std::vector<PipelineStageConfig>& stages);
    
    // EN: Debugging and diagnostics utilities
    // FR: Utilitaires de débogage et diagnostics
    std::string generateSelectionReport(const StageSelectionResult& result);
    std::string generateExecutionPlanReport(const StageExecutionPlan& plan);
    void dumpSelectionDebugInfo(const StageSelectionResult& result, const std::string& filepath);
}

// EN: Helper classes for advanced stage selection functionality
// FR: Classes d'aide pour les fonctionnalités avancées de sélection d'étapes

// EN: Stage dependency analyzer
// FR: Analyseur de dépendances d'étapes
class StageDependencyAnalyzer {
public:
    explicit StageDependencyAnalyzer(const std::vector<PipelineStageConfig>& stages);
    
    std::vector<std::string> getDirectDependencies(const std::string& stage_id) const;
    std::vector<std::string> getTransitiveDependencies(const std::string& stage_id) const;
    std::vector<std::string> getDependents(const std::string& stage_id) const;
    std::vector<std::string> getTransitiveDependents(const std::string& stage_id) const;
    
    bool hasCycle() const;
    std::vector<std::string> findCycles() const;
    std::vector<std::string> topologicalSort() const;
    
    std::map<std::string, int> calculateDependencyDepths() const;
    std::vector<std::string> getCriticalPath() const;
    double calculateParallelismPotential() const;

private:
    class DependencyAnalyzerImpl;
    std::unique_ptr<DependencyAnalyzerImpl> impl_;
};

// EN: Stage constraint validator
// FR: Validateur de contraintes d'étapes
class StageConstraintValidator {
public:
    StageConstraintValidator();
    ~StageConstraintValidator();
    
    bool validateConstraint(const PipelineStageConfig& stage, StageExecutionConstraint constraint) const;
    std::vector<StageExecutionConstraint> findViolatedConstraints(
        const PipelineStageConfig& stage,
        const std::vector<StageExecutionConstraint>& constraints
    ) const;
    
    bool checkConstraintCompatibility(
        const std::vector<StageExecutionConstraint>& constraints
    ) const;
    
    void registerCustomValidator(
        StageExecutionConstraint constraint,
        std::function<bool(const PipelineStageConfig&)> validator
    );
    
    std::vector<StageExecutionConstraint> inferConstraintsFromConfig(
        const PipelineStageConfig& stage
    ) const;

private:
    class ConstraintValidatorImpl;
    std::unique_ptr<ConstraintValidatorImpl> impl_;
};

// EN: Stage execution planner
// FR: Planificateur d'exécution d'étapes
class StageExecutionPlanner {
public:
    explicit StageExecutionPlanner(const StageSelectorConfig& config = {});
    
    StageExecutionPlan createPlan(
        const std::vector<PipelineStageConfig>& stages,
        const PipelineExecutionConfig& execution_config = {}
    );
    
    std::vector<std::string> optimizeExecutionOrder(
        const std::vector<PipelineStageConfig>& stages
    );
    
    std::vector<std::vector<std::string>> identifyParallelGroups(
        const std::vector<PipelineStageConfig>& stages
    );
    
    std::chrono::milliseconds estimateTotalExecutionTime(
        const std::vector<PipelineStageConfig>& stages,
        bool consider_parallelism = true
    ) const;
    
    std::map<std::string, double> estimateResourceRequirements(
        const std::vector<PipelineStageConfig>& stages
    ) const;

private:
    class ExecutionPlannerImpl;
    std::unique_ptr<ExecutionPlannerImpl> impl_;
};

} // namespace Orchestrator
} // namespace BBP