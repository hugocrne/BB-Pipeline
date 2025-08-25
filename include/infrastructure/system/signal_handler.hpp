// EN: Signal Handler for BB-Pipeline - Graceful shutdown with guaranteed CSV flush
// FR: Gestionnaire de signaux pour BB-Pipeline - Arrêt propre avec flush CSV garanti

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <atomic>
#include <chrono>
#include <unordered_map>

namespace BBP {

// EN: Callback function type for cleanup operations
// FR: Type de fonction callback pour les opérations de nettoyage
using CleanupCallback = std::function<void()>;

// EN: Callback function type for CSV flush operations (receives file path)
// FR: Type de fonction callback pour les opérations de flush CSV (reçoit le chemin du fichier)
using CsvFlushCallback = std::function<void(const std::string& csv_path)>;

// EN: Signal handler configuration structure
// FR: Structure de configuration du gestionnaire de signaux
struct SignalHandlerConfig {
    std::chrono::milliseconds shutdown_timeout{5000};  // EN: Max time for graceful shutdown / FR: Temps max pour arrêt propre
    std::chrono::milliseconds csv_flush_timeout{2000}; // EN: Max time for CSV flush operations / FR: Temps max pour flush CSV
    bool enable_emergency_flush{true};                 // EN: Enable emergency CSV flush on timeout / FR: Active le flush CSV d'urgence sur timeout
    bool log_signal_details{true};                     // EN: Log detailed signal information / FR: Log les détails des signaux
};

// EN: Signal handler statistics for monitoring
// FR: Statistiques du gestionnaire de signaux pour monitoring
struct SignalHandlerStats {
    std::chrono::system_clock::time_point created_at;
    size_t signals_received{0};
    size_t cleanup_callbacks_registered{0};
    size_t csv_flush_callbacks_registered{0};
    size_t successful_shutdowns{0};
    size_t timeout_shutdowns{0};
    std::chrono::milliseconds last_shutdown_duration{0};
    std::chrono::milliseconds total_csv_flush_time{0};
    std::unordered_map<int, size_t> signal_counts; // EN: Count per signal type / FR: Compteur par type de signal
};

// EN: Thread-safe signal handler with guaranteed CSV flush capabilities
// FR: Gestionnaire de signaux thread-safe avec capacités de flush CSV garanti
class SignalHandler {
public:
    // EN: Get the singleton instance
    // FR: Obtient l'instance singleton
    static SignalHandler& getInstance();
    
    // EN: Configure the signal handler
    // FR: Configure le gestionnaire de signaux
    void configure(const SignalHandlerConfig& config);
    
    // EN: Initialize signal handling (registers SIGINT, SIGTERM handlers)
    // FR: Initialise la gestion des signaux (enregistre les handlers SIGINT, SIGTERM)
    void initialize();
    
    // EN: Register a cleanup callback to be called during shutdown
    // FR: Enregistre un callback de nettoyage à appeler lors de l'arrêt
    void registerCleanupCallback(const std::string& name, CleanupCallback callback);
    
    // EN: Register a CSV flush callback for guaranteed data persistence
    // FR: Enregistre un callback de flush CSV pour la persistance garantie des données
    void registerCsvFlushCallback(const std::string& csv_path, CsvFlushCallback callback);
    
    // EN: Unregister a cleanup callback
    // FR: Désenregistre un callback de nettoyage
    void unregisterCleanupCallback(const std::string& name);
    
    // EN: Unregister a CSV flush callback
    // FR: Désenregistre un callback de flush CSV
    void unregisterCsvFlushCallback(const std::string& csv_path);
    
    // EN: Manually trigger graceful shutdown (useful for testing)
    // FR: Déclenche manuellement un arrêt propre (utile pour les tests)
    void triggerShutdown(int signal_number = SIGTERM);
    
    // EN: Check if shutdown has been requested
    // FR: Vérifie si un arrêt a été demandé
    bool isShutdownRequested() const;
    
    // EN: Check if currently in shutdown process
    // FR: Vérifie si actuellement en processus d'arrêt
    bool isShuttingDown() const;
    
    // EN: Wait for shutdown to complete (blocks until finished)
    // FR: Attend que l'arrêt soit terminé (bloque jusqu'à la fin)
    void waitForShutdown();
    
    // EN: Get current signal handler statistics
    // FR: Obtient les statistiques actuelles du gestionnaire de signaux
    SignalHandlerStats getStats() const;
    
    // EN: Reset the signal handler (mainly for testing)
    // FR: Remet à zéro le gestionnaire de signaux (principalement pour les tests)
    void reset();
    
    // EN: Enable/disable signal handling (for testing purposes)
    // FR: Active/désactive la gestion des signaux (pour les tests)
    void setEnabled(bool enabled);
    
    // EN: Destructor - ensures proper cleanup
    // FR: Destructeur - assure un nettoyage approprié
    ~SignalHandler();

private:
    // EN: Private constructor for singleton pattern
    // FR: Constructeur privé pour le pattern singleton
    SignalHandler();
    
    // EN: Non-copyable and non-movable
    // FR: Non-copiable et non-déplaçable
    SignalHandler(const SignalHandler&) = delete;
    SignalHandler& operator=(const SignalHandler&) = delete;
    SignalHandler(SignalHandler&&) = delete;
    SignalHandler& operator=(SignalHandler&&) = delete;
    
    // EN: Static signal handler function (C-style callback)
    // FR: Fonction gestionnaire de signaux statique (callback style C)
    static void signalCallback(int signal_number);
    
    // EN: Instance method to handle received signals
    // FR: Méthode d'instance pour traiter les signaux reçus
    void handleSignal(int signal_number);
    
    // EN: Execute graceful shutdown process
    // FR: Exécute le processus d'arrêt propre
    void executeShutdown();
    
    // EN: Execute all registered cleanup callbacks
    // FR: Exécute tous les callbacks de nettoyage enregistrés
    void executeCleanupCallbacks();
    
    // EN: Execute all registered CSV flush callbacks
    // FR: Exécute tous les callbacks de flush CSV enregistrés
    void executeCsvFlushCallbacks();
    
    // EN: Emergency flush of all CSV files (called on timeout)
    // FR: Flush d'urgence de tous les fichiers CSV (appelé sur timeout)
    void emergencyFlushAllCsv();
    
    // EN: Update statistics
    // FR: Met à jour les statistiques
    void updateStats(int signal_number, std::chrono::milliseconds shutdown_duration);

private:
    mutable std::mutex mutex_;                           // EN: Thread-safety mutex / FR: Mutex pour thread-safety
    SignalHandlerConfig config_;                         // EN: Configuration / FR: Configuration
    SignalHandlerStats stats_;                           // EN: Statistics / FR: Statistiques
    
    std::atomic<bool> initialized_{false};               // EN: Initialization flag / FR: Flag d'initialisation
    std::atomic<bool> enabled_{true};                    // EN: Enable/disable flag / FR: Flag activation/désactivation
    std::atomic<bool> shutdown_requested_{false};        // EN: Shutdown request flag / FR: Flag de demande d'arrêt
    std::atomic<bool> shutting_down_{false};            // EN: Currently shutting down flag / FR: Flag d'arrêt en cours
    std::atomic<bool> shutdown_complete_{false};        // EN: Shutdown completed flag / FR: Flag d'arrêt terminé
    
    std::unordered_map<std::string, CleanupCallback> cleanup_callbacks_;   // EN: Registered cleanup callbacks / FR: Callbacks de nettoyage enregistrés
    std::unordered_map<std::string, CsvFlushCallback> csv_flush_callbacks_; // EN: Registered CSV flush callbacks / FR: Callbacks de flush CSV enregistrés
    
    std::chrono::system_clock::time_point start_time_;  // EN: Creation time / FR: Temps de création
    std::chrono::system_clock::time_point shutdown_start_time_; // EN: Shutdown start time / FR: Temps de début d'arrêt
    
    static SignalHandler* instance_;                     // EN: Singleton instance pointer / FR: Pointeur vers l'instance singleton
};

} // namespace BBP