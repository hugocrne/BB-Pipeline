// EN: Progress Monitor implementation for BB-Pipeline - Real-time progress tracking with ETA
// FR: Implémentation du Moniteur de Progression pour BB-Pipeline - Suivi de progression temps réel avec ETA

#include "orchestrator/progress_monitor.hpp"
#include "infrastructure/logging/logger.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <thread>
#include <chrono>
#include <unordered_set>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#endif

namespace BBP {
namespace Orchestrator {

// EN: Internal implementation class for ProgressMonitor using PIMPL pattern
// FR: Classe d'implémentation interne pour ProgressMonitor utilisant le motif PIMPL
class ProgressMonitor::ProgressMonitorImpl {
public:
    // EN: Constructor with configuration
    // FR: Constructeur avec configuration
    explicit ProgressMonitorImpl(const ProgressMonitorConfig& config)
        : config_(config)
        , is_running_(false)
        , is_paused_(false)
        , start_time_(std::chrono::system_clock::now())
        , last_update_time_(start_time_)
        , update_counter_(0)
        , display_thread_running_(false) {
        
        // EN: Initialize ETA calculation history
        // FR: Initialiser l'historique de calcul ETA
        progress_history_.reserve(config_.max_history_size);
        time_history_.reserve(config_.max_history_size);
        
        // EN: Initialize terminal settings
        // FR: Initialiser les paramètres de terminal
        if (config_.enable_colors) {
            initializeTerminalColors();
        }
        
        // EN: Setup file logging if enabled
        // FR: Configurer la journalisation vers fichier si activée
        if (config_.enable_file_logging && !config_.log_file_path.empty()) {
            log_file_.open(config_.log_file_path, std::ios::out | std::ios::app);
        }
    }
    
    // EN: Destructor - cleanup resources
    // FR: Destructeur - nettoyage des ressources
    ~ProgressMonitorImpl() {
        stop();
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }
    
    // EN: Add task to monitoring
    // FR: Ajouter une tâche à la surveillance
    bool addTask(const ProgressTaskConfig& task) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        // EN: Validate task configuration
        // FR: Valider la configuration de la tâche
        if (task.id.empty() || tasks_.find(task.id) != tasks_.end()) {
            return false;
        }
        
        tasks_[task.id] = task;
        
        // EN: Initialize task progress
        // FR: Initialiser la progression de la tâche
        TaskProgress& progress = task_progress_[task.id];
        progress.task_id = task.id;
        progress.total_units = task.total_units;
        progress.weight = task.weight;
        progress.status = TaskStatus::PENDING;
        progress.start_time = std::chrono::system_clock::now();
        
        // EN: Update total statistics
        // FR: Mettre à jour les statistiques totales
        updateTotalUnits();
        
        return true;
    }
    
    // EN: Update task configuration
    // FR: Mettre à jour la configuration de tâche
    bool updateTask(const std::string& task_id, const ProgressTaskConfig& task) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        auto it = tasks_.find(task_id);
        if (it == tasks_.end()) {
            return false;
        }
        
        tasks_[task_id] = task;
        
        // EN: Update progress tracking info
        // FR: Mettre à jour les informations de suivi de progression
        auto progress_it = task_progress_.find(task_id);
        if (progress_it != task_progress_.end()) {
            progress_it->second.total_units = task.total_units;
            progress_it->second.weight = task.weight;
        }
        
        updateTotalUnits();
        return true;
    }
    
    // EN: Get all task IDs
    // FR: Obtenir tous les IDs de tâche
    std::vector<std::string> getTaskIds() const {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        std::vector<std::string> ids;
        ids.reserve(tasks_.size());
        for (const auto& [task_id, task] : tasks_) {
            ids.push_back(task_id);
        }
        return ids;
    }
    
    // EN: Get task configuration
    // FR: Obtenir la configuration de tâche
    std::optional<ProgressTaskConfig> getTask(const std::string& task_id) const {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto it = tasks_.find(task_id);
        if (it == tasks_.end()) {
            return std::nullopt;
        }
        return it->second;
    }
    
    // EN: Clear all tasks
    // FR: Effacer toutes les tâches
    void clearTasks() {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        tasks_.clear();
        task_progress_.clear();
        updateTotalUnits();
    }
    
    // EN: Add missing lifecycle methods
    // FR: Ajouter les méthodes de cycle de vie manquantes
    bool isRunning() const {
        return is_running_.load();
    }
    
    bool isPaused() const {
        return is_paused_.load();
    }
    
    void pause() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (is_running_.load()) {
            is_paused_.store(true);
        }
    }
    
    void resume() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (is_running_.load()) {
            is_paused_.store(false);
        }
    }
    
    // EN: Remove task from monitoring
    // FR: Supprimer une tâche de la surveillance
    bool removeTask(const std::string& task_id) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        auto it = tasks_.find(task_id);
        if (it == tasks_.end()) {
            return false;
        }
        
        tasks_.erase(it);
        task_progress_.erase(task_id);
        
        // EN: Update dependencies
        // FR: Mettre à jour les dépendances
        removeDependenciesFor(task_id);
        updateTotalUnits();
        
        return true;
    }
    
    // EN: Start progress monitoring
    // FR: Démarrer la surveillance de progression
    bool start() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (is_running_) {
            return false;
        }
        
        is_running_ = true;
        is_paused_ = false;
        start_time_ = std::chrono::system_clock::now();
        last_update_time_ = start_time_;
        
        // EN: Start display thread if real-time mode
        // FR: Démarrer le thread d'affichage si mode temps réel
        if (config_.update_mode == ProgressUpdateMode::REAL_TIME || 
            config_.update_mode == ProgressUpdateMode::THROTTLED) {
            startDisplayThread();
        }
        
        // EN: Emit start event
        // FR: Émettre l'événement de démarrage
        emitEvent(ProgressEventType::STARTED, "", "Progress monitoring started");
        
        return true;
    }
    
    // EN: Start with predefined tasks
    // FR: Démarrer avec des tâches prédéfinies
    bool start(const std::vector<ProgressTaskConfig>& tasks) {
        // EN: Add all tasks first
        // FR: Ajouter toutes les tâches d'abord
        for (const auto& task : tasks) {
            addTask(task);
        }
        
        return start();
    }
    
    // EN: Stop progress monitoring
    // FR: Arrêter la surveillance de progression
    void stop() {
        std::unique_lock<std::mutex> lock(state_mutex_);
        
        if (!is_running_) {
            return;
        }
        
        is_running_ = false;
        is_paused_ = false;
        
        // EN: Stop display thread
        // FR: Arrêter le thread d'affichage
        stopDisplayThread();
        
        // EN: Emit completion event
        // FR: Émettre l'événement de completion
        auto overall_stats = calculateOverallStatistics();
        bool completed_successfully = overall_stats.isComplete() && !overall_stats.hasErrors();
        
        emitEvent(completed_successfully ? ProgressEventType::COMPLETED : ProgressEventType::CANCELLED,
                  "", completed_successfully ? "Progress monitoring completed" : "Progress monitoring stopped");
        
        // EN: Final display update
        // FR: Mise à jour finale d'affichage
        if (config_.display_mode != ProgressDisplayMode::JSON && !config_.auto_hide_on_complete) {
            forceDisplayUpdate();
        }
    }
    
    // EN: Update task progress
    // FR: Mettre à jour la progression de la tâche
    void updateProgress(const std::string& task_id, size_t completed_units) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        auto it = task_progress_.find(task_id);
        if (it == task_progress_.end()) {
            return;
        }
        
        TaskProgress& progress = it->second;
        auto old_completed = progress.completed_units;
        progress.completed_units = std::min(completed_units, progress.total_units);
        progress.last_update_time = std::chrono::system_clock::now();
        
        // EN: Update status if needed
        // FR: Mettre à jour le statut si nécessaire
        if (progress.status == TaskStatus::PENDING && progress.completed_units > 0) {
            progress.status = TaskStatus::RUNNING;
            emitEvent(ProgressEventType::STAGE_STARTED, task_id, "Task started");
        }
        
        // EN: Check for completion
        // FR: Vérifier la completion
        if (progress.completed_units >= progress.total_units && progress.status == TaskStatus::RUNNING) {
            setTaskCompleted(task_id);
        }
        
        // EN: Calculate current speed
        // FR: Calculer la vitesse actuelle
        updateTaskSpeed(progress, old_completed);
        
        // EN: Update history for ETA calculation
        // FR: Mettre à jour l'historique pour le calcul ETA
        updateProgressHistory();
        
        // EN: Emit update event
        // FR: Émettre l'événement de mise à jour
        if (shouldEmitUpdateEvent()) {
            emitEvent(ProgressEventType::UPDATED, task_id, "Progress updated");
        }
    }
    
    // EN: Update task progress by percentage
    // FR: Mettre à jour la progression de la tâche en pourcentage
    void updateProgress(const std::string& task_id, double percentage) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        auto it = task_progress_.find(task_id);
        if (it == task_progress_.end()) {
            return;
        }
        
        size_t completed_units = static_cast<size_t>(
            (percentage / 100.0) * it->second.total_units
        );
        
        // EN: Unlock and call the other method to avoid double locking
        // FR: Déverrouiller et appeler l'autre méthode pour éviter un double verrouillage
        tasks_mutex_.unlock();
        updateProgress(task_id, completed_units);
    }
    
    // EN: Set task as completed
    // FR: Marquer la tâche comme terminée
    void setTaskCompleted(const std::string& task_id) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        auto it = task_progress_.find(task_id);
        if (it == task_progress_.end()) {
            return;
        }
        
        TaskProgress& progress = it->second;
        if (progress.status == TaskStatus::COMPLETED || progress.status == TaskStatus::FAILED) {
            return;
        }
        
        progress.status = TaskStatus::COMPLETED;
        progress.completed_units = progress.total_units;
        progress.end_time = std::chrono::system_clock::now();
        
        // EN: Emit completion event
        // FR: Émettre l'événement de completion
        emitEvent(ProgressEventType::STAGE_COMPLETED, task_id, "Task completed successfully");
        
        // EN: Check if all tasks are completed
        // FR: Vérifier si toutes les tâches sont terminées
        if (areAllTasksCompleted()) {
            emitEvent(ProgressEventType::COMPLETED, "", "All tasks completed");
        }
    }
    
    // EN: Set task as failed
    // FR: Marquer la tâche comme échouée
    void setTaskFailed(const std::string& task_id, const std::string& error_message) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        auto it = task_progress_.find(task_id);
        if (it == task_progress_.end()) {
            return;
        }
        
        TaskProgress& progress = it->second;
        if (progress.status == TaskStatus::COMPLETED || progress.status == TaskStatus::FAILED) {
            return;
        }
        
        progress.status = TaskStatus::FAILED;
        progress.error_message = error_message;
        progress.end_time = std::chrono::system_clock::now();
        
        // EN: Emit failure event
        // FR: Émettre l'événement d'échec
        emitEvent(ProgressEventType::STAGE_FAILED, task_id, 
                  error_message.empty() ? "Task failed" : error_message);
    }
    
    // EN: Report milestone reached
    // FR: Rapporter un jalon atteint
    void reportMilestone(const std::string& task_id, const std::string& milestone_name) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        auto it = task_progress_.find(task_id);
        if (it == task_progress_.end()) {
            return;
        }
        
        emitEvent(ProgressEventType::MILESTONE_REACHED, task_id, "Milestone: " + milestone_name);
    }
    
    // EN: Update multiple tasks' progress
    // FR: Mettre à jour la progression de plusieurs tâches
    void updateMultipleProgress(const std::map<std::string, size_t>& progress_updates) {
        for (const auto& [task_id, completed_units] : progress_updates) {
            updateProgress(task_id, completed_units);
        }
    }
    
    // EN: Set multiple tasks as completed
    // FR: Marquer plusieurs tâches comme terminées
    void setMultipleCompleted(const std::vector<std::string>& task_ids) {
        for (const std::string& task_id : task_ids) {
            setTaskCompleted(task_id);
        }
    }
    
    // EN: Set multiple tasks as failed
    // FR: Marquer plusieurs tâches comme échouées
    void setMultipleFailed(const std::vector<std::string>& task_ids, const std::string& error_message) {
        for (const std::string& task_id : task_ids) {
            setTaskFailed(task_id, error_message);
        }
    }
    
    // EN: Get task-specific statistics
    // FR: Obtenir les statistiques spécifiques à une tâche
    ProgressStatistics getTaskStatistics(const std::string& task_id) const {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        ProgressStatistics stats;
        auto it = task_progress_.find(task_id);
        if (it == task_progress_.end()) {
            return stats;
        }
        
        const TaskProgress& progress = it->second;
        stats.start_time = progress.start_time;
        stats.last_update_time = progress.last_update_time;
        stats.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            stats.last_update_time - stats.start_time);
        stats.total_units = progress.total_units;
        stats.completed_units = progress.completed_units;
        stats.failed_units = (progress.status == TaskStatus::FAILED) ? progress.completed_units : 0;
        
        if (progress.total_units > 0) {
            stats.current_progress = (static_cast<double>(progress.completed_units) / progress.total_units) * 100.0;
        }
        
        stats.current_speed = progress.current_speed;
        if (!progress.speed_history.empty()) {
            stats.average_speed = std::accumulate(progress.speed_history.begin(), 
                                                progress.speed_history.end(), 0.0) / 
                                progress.speed_history.size();
        }
        
        return stats;
    }
    
    // EN: Get all task statistics
    // FR: Obtenir toutes les statistiques de tâches
    std::map<std::string, ProgressStatistics> getAllTaskStatistics() const {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        std::map<std::string, ProgressStatistics> all_stats;
        for (const auto& [task_id, progress] : task_progress_) {
            all_stats[task_id] = getTaskStatistics(task_id);
        }
        return all_stats;
    }
    
    // EN: Get history snapshot
    // FR: Obtenir un instantané de l'historique
    std::vector<ProgressStatistics> getHistorySnapshot() const {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        std::vector<ProgressStatistics> history;
        for (size_t i = 0; i < progress_history_.size() && i < time_history_.size(); ++i) {
            ProgressStatistics stats;
            stats.current_progress = progress_history_[i];
            stats.elapsed_time = time_history_[i];
            history.push_back(stats);
        }
        return history;
    }
    
    // EN: Get overall progress statistics
    // FR: Obtenir les statistiques globales de progression
    ProgressStatistics calculateOverallStatistics() const {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        ProgressStatistics stats;
        stats.start_time = start_time_;
        stats.last_update_time = std::chrono::system_clock::now();
        stats.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            stats.last_update_time - start_time_);
        
        // EN: Calculate totals
        // FR: Calculer les totaux
        double total_weighted_units = 0.0;
        double completed_weighted_units = 0.0;
        size_t total_units = 0;
        size_t completed_units = 0;
        size_t failed_units = 0;
        
        for (const auto& [task_id, progress] : task_progress_) {
            total_weighted_units += progress.total_units * progress.weight;
            completed_weighted_units += progress.completed_units * progress.weight;
            total_units += progress.total_units;
            completed_units += progress.completed_units;
            
            if (progress.status == TaskStatus::FAILED) {
                failed_units += progress.completed_units;
            }
        }
        
        stats.total_units = total_units;
        stats.completed_units = completed_units;
        stats.failed_units = failed_units;
        stats.update_count = update_counter_.load();
        
        // EN: Calculate progress percentage
        // FR: Calculer le pourcentage de progression
        if (total_weighted_units > 0) {
            stats.current_progress = (completed_weighted_units / total_weighted_units) * 100.0;
        }
        
        // EN: Calculate speeds
        // FR: Calculer les vitesses
        if (stats.elapsed_time.count() > 0) {
            stats.average_speed = static_cast<double>(completed_units) / 
                                  (stats.elapsed_time.count() / 1000.0);
            stats.current_speed = calculateCurrentSpeed();
        }
        
        // EN: Calculate ETA
        // FR: Calculer l'ETA
        stats.estimated_remaining_time = calculateETA(stats);
        stats.estimated_total_time = stats.elapsed_time + stats.estimated_remaining_time;
        stats.confidence_level = calculateETAConfidence();
        
        return stats;
    }
    
    // EN: Get estimated time remaining
    // FR: Obtenir le temps restant estimé
    std::chrono::milliseconds getEstimatedTimeRemaining() const {
        return calculateETA(calculateOverallStatistics());
    }
    
    std::chrono::milliseconds getEstimatedTimeRemaining(const std::string& task_id) const {
        auto task_stats = getTaskStatistics(task_id);
        return calculateETA(task_stats);
    }
    
    // EN: Get estimated completion time
    // FR: Obtenir l'heure de completion estimée
    std::chrono::system_clock::time_point getEstimatedCompletionTime() const {
        auto eta = getEstimatedTimeRemaining();
        return std::chrono::system_clock::now() + eta;
    }
    
    // EN: Get ETA confidence level
    // FR: Obtenir le niveau de confiance ETA
    double getETAConfidence() const {
        return calculateETAConfidence();
    }
    
    // EN: Refresh display
    // FR: Actualiser l'affichage
    void refreshDisplay() {
        if (is_running_.load()) {
            forceDisplayUpdate();
        }
    }
    
    // EN: Force display update
    // FR: Forcer la mise à jour d'affichage
    void forceDisplay() {
        forceDisplayUpdate();
    }
    
    // EN: Get progress bar string
    // FR: Obtenir la chaîne de barre de progression
    std::string getProgressBar(double percentage, size_t width) const {
        return createProgressBar(percentage, width);
    }
    
    // EN: Format statistics as string
    // FR: Formater les statistiques en chaîne
    std::string formatStatistics(const ProgressStatistics& stats) const {
        return createVerboseDisplay(stats);
    }
    
    // EN: Update configuration
    // FR: Mettre à jour la configuration
    void updateConfig(const ProgressMonitorConfig& config) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        config_ = config;
    }
    
    // EN: Get current configuration
    // FR: Obtenir la configuration actuelle
    ProgressMonitorConfig getConfig() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return config_;
    }
    
    // EN: Set custom ETA predictor
    // FR: Définir un prédicteur ETA personnalisé
    void setCustomETAPredictor(ProgressETAPredictor predictor) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        custom_eta_predictor_ = predictor;
    }
    
    // EN: Remove event callback
    // FR: Supprimer le callback d'événement
    void removeEventCallback() {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        event_callback_ = nullptr;
    }
    
    // EN: Emit progress event
    // FR: Émettre un événement de progression
    void emitEvent(const ProgressEvent& event) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (event_callback_) {
            try {
                event_callback_(event);
            } catch (const std::exception& e) {
                // EN: Log but don't throw / FR: Logger mais ne pas lever d'exception
            }
        }
    }
    
    // EN: Load state from file
    // FR: Charger l'état depuis un fichier
    bool loadState(const std::string& filepath) {
        try {
            std::ifstream file(filepath);
            if (!file.is_open()) {
                return false;
            }
            
            nlohmann::json state_json;
            file >> state_json;
            
            return deserializeState(state_json.dump());
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    // EN: Serialize state to string
    // FR: Sérialiser l'état en chaîne
    std::string serializeState() const {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        nlohmann::json state_json;
        state_json["version"] = "1.0";
        state_json["start_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            start_time_.time_since_epoch()).count();
        state_json["is_running"] = is_running_.load();
        
        nlohmann::json tasks_json = nlohmann::json::array();
        for (const auto& [task_id, task] : tasks_) {
            nlohmann::json task_json;
            task_json["id"] = task.id;
            task_json["name"] = task.name;
            task_json["total_units"] = task.total_units;
            tasks_json.push_back(task_json);
        }
        state_json["tasks"] = tasks_json;
        
        return state_json.dump();
    }
    
    // EN: Deserialize state from string
    // FR: Désérialiser l'état depuis une chaîne
    bool deserializeState(const std::string& serialized_data) {
        try {
            nlohmann::json state_json = nlohmann::json::parse(serialized_data);
            
            if (!state_json.contains("version") || !state_json.contains("tasks")) {
                return false;
            }
            
            // EN: Clear existing tasks
            // FR: Effacer les tâches existantes
            clearTasks();
            
            // EN: Load tasks
            // FR: Charger les tâches
            for (const auto& task_json : state_json["tasks"]) {
                ProgressTaskConfig task;
                task.id = task_json["id"];
                task.name = task_json["name"];
                task.total_units = task_json["total_units"];
                addTask(task);
            }
            
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    // EN: Get ETA string representation
    // FR: Obtenir la représentation textuelle de l'ETA
    std::string getETAString(bool include_confidence) const {
        auto eta = calculateETA(calculateOverallStatistics());
        
        if (eta.count() <= 0) {
            return "ETA: calculating...";
        }
        
        std::string eta_str = "ETA: " + formatDuration(eta);
        
        if (include_confidence) {
            double confidence = calculateETAConfidence();
            eta_str += " (confidence: " + std::to_string(static_cast<int>(confidence * 100)) + "%)";
        }
        
        return eta_str;
    }
    
    // EN: Create current display string
    // FR: Créer la chaîne d'affichage actuelle
    std::string getCurrentDisplayString() const {
        auto stats = calculateOverallStatistics();
        
        switch (config_.display_mode) {
            case ProgressDisplayMode::SIMPLE_BAR:
                return createSimpleProgressBar(stats);
            case ProgressDisplayMode::DETAILED_BAR:
                return createDetailedProgressBar(stats);
            case ProgressDisplayMode::PERCENTAGE:
                return createPercentageDisplay(stats);
            case ProgressDisplayMode::COMPACT:
                return createCompactDisplay(stats);
            case ProgressDisplayMode::VERBOSE:
                return createVerboseDisplay(stats);
            case ProgressDisplayMode::JSON:
                return createJSONDisplay(stats);
            case ProgressDisplayMode::CUSTOM:
                if (custom_formatter_) {
                    return custom_formatter_(stats, config_);
                }
                return createDetailedProgressBar(stats);
            default:
                return createDetailedProgressBar(stats);
        }
    }
    
    // EN: Set event callback
    // FR: Définir le callback d'événement
    void setEventCallback(ProgressEventCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        event_callback_ = callback;
    }
    
    // EN: Set custom formatter
    // FR: Définir le formateur personnalisé
    void setCustomFormatter(ProgressCustomFormatter formatter) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        custom_formatter_ = formatter;
    }
    
    // EN: Save state to file
    // FR: Sauvegarder l'état vers un fichier
    bool saveState(const std::string& filepath) const {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        
        try {
            nlohmann::json state_json;
            state_json["version"] = "1.0";
            state_json["start_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                start_time_.time_since_epoch()).count();
            state_json["is_running"] = is_running_.load();
            state_json["is_paused"] = is_paused_.load();
            
            // EN: Serialize tasks
            // FR: Sérialiser les tâches
            nlohmann::json tasks_json = nlohmann::json::array();
            for (const auto& [task_id, task] : tasks_) {
                nlohmann::json task_json;
                task_json["id"] = task.id;
                task_json["name"] = task.name;
                task_json["description"] = task.description;
                task_json["total_units"] = task.total_units;
                task_json["weight"] = task.weight;
                
                // EN: Add progress data
                // FR: Ajouter les données de progression
                auto progress_it = task_progress_.find(task_id);
                if (progress_it != task_progress_.end()) {
                    const auto& progress = progress_it->second;
                    task_json["completed_units"] = progress.completed_units;
                    task_json["status"] = static_cast<int>(progress.status);
                    task_json["error_message"] = progress.error_message;
                }
                
                tasks_json.push_back(task_json);
            }
            state_json["tasks"] = tasks_json;
            
            // EN: Write to file
            // FR: Écrire vers le fichier
            std::ofstream file(filepath);
            file << state_json.dump(2);
            return true;
            
        } catch (const std::exception& e) {
            return false;
        }
    }

private:
    // EN: Internal task status enumeration
    // FR: Énumération du statut interne de tâche
    enum class TaskStatus {
        PENDING,
        RUNNING,
        COMPLETED,
        FAILED
    };
    
    // EN: Internal task progress structure
    // FR: Structure interne de progression de tâche
    struct TaskProgress {
        std::string task_id;
        size_t total_units = 0;
        size_t completed_units = 0;
        double weight = 1.0;
        TaskStatus status = TaskStatus::PENDING;
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point last_update_time;
        std::chrono::system_clock::time_point end_time;
        std::string error_message;
        double current_speed = 0.0;
        std::deque<double> speed_history;
        std::vector<std::string> dependencies;
    };
    
    // EN: Configuration and state
    // FR: Configuration et état
    ProgressMonitorConfig config_;
    mutable std::mutex state_mutex_;
    mutable std::mutex tasks_mutex_;
    mutable std::mutex callback_mutex_;
    
    std::atomic<bool> is_running_;
    std::atomic<bool> is_paused_;
    std::atomic<size_t> update_counter_;
    
    std::chrono::system_clock::time_point start_time_;
    std::chrono::system_clock::time_point last_update_time_;
    
    // EN: Task management
    // FR: Gestion des tâches
    std::unordered_map<std::string, ProgressTaskConfig> tasks_;
    std::unordered_map<std::string, TaskProgress> task_progress_;
    size_t total_weighted_units_ = 0;
    
    // EN: ETA calculation data
    // FR: Données de calcul ETA
    std::deque<double> progress_history_;
    std::deque<std::chrono::milliseconds> time_history_;
    
    // EN: Callbacks
    // FR: Callbacks
    ProgressEventCallback event_callback_;
    ProgressCustomFormatter custom_formatter_;
    ProgressETAPredictor custom_eta_predictor_;
    
    // EN: Display thread management
    // FR: Gestion du thread d'affichage
    std::atomic<bool> display_thread_running_;
    std::thread display_thread_;
    std::condition_variable display_cv_;
    
    // EN: File logging
    // FR: Journalisation vers fichier
    std::ofstream log_file_;
    
    // EN: Terminal handling
    // FR: Gestion du terminal
    std::string color_reset_ = "\033[0m";
    std::string color_green_ = "\033[32m";
    std::string color_red_ = "\033[31m";
    std::string color_yellow_ = "\033[33m";
    std::string color_blue_ = "\033[34m";
    
    // EN: Internal helper methods
    // FR: Méthodes d'aide internes
    
    void initializeTerminalColors() {
        #ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            GetConsoleMode(hOut, &dwMode);
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
        #endif
    }
    
    void updateTotalUnits() {
        total_weighted_units_ = 0;
        for (const auto& [task_id, progress] : task_progress_) {
            total_weighted_units_ += progress.total_units * progress.weight;
        }
    }
    
    void removeDependenciesFor(const std::string& task_id) {
        for (auto& [id, progress] : task_progress_) {
            auto& deps = progress.dependencies;
            deps.erase(std::remove(deps.begin(), deps.end(), task_id), deps.end());
        }
    }
    
    void updateTaskSpeed(TaskProgress& progress, size_t old_completed) {
        auto now = std::chrono::system_clock::now();
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - progress.last_update_time);
        
        if (time_diff.count() > 0) {
            double units_per_ms = static_cast<double>(progress.completed_units - old_completed) / 
                                  time_diff.count();
            progress.current_speed = units_per_ms * 1000.0; // Convert to units per second
            
            // EN: Maintain speed history for smoothing
            // FR: Maintenir l'historique de vitesse pour le lissage
            progress.speed_history.push_back(progress.current_speed);
            if (progress.speed_history.size() > config_.moving_average_window) {
                progress.speed_history.pop_front();
            }
        }
    }
    
    double calculateCurrentSpeed() const {
        double total_speed = 0.0;
        size_t running_tasks = 0;
        
        for (const auto& [task_id, progress] : task_progress_) {
            if (progress.status == TaskStatus::RUNNING && !progress.speed_history.empty()) {
                double avg_speed = std::accumulate(progress.speed_history.begin(), 
                                                  progress.speed_history.end(), 0.0) / 
                                  progress.speed_history.size();
                total_speed += avg_speed;
                running_tasks++;
            }
        }
        
        return running_tasks > 0 ? total_speed : 0.0;
    }
    
    void updateProgressHistory() {
        auto stats = calculateOverallStatistics();
        
        progress_history_.push_back(stats.current_progress);
        time_history_.push_back(stats.elapsed_time);
        
        // EN: Limit history size
        // FR: Limiter la taille de l'historique
        while (progress_history_.size() > config_.max_history_size) {
            progress_history_.pop_front();
            time_history_.pop_front();
        }
    }
    
    std::chrono::milliseconds calculateETA(const ProgressStatistics& stats) const {
        if (custom_eta_predictor_) {
            std::vector<ProgressStatistics> history;
            // EN: Convert internal history to ProgressStatistics vector
            // FR: Convertir l'historique interne en vecteur ProgressStatistics
            return custom_eta_predictor_(history);
        }
        
        switch (config_.eta_strategy) {
            case ETACalculationStrategy::LINEAR:
                return calculateLinearETA(stats);
            case ETACalculationStrategy::MOVING_AVERAGE:
                return calculateMovingAverageETA();
            case ETACalculationStrategy::EXPONENTIAL:
                return calculateExponentialETA();
            case ETACalculationStrategy::ADAPTIVE:
                return calculateAdaptiveETA(stats);
            case ETACalculationStrategy::WEIGHTED:
                return calculateWeightedETA(stats);
            case ETACalculationStrategy::HISTORICAL:
                return calculateHistoricalETA();
            default:
                return calculateLinearETA(stats);
        }
    }
    
    std::chrono::milliseconds calculateLinearETA(const ProgressStatistics& stats) const {
        if (stats.current_progress <= 0.0 || stats.current_progress >= 100.0) {
            return std::chrono::milliseconds(0);
        }
        
        double remaining_percentage = 100.0 - stats.current_progress;
        double time_per_percentage = stats.elapsed_time.count() / stats.current_progress;
        
        return std::chrono::milliseconds(
            static_cast<long long>(remaining_percentage * time_per_percentage)
        );
    }
    
    std::chrono::milliseconds calculateMovingAverageETA() const {
        if (progress_history_.size() < 2) {
            return std::chrono::milliseconds(0);
        }
        
        size_t window_size = std::min(config_.moving_average_window, progress_history_.size());
        
        // EN: Calculate average speed over the window
        // FR: Calculer la vitesse moyenne sur la fenêtre
        double total_progress_change = 0.0;
        std::chrono::milliseconds total_time_change{0};
        
        for (size_t i = progress_history_.size() - window_size; i < progress_history_.size() - 1; ++i) {
            total_progress_change += progress_history_[i + 1] - progress_history_[i];
            total_time_change += time_history_[i + 1] - time_history_[i];
        }
        
        if (total_progress_change <= 0.0 || total_time_change.count() <= 0) {
            return std::chrono::milliseconds(0);
        }
        
        double current_progress = progress_history_.back();
        double remaining_progress = 100.0 - current_progress;
        double progress_rate = total_progress_change / total_time_change.count();
        
        return std::chrono::milliseconds(
            static_cast<long long>(remaining_progress / progress_rate)
        );
    }
    
    std::chrono::milliseconds calculateExponentialETA() const {
        // EN: Exponential smoothing implementation
        // FR: Implémentation du lissage exponentiel
        if (progress_history_.size() < 2) {
            return std::chrono::milliseconds(0);
        }
        
        const double alpha = 0.3; // EN: Smoothing factor / FR: Facteur de lissage
        double smoothed_speed = 0.0;
        
        for (size_t i = 1; i < progress_history_.size(); ++i) {
            double speed = (progress_history_[i] - progress_history_[i-1]) / 
                          (time_history_[i] - time_history_[i-1]).count();
            
            if (i == 1) {
                smoothed_speed = speed;
            } else {
                smoothed_speed = alpha * speed + (1 - alpha) * smoothed_speed;
            }
        }
        
        if (smoothed_speed <= 0.0) {
            return std::chrono::milliseconds(0);
        }
        
        double remaining_progress = 100.0 - progress_history_.back();
        return std::chrono::milliseconds(
            static_cast<long long>(remaining_progress / smoothed_speed)
        );
    }
    
    std::chrono::milliseconds calculateAdaptiveETA(const ProgressStatistics& stats) const {
        // EN: Combine multiple strategies and weight them based on confidence
        // FR: Combiner plusieurs stratégies et les pondérer selon la confiance
        
        auto linear_eta = calculateLinearETA(stats);
        auto moving_avg_eta = calculateMovingAverageETA();
        auto exponential_eta = calculateExponentialETA();
        
        // EN: Weight based on history size and confidence
        // FR: Pondérer selon la taille de l'historique et la confiance
        double linear_weight = 0.3;
        double moving_avg_weight = std::min(0.4, progress_history_.size() / 20.0);
        double exponential_weight = 0.7 - moving_avg_weight;
        
        double total_weight = linear_weight + moving_avg_weight + exponential_weight;
        
        long long weighted_eta = static_cast<long long>(
            (linear_eta.count() * linear_weight + 
             moving_avg_eta.count() * moving_avg_weight + 
             exponential_eta.count() * exponential_weight) / total_weight
        );
        
        return std::chrono::milliseconds(weighted_eta);
    }
    
    std::chrono::milliseconds calculateWeightedETA(const ProgressStatistics& stats) const {
        // EN: Calculate ETA based on task weights and complexity
        // FR: Calculer l'ETA basé sur les poids et la complexité des tâches
        
        double total_weighted_remaining = 0.0;
        double total_weighted_completed = 0.0;
        std::chrono::milliseconds total_execution_time{0};
        
        for (const auto& [task_id, progress] : task_progress_) {
            double completed_ratio = progress.total_units > 0 ? 
                static_cast<double>(progress.completed_units) / progress.total_units : 0.0;
            
            total_weighted_completed += completed_ratio * progress.weight;
            total_weighted_remaining += (1.0 - completed_ratio) * progress.weight;
            
            if (progress.status != TaskStatus::PENDING) {
                total_execution_time += std::chrono::duration_cast<std::chrono::milliseconds>(
                    progress.last_update_time - progress.start_time);
            }
        }
        
        if (total_weighted_completed <= 0.0) {
            return std::chrono::milliseconds(0);
        }
        
        double time_per_weighted_unit = total_execution_time.count() / total_weighted_completed;
        
        return std::chrono::milliseconds(
            static_cast<long long>(total_weighted_remaining * time_per_weighted_unit)
        );
    }
    
    std::chrono::milliseconds calculateHistoricalETA() const {
        // EN: Placeholder for historical data-based ETA calculation
        // FR: Placeholder pour le calcul ETA basé sur les données historiques
        // EN: In a real implementation, this would use historical execution data
        // FR: Dans une implémentation réelle, cela utiliserait des données d'exécution historiques
        return calculateLinearETA(calculateOverallStatistics());
    }
    
    double calculateETAConfidence() const {
        if (progress_history_.size() < 3) {
            return 0.1; // EN: Low confidence with little data / FR: Faible confiance avec peu de données
        }
        
        // EN: Calculate confidence based on consistency of progress rate
        // FR: Calculer la confiance basée sur la consistance du taux de progression
        std::vector<double> rates;
        for (size_t i = 1; i < std::min(progress_history_.size(), size_t(10)); ++i) {
            double rate = (progress_history_[i] - progress_history_[i-1]) / 
                          (time_history_[i] - time_history_[i-1]).count();
            if (rate > 0) {
                rates.push_back(rate);
            }
        }
        
        if (rates.empty()) {
            return 0.1;
        }
        
        // EN: Calculate coefficient of variation
        // FR: Calculer le coefficient de variation
        double mean = std::accumulate(rates.begin(), rates.end(), 0.0) / rates.size();
        double variance = 0.0;
        for (double rate : rates) {
            variance += std::pow(rate - mean, 2);
        }
        variance /= rates.size();
        double std_dev = std::sqrt(variance);
        double cv = mean > 0 ? std_dev / mean : 1.0;
        
        // EN: Lower coefficient of variation means higher confidence
        // FR: Un coefficient de variation plus faible signifie une confiance plus élevée
        return std::max(0.1, std::min(1.0, 1.0 - cv));
    }
    
    bool shouldEmitUpdateEvent() const {
        static auto last_emit_time = std::chrono::system_clock::now();
        auto now = std::chrono::system_clock::now();
        auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_emit_time);
        
        if (time_since_last >= config_.update_interval) {
            last_emit_time = now;
            return true;
        }
        return false;
    }
    
    bool areAllTasksCompleted() const {
        for (const auto& [task_id, progress] : task_progress_) {
            if (progress.status != TaskStatus::COMPLETED && progress.status != TaskStatus::FAILED) {
                return false;
            }
        }
        return !task_progress_.empty();
    }
    
    void emitEvent(ProgressEventType type, const std::string& task_id, const std::string& message) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        
        if (!event_callback_) {
            return;
        }
        
        ProgressEvent event(type, task_id, message);
        event.statistics = calculateOverallStatistics();
        
        try {
            event_callback_(event);
        } catch (const std::exception& e) {
            // EN: Log callback error but don't throw
            // FR: Journaliser l'erreur de callback mais ne pas lever d'exception
        }
        
        // EN: Write to log file if enabled
        // FR: Écrire vers le fichier de log si activé
        if (log_file_.is_open()) {
            log_file_ << "[" << formatTimestamp(event.timestamp) << "] " 
                      << "Type: " << static_cast<int>(event.type) 
                      << ", Task: " << event.task_id 
                      << ", Message: " << event.message << std::endl;
        }
    }
    
    void startDisplayThread() {
        if (display_thread_running_.load()) {
            return;
        }
        
        display_thread_running_ = true;
        display_thread_ = std::thread([this]() {
            while (display_thread_running_.load()) {
                if (!is_paused_.load()) {
                    forceDisplayUpdate();
                }
                
                std::unique_lock<std::mutex> lock(state_mutex_);
                display_cv_.wait_for(lock, config_.refresh_interval, [this]() {
                    return !display_thread_running_.load();
                });
            }
        });
    }
    
    void stopDisplayThread() {
        if (!display_thread_running_.load()) {
            return;
        }
        
        display_thread_running_ = false;
        display_cv_.notify_all();
        
        if (display_thread_.joinable()) {
            display_thread_.join();
        }
    }
    
    void forceDisplayUpdate() {
        if (config_.display_mode == ProgressDisplayMode::JSON) {
            return; // EN: JSON mode doesn't do continuous updates / FR: Le mode JSON ne fait pas de mises à jour continues
        }
        
        std::string display_string = getCurrentDisplayString();
        
        if (config_.output_stream) {
            *config_.output_stream << "\r" << display_string << std::flush;
        }
    }
    
    std::string createSimpleProgressBar(const ProgressStatistics& stats) const {
        std::string bar = createProgressBar(stats.current_progress, config_.progress_bar_width);
        return "[" + bar + "] " + std::to_string(static_cast<int>(stats.current_progress)) + "%";
    }
    
    std::string createDetailedProgressBar(const ProgressStatistics& stats) const {
        std::ostringstream oss;
        
        std::string bar = createProgressBar(stats.current_progress, config_.progress_bar_width);
        oss << "[" << bar << "] ";
        oss << std::fixed << std::setprecision(1) << stats.current_progress << "% ";
        oss << "(" << stats.completed_units << "/" << stats.total_units << ") ";
        
        if (config_.show_speed && stats.current_speed > 0) {
            oss << std::fixed << std::setprecision(1) << stats.current_speed << "/s ";
        }
        
        if (config_.show_eta && stats.estimated_remaining_time.count() > 0) {
            oss << "ETA: " << formatDuration(stats.estimated_remaining_time);
        }
        
        return oss.str();
    }
    
    std::string createPercentageDisplay(const ProgressStatistics& stats) const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << stats.current_progress << "%";
        return oss.str();
    }
    
    std::string createCompactDisplay(const ProgressStatistics& stats) const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << stats.current_progress << "% ";
        oss << "(" << stats.completed_units << "/" << stats.total_units << ")";
        
        if (config_.show_eta && stats.estimated_remaining_time.count() > 0) {
            oss << " ETA:" << formatDuration(stats.estimated_remaining_time);
        }
        
        return oss.str();
    }
    
    std::string createVerboseDisplay(const ProgressStatistics& stats) const {
        std::ostringstream oss;
        oss << "\nProgress Report:\n";
        oss << "  Progress: " << std::fixed << std::setprecision(2) << stats.current_progress << "%\n";
        oss << "  Completed: " << stats.completed_units << "/" << stats.total_units << " units\n";
        oss << "  Elapsed: " << formatDuration(stats.elapsed_time) << "\n";
        
        if (stats.estimated_remaining_time.count() > 0) {
            oss << "  ETA: " << formatDuration(stats.estimated_remaining_time) 
                << " (confidence: " << std::fixed << std::setprecision(0) 
                << stats.confidence_level * 100 << "%)\n";
        }
        
        if (stats.current_speed > 0) {
            oss << "  Current Speed: " << std::fixed << std::setprecision(1) << stats.current_speed << "/s\n";
            oss << "  Average Speed: " << std::fixed << std::setprecision(1) << stats.average_speed << "/s\n";
        }
        
        if (stats.failed_units > 0) {
            oss << "  Failed: " << stats.failed_units << " units\n";
        }
        
        return oss.str();
    }
    
    std::string createJSONDisplay(const ProgressStatistics& stats) const {
        nlohmann::json json_stats;
        json_stats["progress_percentage"] = stats.current_progress;
        json_stats["completed_units"] = stats.completed_units;
        json_stats["total_units"] = stats.total_units;
        json_stats["failed_units"] = stats.failed_units;
        json_stats["elapsed_time_ms"] = stats.elapsed_time.count();
        json_stats["estimated_remaining_time_ms"] = stats.estimated_remaining_time.count();
        json_stats["current_speed"] = stats.current_speed;
        json_stats["average_speed"] = stats.average_speed;
        json_stats["confidence_level"] = stats.confidence_level;
        json_stats["update_count"] = stats.update_count;
        
        return json_stats.dump();
    }
    
    std::string createProgressBar(double percentage, size_t width) const {
        if (width == 0) width = config_.progress_bar_width;
        
        size_t filled = static_cast<size_t>((percentage / 100.0) * width);
        std::string bar;
        
        if (config_.enable_colors) {
            bar += color_green_;
        }
        
        for (size_t i = 0; i < filled; ++i) {
            bar += config_.progress_bar_chars[0];
        }
        
        if (config_.enable_colors) {
            bar += color_reset_;
        }
        
        for (size_t i = filled; i < width; ++i) {
            bar += config_.progress_bar_chars.back();
        }
        
        return bar;
    }
    
    std::string formatDuration(std::chrono::milliseconds duration) const {
        auto total_seconds = duration.count() / 1000;
        auto hours = total_seconds / 3600;
        auto minutes = (total_seconds % 3600) / 60;
        auto seconds = total_seconds % 60;
        
        std::ostringstream oss;
        if (hours > 0) {
            oss << hours << "h " << minutes << "m " << seconds << "s";
        } else if (minutes > 0) {
            oss << minutes << "m " << seconds << "s";
        } else {
            oss << seconds << "s";
        }
        
        return oss.str();
    }
    
    std::string formatTimestamp(std::chrono::system_clock::time_point timestamp) const {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }
};

// EN: Additional implementation methods for ProgressMonitorImpl
// FR: Méthodes d'implémentation supplémentaires pour ProgressMonitorImpl

class ProgressMonitor::ProgressMonitorImpl {
public:
    // EN: All the existing methods remain the same...
    // FR: Toutes les méthodes existantes restent les mêmes...
    
    // EN: Missing method implementations that need to be added
    // FR: Implémentations de méthodes manquantes qui doivent être ajoutées
    
    void optimizeForLargeTaskCount() {
        config_.update_mode = ProgressUpdateMode::THROTTLED;
        config_.update_interval = std::chrono::milliseconds(500);
        config_.max_history_size = 100; // Reduce history for memory efficiency
    }
    
    void optimizeForFrequentUpdates() {
        config_.update_mode = ProgressUpdateMode::THROTTLED;
        config_.update_interval = std::chrono::milliseconds(50);
        config_.refresh_interval = std::chrono::milliseconds(16); // ~60 FPS
    }
    
    void enableBatchMode(bool enabled) {
        // EN: Store batch mode state for future use
        // FR: Stocker l'état du mode batch pour usage futur
        batch_mode_enabled_ = enabled;
    }
    
    void setUpdateThrottleRate(double max_updates_per_second) {
        if (max_updates_per_second > 0) {
            config_.update_interval = std::chrono::milliseconds(
                static_cast<long long>(1000.0 / max_updates_per_second)
            );
        }
    }
    
    void addDependency(const std::string& task_id, const std::string& dependency_id) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto it = task_progress_.find(task_id);
        if (it != task_progress_.end()) {
            auto& deps = it->second.dependencies;
            if (std::find(deps.begin(), deps.end(), dependency_id) == deps.end()) {
                deps.push_back(dependency_id);
            }
        }
    }
    
    void removeDependency(const std::string& task_id, const std::string& dependency_id) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto it = task_progress_.find(task_id);
        if (it != task_progress_.end()) {
            auto& deps = it->second.dependencies;
            deps.erase(std::remove(deps.begin(), deps.end(), dependency_id), deps.end());
        }
    }
    
    std::vector<std::string> getReadyTasks() const {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        std::vector<std::string> ready_tasks;
        
        for (const auto& [task_id, progress] : task_progress_) {
            if (progress.status == TaskStatus::PENDING && canExecuteTaskInternal(task_id)) {
                ready_tasks.push_back(task_id);
            }
        }
        
        return ready_tasks;
    }
    
    bool canExecuteTask(const std::string& task_id) const {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        return canExecuteTaskInternal(task_id);
    }
    
    void attachToPipeline(std::shared_ptr<PipelineEngine> pipeline) {
        // EN: Store pipeline reference for integration
        // FR: Stocker la référence du pipeline pour l'intégration
        attached_pipeline_ = pipeline;
    }
    
    void detachFromPipeline() {
        attached_pipeline_.reset();
    }
    
    void attachToLogger(std::shared_ptr<Logger> logger) {
        // EN: Store logger reference for integration
        // FR: Stocker la référence du logger pour l'intégration
        attached_logger_ = logger;
    }
    
private:
    bool batch_mode_enabled_ = false;
    std::weak_ptr<PipelineEngine> attached_pipeline_;
    std::weak_ptr<Logger> attached_logger_;
    
    bool canExecuteTaskInternal(const std::string& task_id) const {
        auto it = task_progress_.find(task_id);
        if (it == task_progress_.end()) {
            return false;
        }
        
        const auto& progress = it->second;
        
        // EN: Check if all dependencies are completed
        // FR: Vérifier si toutes les dépendances sont terminées
        for (const std::string& dep_id : progress.dependencies) {
            auto dep_it = task_progress_.find(dep_id);
            if (dep_it == task_progress_.end() || dep_it->second.status != TaskStatus::COMPLETED) {
                return false;
            }
        }
        
        return true;
    }
};

// EN: ProgressMonitor public interface implementation
// FR: Implémentation de l'interface publique ProgressMonitor

ProgressMonitor::ProgressMonitor(const ProgressMonitorConfig& config)
    : impl_(std::make_unique<ProgressMonitorImpl>(config)) {
}

ProgressMonitor::~ProgressMonitor() = default;

bool ProgressMonitor::addTask(const ProgressTaskConfig& task) {
    return impl_->addTask(task);
}

bool ProgressMonitor::removeTask(const std::string& task_id) {
    return impl_->removeTask(task_id);
}

bool ProgressMonitor::updateTask(const std::string& task_id, const ProgressTaskConfig& task) {
    return impl_->updateTask(task_id, task);
}

std::vector<std::string> ProgressMonitor::getTaskIds() const {
    return impl_->getTaskIds();
}

std::optional<ProgressTaskConfig> ProgressMonitor::getTask(const std::string& task_id) const {
    return impl_->getTask(task_id);
}

void ProgressMonitor::clearTasks() {
    impl_->clearTasks();
}

bool ProgressMonitor::start() {
    return impl_->start();
}

bool ProgressMonitor::start(const std::vector<ProgressTaskConfig>& tasks) {
    return impl_->start(tasks);
}

void ProgressMonitor::stop() {
    impl_->stop();
}

void ProgressMonitor::updateProgress(const std::string& task_id, size_t completed_units) {
    impl_->updateProgress(task_id, completed_units);
}

void ProgressMonitor::updateProgress(const std::string& task_id, double percentage) {
    impl_->updateProgress(task_id, percentage);
}

void ProgressMonitor::incrementProgress(const std::string& task_id, size_t units) {
    auto stats = getTaskStatistics(task_id);
    updateProgress(task_id, stats.completed_units + units);
}

void ProgressMonitor::setTaskCompleted(const std::string& task_id) {
    impl_->setTaskCompleted(task_id);
}

void ProgressMonitor::setTaskFailed(const std::string& task_id, const std::string& error_message) {
    impl_->setTaskFailed(task_id, error_message);
}

void ProgressMonitor::reportMilestone(const std::string& task_id, const std::string& milestone_name) {
    impl_->reportMilestone(task_id, milestone_name);
}

void ProgressMonitor::updateMultipleProgress(const std::map<std::string, size_t>& progress_updates) {
    impl_->updateMultipleProgress(progress_updates);
}

void ProgressMonitor::setMultipleCompleted(const std::vector<std::string>& task_ids) {
    impl_->setMultipleCompleted(task_ids);
}

void ProgressMonitor::setMultipleFailed(const std::vector<std::string>& task_ids, const std::string& error_message) {
    impl_->setMultipleFailed(task_ids, error_message);
}

ProgressStatistics ProgressMonitor::getTaskStatistics(const std::string& task_id) const {
    return impl_->getTaskStatistics(task_id);
}

std::map<std::string, ProgressStatistics> ProgressMonitor::getAllTaskStatistics() const {
    return impl_->getAllTaskStatistics();
}

std::vector<ProgressStatistics> ProgressMonitor::getHistorySnapshot() const {
    return impl_->getHistorySnapshot();
}

ProgressStatistics ProgressMonitor::getOverallStatistics() const {
    return impl_->calculateOverallStatistics();
}

std::string ProgressMonitor::getETAString(bool include_confidence) const {
    return impl_->getETAString(include_confidence);
}

std::string ProgressMonitor::getCurrentDisplayString() const {
    return impl_->getCurrentDisplayString();
}

void ProgressMonitor::setEventCallback(ProgressEventCallback callback) {
    impl_->setEventCallback(callback);
}

void ProgressMonitor::setCustomFormatter(ProgressCustomFormatter formatter) {
    impl_->setCustomFormatter(formatter);
}

std::chrono::milliseconds ProgressMonitor::getEstimatedTimeRemaining() const {
    return impl_->getEstimatedTimeRemaining();
}

std::chrono::milliseconds ProgressMonitor::getEstimatedTimeRemaining(const std::string& task_id) const {
    return impl_->getEstimatedTimeRemaining(task_id);
}

std::chrono::system_clock::time_point ProgressMonitor::getEstimatedCompletionTime() const {
    return impl_->getEstimatedCompletionTime();
}

double ProgressMonitor::getETAConfidence() const {
    return impl_->getETAConfidence();
}

void ProgressMonitor::refreshDisplay() {
    impl_->refreshDisplay();
}

void ProgressMonitor::forceDisplay() {
    impl_->forceDisplay();
}

std::string ProgressMonitor::getProgressBar(double percentage, size_t width) const {
    return impl_->getProgressBar(percentage, width);
}

std::string ProgressMonitor::formatStatistics(const ProgressStatistics& stats) const {
    return impl_->formatStatistics(stats);
}

void ProgressMonitor::updateConfig(const ProgressMonitorConfig& config) {
    impl_->updateConfig(config);
}

ProgressMonitorConfig ProgressMonitor::getConfig() const {
    return impl_->getConfig();
}

void ProgressMonitor::setCustomETAPredictor(ProgressETAPredictor predictor) {
    impl_->setCustomETAPredictor(predictor);
}

void ProgressMonitor::removeEventCallback() {
    impl_->removeEventCallback();
}

void ProgressMonitor::emitEvent(const ProgressEvent& event) {
    impl_->emitEvent(event);
}

bool ProgressMonitor::saveState(const std::string& filepath) const {
    return impl_->saveState(filepath);
}

bool ProgressMonitor::loadState(const std::string& filepath) {
    return impl_->loadState(filepath);
}

std::string ProgressMonitor::serializeState() const {
    return impl_->serializeState();
}

bool ProgressMonitor::deserializeState(const std::string& serialized_data) {
    return impl_->deserializeState(serialized_data);
}

void ProgressMonitor::optimizeForLargeTaskCount() {
    impl_->optimizeForLargeTaskCount();
}

void ProgressMonitor::optimizeForFrequentUpdates() {
    impl_->optimizeForFrequentUpdates();
}

void ProgressMonitor::enableBatchMode(bool enabled) {
    impl_->enableBatchMode(enabled);
}

void ProgressMonitor::setUpdateThrottleRate(double max_updates_per_second) {
    impl_->setUpdateThrottleRate(max_updates_per_second);
}

void ProgressMonitor::addDependency(const std::string& task_id, const std::string& dependency_id) {
    impl_->addDependency(task_id, dependency_id);
}

void ProgressMonitor::removeDependency(const std::string& task_id, const std::string& dependency_id) {
    impl_->removeDependency(task_id, dependency_id);
}

std::vector<std::string> ProgressMonitor::getReadyTasks() const {
    return impl_->getReadyTasks();
}

bool ProgressMonitor::canExecuteTask(const std::string& task_id) const {
    return impl_->canExecuteTask(task_id);
}

void ProgressMonitor::attachToPipeline(std::shared_ptr<PipelineEngine> pipeline) {
    impl_->attachToPipeline(pipeline);
}

void ProgressMonitor::detachFromPipeline() {
    impl_->detachFromPipeline();
}

void ProgressMonitor::attachToLogger(std::shared_ptr<Logger> logger) {
    impl_->attachToLogger(logger);
}

bool ProgressMonitor::isRunning() const {
    return impl_->isRunning();
}

bool ProgressMonitor::isPaused() const {
    return impl_->isPaused();
}

void ProgressMonitor::pause() {
    impl_->pause();
}

void ProgressMonitor::resume() {
    impl_->resume();
}

// EN: Static utility method implementations
// FR: Implémentations des méthodes utilitaires statiques

std::string ProgressMonitor::formatDuration(std::chrono::milliseconds duration) {
    auto total_seconds = duration.count() / 1000;
    auto hours = total_seconds / 3600;
    auto minutes = (total_seconds % 3600) / 60;
    auto seconds = total_seconds % 60;
    
    std::ostringstream oss;
    if (hours > 0) {
        oss << hours << "h " << minutes << "m " << seconds << "s";
    } else if (minutes > 0) {
        oss << minutes << "m " << seconds << "s";
    } else {
        oss << seconds << "s";
    }
    
    return oss.str();
}

std::string ProgressMonitor::formatSpeed(double speed, const std::string& unit) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << speed << " " << unit;
    return oss.str();
}

std::string ProgressMonitor::formatFileSize(size_t bytes) {
    return ProgressUtils::formatBytes(bytes);
}

double ProgressMonitor::calculateConfidence(const std::vector<double>& historical_errors) {
    return ProgressUtils::calculateETAConfidence(historical_errors);
}

// EN: Utility functions implementation
// FR: Implémentation des fonctions utilitaires

namespace ProgressUtils {

ProgressMonitorConfig createDefaultConfig() {
    ProgressMonitorConfig config;
    config.update_mode = ProgressUpdateMode::THROTTLED;
    config.display_mode = ProgressDisplayMode::DETAILED_BAR;
    config.eta_strategy = ETACalculationStrategy::ADAPTIVE;
    config.update_interval = std::chrono::milliseconds(100);
    config.refresh_interval = std::chrono::milliseconds(50);
    config.show_eta = true;
    config.show_speed = true;
    config.enable_colors = true;
    return config;
}

ProgressMonitorConfig createQuietConfig() {
    auto config = createDefaultConfig();
    config.display_mode = ProgressDisplayMode::PERCENTAGE;
    config.show_eta = false;
    config.show_speed = false;
    config.enable_colors = false;
    config.update_interval = std::chrono::milliseconds(1000);
    return config;
}

std::string createProgressBar(double percentage, size_t width, char filled, char empty) {
    size_t filled_width = static_cast<size_t>((percentage / 100.0) * width);
    std::string bar(filled_width, filled);
    bar.append(width - filled_width, empty);
    return bar;
}

std::string formatBytes(size_t bytes) {
    const std::vector<std::string> units = {"B", "KB", "MB", "GB", "TB"};
    size_t unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < units.size() - 1) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit_index];
    return oss.str();
}

std::chrono::milliseconds calculateLinearETA(double current_progress, std::chrono::milliseconds elapsed) {
    if (current_progress <= 0.0 || current_progress >= 100.0) {
        return std::chrono::milliseconds(0);
    }
    
    double remaining_percentage = 100.0 - current_progress;
    double time_per_percentage = elapsed.count() / current_progress;
    
    return std::chrono::milliseconds(
        static_cast<long long>(remaining_percentage * time_per_percentage)
    );
}

ProgressMonitorConfig createVerboseConfig() {
    auto config = createDefaultConfig();
    config.display_mode = ProgressDisplayMode::VERBOSE;
    config.show_statistics = true;
    config.update_interval = std::chrono::milliseconds(200);
    return config;
}

ProgressMonitorConfig createFileTransferConfig() {
    auto config = createDefaultConfig();
    config.display_mode = ProgressDisplayMode::DETAILED_BAR;
    config.eta_strategy = ETACalculationStrategy::MOVING_AVERAGE;
    config.show_speed = true;
    config.progress_bar_chars = "████▓▓▒▒░░ ";
    return config;
}

ProgressMonitorConfig createNetworkConfig() {
    auto config = createDefaultConfig();
    config.update_mode = ProgressUpdateMode::THROTTLED;
    config.update_interval = std::chrono::milliseconds(50);
    config.show_speed = true;
    config.eta_strategy = ETACalculationStrategy::EXPONENTIAL;
    return config;
}

ProgressMonitorConfig createBatchProcessingConfig() {
    auto config = createDefaultConfig();
    config.display_mode = ProgressDisplayMode::COMPACT;
    config.update_interval = std::chrono::milliseconds(1000);
    config.show_eta = true;
    return config;
}

std::vector<ProgressTaskConfig> createTasksFromFileList(const std::vector<std::string>& filenames) {
    std::vector<ProgressTaskConfig> tasks;
    tasks.reserve(filenames.size());
    
    for (size_t i = 0; i < filenames.size(); ++i) {
        ProgressTaskConfig task;
        task.id = "file_" + std::to_string(i);
        task.name = filenames[i];
        task.description = "Processing file: " + filenames[i];
        task.total_units = 1;
        task.weight = 1.0;
        tasks.push_back(task);
    }
    
    return tasks;
}

std::vector<ProgressTaskConfig> createTasksFromRange(const std::string& base_name, size_t count) {
    std::vector<ProgressTaskConfig> tasks;
    tasks.reserve(count);
    
    for (size_t i = 1; i <= count; ++i) {
        ProgressTaskConfig task;
        task.id = base_name + "_" + std::to_string(i);
        task.name = base_name + " " + std::to_string(i);
        task.description = "Task " + std::to_string(i) + " of " + std::to_string(count);
        task.total_units = 100;
        task.weight = 1.0;
        tasks.push_back(task);
    }
    
    return tasks;
}

ProgressTaskConfig createSimpleTask(const std::string& name, size_t total_units) {
    ProgressTaskConfig task;
    task.id = name;
    task.name = name;
    task.description = "Simple task: " + name;
    task.total_units = total_units;
    task.weight = 1.0;
    return task;
}

std::string createColoredProgressBar(double percentage, size_t width) {
    if (width == 0) width = 50;
    
    size_t filled = static_cast<size_t>((percentage / 100.0) * width);
    std::string bar;
    
    // EN: Green for progress, gray for remaining
    // FR: Vert pour la progression, gris pour le restant
    bar += "\033[32m"; // Green
    for (size_t i = 0; i < filled; ++i) {
        bar += "█";
    }
    bar += "\033[37m"; // Gray
    for (size_t i = filled; i < width; ++i) {
        bar += "░";
    }
    bar += "\033[0m"; // Reset
    
    return bar;
}

std::string formatRate(double rate, const std::string& unit) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << rate << " " << unit;
    return oss.str();
}

std::chrono::milliseconds calculateMovingAverageETA(const std::vector<double>& progress_history,
                                                  const std::vector<std::chrono::milliseconds>& time_history) {
    if (progress_history.size() < 2 || time_history.size() < 2 || 
        progress_history.size() != time_history.size()) {
        return std::chrono::milliseconds(0);
    }
    
    size_t window_size = std::min(size_t(10), progress_history.size());
    double total_progress_change = 0.0;
    std::chrono::milliseconds total_time_change{0};
    
    for (size_t i = progress_history.size() - window_size; i < progress_history.size() - 1; ++i) {
        total_progress_change += progress_history[i + 1] - progress_history[i];
        total_time_change += time_history[i + 1] - time_history[i];
    }
    
    if (total_progress_change <= 0.0 || total_time_change.count() <= 0) {
        return std::chrono::milliseconds(0);
    }
    
    double current_progress = progress_history.back();
    double remaining_progress = 100.0 - current_progress;
    double progress_rate = total_progress_change / total_time_change.count();
    
    return std::chrono::milliseconds(
        static_cast<long long>(remaining_progress / progress_rate)
    );
}

double calculateETAConfidence(const std::vector<double>& eta_errors) {
    if (eta_errors.empty()) {
        return 0.1;
    }
    
    double mean_error = std::accumulate(eta_errors.begin(), eta_errors.end(), 0.0) / eta_errors.size();
    double variance = 0.0;
    for (double error : eta_errors) {
        variance += std::pow(error - mean_error, 2);
    }
    variance /= eta_errors.size();
    double std_dev = std::sqrt(variance);
    
    // EN: Lower standard deviation means higher confidence
    // FR: Un écart-type plus faible signifie une confiance plus élevée
    return std::max(0.1, std::min(1.0, 1.0 - std_dev));
}

} // namespace ProgressUtils

// EN: Specialized monitor implementations
// FR: Implémentations de moniteurs spécialisés

FileTransferProgressMonitor::FileTransferProgressMonitor(const ProgressMonitorConfig& config)
    : ProgressMonitor(config) {
}

void FileTransferProgressMonitor::startTransfer(const std::string& filename, size_t total_bytes) {
    ProgressTaskConfig task = ProgressUtils::createSimpleTask("file_transfer", total_bytes);
    task.name = "Transferring " + filename;
    task.description = "File transfer: " + filename;
    addTask(task);
    start();
}

void FileTransferProgressMonitor::updateTransferred(size_t bytes_transferred) {
    updateProgress("file_transfer", bytes_transferred);
}

void FileTransferProgressMonitor::setTransferRate(double bytes_per_second) {
    // EN: Store transfer rate in metadata for display
    // FR: Stocker le taux de transfert dans les métadonnées pour l'affichage
}

std::string FileTransferProgressMonitor::getCurrentTransferInfo() const {
    auto stats = getOverallStatistics();
    std::ostringstream oss;
    oss << "Transfer: " << std::fixed << std::setprecision(1) << stats.current_progress << "% ";
    oss << "(" << ProgressUtils::formatBytes(stats.completed_units) 
        << "/" << ProgressUtils::formatBytes(stats.total_units) << ") ";
    if (stats.current_speed > 0) {
        oss << "at " << ProgressUtils::formatBytes(static_cast<size_t>(stats.current_speed)) << "/s";
    }
    return oss.str();
}

NetworkProgressMonitor::NetworkProgressMonitor(const ProgressMonitorConfig& config)
    : ProgressMonitor(config) {
}

void NetworkProgressMonitor::startNetworkOperation(const std::string& operation_name, size_t total_requests) {
    ProgressTaskConfig task = ProgressUtils::createSimpleTask("network_op", total_requests);
    task.name = operation_name;
    task.description = "Network operation: " + operation_name;
    addTask(task);
    start();
}

void NetworkProgressMonitor::updateCompletedRequests(size_t completed_requests) {
    updateProgress("network_op", completed_requests);
}

void NetworkProgressMonitor::reportNetworkError(const std::string& error_message) {
    reportMilestone("network_op", "Error: " + error_message);
}

void NetworkProgressMonitor::updateNetworkStats(double latency_ms, double throughput) {
    // EN: Store network stats for display
    // FR: Stocker les statistiques réseau pour l'affichage
}

std::string NetworkProgressMonitor::getNetworkSummary() const {
    auto stats = getOverallStatistics();
    std::ostringstream oss;
    oss << "Network: " << stats.completed_units << "/" << stats.total_units << " requests ";
    oss << "(" << std::fixed << std::setprecision(1) << stats.current_progress << "%) ";
    if (stats.current_speed > 0) {
        oss << "at " << std::fixed << std::setprecision(1) << stats.current_speed << " req/s";
    }
    return oss.str();
}

BatchProcessingProgressMonitor::BatchProcessingProgressMonitor(const ProgressMonitorConfig& config)
    : ProgressMonitor(config) {
}

void BatchProcessingProgressMonitor::startBatch(const std::string& batch_name, size_t total_items) {
    ProgressTaskConfig task = ProgressUtils::createSimpleTask("batch_processing", total_items);
    task.name = batch_name;
    task.description = "Batch processing: " + batch_name;
    addTask(task);
    start();
}

void BatchProcessingProgressMonitor::updateBatchProgress(size_t processed_items, size_t failed_items) {
    updateProgress("batch_processing", processed_items);
    if (failed_items > 0) {
        reportMilestone("batch_processing", std::to_string(failed_items) + " items failed");
    }
}

void BatchProcessingProgressMonitor::reportBatchStats(const std::map<std::string, size_t>& category_counts) {
    std::ostringstream oss;
    oss << "Categories: ";
    for (const auto& [category, count] : category_counts) {
        oss << category << "=" << count << " ";
    }
    reportMilestone("batch_processing", oss.str());
}

std::string BatchProcessingProgressMonitor::getBatchSummary() const {
    auto stats = getOverallStatistics();
    std::ostringstream oss;
    oss << "Batch: " << stats.completed_units << "/" << stats.total_units << " items ";
    oss << "(" << std::fixed << std::setprecision(1) << stats.current_progress << "%)";
    if (stats.failed_units > 0) {
        oss << ", " << stats.failed_units << " failed";
    }
    return oss.str();
}

// EN: Progress Monitor Manager implementation
// FR: Implémentation du Gestionnaire de Moniteur de Progression

ProgressMonitorManager& ProgressMonitorManager::getInstance() {
    static ProgressMonitorManager instance;
    return instance;
}

std::string ProgressMonitorManager::createMonitor(const std::string& name, const ProgressMonitorConfig& config) {
    std::lock_guard<std::mutex> lock(monitors_mutex_);
    
    std::string monitor_id = name + "_" + std::to_string(monitor_counter_.fetch_add(1));
    monitors_[monitor_id] = std::make_shared<ProgressMonitor>(config);
    
    return monitor_id;
}

bool ProgressMonitorManager::removeMonitor(const std::string& monitor_id) {
    std::lock_guard<std::mutex> lock(monitors_mutex_);
    
    auto it = monitors_.find(monitor_id);
    if (it == monitors_.end()) {
        return false;
    }
    
    it->second->stop();
    monitors_.erase(it);
    return true;
}

std::shared_ptr<ProgressMonitor> ProgressMonitorManager::getMonitor(const std::string& monitor_id) {
    std::lock_guard<std::mutex> lock(monitors_mutex_);
    
    auto it = monitors_.find(monitor_id);
    if (it == monitors_.end()) {
        return nullptr;
    }
    
    return it->second;
}

std::vector<std::string> ProgressMonitorManager::getMonitorIds() const {
    std::lock_guard<std::mutex> lock(monitors_mutex_);
    
    std::vector<std::string> ids;
    ids.reserve(monitors_.size());
    for (const auto& [monitor_id, monitor] : monitors_) {
        ids.push_back(monitor_id);
    }
    return ids;
}

void ProgressMonitorManager::pauseAll() {
    std::lock_guard<std::mutex> lock(monitors_mutex_);
    for (auto& [monitor_id, monitor] : monitors_) {
        monitor->pause();
    }
}

void ProgressMonitorManager::resumeAll() {
    std::lock_guard<std::mutex> lock(monitors_mutex_);
    for (auto& [monitor_id, monitor] : monitors_) {
        monitor->resume();
    }
}

void ProgressMonitorManager::stopAll() {
    std::lock_guard<std::mutex> lock(monitors_mutex_);
    for (auto& [monitor_id, monitor] : monitors_) {
        monitor->stop();
    }
}

void ProgressMonitorManager::refreshAllDisplays() {
    std::lock_guard<std::mutex> lock(monitors_mutex_);
    for (auto& [monitor_id, monitor] : monitors_) {
        monitor->refreshDisplay();
    }
}

ProgressMonitorManager::GlobalStatistics ProgressMonitorManager::getGlobalStatistics() const {
    std::lock_guard<std::mutex> lock(monitors_mutex_);
    
    GlobalStatistics global_stats;
    global_stats.total_monitors = monitors_.size();
    
    for (const auto& [monitor_id, monitor] : monitors_) {
        if (monitor->isRunning()) {
            global_stats.active_monitors++;
        }
        
        auto stats = monitor->getOverallStatistics();
        global_stats.total_tasks += stats.total_units;
        global_stats.completed_tasks += stats.completed_units;
        global_stats.failed_tasks += stats.failed_units;
        
        if (stats.estimated_remaining_time > global_stats.total_eta) {
            global_stats.total_eta = stats.estimated_remaining_time;
        }
    }
    
    if (global_stats.total_tasks > 0) {
        global_stats.overall_progress = (static_cast<double>(global_stats.completed_tasks) / 
                                       global_stats.total_tasks) * 100.0;
    }
    
    return global_stats;
}

std::string ProgressMonitorManager::getGlobalSummary() const {
    auto stats = getGlobalStatistics();
    std::ostringstream oss;
    oss << "Global: " << stats.active_monitors << "/" << stats.total_monitors << " monitors active, ";
    oss << stats.completed_tasks << "/" << stats.total_tasks << " tasks ";
    oss << "(" << std::fixed << std::setprecision(1) << stats.overall_progress << "%)";
    return oss.str();
}

// EN: Auto Progress Monitor RAII helper implementation
// FR: Implémentation de l'assistant RAII Auto Progress Monitor

AutoProgressMonitor::AutoProgressMonitor(const std::string& name, 
                                       const std::vector<ProgressTaskConfig>& tasks,
                                       const ProgressMonitorConfig& config)
    : auto_cleanup_(true) {
    auto& manager = ProgressMonitorManager::getInstance();
    monitor_id_ = manager.createMonitor(name, config);
    monitor_ = manager.getMonitor(monitor_id_);
    
    if (monitor_) {
        monitor_->start(tasks);
    }
}

AutoProgressMonitor::~AutoProgressMonitor() {
    if (auto_cleanup_ && !monitor_id_.empty()) {
        auto& manager = ProgressMonitorManager::getInstance();
        manager.removeMonitor(monitor_id_);
    }
}

void AutoProgressMonitor::updateProgress(const std::string& task_id, size_t completed_units) {
    if (monitor_) {
        monitor_->updateProgress(task_id, completed_units);
    }
}

void AutoProgressMonitor::incrementProgress(const std::string& task_id, size_t units) {
    if (monitor_) {
        monitor_->incrementProgress(task_id, units);
    }
}

void AutoProgressMonitor::setTaskCompleted(const std::string& task_id) {
    if (monitor_) {
        monitor_->setTaskCompleted(task_id);
    }
}

void AutoProgressMonitor::setTaskFailed(const std::string& task_id, const std::string& error) {
    if (monitor_) {
        monitor_->setTaskFailed(task_id, error);
    }
}

std::shared_ptr<ProgressMonitor> AutoProgressMonitor::getMonitor() const {
    return monitor_;
}

std::string AutoProgressMonitor::getMonitorId() const {
    return monitor_id_;
}

} // namespace Orchestrator
} // namespace BBP