// EN: Progress Monitor for BB-Pipeline - Real-time progress bar with ETA calculation
// FR: Moniteur de Progression pour BB-Pipeline - Barre de progression temps réel avec calcul ETA

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <future>
#include <queue>
#include <deque>
#include <algorithm>
#include <cmath>

namespace BBP {
namespace Orchestrator {

// EN: Forward declarations for integration with other systems
// FR: Déclarations avancées pour l'intégration avec d'autres systèmes
class PipelineEngine;
class Logger;

// EN: Progress update frequency modes
// FR: Modes de fréquence de mise à jour de la progression
enum class ProgressUpdateMode {
    REAL_TIME = 0,      // EN: Update as frequently as possible / FR: Mise à jour aussi fréquemment que possible
    THROTTLED = 1,      // EN: Limit update frequency to prevent spam / FR: Limiter la fréquence de mise à jour pour éviter le spam
    ON_DEMAND = 2,      // EN: Update only when explicitly requested / FR: Mise à jour uniquement sur demande explicite
    MILESTONE = 3       // EN: Update only on significant milestones / FR: Mise à jour uniquement sur les jalons importants
};

// EN: Display format modes for progress visualization
// FR: Modes de format d'affichage pour la visualisation de progression
enum class ProgressDisplayMode {
    SIMPLE_BAR = 0,     // EN: Simple ASCII progress bar / FR: Barre de progression ASCII simple
    DETAILED_BAR = 1,   // EN: Detailed progress bar with statistics / FR: Barre de progression détaillée avec statistiques
    PERCENTAGE = 2,     // EN: Percentage only display / FR: Affichage en pourcentage uniquement
    COMPACT = 3,        // EN: Compact single-line display / FR: Affichage compact sur une ligne
    VERBOSE = 4,        // EN: Multi-line verbose display / FR: Affichage verbeux multi-lignes
    JSON = 5,           // EN: JSON format for machine processing / FR: Format JSON pour traitement machine
    CUSTOM = 6          // EN: Custom format using user callback / FR: Format personnalisé utilisant un callback utilisateur
};

// EN: ETA calculation strategy enumeration
// FR: Énumération des stratégies de calcul ETA
enum class ETACalculationStrategy {
    LINEAR = 0,         // EN: Simple linear extrapolation / FR: Extrapolation linéaire simple
    MOVING_AVERAGE = 1, // EN: Moving average of recent progress / FR: Moyenne mobile de la progression récente
    EXPONENTIAL = 2,    // EN: Exponential smoothing / FR: Lissage exponentiel
    ADAPTIVE = 3,       // EN: Adaptive algorithm based on patterns / FR: Algorithme adaptatif basé sur les motifs
    WEIGHTED = 4,       // EN: Weighted calculation by task complexity / FR: Calcul pondéré par complexité de tâche
    HISTORICAL = 5      // EN: Based on historical execution data / FR: Basé sur les données d'exécution historiques
};

// EN: Progress event types for callback notifications
// FR: Types d'événements de progression pour les notifications de callback
enum class ProgressEventType {
    STARTED,            // EN: Progress monitoring started / FR: Surveillance de progression démarrée
    UPDATED,            // EN: Progress value updated / FR: Valeur de progression mise à jour
    MILESTONE_REACHED,  // EN: Significant milestone reached / FR: Jalon important atteint
    STAGE_COMPLETED,    // EN: Individual stage completed / FR: Étape individuelle terminée
    STAGE_FAILED,       // EN: Individual stage failed / FR: Étape individuelle échouée
    ETA_UPDATED,        // EN: ETA estimation updated / FR: Estimation ETA mise à jour
    SPEED_CHANGED,      // EN: Processing speed changed significantly / FR: Vitesse de traitement changée significativement
    COMPLETED,          // EN: All tasks completed / FR: Toutes les tâches terminées
    CANCELLED,          // EN: Progress monitoring cancelled / FR: Surveillance de progression annulée
    ERROR               // EN: Error occurred during monitoring / FR: Erreur survenue pendant la surveillance
};

// EN: Severity level for progress events
// FR: Niveau de sévérité pour les événements de progression
enum class ProgressEventSeverity {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

// EN: Progress statistics for performance analysis
// FR: Statistiques de progression pour l'analyse de performance
struct ProgressStatistics {
    double current_progress = 0.0;                          // EN: Current progress percentage (0-100) / FR: Pourcentage de progression actuel (0-100)
    double average_speed = 0.0;                             // EN: Average processing speed (units/second) / FR: Vitesse de traitement moyenne (unités/seconde)
    double current_speed = 0.0;                             // EN: Current processing speed / FR: Vitesse de traitement actuelle
    double peak_speed = 0.0;                                // EN: Peak processing speed reached / FR: Vitesse de traitement maximale atteinte
    std::chrono::milliseconds elapsed_time{0};              // EN: Time elapsed since start / FR: Temps écoulé depuis le début
    std::chrono::milliseconds estimated_remaining_time{0};  // EN: Estimated time to completion / FR: Temps estimé jusqu'à la fin
    std::chrono::milliseconds estimated_total_time{0};      // EN: Estimated total time / FR: Temps total estimé
    std::chrono::system_clock::time_point start_time;       // EN: Start timestamp / FR: Horodatage de début
    std::chrono::system_clock::time_point last_update_time; // EN: Last update timestamp / FR: Horodatage de dernière mise à jour
    size_t total_units = 0;                                 // EN: Total units to process / FR: Nombre total d'unités à traiter
    size_t completed_units = 0;                             // EN: Units completed so far / FR: Unités terminées jusqu'à présent
    size_t failed_units = 0;                               // EN: Units that failed processing / FR: Unités ayant échoué au traitement
    size_t update_count = 0;                               // EN: Number of progress updates / FR: Nombre de mises à jour de progression
    double confidence_level = 0.0;                         // EN: ETA confidence level (0-1) / FR: Niveau de confiance ETA (0-1)
    
    // EN: Utility methods for statistics analysis
    // FR: Méthodes utilitaires pour l'analyse des statistiques
    double getCompletionRatio() const { return total_units > 0 ? static_cast<double>(completed_units) / total_units : 0.0; }
    double getFailureRate() const { return completed_units > 0 ? static_cast<double>(failed_units) / (completed_units + failed_units) : 0.0; }
    bool isComplete() const { return completed_units >= total_units; }
    bool hasErrors() const { return failed_units > 0; }
};

// EN: Configuration for individual progress tracking task
// FR: Configuration pour une tâche de suivi de progression individuelle
struct ProgressTaskConfig {
    std::string id;                                         // EN: Unique task identifier / FR: Identifiant unique de tâche
    std::string name;                                       // EN: Human-readable task name / FR: Nom de tâche lisible
    std::string description;                                // EN: Task description / FR: Description de la tâche
    size_t total_units = 1;                                // EN: Total units for this task / FR: Nombre total d'unités pour cette tâche
    double weight = 1.0;                                   // EN: Task weight for overall progress calculation / FR: Poids de la tâche pour le calcul de progression global
    std::chrono::milliseconds estimated_duration{0};       // EN: Estimated task duration / FR: Durée estimée de la tâche
    std::map<std::string, std::string> metadata;          // EN: Additional task metadata / FR: Métadonnées supplémentaires de tâche
    std::vector<std::string> dependencies;                 // EN: Task dependencies / FR: Dépendances de la tâche
    bool allow_parallel = true;                            // EN: Allow parallel execution / FR: Autoriser l'exécution parallèle
    double complexity_factor = 1.0;                       // EN: Task complexity multiplier / FR: Multiplicateur de complexité de tâche
};

// EN: Event data structure for progress callbacks
// FR: Structure de données d'événement pour les callbacks de progression
struct ProgressEvent {
    ProgressEventType type;                                 // EN: Event type / FR: Type d'événement
    ProgressEventSeverity severity = ProgressEventSeverity::INFO;
    std::chrono::system_clock::time_point timestamp;       // EN: Event timestamp / FR: Horodatage de l'événement
    std::string task_id;                                   // EN: Associated task ID / FR: ID de tâche associé
    std::string message;                                   // EN: Event message / FR: Message d'événement
    ProgressStatistics statistics;                         // EN: Current progress statistics / FR: Statistiques de progression actuelles
    std::map<std::string, std::string> metadata;          // EN: Additional event metadata / FR: Métadonnées supplémentaires d'événement
    
    // EN: Convenience constructors
    // FR: Constructeurs de commodité
    ProgressEvent(ProgressEventType t, const std::string& task = "", const std::string& msg = "")
        : type(t), timestamp(std::chrono::system_clock::now()), task_id(task), message(msg) {}
};

// EN: Configuration for progress monitor behavior
// FR: Configuration pour le comportement du moniteur de progression
struct ProgressMonitorConfig {
    ProgressUpdateMode update_mode = ProgressUpdateMode::THROTTLED;
    ProgressDisplayMode display_mode = ProgressDisplayMode::DETAILED_BAR;
    ETACalculationStrategy eta_strategy = ETACalculationStrategy::ADAPTIVE;
    
    std::chrono::milliseconds update_interval{100};        // EN: Minimum interval between updates / FR: Intervalle minimum entre les mises à jour
    std::chrono::milliseconds refresh_interval{50};        // EN: Display refresh interval / FR: Intervalle de rafraîchissement d'affichage
    size_t moving_average_window = 10;                     // EN: Window size for moving average / FR: Taille de fenêtre pour moyenne mobile
    double eta_confidence_threshold = 0.7;                // EN: Minimum confidence for ETA display / FR: Confiance minimale pour affichage ETA
    size_t max_history_size = 1000;                       // EN: Maximum history entries to keep / FR: Nombre maximum d'entrées historiques à conserver
    
    bool enable_colors = true;                             // EN: Enable colored output / FR: Activer la sortie colorée
    bool show_eta = true;                                  // EN: Show ETA estimation / FR: Afficher l'estimation ETA
    bool show_speed = true;                                // EN: Show processing speed / FR: Afficher la vitesse de traitement
    bool show_statistics = false;                          // EN: Show detailed statistics / FR: Afficher les statistiques détaillées
    bool enable_sound_notifications = false;               // EN: Enable sound notifications / FR: Activer les notifications sonores
    bool auto_hide_on_complete = true;                     // EN: Auto-hide progress on completion / FR: Masquer automatiquement la progression à la fin
    
    std::string progress_bar_chars = "█▇▆▅▄▃▂▁ ";          // EN: Characters for progress bar / FR: Caractères pour la barre de progression
    size_t progress_bar_width = 50;                       // EN: Width of progress bar / FR: Largeur de la barre de progression
    std::string eta_format = "ETA: {eta}";                // EN: ETA display format / FR: Format d'affichage ETA
    std::string speed_format = "{speed}/s";                // EN: Speed display format / FR: Format d'affichage de vitesse
    
    std::ostream* output_stream = &std::cout;              // EN: Output stream for display / FR: Flux de sortie pour l'affichage
    std::string log_file_path;                            // EN: Optional log file path / FR: Chemin optionnel du fichier de log
    bool enable_file_logging = false;                     // EN: Enable logging to file / FR: Activer la journalisation vers fichier
};

// EN: Callback function types for progress monitoring
// FR: Types de fonctions de callback pour la surveillance de progression
using ProgressEventCallback = std::function<void(const ProgressEvent&)>;
using ProgressCustomFormatter = std::function<std::string(const ProgressStatistics&, const ProgressMonitorConfig&)>;
using ProgressETAPredictor = std::function<std::chrono::milliseconds(const std::vector<ProgressStatistics>&)>;

// EN: Main progress monitor class for real-time tracking
// FR: Classe principale du moniteur de progression pour le suivi temps réel
class ProgressMonitor {
public:
    // EN: Constructor and destructor
    // FR: Constructeur et destructeur
    explicit ProgressMonitor(const ProgressMonitorConfig& config = {});
    ~ProgressMonitor();
    
    // EN: Task management
    // FR: Gestion des tâches
    bool addTask(const ProgressTaskConfig& task);
    bool removeTask(const std::string& task_id);
    bool updateTask(const std::string& task_id, const ProgressTaskConfig& task);
    std::vector<std::string> getTaskIds() const;
    std::optional<ProgressTaskConfig> getTask(const std::string& task_id) const;
    void clearTasks();
    
    // EN: Progress tracking
    // FR: Suivi de progression
    bool start();
    bool start(const std::vector<ProgressTaskConfig>& tasks);
    void stop();
    void pause();
    void resume();
    bool isRunning() const;
    bool isPaused() const;
    
    // EN: Progress updates
    // FR: Mises à jour de progression
    void updateProgress(const std::string& task_id, size_t completed_units);
    void updateProgress(const std::string& task_id, double percentage);
    void incrementProgress(const std::string& task_id, size_t units = 1);
    void setTaskCompleted(const std::string& task_id);
    void setTaskFailed(const std::string& task_id, const std::string& error_message = "");
    void reportMilestone(const std::string& task_id, const std::string& milestone_name);
    
    // EN: Batch progress operations
    // FR: Opérations de progression par lot
    void updateMultipleProgress(const std::map<std::string, size_t>& progress_updates);
    void setMultipleCompleted(const std::vector<std::string>& task_ids);
    void setMultipleFailed(const std::vector<std::string>& task_ids, const std::string& error_message = "");
    
    // EN: Statistics and information
    // FR: Statistiques et informations
    ProgressStatistics getOverallStatistics() const;
    ProgressStatistics getTaskStatistics(const std::string& task_id) const;
    std::map<std::string, ProgressStatistics> getAllTaskStatistics() const;
    std::vector<ProgressStatistics> getHistorySnapshot() const;
    
    // EN: ETA and prediction
    // FR: ETA et prédiction
    std::chrono::milliseconds getEstimatedTimeRemaining() const;
    std::chrono::milliseconds getEstimatedTimeRemaining(const std::string& task_id) const;
    std::chrono::system_clock::time_point getEstimatedCompletionTime() const;
    double getETAConfidence() const;
    std::string getETAString(bool include_confidence = false) const;
    
    // EN: Display and formatting
    // FR: Affichage et formatage
    void refreshDisplay();
    void forceDisplay();
    std::string getCurrentDisplayString() const;
    std::string getProgressBar(double percentage, size_t width = 0) const;
    std::string formatStatistics(const ProgressStatistics& stats) const;
    
    // EN: Configuration management
    // FR: Gestion de configuration
    void updateConfig(const ProgressMonitorConfig& config);
    ProgressMonitorConfig getConfig() const;
    void setCustomFormatter(ProgressCustomFormatter formatter);
    void setCustomETAPredictor(ProgressETAPredictor predictor);
    
    // EN: Event handling and callbacks
    // FR: Gestion d'événements et callbacks
    void setEventCallback(ProgressEventCallback callback);
    void removeEventCallback();
    void emitEvent(const ProgressEvent& event);
    
    // EN: Integration with external systems
    // FR: Intégration avec les systèmes externes
    void attachToPipeline(std::shared_ptr<PipelineEngine> pipeline);
    void detachFromPipeline();
    void attachToLogger(std::shared_ptr<Logger> logger);
    
    // EN: State serialization and persistence
    // FR: Sérialisation et persistance d'état
    bool saveState(const std::string& filepath) const;
    bool loadState(const std::string& filepath);
    std::string serializeState() const;
    bool deserializeState(const std::string& serialized_data);
    
    // EN: Performance and optimization
    // FR: Performance et optimisation
    void optimizeForLargeTaskCount();
    void optimizeForFrequentUpdates();
    void enableBatchMode(bool enabled = true);
    void setUpdateThrottleRate(double max_updates_per_second);
    
    // EN: Advanced features
    // FR: Fonctionnalités avancées
    void addDependency(const std::string& task_id, const std::string& dependency_id);
    void removeDependency(const std::string& task_id, const std::string& dependency_id);
    std::vector<std::string> getReadyTasks() const;
    bool canExecuteTask(const std::string& task_id) const;
    
    // EN: Utility and helper methods
    // FR: Méthodes utilitaires et d'aide
    static std::string formatDuration(std::chrono::milliseconds duration);
    static std::string formatSpeed(double speed, const std::string& unit = "items");
    static std::string formatFileSize(size_t bytes);
    static double calculateConfidence(const std::vector<double>& historical_errors);

private:
    // EN: Internal implementation using PIMPL pattern
    // FR: Implémentation interne utilisant le motif PIMPL
    class ProgressMonitorImpl;
    std::unique_ptr<ProgressMonitorImpl> impl_;
};

// EN: Specialized progress monitors for different use cases
// FR: Moniteurs de progression spécialisés pour différents cas d'usage

// EN: File transfer progress monitor
// FR: Moniteur de progression de transfert de fichier
class FileTransferProgressMonitor : public ProgressMonitor {
public:
    explicit FileTransferProgressMonitor(const ProgressMonitorConfig& config = {});
    
    void startTransfer(const std::string& filename, size_t total_bytes);
    void updateTransferred(size_t bytes_transferred);
    void setTransferRate(double bytes_per_second);
    std::string getCurrentTransferInfo() const;
};

// EN: Network operation progress monitor
// FR: Moniteur de progression d'opération réseau
class NetworkProgressMonitor : public ProgressMonitor {
public:
    explicit NetworkProgressMonitor(const ProgressMonitorConfig& config = {});
    
    void startNetworkOperation(const std::string& operation_name, size_t total_requests);
    void updateCompletedRequests(size_t completed_requests);
    void reportNetworkError(const std::string& error_message);
    void updateNetworkStats(double latency_ms, double throughput);
    std::string getNetworkSummary() const;
};

// EN: Batch processing progress monitor
// FR: Moniteur de progression de traitement par lot
class BatchProcessingProgressMonitor : public ProgressMonitor {
public:
    explicit BatchProcessingProgressMonitor(const ProgressMonitorConfig& config = {});
    
    void startBatch(const std::string& batch_name, size_t total_items);
    void updateBatchProgress(size_t processed_items, size_t failed_items = 0);
    void reportBatchStats(const std::map<std::string, size_t>& category_counts);
    std::string getBatchSummary() const;
};

// EN: Manager class for coordinating multiple progress monitors
// FR: Classe gestionnaire pour coordonner plusieurs moniteurs de progression
class ProgressMonitorManager {
public:
    // EN: Singleton access
    // FR: Accès singleton
    static ProgressMonitorManager& getInstance();
    
    // EN: Monitor management
    // FR: Gestion des moniteurs
    std::string createMonitor(const std::string& name, const ProgressMonitorConfig& config = {});
    bool removeMonitor(const std::string& monitor_id);
    std::shared_ptr<ProgressMonitor> getMonitor(const std::string& monitor_id);
    std::vector<std::string> getMonitorIds() const;
    
    // EN: Global operations
    // FR: Opérations globales
    void pauseAll();
    void resumeAll();
    void stopAll();
    void refreshAllDisplays();
    
    // EN: Statistics aggregation
    // FR: Agrégation de statistiques
    struct GlobalStatistics {
        size_t total_monitors = 0;
        size_t active_monitors = 0;
        size_t total_tasks = 0;
        size_t completed_tasks = 0;
        size_t failed_tasks = 0;
        double overall_progress = 0.0;
        std::chrono::milliseconds total_eta{0};
    };
    
    GlobalStatistics getGlobalStatistics() const;
    std::string getGlobalSummary() const;

private:
    // EN: Private constructor for singleton
    // FR: Constructeur privé pour singleton
    ProgressMonitorManager() = default;
    ~ProgressMonitorManager() = default;
    
    // EN: Internal state
    // FR: État interne
    mutable std::mutex monitors_mutex_;
    std::unordered_map<std::string, std::shared_ptr<ProgressMonitor>> monitors_;
    std::atomic<size_t> monitor_counter_{0};
};

// EN: RAII helper class for automatic progress monitoring
// FR: Classe d'aide RAII pour la surveillance automatique de progression
class AutoProgressMonitor {
public:
    AutoProgressMonitor(const std::string& name, const std::vector<ProgressTaskConfig>& tasks,
                       const ProgressMonitorConfig& config = {});
    ~AutoProgressMonitor();
    
    // EN: Progress operations
    // FR: Opérations de progression
    void updateProgress(const std::string& task_id, size_t completed_units);
    void incrementProgress(const std::string& task_id, size_t units = 1);
    void setTaskCompleted(const std::string& task_id);
    void setTaskFailed(const std::string& task_id, const std::string& error = "");
    
    // EN: Monitor access
    // FR: Accès au moniteur
    std::shared_ptr<ProgressMonitor> getMonitor() const;
    std::string getMonitorId() const;

private:
    std::string monitor_id_;
    std::shared_ptr<ProgressMonitor> monitor_;
    bool auto_cleanup_;
};

// EN: Utility functions for progress monitoring
// FR: Fonctions utilitaires pour la surveillance de progression
namespace ProgressUtils {
    
    // EN: Configuration helpers
    // FR: Assistants de configuration
    ProgressMonitorConfig createDefaultConfig();
    ProgressMonitorConfig createQuietConfig();
    ProgressMonitorConfig createVerboseConfig();
    ProgressMonitorConfig createFileTransferConfig();
    ProgressMonitorConfig createNetworkConfig();
    ProgressMonitorConfig createBatchProcessingConfig();
    
    // EN: Task generation helpers
    // FR: Assistants de génération de tâches
    std::vector<ProgressTaskConfig> createTasksFromFileList(const std::vector<std::string>& filenames);
    std::vector<ProgressTaskConfig> createTasksFromRange(const std::string& base_name, size_t count);
    ProgressTaskConfig createSimpleTask(const std::string& name, size_t total_units);
    
    // EN: Display utilities
    // FR: Utilitaires d'affichage
    std::string createProgressBar(double percentage, size_t width = 50, char filled = '#', char empty = '-');
    std::string createColoredProgressBar(double percentage, size_t width = 50);
    std::string formatBytes(size_t bytes);
    std::string formatRate(double rate, const std::string& unit);
    
    // EN: ETA calculation utilities
    // FR: Utilitaires de calcul ETA
    std::chrono::milliseconds calculateLinearETA(double current_progress, std::chrono::milliseconds elapsed);
    std::chrono::milliseconds calculateMovingAverageETA(const std::vector<double>& progress_history,
                                                      const std::vector<std::chrono::milliseconds>& time_history);
    double calculateETAConfidence(const std::vector<double>& eta_errors);
    
    // EN: Performance utilities
    // FR: Utilitaires de performance
    void optimizeTerminalOutput();
    void disableTerminalBuffering();
    void restoreTerminalSettings();
    
    // EN: Integration helpers
    // FR: Assistants d'intégration
    ProgressEventCallback createLoggerCallback(std::shared_ptr<Logger> logger);
    ProgressEventCallback createFileCallback(const std::string& filename);
    ProgressEventCallback createNetworkCallback(const std::string& webhook_url);
}

} // namespace Orchestrator
} // namespace BBP