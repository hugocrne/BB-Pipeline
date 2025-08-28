// EN: Kill Switch System for BB-Pipeline - Emergency shutdown with comprehensive state preservation
// FR: Système Kill Switch pour BB-Pipeline - Arrêt d'urgence avec préservation complète de l'état

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <string>
#include <optional>
#include <future>
#include <fstream>
#include <queue>
#include <thread>
#include <condition_variable>

#include "infrastructure/config/config_manager.hpp"
#include "infrastructure/logging/logger.hpp"
#include "infrastructure/system/signal_handler.hpp"

namespace BBP {
namespace Orchestrator {

// EN: Kill Switch trigger reasons for categorizing shutdown causes
// FR: Raisons de déclenchement du Kill Switch pour catégoriser les causes d'arrêt
enum class KillSwitchTrigger {
    USER_REQUEST = 0,           // EN: Manual user request / FR: Demande manuelle de l'utilisateur
    SYSTEM_SIGNAL = 1,          // EN: System signal (SIGINT/SIGTERM) / FR: Signal système (SIGINT/SIGTERM)
    TIMEOUT = 2,                // EN: Operation timeout exceeded / FR: Timeout d'opération dépassé
    RESOURCE_EXHAUSTION = 3,    // EN: System resources exhausted / FR: Ressources système épuisées
    CRITICAL_ERROR = 4,         // EN: Critical error detected / FR: Erreur critique détectée
    DEPENDENCY_FAILURE = 5,     // EN: Critical dependency failure / FR: Échec de dépendance critique
    SECURITY_THREAT = 6,        // EN: Security threat detected / FR: Menace de sécurité détectée
    EXTERNAL_COMMAND = 7        // EN: External management command / FR: Commande de gestion externe
};

// EN: Kill Switch execution phase for granular control
// FR: Phase d'exécution du Kill Switch pour contrôle granulaire
enum class KillSwitchPhase {
    INACTIVE = 0,               // EN: Kill Switch not active / FR: Kill Switch non actif
    TRIGGERED = 1,              // EN: Kill Switch triggered, preparing / FR: Kill Switch déclenché, en préparation
    STOPPING_TASKS = 2,         // EN: Stopping running tasks / FR: Arrêt des tâches en cours
    SAVING_STATE = 3,           // EN: Saving state to persistent storage / FR: Sauvegarde de l'état vers stockage persistant
    CLEANUP = 4,                // EN: Executing cleanup operations / FR: Exécution des opérations de nettoyage
    FINALIZING = 5,             // EN: Final cleanup and resource release / FR: Nettoyage final et libération des ressources
    COMPLETED = 6               // EN: Shutdown completed / FR: Arrêt terminé
};

// EN: State snapshot entry for preserving operation state
// FR: Entrée d'instantané d'état pour préserver l'état d'opération
struct StateSnapshot {
    std::string component_id;                           // EN: Component identifier / FR: Identifiant du composant
    std::string operation_id;                          // EN: Operation identifier / FR: Identifiant d'opération
    std::chrono::system_clock::time_point timestamp;   // EN: Snapshot timestamp / FR: Horodatage de l'instantané
    std::string state_type;                            // EN: Type of state data / FR: Type de données d'état
    std::string state_data;                            // EN: Serialized state data (JSON/Binary) / FR: Données d'état sérialisées (JSON/Binary)
    std::unordered_map<std::string, std::string> metadata; // EN: Additional metadata / FR: Métadonnées additionnelles
    size_t data_size;                                  // EN: Size of state data in bytes / FR: Taille des données d'état en octets
    uint32_t checksum;                                 // EN: Data integrity checksum / FR: Somme de contrôle d'intégrité des données
    int priority;                                      // EN: Recovery priority (0=highest) / FR: Priorité de récupération (0=la plus haute)
    std::optional<std::chrono::seconds> expiry_time;   // EN: When this state expires / FR: Quand cet état expire
};

// EN: Kill Switch configuration structure
// FR: Structure de configuration du Kill Switch
struct KillSwitchConfig {
    // EN: Timeouts and limits
    // FR: Timeouts et limites
    std::chrono::milliseconds trigger_timeout{500};            // EN: Max time to trigger kill switch / FR: Temps max pour déclencher kill switch
    std::chrono::milliseconds task_stop_timeout{5000};         // EN: Max time to stop running tasks / FR: Temps max pour arrêter les tâches
    std::chrono::milliseconds state_save_timeout{10000};       // EN: Max time for state saving / FR: Temps max pour sauvegarde d'état
    std::chrono::milliseconds cleanup_timeout{3000};           // EN: Max time for cleanup operations / FR: Temps max pour opérations de nettoyage
    std::chrono::milliseconds total_shutdown_timeout{30000};   // EN: Total shutdown timeout / FR: Timeout total d'arrêt
    
    // EN: State preservation settings
    // FR: Paramètres de préservation d'état
    std::string state_directory{"./.kill_switch_state"};       // EN: Directory for state files / FR: Répertoire pour fichiers d'état
    std::string state_file_prefix{"bb_pipeline_state_"};       // EN: Prefix for state files / FR: Préfixe pour fichiers d'état
    size_t max_state_files{100};                              // EN: Maximum number of state files / FR: Nombre maximum de fichiers d'état
    size_t max_state_size_mb{50};                             // EN: Maximum total state size in MB / FR: Taille maximum totale d'état en MB
    bool compress_state_data{true};                           // EN: Compress state data / FR: Comprimer les données d'état
    bool encrypt_state_data{false};                           // EN: Encrypt sensitive state data / FR: Chiffrer les données d'état sensibles
    
    // EN: Behavior configuration
    // FR: Configuration de comportement
    bool force_immediate_stop{false};                         // EN: Force immediate stop without graceful shutdown / FR: Forcer arrêt immédiat sans arrêt propre
    bool preserve_partial_results{true};                      // EN: Save partial results during shutdown / FR: Sauvegarder résultats partiels pendant arrêt
    bool auto_resume_on_restart{true};                        // EN: Automatically resume on next start / FR: Reprendre automatiquement au prochain démarrage
    bool send_termination_notifications{true};                // EN: Send notifications to external systems / FR: Envoyer notifications aux systèmes externes
    
    // EN: Monitoring and logging
    // FR: Surveillance et journalisation
    bool log_detailed_state{true};                            // EN: Log detailed state information / FR: Logger informations d'état détaillées
    bool collect_performance_metrics{true};                   // EN: Collect shutdown performance metrics / FR: Collecter métriques de performance d'arrêt
    std::chrono::milliseconds state_snapshot_interval{1000};  // EN: Interval for periodic state snapshots / FR: Intervalle pour instantanés d'état périodiques
};

// EN: Kill Switch execution statistics for monitoring
// FR: Statistiques d'exécution du Kill Switch pour surveillance
struct KillSwitchStats {
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_triggered_at;
    size_t total_triggers{0};                                 // EN: Total number of triggers / FR: Nombre total de déclenchements
    size_t successful_shutdowns{0};                           // EN: Successful complete shutdowns / FR: Arrêts complets réussis
    size_t timeout_shutdowns{0};                              // EN: Shutdowns that timed out / FR: Arrêts qui ont expiré
    size_t forced_shutdowns{0};                               // EN: Forced immediate shutdowns / FR: Arrêts immédiats forcés
    
    // EN: Performance metrics
    // FR: Métriques de performance
    std::chrono::milliseconds avg_shutdown_time{0};           // EN: Average shutdown time / FR: Temps d'arrêt moyen
    std::chrono::milliseconds max_shutdown_time{0};           // EN: Maximum shutdown time / FR: Temps d'arrêt maximum
    std::chrono::milliseconds min_shutdown_time{999999};      // EN: Minimum shutdown time / FR: Temps d'arrêt minimum
    
    // EN: State preservation metrics
    // FR: Métriques de préservation d'état
    size_t total_states_saved{0};                            // EN: Total states saved / FR: Total d'états sauvegardés
    size_t total_state_size_bytes{0};                        // EN: Total size of saved states / FR: Taille totale des états sauvegardés
    size_t state_save_failures{0};                           // EN: State save operation failures / FR: Échecs d'opération de sauvegarde d'état
    std::chrono::milliseconds avg_state_save_time{0};        // EN: Average state save time / FR: Temps moyen de sauvegarde d'état
    
    // EN: Trigger statistics
    // FR: Statistiques de déclenchement
    std::unordered_map<KillSwitchTrigger, size_t> trigger_counts; // EN: Count per trigger type / FR: Compteur par type de déclenchement
    std::vector<std::string> recent_trigger_reasons;              // EN: Recent trigger reasons / FR: Raisons de déclenchement récentes
    std::vector<KillSwitchPhase> phase_execution_history;         // EN: Phase execution history / FR: Historique d'exécution des phases
};

// EN: Callback function types for extensibility
// FR: Types de fonction callback pour extensibilité
using StatePreservationCallback = std::function<std::optional<StateSnapshot>(const std::string& component_id)>;
using TaskTerminationCallback = std::function<bool(const std::string& task_id, std::chrono::milliseconds timeout)>;
using CleanupOperationCallback = std::function<void(const std::string& operation_name)>;
using NotificationCallback = std::function<void(KillSwitchTrigger trigger, KillSwitchPhase phase, const std::string& details)>;

// EN: Thread-safe Kill Switch system with comprehensive state preservation
// FR: Système Kill Switch thread-safe avec préservation d'état complète
class KillSwitch {
public:
    // EN: Get the singleton instance
    // FR: Obtient l'instance singleton
    static KillSwitch& getInstance();
    
    // EN: Configure the Kill Switch system
    // FR: Configure le système Kill Switch
    void configure(const KillSwitchConfig& config);
    
    // EN: Initialize the Kill Switch system
    // FR: Initialise le système Kill Switch
    void initialize();
    
    // EN: Register a state preservation callback for a component
    // FR: Enregistre un callback de préservation d'état pour un composant
    void registerStatePreservationCallback(const std::string& component_id, StatePreservationCallback callback);
    
    // EN: Register a task termination callback for graceful task stopping
    // FR: Enregistre un callback de terminaison de tâche pour arrêt propre des tâches
    void registerTaskTerminationCallback(const std::string& task_type, TaskTerminationCallback callback);
    
    // EN: Register a cleanup operation callback
    // FR: Enregistre un callback d'opération de nettoyage
    void registerCleanupCallback(const std::string& operation_name, CleanupOperationCallback callback);
    
    // EN: Register a notification callback for external system integration
    // FR: Enregistre un callback de notification pour intégration système externe
    void registerNotificationCallback(const std::string& notification_id, NotificationCallback callback);
    
    // EN: Trigger emergency shutdown with specified reason
    // FR: Déclenche l'arrêt d'urgence avec raison spécifiée
    void trigger(KillSwitchTrigger trigger_reason, const std::string& details = "");
    
    // EN: Trigger shutdown with custom timeout
    // FR: Déclenche l'arrêt avec timeout personnalisé
    void triggerWithTimeout(KillSwitchTrigger trigger_reason, std::chrono::milliseconds custom_timeout, const std::string& details = "");
    
    // EN: Check if Kill Switch has been triggered
    // FR: Vérifie si le Kill Switch a été déclenché
    bool isTriggered() const;
    
    // EN: Check if shutdown is currently in progress
    // FR: Vérifie si l'arrêt est actuellement en cours
    bool isShuttingDown() const;
    
    // EN: Get current execution phase
    // FR: Obtient la phase d'exécution actuelle
    KillSwitchPhase getCurrentPhase() const;
    
    // EN: Wait for shutdown completion (blocks until finished)
    // FR: Attend la completion de l'arrêt (bloque jusqu'à la fin)
    bool waitForCompletion(std::chrono::milliseconds timeout = std::chrono::milliseconds::max());
    
    // EN: Force immediate shutdown (bypasses graceful shutdown)
    // FR: Force l'arrêt immédiat (contourne l'arrêt propre)
    void forceImmediate(const std::string& reason = "Force immediate shutdown requested");
    
    // EN: Cancel shutdown if still in early phases (not always possible)
    // FR: Annule l'arrêt si encore dans les phases précoces (pas toujours possible)
    bool cancelShutdown(const std::string& reason = "Shutdown cancelled by user request");
    
    // EN: Get current Kill Switch statistics
    // FR: Obtient les statistiques actuelles du Kill Switch
    KillSwitchStats getStats() const;
    
    // EN: Get current configuration
    // FR: Obtient la configuration actuelle
    KillSwitchConfig getConfig() const;
    
    // EN: Load preserved state from previous shutdown
    // FR: Charge l'état préservé depuis l'arrêt précédent
    std::vector<StateSnapshot> loadPreservedState();
    
    // EN: Manually save current state (useful for checkpointing)
    // FR: Sauvegarde manuelle de l'état actuel (utile pour checkpointing)
    bool saveCurrentState(const std::string& checkpoint_name = "");
    
    // EN: Clean up old state files
    // FR: Nettoie les anciens fichiers d'état
    void cleanupOldStateFiles();
    
    // EN: Get list of available state snapshots
    // FR: Obtient la liste des instantanés d'état disponibles
    std::vector<std::string> getAvailableStateSnapshots() const;
    
    // EN: Reset the Kill Switch (mainly for testing)
    // FR: Remet à zéro le Kill Switch (principalement pour les tests)
    void reset();
    
    // EN: Enable/disable Kill Switch functionality (for testing)
    // FR: Active/désactive la fonctionnalité Kill Switch (pour les tests)
    void setEnabled(bool enabled);
    
    // EN: Destructor - ensures proper cleanup
    // FR: Destructeur - assure un nettoyage approprié
    ~KillSwitch();

private:
    // EN: Private constructor for singleton pattern
    // FR: Constructeur privé pour le pattern singleton
    KillSwitch();
    
    // EN: Non-copyable and non-movable
    // FR: Non-copiable et non-déplaçable
    KillSwitch(const KillSwitch&) = delete;
    KillSwitch& operator=(const KillSwitch&) = delete;
    KillSwitch(KillSwitch&&) = delete;
    KillSwitch& operator=(KillSwitch&&) = delete;
    
    // EN: Execute the shutdown sequence
    // FR: Exécute la séquence d'arrêt
    void executeShutdown();
    
    // EN: Phase 1: Stop all running tasks gracefully
    // FR: Phase 1 : Arrêter toutes les tâches en cours proprement
    bool stopRunningTasks();
    
    // EN: Phase 2: Preserve current state to persistent storage
    // FR: Phase 2 : Préserver l'état actuel vers stockage persistant
    bool preserveCurrentState();
    
    // EN: Phase 3: Execute cleanup operations
    // FR: Phase 3 : Exécuter les opérations de nettoyage
    bool executeCleanupOperations();
    
    // EN: Phase 4: Final resource cleanup and shutdown
    // FR: Phase 4 : Nettoyage final des ressources et arrêt
    bool finalizeShutdown();
    
    // EN: Send notifications to registered callbacks
    // FR: Envoie des notifications aux callbacks enregistrés
    void sendNotifications(KillSwitchTrigger trigger, KillSwitchPhase phase, const std::string& details);
    
    // EN: Update execution statistics
    // FR: Met à jour les statistiques d'exécution
    void updateStats(KillSwitchTrigger trigger, std::chrono::milliseconds execution_time);
    
    // EN: Create state directory if it doesn't exist
    // FR: Crée le répertoire d'état s'il n'existe pas
    bool ensureStateDirectoryExists();
    
    // EN: Generate unique state filename
    // FR: Génère un nom de fichier d'état unique
    std::string generateStateFilename(const std::string& prefix = "") const;
    
    // EN: Save state snapshot to file
    // FR: Sauvegarde l'instantané d'état vers fichier
    bool saveStateToFile(const std::vector<StateSnapshot>& snapshots, const std::string& filename);
    
    // EN: Load state snapshots from file
    // FR: Charge les instantanés d'état depuis fichier
    std::vector<StateSnapshot> loadStateFromFile(const std::string& filename);
    
    // EN: Calculate checksum for data integrity
    // FR: Calcule la somme de contrôle pour l'intégrité des données
    uint32_t calculateChecksum(const std::string& data) const;
    
    // EN: Compress data if compression is enabled
    // FR: Compresse les données si la compression est activée
    std::string compressData(const std::string& data) const;
    
    // EN: Decompress data if compression was used
    // FR: Décompresse les données si la compression a été utilisée
    std::string decompressData(const std::string& compressed_data) const;
    
    // EN: Transition to next phase
    // FR: Transition vers la phase suivante
    void transitionToPhase(KillSwitchPhase new_phase);
    
    // EN: Check if timeout has been exceeded
    // FR: Vérifie si le timeout a été dépassé
    bool isTimeoutExceeded(std::chrono::system_clock::time_point start_time, std::chrono::milliseconds timeout) const;

private:
    mutable std::mutex mutex_;                              // EN: Thread-safety mutex / FR: Mutex pour thread-safety
    std::condition_variable cv_;                           // EN: Condition variable for waiting / FR: Variable de condition pour attente
    
    KillSwitchConfig config_;                              // EN: Configuration / FR: Configuration
    KillSwitchStats stats_;                                // EN: Statistics / FR: Statistiques
    
    std::atomic<bool> initialized_{false};                 // EN: Initialization flag / FR: Flag d'initialisation
    std::atomic<bool> enabled_{true};                      // EN: Enable/disable flag / FR: Flag activation/désactivation
    std::atomic<bool> triggered_{false};                   // EN: Trigger flag / FR: Flag de déclenchement
    std::atomic<bool> shutting_down_{false};              // EN: Shutdown in progress flag / FR: Flag d'arrêt en cours
    std::atomic<bool> shutdown_completed_{false};         // EN: Shutdown completed flag / FR: Flag d'arrêt terminé
    std::atomic<KillSwitchPhase> current_phase_{KillSwitchPhase::INACTIVE}; // EN: Current execution phase / FR: Phase d'exécution actuelle
    
    KillSwitchTrigger current_trigger_{KillSwitchTrigger::USER_REQUEST}; // EN: Current trigger reason / FR: Raison de déclenchement actuelle
    std::string trigger_details_;                          // EN: Detailed trigger information / FR: Information détaillée de déclenchement
    
    // EN: Callback registrations
    // FR: Enregistrements de callbacks
    std::unordered_map<std::string, StatePreservationCallback> state_callbacks_;       // EN: State preservation callbacks / FR: Callbacks de préservation d'état
    std::unordered_map<std::string, TaskTerminationCallback> task_callbacks_;          // EN: Task termination callbacks / FR: Callbacks de terminaison de tâche
    std::unordered_map<std::string, CleanupOperationCallback> cleanup_callbacks_;      // EN: Cleanup operation callbacks / FR: Callbacks d'opération de nettoyage
    std::unordered_map<std::string, NotificationCallback> notification_callbacks_;     // EN: Notification callbacks / FR: Callbacks de notification
    
    // EN: Timing information
    // FR: Information de timing
    std::chrono::system_clock::time_point created_at_;     // EN: Creation timestamp / FR: Horodatage de création
    std::chrono::system_clock::time_point triggered_at_;   // EN: Trigger timestamp / FR: Horodatage de déclenchement
    std::chrono::system_clock::time_point shutdown_started_at_; // EN: Shutdown start timestamp / FR: Horodatage de début d'arrêt
    
    // EN: Shutdown execution thread
    // FR: Thread d'exécution d'arrêt
    std::unique_ptr<std::thread> shutdown_thread_;         // EN: Background shutdown thread / FR: Thread d'arrêt en arrière-plan
    
    // EN: State management
    // FR: Gestion d'état
    std::queue<StateSnapshot> pending_state_snapshots_;    // EN: Pending state snapshots to save / FR: Instantanés d'état en attente de sauvegarde
    std::vector<std::string> preserved_state_files_;       // EN: List of preserved state files / FR: Liste des fichiers d'état préservés
    
    static KillSwitch* instance_;                          // EN: Singleton instance pointer / FR: Pointeur vers l'instance singleton
};

// EN: Utility functions for Kill Switch operations
// FR: Fonctions utilitaires pour les opérations Kill Switch
namespace KillSwitchUtils {
    
    // EN: Convert trigger enum to string
    // FR: Convertit l'enum trigger en chaîne
    std::string triggerToString(KillSwitchTrigger trigger);
    
    // EN: Convert phase enum to string
    // FR: Convertit l'enum phase en chaîne
    std::string phaseToString(KillSwitchPhase phase);
    
    // EN: Parse trigger from string
    // FR: Analyse trigger depuis chaîne
    std::optional<KillSwitchTrigger> stringToTrigger(const std::string& str);
    
    // EN: Parse phase from string
    // FR: Analyse phase depuis chaîne
    std::optional<KillSwitchPhase> stringToPhase(const std::string& str);
    
    // EN: Validate Kill Switch configuration
    // FR: Valide la configuration Kill Switch
    std::vector<std::string> validateConfig(const KillSwitchConfig& config);
    
    // EN: Create default configuration
    // FR: Crée une configuration par défaut
    KillSwitchConfig createDefaultConfig();
    
    // EN: Serialize state snapshot to JSON
    // FR: Sérialise l'instantané d'état vers JSON
    std::string serializeStateSnapshot(const StateSnapshot& snapshot);
    
    // EN: Deserialize state snapshot from JSON
    // FR: Désérialise l'instantané d'état depuis JSON
    std::optional<StateSnapshot> deserializeStateSnapshot(const std::string& json_data);
    
    // EN: Calculate estimated shutdown time based on current operations
    // FR: Calcule le temps d'arrêt estimé basé sur les opérations actuelles
    std::chrono::milliseconds estimateShutdownTime(const KillSwitchConfig& config, size_t active_tasks, size_t state_size_mb);
    
} // namespace KillSwitchUtils

} // namespace Orchestrator
} // namespace BBP