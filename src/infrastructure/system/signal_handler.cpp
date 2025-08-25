// EN: Implementation of the SignalHandler class. Provides graceful shutdown with guaranteed CSV flush.
// FR: Implémentation de la classe SignalHandler. Fournit un arrêt propre avec flush CSV garanti.

#include "../../../include/infrastructure/system/signal_handler.hpp"
#include "../../../include/infrastructure/logging/logger.hpp"
#include <signal.h>
#include <thread>
#include <future>
#include <chrono>
#include <iostream>
#include <sstream>

#define LOG_DEBUG(module, message) BBP::Logger::getInstance().debug(module, message)
#define LOG_INFO(module, message) BBP::Logger::getInstance().info(module, message)
#define LOG_WARN(module, message) BBP::Logger::getInstance().warn(module, message)
#define LOG_ERROR(module, message) BBP::Logger::getInstance().error(module, message)

namespace BBP {

// EN: Static instance pointer for singleton
// FR: Pointeur d'instance statique pour le singleton
SignalHandler* SignalHandler::instance_ = nullptr;

// EN: Get the singleton signal handler instance.
// FR: Obtient l'instance singleton du gestionnaire de signaux.
SignalHandler& SignalHandler::getInstance() {
    static SignalHandler instance;
    instance_ = &instance;
    return instance;
}

// EN: Private constructor - initializes default configuration and statistics.
// FR: Constructeur privé - initialise la configuration et statistiques par défaut.
SignalHandler::SignalHandler() 
    : start_time_(std::chrono::system_clock::now()) {
    
    // EN: Initialize default configuration
    // FR: Initialise la configuration par défaut
    config_ = SignalHandlerConfig{};
    
    // EN: Initialize statistics
    // FR: Initialise les statistiques
    stats_.created_at = start_time_;
    
    LOG_DEBUG("signal_handler", "SignalHandler instance created");
}

// EN: Destructor - ensures proper cleanup and signal restoration.
// FR: Destructeur - assure un nettoyage approprié et la restauration des signaux.
SignalHandler::~SignalHandler() {
    if (initialized_ && enabled_) {
        // EN: Restore default signal handlers
        // FR: Restaure les gestionnaires de signaux par défaut
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        
        LOG_DEBUG("signal_handler", "SignalHandler destroyed and signal handlers restored");
    }
}

// EN: Configure the signal handler with custom settings.
// FR: Configure le gestionnaire de signaux avec des paramètres personnalisés.
void SignalHandler::configure(const SignalHandlerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (shutting_down_.load()) {
        LOG_WARN("signal_handler", "Cannot configure during shutdown");
        return;
    }
    
    config_ = config;
    
    LOG_INFO("signal_handler", "SignalHandler configured - shutdown_timeout: " + 
             std::to_string(config_.shutdown_timeout.count()) + "ms, csv_flush_timeout: " +
             std::to_string(config_.csv_flush_timeout.count()) + "ms");
}

// EN: Initialize signal handling by registering SIGINT and SIGTERM handlers.
// FR: Initialise la gestion des signaux en enregistrant les handlers SIGINT et SIGTERM.
void SignalHandler::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_.load()) {
        LOG_WARN("signal_handler", "SignalHandler already initialized");
        return;
    }
    
    if (!enabled_.load()) {
        LOG_WARN("signal_handler", "SignalHandler is disabled, skipping initialization");
        return;
    }
    
    // EN: Register signal handlers for graceful shutdown
    // FR: Enregistre les gestionnaires de signaux pour arrêt propre
    if (signal(SIGINT, signalCallback) == SIG_ERR) {
        LOG_ERROR("signal_handler", "Failed to register SIGINT handler");
        throw std::runtime_error("Failed to register SIGINT handler");
    }
    
    if (signal(SIGTERM, signalCallback) == SIG_ERR) {
        LOG_ERROR("signal_handler", "Failed to register SIGTERM handler");
        // EN: Restore SIGINT handler before throwing
        // FR: Restaure le handler SIGINT avant de lancer l'exception
        signal(SIGINT, SIG_DFL);
        throw std::runtime_error("Failed to register SIGTERM handler");
    }
    
    initialized_ = true;
    
    LOG_INFO("signal_handler", "SignalHandler initialized successfully - SIGINT and SIGTERM handlers registered");
}

// EN: Register a cleanup callback to be executed during shutdown.
// FR: Enregistre un callback de nettoyage à exécuter lors de l'arrêt.
void SignalHandler::registerCleanupCallback(const std::string& name, CleanupCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (shutting_down_.load()) {
        LOG_WARN("signal_handler", "Cannot register cleanup callback during shutdown: " + name);
        return;
    }
    
    cleanup_callbacks_[name] = std::move(callback);
    stats_.cleanup_callbacks_registered = cleanup_callbacks_.size();
    
    LOG_DEBUG("signal_handler", "Registered cleanup callback: " + name + 
              " (total: " + std::to_string(cleanup_callbacks_.size()) + ")");
}

// EN: Register a CSV flush callback for guaranteed data persistence.
// FR: Enregistre un callback de flush CSV pour la persistance garantie des données.
void SignalHandler::registerCsvFlushCallback(const std::string& csv_path, CsvFlushCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (shutting_down_.load()) {
        LOG_WARN("signal_handler", "Cannot register CSV flush callback during shutdown: " + csv_path);
        return;
    }
    
    csv_flush_callbacks_[csv_path] = std::move(callback);
    stats_.csv_flush_callbacks_registered = csv_flush_callbacks_.size();
    
    LOG_DEBUG("signal_handler", "Registered CSV flush callback: " + csv_path + 
              " (total: " + std::to_string(csv_flush_callbacks_.size()) + ")");
}

// EN: Unregister a cleanup callback.
// FR: Désenregistre un callback de nettoyage.
void SignalHandler::unregisterCleanupCallback(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cleanup_callbacks_.find(name);
    if (it != cleanup_callbacks_.end()) {
        cleanup_callbacks_.erase(it);
        stats_.cleanup_callbacks_registered = cleanup_callbacks_.size();
        
        LOG_DEBUG("signal_handler", "Unregistered cleanup callback: " + name);
    } else {
        LOG_WARN("signal_handler", "Cleanup callback not found for unregistration: " + name);
    }
}

// EN: Unregister a CSV flush callback.
// FR: Désenregistre un callback de flush CSV.
void SignalHandler::unregisterCsvFlushCallback(const std::string& csv_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = csv_flush_callbacks_.find(csv_path);
    if (it != csv_flush_callbacks_.end()) {
        csv_flush_callbacks_.erase(it);
        stats_.csv_flush_callbacks_registered = csv_flush_callbacks_.size();
        
        LOG_DEBUG("signal_handler", "Unregistered CSV flush callback: " + csv_path);
    } else {
        LOG_WARN("signal_handler", "CSV flush callback not found for unregistration: " + csv_path);
    }
}

// EN: Manually trigger graceful shutdown (useful for testing).
// FR: Déclenche manuellement un arrêt propre (utile pour les tests).
void SignalHandler::triggerShutdown(int signal_number) {
    if (shutting_down_.exchange(true)) {
        LOG_WARN("signal_handler", "Shutdown already in progress");
        return;
    }
    
    LOG_INFO("signal_handler", "Manual shutdown triggered with signal: " + std::to_string(signal_number));
    handleSignal(signal_number);
}

// EN: Check if shutdown has been requested.
// FR: Vérifie si un arrêt a été demandé.
bool SignalHandler::isShutdownRequested() const {
    return shutdown_requested_.load();
}

// EN: Check if currently in shutdown process.
// FR: Vérifie si actuellement en processus d'arrêt.
bool SignalHandler::isShuttingDown() const {
    return shutting_down_.load();
}

// EN: Wait for shutdown to complete (blocks until finished).
// FR: Attend que l'arrêt soit terminé (bloque jusqu'à la fin).
void SignalHandler::waitForShutdown() {
    while (!shutdown_complete_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// EN: Get current signal handler statistics.
// FR: Obtient les statistiques actuelles du gestionnaire de signaux.
SignalHandlerStats SignalHandler::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

// EN: Reset the signal handler (mainly for testing).
// FR: Remet à zéro le gestionnaire de signaux (principalement pour les tests).
void SignalHandler::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // EN: Only reset if not currently shutting down
    // FR: Remet à zéro seulement si pas en cours d'arrêt
    if (shutting_down_.load()) {
        LOG_WARN("signal_handler", "Cannot reset during shutdown");
        return;
    }
    
    // EN: Clear all callbacks
    // FR: Efface tous les callbacks
    cleanup_callbacks_.clear();
    csv_flush_callbacks_.clear();
    
    // EN: Reset flags
    // FR: Remet à zéro les flags
    shutdown_requested_ = false;
    shutdown_complete_ = false;
    
    // EN: Reset statistics
    // FR: Remet à zéro les statistiques
    stats_ = SignalHandlerStats{};
    stats_.created_at = std::chrono::system_clock::now();
    
    LOG_DEBUG("signal_handler", "SignalHandler reset completed");
}

// EN: Enable/disable signal handling (for testing purposes).
// FR: Active/désactive la gestion des signaux (pour les tests).
void SignalHandler::setEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enabled;
    
    LOG_DEBUG("signal_handler", "SignalHandler " + std::string(enabled ? "enabled" : "disabled"));
}

// EN: Static signal handler function - C-style callback required by signal().
// FR: Fonction gestionnaire de signaux statique - callback style C requis par signal().
void SignalHandler::signalCallback(int signal_number) {
    // EN: Forward to instance method if instance exists
    // FR: Transmet à la méthode d'instance si l'instance existe
    if (instance_ && instance_->enabled_.load()) {
        instance_->handleSignal(signal_number);
    }
}

// EN: Instance method to handle received signals.
// FR: Méthode d'instance pour traiter les signaux reçus.
void SignalHandler::handleSignal(int signal_number) {
    // EN: Set shutdown requested flag first
    // FR: Définit d'abord le flag de demande d'arrêt
    shutdown_requested_ = true;
    
    // EN: Check if already shutting down to avoid recursive calls
    // FR: Vérifie si déjà en cours d'arrêt pour éviter les appels récursifs
    if (shutting_down_.exchange(true)) {
        LOG_WARN("signal_handler", "Signal received during shutdown, ignoring: " + std::to_string(signal_number));
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.signals_received++;
        stats_.signal_counts[signal_number]++;
        shutdown_start_time_ = std::chrono::system_clock::now();
    }
    
    if (config_.log_signal_details) {
        std::string signal_name = (signal_number == SIGINT) ? "SIGINT" : 
                                 (signal_number == SIGTERM) ? "SIGTERM" : 
                                 "SIGNAL_" + std::to_string(signal_number);
        LOG_INFO("signal_handler", "Received signal: " + signal_name + " - initiating graceful shutdown");
    }
    
    // EN: Execute shutdown in separate thread to avoid signal handler limitations
    // FR: Exécute l'arrêt dans un thread séparé pour éviter les limitations du gestionnaire de signaux
    std::thread shutdown_thread(&SignalHandler::executeShutdown, this);
    shutdown_thread.detach();
}

// EN: Execute graceful shutdown process.
// FR: Exécute le processus d'arrêt propre.
void SignalHandler::executeShutdown() {
    auto shutdown_start = std::chrono::system_clock::now();
    
    try {
        LOG_INFO("signal_handler", "Starting graceful shutdown process");
        
        // EN: Execute CSV flush callbacks first (most critical)
        // FR: Exécute d'abord les callbacks de flush CSV (le plus critique)
        executeCsvFlushCallbacks();
        
        // EN: Then execute general cleanup callbacks
        // FR: Puis exécute les callbacks de nettoyage général
        executeCleanupCallbacks();
        
        auto shutdown_end = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(shutdown_end - shutdown_start);
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.successful_shutdowns++;
        }
        
        LOG_INFO("signal_handler", "Graceful shutdown completed successfully in " + 
                 std::to_string(duration.count()) + "ms");
        
    } catch (const std::exception& e) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.timeout_shutdowns++;
        }
        
        LOG_ERROR("signal_handler", "Exception during shutdown: " + std::string(e.what()));
        
        // EN: Attempt emergency CSV flush on failure
        // FR: Tente un flush CSV d'urgence en cas d'échec
        if (config_.enable_emergency_flush) {
            LOG_WARN("signal_handler", "Attempting emergency CSV flush");
            emergencyFlushAllCsv();
        }
    }
    
    // EN: Update final statistics
    // FR: Met à jour les statistiques finales
    auto shutdown_end = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(shutdown_end - shutdown_start);
    updateStats(0, duration); // EN: Signal number not relevant here / FR: Numéro de signal pas pertinent ici
    
    shutdown_complete_ = true;
    
    // EN: Flush logger to ensure all messages are written
    // FR: Flush le logger pour s'assurer que tous les messages sont écrits
    Logger::getInstance().flush();
}

// EN: Execute all registered cleanup callbacks.
// FR: Exécute tous les callbacks de nettoyage enregistrés.
void SignalHandler::executeCleanupCallbacks() {
    std::vector<std::pair<std::string, CleanupCallback>> callbacks_copy;
    
    // EN: Copy callbacks under lock to avoid holding mutex during execution
    // FR: Copie les callbacks sous verrou pour éviter de garder le mutex pendant l'exécution
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_copy.reserve(cleanup_callbacks_.size());
        for (const auto& [name, callback] : cleanup_callbacks_) {
            callbacks_copy.emplace_back(name, callback);
        }
    }
    
    LOG_INFO("signal_handler", "Executing " + std::to_string(callbacks_copy.size()) + " cleanup callbacks");
    
    // EN: Execute callbacks with timeout protection
    // FR: Exécute les callbacks avec protection de timeout
    auto deadline = std::chrono::system_clock::now() + config_.shutdown_timeout;
    
    for (const auto& [name, callback] : callbacks_copy) {
        if (std::chrono::system_clock::now() >= deadline) {
            LOG_WARN("signal_handler", "Cleanup timeout reached, skipping remaining callbacks");
            break;
        }
        
        try {
            LOG_DEBUG("signal_handler", "Executing cleanup callback: " + name);
            callback();
            LOG_DEBUG("signal_handler", "Cleanup callback completed: " + name);
        } catch (const std::exception& e) {
            LOG_ERROR("signal_handler", "Cleanup callback failed: " + name + " - " + e.what());
        } catch (...) {
            LOG_ERROR("signal_handler", "Cleanup callback failed with unknown exception: " + name);
        }
    }
}

// EN: Execute all registered CSV flush callbacks.
// FR: Exécute tous les callbacks de flush CSV enregistrés.
void SignalHandler::executeCsvFlushCallbacks() {
    std::vector<std::pair<std::string, CsvFlushCallback>> callbacks_copy;
    
    // EN: Copy callbacks under lock
    // FR: Copie les callbacks sous verrou
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_copy.reserve(csv_flush_callbacks_.size());
        for (const auto& [path, callback] : csv_flush_callbacks_) {
            callbacks_copy.emplace_back(path, callback);
        }
    }
    
    LOG_INFO("signal_handler", "Executing " + std::to_string(callbacks_copy.size()) + " CSV flush callbacks");
    
    auto csv_start = std::chrono::system_clock::now();
    auto deadline = csv_start + config_.csv_flush_timeout;
    
    for (const auto& [csv_path, callback] : callbacks_copy) {
        if (std::chrono::system_clock::now() >= deadline) {
            LOG_WARN("signal_handler", "CSV flush timeout reached, skipping remaining files");
            break;
        }
        
        try {
            LOG_DEBUG("signal_handler", "Flushing CSV file: " + csv_path);
            callback(csv_path);
            LOG_DEBUG("signal_handler", "CSV flush completed: " + csv_path);
        } catch (const std::exception& e) {
            LOG_ERROR("signal_handler", "CSV flush failed: " + csv_path + " - " + e.what());
        } catch (...) {
            LOG_ERROR("signal_handler", "CSV flush failed with unknown exception: " + csv_path);
        }
    }
    
    auto csv_end = std::chrono::system_clock::now();
    auto csv_duration = std::chrono::duration_cast<std::chrono::milliseconds>(csv_end - csv_start);
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.total_csv_flush_time += csv_duration;
    }
    
    LOG_INFO("signal_handler", "CSV flush callbacks completed in " + std::to_string(csv_duration.count()) + "ms");
}

// EN: Emergency flush of all CSV files (called on timeout).
// FR: Flush d'urgence de tous les fichiers CSV (appelé sur timeout).
void SignalHandler::emergencyFlushAllCsv() {
    LOG_WARN("signal_handler", "Executing emergency CSV flush");
    
    std::vector<std::pair<std::string, CsvFlushCallback>> callbacks_copy;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_copy.reserve(csv_flush_callbacks_.size());
        for (const auto& [path, callback] : csv_flush_callbacks_) {
            callbacks_copy.emplace_back(path, callback);
        }
    }
    
    // EN: Emergency flush with minimal timeout
    // FR: Flush d'urgence avec timeout minimal
    auto emergency_deadline = std::chrono::system_clock::now() + std::chrono::seconds(1);
    
    for (const auto& [csv_path, callback] : callbacks_copy) {
        if (std::chrono::system_clock::now() >= emergency_deadline) {
            LOG_ERROR("signal_handler", "Emergency flush timeout - some data may be lost");
            break;
        }
        
        try {
            callback(csv_path);
        } catch (...) {
            // EN: Ignore all exceptions in emergency flush
            // FR: Ignore toutes les exceptions dans le flush d'urgence
            LOG_ERROR("signal_handler", "Emergency flush failed for: " + csv_path);
        }
    }
}

// EN: Update statistics.
// FR: Met à jour les statistiques.
void SignalHandler::updateStats(int /* signal_number */, std::chrono::milliseconds shutdown_duration) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.last_shutdown_duration = shutdown_duration;
}

} // namespace BBP