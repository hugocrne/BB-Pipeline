// EN: Error Recovery system for BB-Pipeline - Auto-retry with exponential backoff on network failures
// FR: Système de récupération d'erreurs pour BB-Pipeline - Auto-retry avec exponential backoff sur échecs réseau

#pragma once

#include <functional>
#include <chrono>
#include <string>
#include <unordered_set>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <random>
#include <future>
#include <exception>
#include <type_traits>

namespace BBP {

// EN: Error types that can be recovered from
// FR: Types d'erreurs qui peuvent être récupérés
enum class RecoverableErrorType {
    NETWORK_TIMEOUT,        // EN: Network timeout error / FR: Erreur de timeout réseau
    CONNECTION_REFUSED,     // EN: Connection refused / FR: Connexion refusée
    DNS_RESOLUTION,         // EN: DNS resolution failure / FR: Échec de résolution DNS
    SSL_HANDSHAKE,         // EN: SSL handshake failure / FR: Échec du handshake SSL
    HTTP_5XX,              // EN: HTTP 5xx server errors / FR: Erreurs serveur HTTP 5xx
    HTTP_429,              // EN: HTTP 429 rate limit / FR: HTTP 429 limite de débit
    SOCKET_ERROR,          // EN: General socket error / FR: Erreur de socket générale
    TEMPORARY_FAILURE,     // EN: Temporary service failure / FR: Échec temporaire du service
    CUSTOM                 // EN: Custom recoverable error / FR: Erreur récupérable personnalisée
};

// EN: Retry strategy configuration
// FR: Configuration de la stratégie de retry
struct RetryConfig {
    size_t max_attempts{3};                          // EN: Maximum number of retry attempts / FR: Nombre maximum de tentatives
    std::chrono::milliseconds initial_delay{100};   // EN: Initial delay before first retry / FR: Délai initial avant le premier retry
    std::chrono::milliseconds max_delay{30000};     // EN: Maximum delay between retries / FR: Délai maximum entre retries
    double backoff_multiplier{2.0};                 // EN: Backoff multiplier for exponential backoff / FR: Multiplicateur pour backoff exponentiel
    double jitter_factor{0.1};                      // EN: Jitter factor (0-1) to avoid thundering herd / FR: Facteur de jitter pour éviter l'effet de troupeau
    bool enable_jitter{true};                       // EN: Enable random jitter / FR: Active le jitter aléatoire
    std::unordered_set<RecoverableErrorType> recoverable_errors; // EN: Set of recoverable error types / FR: Ensemble des types d'erreurs récupérables
};

// EN: Retry attempt information for monitoring and logging
// FR: Information de tentative de retry pour monitoring et logging
struct RetryAttempt {
    size_t attempt_number{0};                        // EN: Current attempt number (1-based) / FR: Numéro de tentative actuelle (base 1)
    std::chrono::milliseconds delay{0};             // EN: Delay before this attempt / FR: Délai avant cette tentative
    std::chrono::system_clock::time_point timestamp; // EN: Timestamp of this attempt / FR: Timestamp de cette tentative
    std::string error_message;                       // EN: Error message from previous attempt / FR: Message d'erreur de la tentative précédente
    RecoverableErrorType error_type;                 // EN: Type of error that occurred / FR: Type d'erreur qui s'est produite
};

// EN: Retry statistics for monitoring and optimization
// FR: Statistiques de retry pour monitoring et optimisation
struct RetryStatistics {
    std::chrono::system_clock::time_point created_at; // EN: When statistics were created / FR: Quand les statistiques ont été créées
    size_t total_operations{0};                      // EN: Total operations attempted / FR: Total des opérations tentées
    size_t successful_operations{0};                 // EN: Operations that succeeded / FR: Opérations qui ont réussi
    size_t failed_operations{0};                     // EN: Operations that failed permanently / FR: Opérations qui ont échoué définitivement
    size_t total_retries{0};                         // EN: Total number of retries performed / FR: Nombre total de retries effectués
    std::chrono::milliseconds total_retry_time{0};   // EN: Total time spent in retries / FR: Temps total passé en retries
    std::chrono::milliseconds average_retry_time{0}; // EN: Average time per retry / FR: Temps moyen par retry
    std::unordered_map<RecoverableErrorType, size_t> error_counts; // EN: Count by error type / FR: Compte par type d'erreur
    std::vector<RetryAttempt> recent_attempts;       // EN: Recent retry attempts for analysis / FR: Tentatives récentes pour analyse
};

// EN: Retry context for tracking individual operations
// FR: Contexte de retry pour suivre les opérations individuelles
class RetryContext {
public:
    // EN: Constructor with operation name and config
    // FR: Constructeur avec nom d'opération et configuration
    explicit RetryContext(const std::string& operation_name, const RetryConfig& config = {});
    
    // EN: Record an attempt
    // FR: Enregistre une tentative
    void recordAttempt(RecoverableErrorType error_type, const std::string& error_message);
    
    // EN: Get current attempt number
    // FR: Obtient le numéro de tentative actuel
    size_t getCurrentAttempt() const { return current_attempt_; }
    
    // EN: Check if more retries are allowed
    // FR: Vérifie si plus de retries sont autorisés
    bool canRetry() const;
    
    // EN: Get delay for next retry
    // FR: Obtient le délai pour le prochain retry
    std::chrono::milliseconds getNextDelay() const;
    
    // EN: Get operation name
    // FR: Obtient le nom de l'opération
    const std::string& getOperationName() const { return operation_name_; }
    
    // EN: Get all retry attempts
    // FR: Obtient toutes les tentatives de retry
    const std::vector<RetryAttempt>& getAttempts() const { return attempts_; }
    
    // EN: Reset context for new operation
    // FR: Remet à zéro le contexte pour une nouvelle opération
    void reset();

public:
    RetryConfig config_;  // EN: Made public for AutoRetryGuard access / FR: Rendu public pour l'accès AutoRetryGuard

private:
    std::string operation_name_;
    size_t current_attempt_{0};
    std::vector<RetryAttempt> attempts_;
    mutable std::mt19937 jitter_generator_;
    
    // EN: Calculate delay with jitter
    // FR: Calcule le délai avec jitter
    std::chrono::milliseconds calculateDelayWithJitter(std::chrono::milliseconds base_delay) const;
};

// EN: Exception for non-recoverable errors
// FR: Exception pour les erreurs non récupérables
class NonRecoverableError : public std::runtime_error {
public:
    explicit NonRecoverableError(const std::string& message) : std::runtime_error(message) {}
};

// EN: Exception for retry exhaustion
// FR: Exception pour l'épuisement des retries
class RetryExhaustedException : public std::runtime_error {
public:
    explicit RetryExhaustedException(const std::string& operation, size_t attempts) 
        : std::runtime_error("Retry exhausted for operation '" + operation + "' after " + std::to_string(attempts) + " attempts") {}
};

// EN: Error Recovery Manager - Main class for handling retries with exponential backoff
// FR: Gestionnaire de récupération d'erreurs - Classe principale pour gérer les retries avec backoff exponentiel
class ErrorRecoveryManager {
    friend class AutoRetryGuard;  // EN: Allow AutoRetryGuard to access private methods / FR: Permet à AutoRetryGuard d'accéder aux méthodes privées

public:
    // EN: Get singleton instance
    // FR: Obtient l'instance singleton
    static ErrorRecoveryManager& getInstance();
    
    // EN: Configure default retry behavior
    // FR: Configure le comportement de retry par défaut
    void configure(const RetryConfig& config);
    
    // EN: Execute operation with automatic retry
    // FR: Exécute l'opération avec retry automatique
    template<typename Func, typename... Args>
    auto executeWithRetry(const std::string& operation_name, Func&& func, Args&&... args) 
        -> decltype(func(args...));
    
    // EN: Execute operation with custom retry config
    // FR: Exécute l'opération avec configuration de retry personnalisée
    template<typename Func, typename... Args>
    auto executeWithRetry(const std::string& operation_name, const RetryConfig& config, 
                         Func&& func, Args&&... args) -> decltype(func(args...));
    
    // EN: Execute async operation with retry
    // FR: Exécute l'opération asynchrone avec retry
    template<typename Func, typename... Args>
    std::future<decltype(std::declval<Func>()(std::declval<Args>()...))>
    executeAsyncWithRetry(const std::string& operation_name, Func&& func, Args&&... args);
    
    // EN: Execute async operation with custom config
    // FR: Exécute l'opération asynchrone avec configuration personnalisée
    template<typename Func, typename... Args>
    std::future<decltype(std::declval<Func>()(std::declval<Args>()...))>
    executeAsyncWithRetry(const std::string& operation_name, const RetryConfig& config,
                         Func&& func, Args&&... args);
    
    // EN: Check if error type is recoverable
    // FR: Vérifie si le type d'erreur est récupérable
    bool isRecoverable(RecoverableErrorType error_type) const;
    bool isRecoverable(const std::exception& error) const;
    
    // EN: Add custom error classifier
    // FR: Ajoute un classificateur d'erreur personnalisé
    void addErrorClassifier(std::function<RecoverableErrorType(const std::exception&)> classifier);
    
    // EN: Get retry statistics
    // FR: Obtient les statistiques de retry
    RetryStatistics getStatistics() const;
    
    // EN: Reset statistics
    // FR: Remet à zéro les statistiques
    void resetStatistics();
    
    // EN: Enable/disable detailed logging
    // FR: Active/désactive le logging détaillé
    void setDetailedLogging(bool enabled);
    
    // EN: Set circuit breaker threshold (auto-disable retries after X failures)
    // FR: Définit le seuil de circuit breaker (désactive auto retries après X échecs)
    void setCircuitBreakerThreshold(size_t threshold);
    
    // EN: Check if circuit breaker is open
    // FR: Vérifie si le circuit breaker est ouvert
    bool isCircuitBreakerOpen() const;
    
    // EN: Reset circuit breaker
    // FR: Remet à zéro le circuit breaker
    void resetCircuitBreaker();
    
    // EN: Classify error type from exception (public for utils)
    // FR: Classifie le type d'erreur à partir de l'exception (public pour utils)
    RecoverableErrorType classifyError(const std::exception& error) const;

private:
    // EN: Private constructor for singleton
    // FR: Constructeur privé pour singleton
    ErrorRecoveryManager();
    
    // EN: Non-copyable and non-movable
    // FR: Non-copiable et non-déplaçable
    ErrorRecoveryManager(const ErrorRecoveryManager&) = delete;
    ErrorRecoveryManager& operator=(const ErrorRecoveryManager&) = delete;
    ErrorRecoveryManager(ErrorRecoveryManager&&) = delete;
    ErrorRecoveryManager& operator=(ErrorRecoveryManager&&) = delete;
    
    // EN: Internal retry execution
    // FR: Exécution interne du retry
    template<typename Func, typename... Args>
    auto executeWithRetryInternal(const std::string& operation_name, const RetryConfig& config,
                                 Func&& func, Args&&... args) -> decltype(func(args...));
    
    // EN: Classify error type from exception (moved to public)
    // FR: Classifie le type d'erreur à partir de l'exception (déplacé en public)
    
    // EN: Update statistics
    // FR: Met à jour les statistiques
    void updateStatistics(const RetryContext& context, bool success);
    
    // EN: Log retry attempt
    // FR: Log la tentative de retry
    void logRetryAttempt(const RetryContext& context, const RetryAttempt& attempt) const;
    
    // EN: Sleep with interruptible wait
    // FR: Sommeil avec attente interruptible
    void interruptibleSleep(std::chrono::milliseconds duration) const;

private:
    mutable std::mutex mutex_;                       // EN: Thread-safety mutex / FR: Mutex pour thread-safety
    RetryConfig default_config_;                     // EN: Default retry configuration / FR: Configuration de retry par défaut
    RetryStatistics statistics_;                     // EN: Retry statistics / FR: Statistiques de retry
    std::vector<std::function<RecoverableErrorType(const std::exception&)>> error_classifiers_; // EN: Custom error classifiers / FR: Classificateurs d'erreur personnalisés
    std::atomic<bool> detailed_logging_{false};     // EN: Detailed logging flag / FR: Flag de logging détaillé
    std::atomic<size_t> circuit_breaker_threshold_{100}; // EN: Circuit breaker threshold / FR: Seuil du circuit breaker
    std::atomic<size_t> consecutive_failures_{0};   // EN: Consecutive failures counter / FR: Compteur d'échecs consécutifs
    std::atomic<bool> circuit_breaker_open_{false}; // EN: Circuit breaker state / FR: État du circuit breaker
    
    static ErrorRecoveryManager* instance_;         // EN: Singleton instance / FR: Instance singleton
};

// EN: RAII helper for automatic retry context management
// FR: Helper RAII pour la gestion automatique du contexte de retry
class AutoRetryGuard {
public:
    // EN: Constructor with operation name
    // FR: Constructeur avec nom d'opération
    explicit AutoRetryGuard(const std::string& operation_name);
    explicit AutoRetryGuard(const std::string& operation_name, const RetryConfig& config);
    
    // EN: Destructor
    // FR: Destructeur
    ~AutoRetryGuard();
    
    // EN: Execute operation with retry
    // FR: Exécute l'opération avec retry
    template<typename Func, typename... Args>
    auto execute(Func&& func, Args&&... args) -> decltype(func(args...));
    
    // EN: Get retry context
    // FR: Obtient le contexte de retry
    const RetryContext& getContext() const { return context_; }

private:
    RetryContext context_;
    ErrorRecoveryManager& manager_;
};

// EN: Utility functions for common error recovery patterns
// FR: Fonctions utilitaires pour les patterns courants de récupération d'erreur
namespace ErrorRecoveryUtils {
    
    // EN: Create default network retry configuration
    // FR: Crée la configuration de retry réseau par défaut
    RetryConfig createNetworkRetryConfig();
    
    // EN: Create configuration for HTTP operations
    // FR: Crée la configuration pour les opérations HTTP
    RetryConfig createHttpRetryConfig();
    
    // EN: Create configuration for database operations
    // FR: Crée la configuration pour les opérations de base de données
    RetryConfig createDatabaseRetryConfig();
    
    // EN: Classify HTTP error codes
    // FR: Classifie les codes d'erreur HTTP
    RecoverableErrorType classifyHttpError(int status_code);
    
    // EN: Classify network error from errno
    // FR: Classifie l'erreur réseau à partir d'errno
    RecoverableErrorType classifyNetworkError(int errno_value);
    
    // EN: Convert exception to recoverable error type
    // FR: Convertit l'exception en type d'erreur récupérable
    RecoverableErrorType classifyException(const std::exception& error);
    
} // namespace ErrorRecoveryUtils

} // namespace BBP

// EN: Template implementations
// FR: Implémentations des templates

template<typename Func, typename... Args>
auto BBP::ErrorRecoveryManager::executeWithRetry(const std::string& operation_name, Func&& func, Args&&... args) 
    -> decltype(func(args...)) {
    return executeWithRetryInternal(operation_name, default_config_, std::forward<Func>(func), std::forward<Args>(args)...);
}

template<typename Func, typename... Args>
auto BBP::ErrorRecoveryManager::executeWithRetry(const std::string& operation_name, const RetryConfig& config,
                                                 Func&& func, Args&&... args) -> decltype(func(args...)) {
    return executeWithRetryInternal(operation_name, config, std::forward<Func>(func), std::forward<Args>(args)...);
}

template<typename Func, typename... Args>
std::future<decltype(std::declval<Func>()(std::declval<Args>()...))>
BBP::ErrorRecoveryManager::executeAsyncWithRetry(const std::string& operation_name, Func&& func, Args&&... args) {
    return std::async(std::launch::async, [this, operation_name, func = std::forward<Func>(func), args...]() mutable {
        return executeWithRetryInternal(operation_name, default_config_, func, args...);
    });
}

template<typename Func, typename... Args>
std::future<decltype(std::declval<Func>()(std::declval<Args>()...))>
BBP::ErrorRecoveryManager::executeAsyncWithRetry(const std::string& operation_name, const RetryConfig& config,
                                                 Func&& func, Args&&... args) {
    return std::async(std::launch::async, [this, operation_name, config, func = std::forward<Func>(func), args...]() mutable {
        return executeWithRetryInternal(operation_name, config, func, args...);
    });
}

template<typename Func, typename... Args>
auto BBP::ErrorRecoveryManager::executeWithRetryInternal(const std::string& operation_name, const RetryConfig& config,
                                                        Func&& func, Args&&... args) -> decltype(func(args...)) {
    // EN: Check circuit breaker
    // FR: Vérifie le circuit breaker
    if (isCircuitBreakerOpen()) {
        throw NonRecoverableError("Circuit breaker is open for operation: " + operation_name);
    }
    
    RetryContext context(operation_name, config);
    
    while (true) {
        try {
            // EN: Execute the operation
            // FR: Exécute l'opération
            if constexpr (std::is_void_v<decltype(func(args...))>) {
                // EN: Handle void return type
                // FR: Gère le type de retour void
                func(args...);
                
                // EN: Success - update statistics and return
                // FR: Succès - met à jour les statistiques et retourne
                updateStatistics(context, true);
                consecutive_failures_.store(0);
                return;
            } else {
                // EN: Handle non-void return type
                // FR: Gère le type de retour non-void
                auto result = func(args...);
                
                // EN: Success - update statistics and return
                // FR: Succès - met à jour les statistiques et retourne
                updateStatistics(context, true);
                consecutive_failures_.store(0);
                return result;
            }
            
        } catch (const NonRecoverableError&) {
            // EN: Non-recoverable error - don't retry
            // FR: Erreur non récupérable - pas de retry
            updateStatistics(context, false);
            throw;
            
        } catch (const std::exception& e) {
            RecoverableErrorType error_type = classifyError(e);
            
            // EN: Check if error is recoverable
            // FR: Vérifie si l'erreur est récupérable
            if (config.recoverable_errors.find(error_type) == config.recoverable_errors.end()) {
                updateStatistics(context, false);
                throw NonRecoverableError("Non-recoverable error in operation '" + operation_name + "': " + e.what());
            }
            
            // EN: Record attempt
            // FR: Enregistre la tentative
            context.recordAttempt(error_type, e.what());
            
            // EN: Check if we can retry
            // FR: Vérifie si on peut réessayer
            if (!context.canRetry()) {
                updateStatistics(context, false);
                consecutive_failures_.fetch_add(1);
                
                // EN: Check circuit breaker
                // FR: Vérifie le circuit breaker
                if (consecutive_failures_.load() >= circuit_breaker_threshold_.load()) {
                    circuit_breaker_open_.store(true);
                }
                
                throw RetryExhaustedException(operation_name, context.getCurrentAttempt());
            }
            
            // EN: Log retry attempt
            // FR: Log la tentative de retry
            if (detailed_logging_.load()) {
                logRetryAttempt(context, context.getAttempts().back());
            }
            
            // EN: Sleep before retry
            // FR: Dort avant le retry
            auto delay = context.getNextDelay();
            interruptibleSleep(delay);
        }
    }
}

template<typename Func, typename... Args>
auto BBP::AutoRetryGuard::execute(Func&& func, Args&&... args) -> decltype(func(args...)) {
    return manager_.executeWithRetryInternal(context_.getOperationName(), context_.config_, 
                                           std::forward<Func>(func), std::forward<Args>(args)...);
}