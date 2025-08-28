// EN: Kill Switch System Implementation for BB-Pipeline - Emergency shutdown with comprehensive state preservation
// FR: Implémentation du Système Kill Switch pour BB-Pipeline - Arrêt d'urgence avec préservation complète de l'état

#include "orchestrator/kill_switch.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <random>
#include <nlohmann/json.hpp>
#include <zlib.h>

// EN: Built-in CRC32 implementation for systems without crc32c
// FR: Implémentation CRC32 intégrée pour systèmes sans crc32c
namespace {
    uint32_t crc32_builtin(const std::string& data) {
        uint32_t crc = 0xFFFFFFFF;
        for (char c : data) {
            crc ^= static_cast<uint32_t>(c);
            for (int i = 0; i < 8; ++i) {
                crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
            }
        }
        return ~crc;
    }
}

namespace BBP {
namespace Orchestrator {

// EN: Static singleton instance pointer
// FR: Pointeur d'instance singleton statique
KillSwitch* KillSwitch::instance_ = nullptr;

// EN: Singleton instance access
// FR: Accès à l'instance singleton
KillSwitch& KillSwitch::getInstance() {
    // EN: Thread-safe singleton initialization with double-checked locking
    // FR: Initialisation singleton thread-safe avec verrouillage à double vérification
    static std::once_flag once_flag;
    std::call_once(once_flag, []() {
        instance_ = new KillSwitch();
    });
    return *instance_;
}

// EN: Private constructor - initializes basic state
// FR: Constructeur privé - initialise l'état de base
KillSwitch::KillSwitch() 
    : created_at_(std::chrono::system_clock::now()) {
    
    // EN: Initialize statistics
    // FR: Initialiser les statistiques
    stats_.created_at = created_at_;
    
    // EN: Set default configuration
    // FR: Définir la configuration par défaut
    config_ = KillSwitchUtils::createDefaultConfig();
    
    // EN: Log Kill Switch creation
    // FR: Logger la création du Kill Switch
    Logger::getInstance().info("KillSwitch", "Instance created and ready for initialization / Instance créée et prête pour l'initialisation");
}

// EN: Destructor - ensures proper cleanup
// FR: Destructeur - assure un nettoyage approprié
KillSwitch::~KillSwitch() {
    try {
        if (shutting_down_ && shutdown_thread_ && shutdown_thread_->joinable()) {
            // EN: Wait for shutdown thread to complete
            // FR: Attendre que le thread d'arrêt se termine
            shutdown_thread_->join();
        }
        
        Logger::getInstance().info("KillSwitch", "Instance destroyed / Instance détruite");
    } catch (const std::exception& e) {
        // EN: Log error but don't throw from destructor
        // FR: Logger l'erreur mais ne pas lancer depuis le destructeur
        Logger::getInstance().error("KillSwitch", "Error in destructor: " + std::string(e.what()) + " / Erreur dans le destructeur: " + std::string(e.what()));
    }
}

// EN: Configure the Kill Switch system
// FR: Configure le système Kill Switch
void KillSwitch::configure(const KillSwitchConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // EN: Validate configuration before applying
    // FR: Valider la configuration avant application
    auto validation_errors = KillSwitchUtils::validateConfig(config);
    if (!validation_errors.empty()) {
        std::string error_msg = "Invalid Kill Switch configuration: ";
        for (const auto& error : validation_errors) {
            error_msg += error + "; ";
        }
        Logger::getInstance().error("KillSwitch", "Invalid configuration: " + error_msg + " / Configuration invalide: " + error_msg);
        throw std::invalid_argument(error_msg);
    }
    
    config_ = config;
    
    // EN: Ensure state directory exists
    // FR: S'assurer que le répertoire d'état existe
    if (!ensureStateDirectoryExists()) {
        Logger::getInstance().warn("KillSwitch", "Failed to create state directory: " + config_.state_directory + " / Échec de création du répertoire d'état: " + config_.state_directory);
    }
    
    Logger::getInstance().info("KillSwitch", "Configured successfully / Configuré avec succès");
}

// EN: Initialize the Kill Switch system
// FR: Initialise le système Kill Switch
void KillSwitch::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        Logger::getInstance().warn("KillSwitch", "Already initialized / Déjà initialisé");
        return;
    }
    
    try {
        // EN: Ensure state directory exists
        // FR: S'assurer que le répertoire d'état existe
        if (!ensureStateDirectoryExists()) {
            throw std::runtime_error("Failed to create state directory");
        }
        
        // EN: Clean up old state files if configured
        // FR: Nettoyer les anciens fichiers d'état si configuré
        cleanupOldStateFiles();
        
        // EN: Integration with existing SignalHandler for emergency triggers
        // FR: Intégration avec le SignalHandler existant pour déclencheurs d'urgence
        auto& signal_handler = SignalHandler::getInstance();
        signal_handler.registerCleanupCallback("kill_switch_emergency", [this]() {
            if (!triggered_) {
                trigger(KillSwitchTrigger::SYSTEM_SIGNAL, "Signal handler triggered emergency shutdown");
            }
        });
        
        initialized_ = true;
        
        Logger::getInstance().info("KillSwitch", "Initialized successfully / Initialisé avec succès");
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("KillSwitch", "Failed to initialize: " + std::string(e.what()) + " / Échec d'initialisation: " + std::string(e.what()));
        throw;
    }
}

// EN: Register a state preservation callback for a component
// FR: Enregistre un callback de préservation d'état pour un composant
void KillSwitch::registerStatePreservationCallback(const std::string& component_id, StatePreservationCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (component_id.empty()) {
        throw std::invalid_argument("Component ID cannot be empty");
    }
    
    state_callbacks_[component_id] = callback;
    
    Logger::getInstance().debug("KillSwitch", "Registered state preservation callback for component: " + component_id + " / Callback de préservation d'état enregistré pour le composant: " + component_id);
}

// EN: Register a task termination callback for graceful task stopping
// FR: Enregistre un callback de terminaison de tâche pour arrêt propre des tâches
void KillSwitch::registerTaskTerminationCallback(const std::string& task_type, TaskTerminationCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (task_type.empty()) {
        throw std::invalid_argument("Task type cannot be empty");
    }
    
    task_callbacks_[task_type] = callback;
    
    Logger::getInstance().debug("KillSwitch", "Registered task termination callback for task type: " + task_type + " / Callback de terminaison de tâche enregistré pour le type de tâche: " + task_type);
}

// EN: Register a cleanup operation callback
// FR: Enregistre un callback d'opération de nettoyage
void KillSwitch::registerCleanupCallback(const std::string& operation_name, CleanupOperationCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (operation_name.empty()) {
        throw std::invalid_argument("Operation name cannot be empty");
    }
    
    cleanup_callbacks_[operation_name] = callback;
    
    Logger::getInstance().debug("KillSwitch", "Registered cleanup callback for operation: " + operation_name + " / Callback de nettoyage enregistré pour l'opération: " + operation_name);
}

// EN: Register a notification callback for external system integration
// FR: Enregistre un callback de notification pour intégration système externe
void KillSwitch::registerNotificationCallback(const std::string& notification_id, NotificationCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (notification_id.empty()) {
        throw std::invalid_argument("Notification ID cannot be empty");
    }
    
    notification_callbacks_[notification_id] = callback;
    
    Logger::getInstance().debug("KillSwitch", "Registered notification callback: " + notification_id + " / Callback de notification enregistré: " + notification_id);
}

// EN: Trigger emergency shutdown with specified reason
// FR: Déclenche l'arrêt d'urgence avec raison spécifiée
void KillSwitch::trigger(KillSwitchTrigger trigger_reason, const std::string& details) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!enabled_) {
        Logger::getInstance().warn("KillSwitch", "Disabled, ignoring trigger request / Désactivé, requête de déclenchement ignorée");
        return;
    }
    
    if (triggered_) {
        Logger::getInstance().warn("KillSwitch", "Already triggered, ignoring duplicate request / Déjà déclenché, requête dupliquée ignorée");
        return;
    }
    
    // EN: Record trigger information
    // FR: Enregistrer les informations de déclenchement
    triggered_ = true;
    current_trigger_ = trigger_reason;
    trigger_details_ = details;
    triggered_at_ = std::chrono::system_clock::now();
    
    // EN: Update statistics
    // FR: Mettre à jour les statistiques
    stats_.total_triggers++;
    stats_.trigger_counts[trigger_reason]++;
    stats_.last_triggered_at = triggered_at_;
    stats_.recent_trigger_reasons.push_back(KillSwitchUtils::triggerToString(trigger_reason) + ": " + details);
    
    // EN: Keep only recent triggers (last 10)
    // FR: Garder seulement les déclencheurs récents (10 derniers)
    if (stats_.recent_trigger_reasons.size() > 10) {
        stats_.recent_trigger_reasons.erase(stats_.recent_trigger_reasons.begin());
    }
    
    Logger::getInstance().error("KillSwitch", "TRIGGERED: " + KillSwitchUtils::triggerToString(trigger_reason) + " - " + details + " / DÉCLENCHÉ: " + KillSwitchUtils::triggerToString(trigger_reason) + " - " + details);
    
    // EN: Send trigger notifications
    // FR: Envoyer les notifications de déclenchement
    sendNotifications(trigger_reason, KillSwitchPhase::TRIGGERED, details);
    
    // EN: Start shutdown execution in separate thread to avoid blocking
    // FR: Démarrer l'exécution d'arrêt dans un thread séparé pour éviter le blocage
    shutdown_thread_ = std::make_unique<std::thread>(&KillSwitch::executeShutdown, this);
    
    // EN: Notify waiting threads
    // FR: Notifier les threads en attente
    cv_.notify_all();
}

// EN: Trigger shutdown with custom timeout
// FR: Déclenche l'arrêt avec timeout personnalisé
void KillSwitch::triggerWithTimeout(KillSwitchTrigger trigger_reason, std::chrono::milliseconds custom_timeout, const std::string& details) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // EN: Temporarily modify timeout configuration
    // FR: Modifier temporairement la configuration de timeout
    auto original_timeout = config_.total_shutdown_timeout;
    config_.total_shutdown_timeout = custom_timeout;
    
    // EN: Unlock for trigger call
    // FR: Déverrouiller pour l'appel trigger
    mutex_.unlock();
    trigger(trigger_reason, details + " (custom timeout: " + std::to_string(custom_timeout.count()) + "ms)");
    mutex_.lock();
    
    // EN: Restore original timeout
    // FR: Restaurer le timeout original
    config_.total_shutdown_timeout = original_timeout;
}

// EN: Check if Kill Switch has been triggered
// FR: Vérifie si le Kill Switch a été déclenché
bool KillSwitch::isTriggered() const {
    return triggered_.load();
}

// EN: Check if shutdown is currently in progress
// FR: Vérifie si l'arrêt est actuellement en cours
bool KillSwitch::isShuttingDown() const {
    return shutting_down_.load();
}

// EN: Get current execution phase
// FR: Obtient la phase d'exécution actuelle
KillSwitchPhase KillSwitch::getCurrentPhase() const {
    return current_phase_.load();
}

// EN: Wait for shutdown completion (blocks until finished)
// FR: Attend la completion de l'arrêt (bloque jusqu'à la fin)
bool KillSwitch::waitForCompletion(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!triggered_) {
        return true; // EN: Not triggered, already "complete" / FR: Pas déclenché, déjà "terminé"
    }
    
    return cv_.wait_for(lock, timeout, [this] {
        return shutdown_completed_.load();
    });
}

// EN: Force immediate shutdown (bypasses graceful shutdown)
// FR: Force l'arrêt immédiat (contourne l'arrêt propre)
void KillSwitch::forceImmediate(const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_.force_immediate_stop = true;
    stats_.forced_shutdowns++;
    
    Logger::getInstance().error("KillSwitch", "FORCING IMMEDIATE SHUTDOWN: " + reason + " / FORÇAGE D'ARRÊT IMMÉDIAT: " + reason);
    
    if (!triggered_) {
        mutex_.unlock();
        trigger(KillSwitchTrigger::USER_REQUEST, "Force immediate: " + reason);
    }
}

// EN: Cancel shutdown if still in early phases (not always possible)
// FR: Annule l'arrêt si encore dans les phases précoces (pas toujours possible)
bool KillSwitch::cancelShutdown(const std::string& reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto current_phase = current_phase_.load();
    
    // EN: Can only cancel in very early phases
    // FR: Peut seulement annuler dans les phases très précoces
    if (current_phase == KillSwitchPhase::TRIGGERED || current_phase == KillSwitchPhase::INACTIVE) {
        triggered_ = false;
        shutting_down_ = false;
        current_phase_ = KillSwitchPhase::INACTIVE;
        
        Logger::getInstance().info("KillSwitch", "Shutdown cancelled: " + reason + " / Arrêt annulé: " + reason);
        
        return true;
    }
    
    Logger::getInstance().warn("KillSwitch", "Cannot cancel shutdown in phase: " + KillSwitchUtils::phaseToString(current_phase) + " / Impossible d'annuler l'arrêt en phase: " + KillSwitchUtils::phaseToString(current_phase));
    return false;
}

// EN: Get current Kill Switch statistics
// FR: Obtient les statistiques actuelles du Kill Switch
KillSwitchStats KillSwitch::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

// EN: Get current configuration
// FR: Obtient la configuration actuelle
KillSwitchConfig KillSwitch::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

// EN: Execute the shutdown sequence
// FR: Exécute la séquence d'arrêt
void KillSwitch::executeShutdown() {
    auto shutdown_start = std::chrono::system_clock::now();
    shutting_down_ = true;
    shutdown_started_at_ = shutdown_start;
    
    try {
        Logger::getInstance().info("KillSwitch", "Starting shutdown sequence / Démarrage de la séquence d'arrêt");
        
        bool success = true;
        
        // EN: Phase 1: Stop running tasks
        // FR: Phase 1 : Arrêter les tâches en cours
        if (success && !config_.force_immediate_stop) {
            transitionToPhase(KillSwitchPhase::STOPPING_TASKS);
            success = stopRunningTasks();
        }
        
        // EN: Phase 2: Preserve current state
        // FR: Phase 2 : Préserver l'état actuel
        if (success && config_.preserve_partial_results) {
            transitionToPhase(KillSwitchPhase::SAVING_STATE);
            success = preserveCurrentState();
        }
        
        // EN: Phase 3: Execute cleanup operations
        // FR: Phase 3 : Exécuter les opérations de nettoyage
        if (success || config_.force_immediate_stop) {
            transitionToPhase(KillSwitchPhase::CLEANUP);
            success = executeCleanupOperations() && success;
        }
        
        // EN: Phase 4: Finalize shutdown
        // FR: Phase 4 : Finaliser l'arrêt
        transitionToPhase(KillSwitchPhase::FINALIZING);
        finalizeShutdown();
        
        transitionToPhase(KillSwitchPhase::COMPLETED);
        
        auto shutdown_end = std::chrono::system_clock::now();
        auto shutdown_duration = std::chrono::duration_cast<std::chrono::milliseconds>(shutdown_end - shutdown_start);
        
        if (success) {
            stats_.successful_shutdowns++;
            Logger::getInstance().info("KillSwitch", "Shutdown completed successfully in " + std::to_string(shutdown_duration.count()) + "ms / Arrêt terminé avec succès en " + std::to_string(shutdown_duration.count()) + "ms");
        } else {
            stats_.timeout_shutdowns++;
            Logger::getInstance().warn("KillSwitch", "Shutdown completed with timeouts in " + std::to_string(shutdown_duration.count()) + "ms / Arrêt terminé avec des timeouts en " + std::to_string(shutdown_duration.count()) + "ms");
        }
        
        updateStats(current_trigger_, shutdown_duration);
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("KillSwitch", "Error during shutdown: " + std::string(e.what()) + " / Erreur pendant l'arrêt: " + std::string(e.what()));
        stats_.timeout_shutdowns++;
    }
    
    shutdown_completed_ = true;
    cv_.notify_all();
}

// EN: Phase 1: Stop all running tasks gracefully
// FR: Phase 1 : Arrêter toutes les tâches en cours proprement
bool KillSwitch::stopRunningTasks() {
    auto phase_start = std::chrono::system_clock::now();
    bool all_stopped = true;
    
    Logger::getInstance().info("KillSwitch", "Stopping running tasks gracefully... / Arrêt propre des tâches en cours...");
    
    sendNotifications(current_trigger_, KillSwitchPhase::STOPPING_TASKS, "Stopping running tasks");
    
    // EN: Call all registered task termination callbacks
    // FR: Appeler tous les callbacks de terminaison de tâche enregistrés
    for (const auto& [task_type, callback] : task_callbacks_) {
        try {
            if (isTimeoutExceeded(phase_start, config_.task_stop_timeout)) {
                Logger::getInstance().warn("KillSwitch", "Task stop timeout exceeded, skipping remaining tasks / Timeout d'arrêt de tâches dépassé, tâches restantes ignorées");
                
                all_stopped = false;
                break;
            }
            
            auto remaining_timeout = config_.task_stop_timeout - 
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - phase_start);
            
            bool task_stopped = callback(task_type, remaining_timeout);
            if (!task_stopped) {
                Logger::getInstance().warn("KillSwitch", "Failed to stop task type: " + task_type + " / Échec d'arrêt du type de tâche: " + task_type);
                
                all_stopped = false;
            }
            
        } catch (const std::exception& e) {
            Logger::getInstance().error("KillSwitch", "Error stopping task " + task_type + ": " + e.what() + " / Erreur d'arrêt de tâche " + task_type + ": " + e.what());
            
            all_stopped = false;
        }
    }
    
    auto phase_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - phase_start);
    
    Logger::getInstance().info("KillSwitch", "Task stopping phase completed in " + std::to_string(phase_duration.count()) + "ms / Phase d'arrêt de tâches terminée en " + std::to_string(phase_duration.count()) + "ms");
    
    
    return all_stopped;
}

// EN: Phase 2: Preserve current state to persistent storage
// FR: Phase 2 : Préserver l'état actuel vers stockage persistant
bool KillSwitch::preserveCurrentState() {
    auto phase_start = std::chrono::system_clock::now();
    bool all_preserved = true;
    
    Logger::getInstance().info("KillSwitch", "Preserving current state to persistent storage... / Préservation de l'état actuel vers stockage persistant...");
    
    
    sendNotifications(current_trigger_, KillSwitchPhase::SAVING_STATE, "Preserving current state");
    
    std::vector<StateSnapshot> snapshots;
    
    // EN: Collect state snapshots from all registered components
    // FR: Collecter les instantanés d'état de tous les composants enregistrés
    for (const auto& [component_id, callback] : state_callbacks_) {
        try {
            if (isTimeoutExceeded(phase_start, config_.state_save_timeout)) {
                Logger::getInstance().warn("KillSwitch", "State save timeout exceeded, skipping remaining components / Timeout de sauvegarde d'état dépassé, composants restants ignorés");
                
                all_preserved = false;
                break;
            }
            
            auto snapshot = callback(component_id);
            if (snapshot.has_value()) {
                snapshots.push_back(snapshot.value());
                stats_.total_states_saved++;
                stats_.total_state_size_bytes += snapshot->data_size;
            } else {
                Logger::getInstance().warn("KillSwitch", "No state to preserve for component: " + component_id + " / Aucun état à préserver pour le composant: " + component_id);
                
            }
            
        } catch (const std::exception& e) {
            Logger::getInstance().error("KillSwitch", "Error preserving state for " + component_id + ": " + e.what() + " / Erreur de préservation d'état pour " + component_id + ": " + e.what());
            
            stats_.state_save_failures++;
            all_preserved = false;
        }
    }
    
    // EN: Save all collected snapshots to file
    // FR: Sauvegarder tous les instantanés collectés vers fichier
    if (!snapshots.empty()) {
        std::string state_filename = generateStateFilename("emergency_");
        if (saveStateToFile(snapshots, state_filename)) {
            preserved_state_files_.push_back(state_filename);
            Logger::getInstance().info("KillSwitch", "State snapshots saved to: " + state_filename + " / Instantanés d'état sauvegardés vers: " + state_filename);
            
        } else {
            Logger::getInstance().error("KillSwitch", "Failed to save state snapshots to file / Échec de sauvegarde des instantanés d'état vers fichier");
            
            all_preserved = false;
        }
    }
    
    auto phase_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - phase_start);
    stats_.avg_state_save_time = phase_duration;
    
    Logger::getInstance().info("KillSwitch", "State preservation phase completed in " + std::to_string(phase_duration.count()) + "ms / Phase de préservation d'état terminée en " + std::to_string(phase_duration.count()) + "ms");
    
    
    return all_preserved;
}

// EN: Phase 3: Execute cleanup operations
// FR: Phase 3 : Exécuter les opérations de nettoyage
bool KillSwitch::executeCleanupOperations() {
    auto phase_start = std::chrono::system_clock::now();
    bool all_cleaned = true;
    
    Logger::getInstance().info("KillSwitch", "Executing cleanup operations... / Exécution des opérations de nettoyage...");
    
    
    sendNotifications(current_trigger_, KillSwitchPhase::CLEANUP, "Executing cleanup operations");
    
    // EN: Execute all registered cleanup callbacks
    // FR: Exécuter tous les callbacks de nettoyage enregistrés
    for (const auto& [operation_name, callback] : cleanup_callbacks_) {
        try {
            if (isTimeoutExceeded(phase_start, config_.cleanup_timeout)) {
                Logger::getInstance().warn("KillSwitch", "Cleanup timeout exceeded, skipping remaining operations / Timeout de nettoyage dépassé, opérations restantes ignorées");
                
                all_cleaned = false;
                break;
            }
            
            callback(operation_name);
            Logger::getInstance().debug("KillSwitch", "Completed cleanup operation: " + operation_name + " / Opération de nettoyage terminée: " + operation_name);
            
            
        } catch (const std::exception& e) {
            Logger::getInstance().error("KillSwitch", "Error in cleanup operation " + operation_name + ": " + e.what() + " / Erreur dans l'opération de nettoyage " + operation_name + ": " + e.what());
            
            all_cleaned = false;
        }
    }
    
    auto phase_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - phase_start);
    
    Logger::getInstance().info("KillSwitch", "Cleanup phase completed in " + std::to_string(phase_duration.count()) + "ms / Phase de nettoyage terminée en " + std::to_string(phase_duration.count()) + "ms");
    
    
    return all_cleaned;
}

// EN: Phase 4: Final resource cleanup and shutdown
// FR: Phase 4 : Nettoyage final des ressources et arrêt
bool KillSwitch::finalizeShutdown() {
    Logger::getInstance().info("KillSwitch", "Finalizing shutdown... / Finalisation de l'arrêt...");
    
    
    sendNotifications(current_trigger_, KillSwitchPhase::FINALIZING, "Finalizing shutdown");
    
    try {
        // EN: Integration with SignalHandler for final cleanup
        // FR: Intégration avec SignalHandler pour nettoyage final
        auto& signal_handler = SignalHandler::getInstance();
        signal_handler.triggerShutdown(SIGTERM);
        
        Logger::getInstance().info("KillSwitch", "Finalization completed / Finalisation terminée");
        
        
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("KillSwitch", "Error during finalization: " + std::string(e.what()) + " / Erreur pendant la finalisation: " + std::string(e.what()));
        
        return false;
    }
}

// EN: Send notifications to registered callbacks
// FR: Envoie des notifications aux callbacks enregistrés
void KillSwitch::sendNotifications(KillSwitchTrigger trigger, KillSwitchPhase phase, const std::string& details) {
    for (const auto& [notification_id, callback] : notification_callbacks_) {
        try {
            callback(trigger, phase, details);
        } catch (const std::exception& e) {
            Logger::getInstance().warn("KillSwitch", "Error in notification callback " + notification_id + ": " + e.what() + " / Erreur dans le callback de notification " + notification_id + ": " + e.what());
            
        }
    }
}

// EN: Update execution statistics
// FR: Met à jour les statistiques d'exécution
void KillSwitch::updateStats(KillSwitchTrigger /*trigger*/, std::chrono::milliseconds execution_time) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // EN: Update timing statistics
    // FR: Mettre à jour les statistiques de timing
    if (stats_.min_shutdown_time > execution_time) {
        stats_.min_shutdown_time = execution_time;
    }
    if (stats_.max_shutdown_time < execution_time) {
        stats_.max_shutdown_time = execution_time;
    }
    
    // EN: Calculate average (simple moving average)
    // FR: Calculer la moyenne (moyenne mobile simple)
    auto total_shutdowns = stats_.successful_shutdowns + stats_.timeout_shutdowns + stats_.forced_shutdowns;
    if (total_shutdowns > 0) {
        stats_.avg_shutdown_time = std::chrono::milliseconds(
            (stats_.avg_shutdown_time.count() * (total_shutdowns - 1) + execution_time.count()) / total_shutdowns
        );
    }
    
    // EN: Add phase to execution history
    // FR: Ajouter la phase à l'historique d'exécution
    stats_.phase_execution_history.push_back(KillSwitchPhase::COMPLETED);
    if (stats_.phase_execution_history.size() > 50) { // Keep last 50 executions
        stats_.phase_execution_history.erase(stats_.phase_execution_history.begin());
    }
}

// EN: Create state directory if it doesn't exist
// FR: Crée le répertoire d'état s'il n'existe pas
bool KillSwitch::ensureStateDirectoryExists() {
    try {
        std::filesystem::create_directories(config_.state_directory);
        return std::filesystem::exists(config_.state_directory);
    } catch (const std::exception& e) {
        Logger::getInstance().error("KillSwitch", "Failed to create state directory: " + std::string(e.what()) + " / Échec de création du répertoire d'état: " + std::string(e.what()));
        
        return false;
    }
}

// EN: Generate unique state filename
// FR: Génère un nom de fichier d'état unique
std::string KillSwitch::generateStateFilename(const std::string& prefix) const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << config_.state_directory << "/" << config_.state_file_prefix << prefix;
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    
    // EN: Add milliseconds for uniqueness
    // FR: Ajouter les millisecondes pour l'unicité
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    
    ss << ".json";
    return ss.str();
}

// EN: Save state snapshot to file
// FR: Sauvegarde l'instantané d'état vers fichier
bool KillSwitch::saveStateToFile(const std::vector<StateSnapshot>& snapshots, const std::string& filename) {
    try {
        nlohmann::json state_json;
        state_json["version"] = "1.0";
        state_json["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        state_json["trigger"] = KillSwitchUtils::triggerToString(current_trigger_);
        state_json["trigger_details"] = trigger_details_;
        
        nlohmann::json snapshots_array = nlohmann::json::array();
        for (const auto& snapshot : snapshots) {
            nlohmann::json snapshot_json;
            snapshot_json["component_id"] = snapshot.component_id;
            snapshot_json["operation_id"] = snapshot.operation_id;
            snapshot_json["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                snapshot.timestamp.time_since_epoch()).count();
            snapshot_json["state_type"] = snapshot.state_type;
            snapshot_json["state_data"] = config_.compress_state_data ? 
                compressData(snapshot.state_data) : snapshot.state_data;
            snapshot_json["metadata"] = snapshot.metadata;
            snapshot_json["data_size"] = snapshot.data_size;
            snapshot_json["checksum"] = snapshot.checksum;
            snapshot_json["priority"] = snapshot.priority;
            
            if (snapshot.expiry_time.has_value()) {
                snapshot_json["expiry_time"] = snapshot.expiry_time->count();
            }
            
            snapshots_array.push_back(snapshot_json);
        }
        
        state_json["snapshots"] = snapshots_array;
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            Logger::getInstance().error("KillSwitch", "Failed to open state file for writing: " + filename + " / Échec d'ouverture du fichier d'état en écriture: " + filename);
            
            return false;
        }
        
        file << state_json.dump(2);
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("KillSwitch", "Error saving state to file: " + std::string(e.what()) + " / Erreur de sauvegarde d'état vers fichier: " + std::string(e.what()));
        
        return false;
    }
}

// EN: Load preserved state from previous shutdown
// FR: Charge l'état préservé depuis l'arrêt précédent
std::vector<StateSnapshot> KillSwitch::loadPreservedState() {
    std::vector<StateSnapshot> all_snapshots;
    
    try {
        if (!std::filesystem::exists(config_.state_directory)) {
            Logger::getInstance().info("KillSwitch", "State directory does not exist, no state to load / Le répertoire d'état n'existe pas, aucun état à charger");
            
            return all_snapshots;
        }
        
        // EN: Find all state files
        // FR: Trouver tous les fichiers d'état
        for (const auto& entry : std::filesystem::directory_iterator(config_.state_directory)) {
            if (entry.path().extension() == ".json" && 
                entry.path().filename().string().find(config_.state_file_prefix) == 0) {
                
                auto file_snapshots = loadStateFromFile(entry.path().string());
                all_snapshots.insert(all_snapshots.end(), file_snapshots.begin(), file_snapshots.end());
            }
        }
        
        // EN: Sort by priority (highest priority first)
        // FR: Trier par priorité (priorité la plus haute en premier)
        std::sort(all_snapshots.begin(), all_snapshots.end(), 
            [](const StateSnapshot& a, const StateSnapshot& b) {
                return a.priority < b.priority;
            });
        
        Logger::getInstance().info("KillSwitch", "Loaded " + std::to_string(all_snapshots.size()) + " state snapshots from preserved state files / Chargé " + std::to_string(all_snapshots.size()) + " instantanés d'état depuis les fichiers d'état préservés");
        
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("KillSwitch", "Error loading preserved state: " + std::string(e.what()) + " / Erreur de chargement d'état préservé: " + std::string(e.what()));
        
    }
    
    return all_snapshots;
}

// EN: Clean up old state files
// FR: Nettoie les anciens fichiers d'état
void KillSwitch::cleanupOldStateFiles() {
    try {
        if (!std::filesystem::exists(config_.state_directory)) {
            return;
        }
        
        std::vector<std::filesystem::directory_entry> state_files;
        
        // EN: Collect all state files
        // FR: Collecter tous les fichiers d'état
        for (const auto& entry : std::filesystem::directory_iterator(config_.state_directory)) {
            if (entry.path().extension() == ".json" && 
                entry.path().filename().string().find(config_.state_file_prefix) == 0) {
                state_files.push_back(entry);
            }
        }
        
        // EN: Sort by modification time (newest first)
        // FR: Trier par heure de modification (plus récent en premier)
        std::sort(state_files.begin(), state_files.end(),
            [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
                return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
            });
        
        // EN: Remove excess files
        // FR: Supprimer les fichiers en excès
        size_t files_to_remove = state_files.size() > config_.max_state_files ? 
            state_files.size() - config_.max_state_files : 0;
        
        for (size_t i = config_.max_state_files; i < state_files.size(); ++i) {
            try {
                std::filesystem::remove(state_files[i].path());
                Logger::getInstance().debug("KillSwitch", "Removed old state file: " + state_files[i].path().string() + " / Supprimé ancien fichier d'état: " + state_files[i].path().string());
                
            } catch (const std::exception& e) {
                Logger::getInstance().warn("KillSwitch", "Failed to remove old state file: " + std::string(e.what()) + " / Échec de suppression de l'ancien fichier d'état: " + std::string(e.what()));
                
            }
        }
        
        if (files_to_remove > 0) {
            Logger::getInstance().info("KillSwitch", "Cleaned up " + std::to_string(files_to_remove) + " old state files / Nettoyé " + std::to_string(files_to_remove) + " anciens fichiers d'état");
            
        }
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("KillSwitch", "Error during state file cleanup: " + std::string(e.what()) + " / Erreur pendant le nettoyage des fichiers d'état: " + std::string(e.what()));
        
    }
}

// EN: Reset the Kill Switch (mainly for testing)
// FR: Remet à zéro le Kill Switch (principalement pour les tests)
void KillSwitch::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // EN: Wait for any running shutdown to complete
    // FR: Attendre qu'un arrêt en cours se termine
    if (shutdown_thread_ && shutdown_thread_->joinable()) {
        shutdown_thread_->join();
    }
    
    // EN: Reset all state
    // FR: Remettre à zéro tout l'état
    triggered_ = false;
    shutting_down_ = false;
    shutdown_completed_ = false;
    current_phase_ = KillSwitchPhase::INACTIVE;
    
    // EN: Clear callbacks
    // FR: Effacer les callbacks
    state_callbacks_.clear();
    task_callbacks_.clear();
    cleanup_callbacks_.clear();
    notification_callbacks_.clear();
    
    // EN: Reset statistics
    // FR: Remettre à zéro les statistiques
    stats_ = KillSwitchStats{};
    stats_.created_at = std::chrono::system_clock::now();
    stats_.min_shutdown_time = std::chrono::milliseconds(999999);
    
    // EN: Clear preserved state files list
    // FR: Effacer la liste des fichiers d'état préservés
    preserved_state_files_.clear();
    
    Logger::getInstance().info("KillSwitch", "Reset completed / Remise à zéro terminée");
    
}

// EN: Enable/disable Kill Switch functionality (for testing)
// FR: Active/désactive la fonctionnalité Kill Switch (pour les tests)
void KillSwitch::setEnabled(bool enabled) {
    enabled_ = enabled;
    Logger::getInstance().info("KillSwitch", "Kill Switch " + std::string(enabled ? "enabled" : "disabled") + " / Kill Switch " + std::string(enabled ? "activé" : "désactivé"));
    
}

// EN: Transition to next phase
// FR: Transition vers la phase suivante
void KillSwitch::transitionToPhase(KillSwitchPhase new_phase) {
    current_phase_ = new_phase;
    stats_.phase_execution_history.push_back(new_phase);
    
    Logger::getInstance().debug("KillSwitch", "Transitioned to phase: " + KillSwitchUtils::phaseToString(new_phase) + " / Transitionné vers la phase: " + KillSwitchUtils::phaseToString(new_phase));
    
}

// EN: Check if timeout has been exceeded
// FR: Vérifie si le timeout a été dépassé
bool KillSwitch::isTimeoutExceeded(std::chrono::system_clock::time_point start_time, std::chrono::milliseconds timeout) const {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now() - start_time);
    return elapsed >= timeout;
}

// EN: Calculate checksum for data integrity
// FR: Calcule la somme de contrôle pour l'intégrité des données
uint32_t KillSwitch::calculateChecksum(const std::string& data) const {
    return crc32_builtin(data);
}

// EN: Compress data if compression is enabled
// FR: Compresse les données si la compression est activée
std::string KillSwitch::compressData(const std::string& data) const {
    if (!config_.compress_state_data) {
        return data;
    }
    
    // EN: Simple zlib compression implementation
    // FR: Implémentation de compression zlib simple
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib compression");
    }
    
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    zs.avail_in = data.size();
    
    int ret;
    char outbuffer[32768];
    std::string compressed;
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        
        ret = deflate(&zs, Z_FINISH);
        
        if (compressed.size() < zs.total_out) {
            compressed.append(outbuffer, zs.total_out - compressed.size());
        }
    } while (ret == Z_OK);
    
    deflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {
        throw std::runtime_error("Failed to compress data");
    }
    
    return compressed;
}

// EN: Load state snapshots from file
// FR: Charge les instantanés d'état depuis fichier
std::vector<StateSnapshot> KillSwitch::loadStateFromFile(const std::string& filename) {
    std::vector<StateSnapshot> snapshots;
    
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            Logger::getInstance().warn("KillSwitch", "Failed to open state file: " + filename + " / Échec d'ouverture du fichier d'état: " + filename);
            
            return snapshots;
        }
        
        nlohmann::json state_json;
        file >> state_json;
        file.close();
        
        if (state_json.contains("snapshots") && state_json["snapshots"].is_array()) {
            for (const auto& snapshot_json : state_json["snapshots"]) {
                StateSnapshot snapshot;
                snapshot.component_id = snapshot_json["component_id"];
                snapshot.operation_id = snapshot_json["operation_id"];
                snapshot.timestamp = std::chrono::system_clock::from_time_t(snapshot_json["timestamp"]);
                snapshot.state_type = snapshot_json["state_type"];
                snapshot.state_data = snapshot_json["state_data"];
                snapshot.metadata = snapshot_json["metadata"];
                snapshot.data_size = snapshot_json["data_size"];
                snapshot.checksum = snapshot_json["checksum"];
                snapshot.priority = snapshot_json["priority"];
                
                if (snapshot_json.contains("expiry_time")) {
                    snapshot.expiry_time = std::chrono::seconds(snapshot_json["expiry_time"]);
                }
                
                snapshots.push_back(snapshot);
            }
        }
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("KillSwitch", "Error loading state from file " + filename + ": " + e.what() + " / Erreur de chargement d'état depuis fichier " + filename + ": " + e.what());
        
    }
    
    return snapshots;
}

// EN: Utility functions implementation
// FR: Implémentation des fonctions utilitaires
namespace KillSwitchUtils {

std::string triggerToString(KillSwitchTrigger trigger) {
    switch (trigger) {
        case KillSwitchTrigger::USER_REQUEST: return "USER_REQUEST";
        case KillSwitchTrigger::SYSTEM_SIGNAL: return "SYSTEM_SIGNAL";
        case KillSwitchTrigger::TIMEOUT: return "TIMEOUT";
        case KillSwitchTrigger::RESOURCE_EXHAUSTION: return "RESOURCE_EXHAUSTION";
        case KillSwitchTrigger::CRITICAL_ERROR: return "CRITICAL_ERROR";
        case KillSwitchTrigger::DEPENDENCY_FAILURE: return "DEPENDENCY_FAILURE";
        case KillSwitchTrigger::SECURITY_THREAT: return "SECURITY_THREAT";
        case KillSwitchTrigger::EXTERNAL_COMMAND: return "EXTERNAL_COMMAND";
        default: return "UNKNOWN";
    }
}

std::string phaseToString(KillSwitchPhase phase) {
    switch (phase) {
        case KillSwitchPhase::INACTIVE: return "INACTIVE";
        case KillSwitchPhase::TRIGGERED: return "TRIGGERED";
        case KillSwitchPhase::STOPPING_TASKS: return "STOPPING_TASKS";
        case KillSwitchPhase::SAVING_STATE: return "SAVING_STATE";
        case KillSwitchPhase::CLEANUP: return "CLEANUP";
        case KillSwitchPhase::FINALIZING: return "FINALIZING";
        case KillSwitchPhase::COMPLETED: return "COMPLETED";
        default: return "UNKNOWN";
    }
}

std::vector<std::string> validateConfig(const KillSwitchConfig& config) {
    std::vector<std::string> errors;
    
    // EN: Validate timeouts
    // FR: Valider les timeouts
    if (config.trigger_timeout.count() <= 0) {
        errors.push_back("Trigger timeout must be positive");
    }
    if (config.task_stop_timeout.count() <= 0) {
        errors.push_back("Task stop timeout must be positive");
    }
    if (config.state_save_timeout.count() <= 0) {
        errors.push_back("State save timeout must be positive");
    }
    if (config.cleanup_timeout.count() <= 0) {
        errors.push_back("Cleanup timeout must be positive");
    }
    if (config.total_shutdown_timeout.count() <= 0) {
        errors.push_back("Total shutdown timeout must be positive");
    }
    
    // EN: Validate state configuration
    // FR: Valider la configuration d'état
    if (config.state_directory.empty()) {
        errors.push_back("State directory cannot be empty");
    }
    if (config.state_file_prefix.empty()) {
        errors.push_back("State file prefix cannot be empty");
    }
    if (config.max_state_files == 0) {
        errors.push_back("Max state files must be greater than 0");
    }
    if (config.max_state_size_mb == 0) {
        errors.push_back("Max state size must be greater than 0");
    }
    
    return errors;
}

KillSwitchConfig createDefaultConfig() {
    KillSwitchConfig config;
    // EN: Default values are already set in the struct definition
    // FR: Les valeurs par défaut sont déjà définies dans la définition de structure
    return config;
}

std::chrono::milliseconds estimateShutdownTime(const KillSwitchConfig& config, size_t active_tasks, size_t state_size_mb) {
    auto estimated_time = config.trigger_timeout;
    
    // EN: Add time for task stopping (proportional to number of tasks)
    // FR: Ajouter du temps pour l'arrêt de tâches (proportionnel au nombre de tâches)
    estimated_time += std::chrono::milliseconds(active_tasks * 100);
    
    // EN: Add time for state saving (proportional to state size)
    // FR: Ajouter du temps pour la sauvegarde d'état (proportionnel à la taille d'état)
    estimated_time += std::chrono::milliseconds(state_size_mb * 50);
    
    // EN: Add cleanup time
    // FR: Ajouter le temps de nettoyage
    estimated_time += config.cleanup_timeout;
    
    // EN: Add some buffer time (20%)
    // FR: Ajouter du temps de buffer (20%)
    estimated_time += estimated_time / 5;
    
    return std::min(estimated_time, config.total_shutdown_timeout);
}

} // namespace KillSwitchUtils

} // namespace Orchestrator
} // namespace BBP