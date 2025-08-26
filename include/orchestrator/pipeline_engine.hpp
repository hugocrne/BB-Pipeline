#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <future>
#include <chrono>
#include <map>
#include <set>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <unordered_map>
#include <queue>
#include <optional>

#include "infrastructure/threading/thread_pool.hpp"
#include "infrastructure/logging/logger.hpp"
#include "infrastructure/config/config_manager.hpp"

namespace BBP {
namespace Orchestrator {

// EN: Forward declarations
// FR: Déclarations avancées
class PipelineTask;
class PipelineDependencyResolver;
class PipelineExecutionContext;

// EN: Status of a pipeline stage execution
// FR: Statut de l'exécution d'une étape du pipeline
enum class PipelineStageStatus {
    PENDING = 0,        // EN: Not yet started / FR: Pas encore commencé
    WAITING = 1,        // EN: Waiting for dependencies / FR: En attente des dépendances
    READY = 2,          // EN: Ready to execute / FR: Prêt à exécuter
    RUNNING = 3,        // EN: Currently executing / FR: En cours d'exécution
    COMPLETED = 4,      // EN: Successfully completed / FR: Terminé avec succès
    FAILED = 5,         // EN: Failed with error / FR: Échec avec erreur
    CANCELLED = 6,      // EN: Cancelled by user / FR: Annulé par l'utilisateur
    SKIPPED = 7         // EN: Skipped due to conditions / FR: Ignoré en raison de conditions
};

// EN: Execution mode for pipeline stages
// FR: Mode d'exécution pour les étapes du pipeline
enum class PipelineExecutionMode {
    SEQUENTIAL = 0,     // EN: Execute stages one by one / FR: Exécuter les étapes une par une
    PARALLEL = 1,       // EN: Execute independent stages in parallel / FR: Exécuter les étapes indépendantes en parallèle
    HYBRID = 2          // EN: Mix of sequential and parallel / FR: Mélange de séquentiel et parallèle
};

// EN: Priority levels for pipeline stages
// FR: Niveaux de priorité pour les étapes du pipeline
enum class PipelineStagePriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

// EN: Error handling strategy for pipeline execution
// FR: Stratégie de gestion d'erreur pour l'exécution du pipeline
enum class PipelineErrorStrategy {
    FAIL_FAST = 0,      // EN: Stop immediately on first error / FR: Arrêter immédiatement à la première erreur
    CONTINUE = 1,       // EN: Continue with other stages / FR: Continuer avec les autres étapes
    RETRY = 2,          // EN: Retry failed stages / FR: Réessayer les étapes échouées
    SKIP = 3           // EN: Skip failed stages and continue / FR: Ignorer les étapes échouées et continuer
};

// EN: Result of a pipeline stage execution
// FR: Résultat de l'exécution d'une étape du pipeline
struct PipelineStageResult {
    std::string stage_id;
    PipelineStageStatus status = PipelineStageStatus::PENDING;
    std::chrono::milliseconds execution_time{0};
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::string error_message;
    int exit_code = 0;
    std::map<std::string, std::string> metadata;
    
    // EN: Success indicator
    // FR: Indicateur de succès
    bool isSuccess() const { return status == PipelineStageStatus::COMPLETED; }
    bool isFailure() const { return status == PipelineStageStatus::FAILED; }
    bool isRunning() const { return status == PipelineStageStatus::RUNNING; }
    bool isWaiting() const { return status == PipelineStageStatus::WAITING; }
};

// EN: Configuration for a single pipeline stage
// FR: Configuration pour une étape unique du pipeline
struct PipelineStageConfig {
    std::string id;                              // EN: Unique identifier / FR: Identifiant unique
    std::string name;                            // EN: Human-readable name / FR: Nom lisible
    std::string description;                     // EN: Stage description / FR: Description de l'étape
    std::string executable;                      // EN: Binary to execute / FR: Binaire à exécuter
    std::vector<std::string> arguments;          // EN: Command line arguments / FR: Arguments de ligne de commande
    std::vector<std::string> dependencies;       // EN: Stage dependencies / FR: Dépendances d'étapes
    std::map<std::string, std::string> environment; // EN: Environment variables / FR: Variables d'environnement
    std::string working_directory;               // EN: Working directory / FR: Répertoire de travail
    PipelineStagePriority priority = PipelineStagePriority::NORMAL;
    std::chrono::seconds timeout{300};           // EN: Execution timeout / FR: Délai d'expiration
    int max_retries = 0;                        // EN: Maximum retry attempts / FR: Nombre maximum de tentatives
    std::chrono::seconds retry_delay{5};         // EN: Delay between retries / FR: Délai entre les tentatives
    bool allow_failure = false;                  // EN: Continue pipeline on failure / FR: Continuer le pipeline en cas d'échec
    std::function<bool()> condition;             // EN: Execution condition / FR: Condition d'exécution
    std::map<std::string, std::string> metadata; // EN: Additional metadata / FR: Métadonnées supplémentaires
};

// EN: Configuration for the entire pipeline execution
// FR: Configuration pour l'exécution complète du pipeline
struct PipelineExecutionConfig {
    PipelineExecutionMode execution_mode = PipelineExecutionMode::HYBRID;
    PipelineErrorStrategy error_strategy = PipelineErrorStrategy::FAIL_FAST;
    size_t max_concurrent_stages = std::thread::hardware_concurrency();
    std::chrono::seconds global_timeout{3600};   // EN: Global pipeline timeout / FR: Délai d'expiration global du pipeline
    bool enable_progress_reporting = true;       // EN: Enable progress updates / FR: Activer les mises à jour de progression
    std::chrono::milliseconds progress_interval{1000}; // EN: Progress reporting interval / FR: Intervalle de rapport de progression
    bool enable_checkpointing = false;           // EN: Enable state checkpointing / FR: Activer la sauvegarde d'état
    std::string checkpoint_directory;            // EN: Directory for checkpoints / FR: Répertoire pour les points de contrôle
    bool dry_run = false;                        // EN: Simulate execution without running / FR: Simuler l'exécution sans lancer
    std::string log_level = "INFO";              // EN: Logging level / FR: Niveau de journalisation
    std::map<std::string, std::string> global_environment; // EN: Global environment variables / FR: Variables d'environnement globales
};

// EN: Progress information for pipeline execution
// FR: Informations de progression pour l'exécution du pipeline
struct PipelineProgress {
    size_t total_stages = 0;
    size_t completed_stages = 0;
    size_t failed_stages = 0;
    size_t running_stages = 0;
    size_t pending_stages = 0;
    double completion_percentage = 0.0;
    std::chrono::milliseconds elapsed_time{0};
    std::chrono::milliseconds estimated_remaining_time{0};
    std::string current_stage;
    std::map<std::string, PipelineStageResult> stage_results;
    
    // EN: Progress indicators
    // FR: Indicateurs de progression
    bool isComplete() const { return completed_stages + failed_stages == total_stages; }
    bool hasFailures() const { return failed_stages > 0; }
    bool isRunning() const { return running_stages > 0; }
};

// EN: Statistics for pipeline execution
// FR: Statistiques pour l'exécution du pipeline
struct PipelineExecutionStatistics {
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::chrono::milliseconds total_execution_time{0};
    std::chrono::milliseconds avg_stage_execution_time{0};
    std::chrono::milliseconds max_stage_execution_time{0};
    std::chrono::milliseconds min_stage_execution_time{0};
    size_t total_stages_executed = 0;
    size_t successful_stages = 0;
    size_t failed_stages = 0;
    size_t retried_stages = 0;
    double success_rate = 0.0;
    size_t peak_concurrent_stages = 0;
    size_t total_cpu_time_ms = 0;
    size_t peak_memory_usage_bytes = 0;
    std::map<std::string, std::chrono::milliseconds> stage_execution_times;
    std::vector<std::string> critical_path;      // EN: Longest execution path / FR: Chemin d'exécution le plus long
};

// EN: Event types for pipeline execution monitoring
// FR: Types d'événements pour le monitoring de l'exécution du pipeline
enum class PipelineEventType {
    PIPELINE_STARTED,
    PIPELINE_COMPLETED,
    PIPELINE_FAILED,
    PIPELINE_CANCELLED,
    STAGE_STARTED,
    STAGE_COMPLETED,
    STAGE_FAILED,
    STAGE_RETRYING,
    DEPENDENCY_RESOLVED,
    PROGRESS_UPDATE
};

// EN: Event data for pipeline monitoring
// FR: Données d'événement pour le monitoring du pipeline
struct PipelineEvent {
    PipelineEventType type;
    std::chrono::system_clock::time_point timestamp;
    std::string pipeline_id;
    std::string stage_id;
    std::string message;
    std::map<std::string, std::string> metadata;
};

// EN: Callback function type for pipeline events
// FR: Type de fonction de rappel pour les événements du pipeline
using PipelineEventCallback = std::function<void(const PipelineEvent&)>;

// EN: Main pipeline engine class for orchestrating execution
// FR: Classe principale du moteur de pipeline pour orchestrer l'exécution
class PipelineEngine {
public:
    // EN: Constructor with configuration
    // FR: Constructeur avec configuration
    struct Config {
        size_t max_concurrent_pipelines = 1;     // EN: Maximum concurrent pipeline executions / FR: Nombre maximum d'exécutions de pipeline simultanées
        size_t thread_pool_size = std::thread::hardware_concurrency();
        bool enable_metrics = true;              // EN: Enable performance metrics / FR: Activer les métriques de performance
        bool enable_logging = true;              // EN: Enable detailed logging / FR: Activer la journalisation détaillée
        std::string log_directory = "logs";      // EN: Directory for log files / FR: Répertoire pour les fichiers de log
        std::chrono::seconds health_check_interval{30}; // EN: Health check interval / FR: Intervalle de vérification de santé
        size_t max_pipeline_history = 100;       // EN: Maximum pipeline execution history / FR: Historique maximum d'exécution de pipeline
    };

    explicit PipelineEngine(const Config& config);
    ~PipelineEngine();

    // EN: Pipeline management
    // FR: Gestion des pipelines
    std::string createPipeline(const std::vector<PipelineStageConfig>& stages);
    bool addStage(const std::string& pipeline_id, const PipelineStageConfig& stage);
    bool removeStage(const std::string& pipeline_id, const std::string& stage_id);
    bool updateStage(const std::string& pipeline_id, const PipelineStageConfig& stage);
    std::vector<std::string> getPipelineIds() const;
    std::optional<std::vector<PipelineStageConfig>> getPipelineStages(const std::string& pipeline_id) const;

    // EN: Pipeline execution
    // FR: Exécution de pipeline
    std::future<PipelineExecutionStatistics> executePipelineAsync(
        const std::string& pipeline_id,
        const PipelineExecutionConfig& config = {}
    );
    
    PipelineExecutionStatistics executePipeline(
        const std::string& pipeline_id,
        const PipelineExecutionConfig& config = {}
    );
    
    std::future<PipelineExecutionStatistics> executeStagesAsync(
        const std::vector<PipelineStageConfig>& stages,
        const PipelineExecutionConfig& config = {}
    );

    // EN: Pipeline control
    // FR: Contrôle de pipeline
    bool pausePipeline(const std::string& pipeline_id);
    bool resumePipeline(const std::string& pipeline_id);
    bool cancelPipeline(const std::string& pipeline_id);
    bool retryFailedStages(const std::string& pipeline_id);

    // EN: Progress monitoring
    // FR: Surveillance de la progression
    std::optional<PipelineProgress> getPipelineProgress(const std::string& pipeline_id) const;
    void registerEventCallback(PipelineEventCallback callback);
    void unregisterEventCallback();
    
    // EN: Stage management
    // FR: Gestion des étapes
    std::optional<PipelineStageResult> getStageResult(
        const std::string& pipeline_id, 
        const std::string& stage_id
    ) const;
    
    std::vector<PipelineStageResult> getAllStageResults(const std::string& pipeline_id) const;

    // EN: Dependency resolution
    // FR: Résolution des dépendances
    std::vector<std::string> getExecutionOrder(const std::string& pipeline_id) const;
    bool validateDependencies(const std::string& pipeline_id) const;
    std::vector<std::string> detectCircularDependencies(const std::string& pipeline_id) const;

    // EN: Pipeline validation
    // FR: Validation de pipeline
    struct ValidationResult {
        bool is_valid = false;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    ValidationResult validatePipeline(const std::string& pipeline_id) const;
    ValidationResult validateStages(const std::vector<PipelineStageConfig>& stages) const;

    // EN: State management
    // FR: Gestion d'état
    bool savePipelineState(const std::string& pipeline_id, const std::string& filepath) const;
    bool loadPipelineState(const std::string& pipeline_id, const std::string& filepath);
    void clearPipelineState(const std::string& pipeline_id);

    // EN: Performance and monitoring
    // FR: Performance et surveillance
    std::optional<PipelineExecutionStatistics> getPipelineStatistics(const std::string& pipeline_id) const;
    std::vector<PipelineExecutionStatistics> getAllPipelineStatistics() const;
    void clearStatistics();
    
    struct EngineStatistics {
        size_t total_pipelines_executed = 0;
        size_t successful_pipelines = 0;
        size_t failed_pipelines = 0;
        size_t currently_running_pipelines = 0;
        std::chrono::milliseconds total_execution_time{0};
        std::chrono::milliseconds average_pipeline_duration{0};
        size_t peak_concurrent_pipelines = 0;
        size_t total_stages_executed = 0;
        double overall_success_rate = 0.0;
        std::chrono::system_clock::time_point engine_start_time;
        std::chrono::milliseconds engine_uptime{0};
    };
    
    EngineStatistics getEngineStatistics() const;

    // EN: Configuration management
    // FR: Gestion de configuration
    void updateConfig(const Config& new_config);
    Config getConfig() const;

    // EN: Health and status
    // FR: Santé et statut
    bool isHealthy() const;
    std::string getStatus() const;
    void shutdown();

private:
    // EN: Internal implementation
    // FR: Implémentation interne
    class PipelineEngineImpl;
    std::unique_ptr<PipelineEngineImpl> impl_;
};

// EN: Pipeline task representation for internal execution
// FR: Représentation de tâche de pipeline pour l'exécution interne
class PipelineTask {
public:
    PipelineTask(const PipelineStageConfig& config, 
                 PipelineExecutionContext* context);
    ~PipelineTask() = default;

    // EN: Task execution
    // FR: Exécution de tâche
    PipelineStageResult execute();
    void cancel();
    bool isCancelled() const;

    // EN: Task information
    // FR: Informations de tâche
    const std::string& getId() const { return config_.id; }
    const PipelineStageConfig& getConfig() const { return config_; }
    PipelineStageStatus getStatus() const;

    // EN: Dependency management
    // FR: Gestion des dépendances
    const std::vector<std::string>& getDependencies() const { return config_.dependencies; }
    bool areDependenciesMet() const;
    void addDependency(const std::string& dep_id);
    void removeDependency(const std::string& dep_id);

private:
    PipelineStageConfig config_;
    PipelineExecutionContext* context_;
    std::atomic<bool> cancelled_{false};
    std::atomic<PipelineStageStatus> status_{PipelineStageStatus::PENDING};
    mutable std::mutex mutex_;
    
    // EN: Internal execution methods
    // FR: Méthodes d'exécution internes
    PipelineStageResult executeInternal();
    bool checkExecutionCondition();
    void updateStatus(PipelineStageStatus status);
};

// EN: Dependency resolution utility class
// FR: Classe utilitaire de résolution de dépendances
class PipelineDependencyResolver {
public:
    explicit PipelineDependencyResolver(const std::vector<PipelineStageConfig>& stages);
    
    // EN: Dependency resolution
    // FR: Résolution de dépendances
    std::vector<std::string> getExecutionOrder() const;
    std::vector<std::vector<std::string>> getExecutionLevels() const;
    bool hasCircularDependency() const;
    std::vector<std::string> getCircularDependencies() const;
    
    // EN: Dependency queries
    // FR: Requêtes de dépendances
    std::vector<std::string> getDependents(const std::string& stage_id) const;
    std::vector<std::string> getDependencies(const std::string& stage_id) const;
    bool canExecute(const std::string& stage_id, const std::set<std::string>& completed_stages) const;

private:
    std::unordered_map<std::string, PipelineStageConfig> stages_;
    std::unordered_map<std::string, std::vector<std::string>> dependency_graph_;
    std::unordered_map<std::string, std::vector<std::string>> reverse_dependency_graph_;
    
    void buildDependencyGraph();
    bool detectCircularDependencyDFS(const std::string& node, 
                                   std::unordered_map<std::string, int>& colors) const;
    std::vector<std::string> topologicalSort() const;
};

// EN: Execution context for pipeline runs
// FR: Contexte d'exécution pour les exécutions de pipeline
class PipelineExecutionContext {
public:
    PipelineExecutionContext(const std::string& pipeline_id,
                           const PipelineExecutionConfig& config);
    ~PipelineExecutionContext() = default;

    // EN: Context accessors
    // FR: Accesseurs de contexte
    const std::string& getPipelineId() const { return pipeline_id_; }
    const PipelineExecutionConfig& getConfig() const { return config_; }
    
    // EN: State management
    // FR: Gestion d'état
    void updateStageResult(const std::string& stage_id, const PipelineStageResult& result);
    std::optional<PipelineStageResult> getStageResult(const std::string& stage_id) const;
    std::vector<PipelineStageResult> getAllStageResults() const;
    
    // EN: Execution control
    // FR: Contrôle d'exécution
    bool shouldContinue() const;
    void requestCancellation();
    bool isCancelled() const;
    
    // EN: Progress tracking
    // FR: Suivi de progression
    PipelineProgress getCurrentProgress() const;
    void notifyStageStarted(const std::string& stage_id);
    void notifyStageCompleted(const std::string& stage_id, const PipelineStageResult& result);
    
    // EN: Event handling
    // FR: Gestion d'événements
    void emitEvent(PipelineEventType type, const std::string& stage_id = "", 
                   const std::string& message = "");
    void setEventCallback(PipelineEventCallback callback);

private:
    std::string pipeline_id_;
    PipelineExecutionConfig config_;
    mutable std::mutex results_mutex_;
    std::unordered_map<std::string, PipelineStageResult> stage_results_;
    std::atomic<bool> cancelled_{false};
    std::chrono::system_clock::time_point start_time_;
    PipelineEventCallback event_callback_;
    mutable std::mutex callback_mutex_;
    
    // EN: Progress calculation helpers
    // FR: Assistants de calcul de progression
    void updateProgress();
};

// EN: Utility functions for pipeline management
// FR: Fonctions utilitaires pour la gestion de pipeline
namespace PipelineUtils {
    
    // EN: Pipeline configuration validation
    // FR: Validation de configuration de pipeline
    bool isValidStageId(const std::string& id);
    bool isValidExecutable(const std::string& executable);
    std::vector<std::string> validateStageConfig(const PipelineStageConfig& config);
    
    // EN: Dependency utilities
    // FR: Utilitaires de dépendances
    bool hasCyclicDependency(const std::vector<PipelineStageConfig>& stages);
    std::vector<std::string> findMissingDependencies(const std::vector<PipelineStageConfig>& stages);
    
    // EN: Time and duration utilities
    // FR: Utilitaires de temps et de durée
    std::string formatDuration(std::chrono::milliseconds duration);
    std::string formatTimestamp(std::chrono::system_clock::time_point timestamp);
    
    // EN: Status conversion utilities
    // FR: Utilitaires de conversion de statut
    std::string statusToString(PipelineStageStatus status);
    std::string executionModeToString(PipelineExecutionMode mode);
    std::string errorStrategyToString(PipelineErrorStrategy strategy);
    
    // EN: Configuration file management
    // FR: Gestion des fichiers de configuration
    bool loadPipelineFromYAML(const std::string& filepath, std::vector<PipelineStageConfig>& stages);
    bool savePipelineToYAML(const std::string& filepath, const std::vector<PipelineStageConfig>& stages);
    bool loadPipelineFromJSON(const std::string& filepath, std::vector<PipelineStageConfig>& stages);
    bool savePipelineToJSON(const std::string& filepath, const std::vector<PipelineStageConfig>& stages);
}

} // namespace Orchestrator
} // namespace BBP