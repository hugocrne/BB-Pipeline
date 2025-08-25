// EN: Error Recovery implementation for BB-Pipeline - Auto-retry with exponential backoff
// FR: Implémentation Error Recovery pour BB-Pipeline - Auto-retry avec exponential backoff

#include "infrastructure/system/error_recovery.hpp"
#include "infrastructure/logging/logger.hpp"
#include <algorithm>
#include <cmath>
#include <thread>
#include <sstream>
#include <regex>
#include <cerrno>
#include <cstring>

namespace BBP {

// EN: Static instance for singleton
// FR: Instance statique pour singleton
ErrorRecoveryManager* ErrorRecoveryManager::instance_ = nullptr;

// EN: RetryContext implementation
// FR: Implémentation de RetryContext
RetryContext::RetryContext(const std::string& operation_name, const RetryConfig& config)
    : config_(config), operation_name_(operation_name), jitter_generator_(std::random_device{}()) {
    
    // EN: Initialize with default recoverable errors if none specified
    // FR: Initialise avec les erreurs récupérables par défaut si aucune spécifiée
    if (config_.recoverable_errors.empty()) {
        config_.recoverable_errors = {
            RecoverableErrorType::NETWORK_TIMEOUT,
            RecoverableErrorType::CONNECTION_REFUSED,
            RecoverableErrorType::HTTP_5XX,
            RecoverableErrorType::HTTP_429,
            RecoverableErrorType::TEMPORARY_FAILURE
        };
    }
}

void RetryContext::recordAttempt(RecoverableErrorType error_type, const std::string& error_message) {
    RetryAttempt attempt;
    attempt.attempt_number = ++current_attempt_;
    attempt.timestamp = std::chrono::system_clock::now();
    attempt.error_message = error_message;
    attempt.error_type = error_type;
    attempt.delay = getNextDelay();
    
    attempts_.push_back(attempt);
}

bool RetryContext::canRetry() const {
    return current_attempt_ < config_.max_attempts;
}

std::chrono::milliseconds RetryContext::getNextDelay() const {
    if (current_attempt_ == 0) {
        return std::chrono::milliseconds{0};
    }
    
    // EN: Calculate exponential backoff delay
    // FR: Calcule le délai de backoff exponentiel
    double delay_ms = config_.initial_delay.count() * 
                     std::pow(config_.backoff_multiplier, current_attempt_ - 1);
    
    // EN: Cap to maximum delay
    // FR: Limite au délai maximum
    delay_ms = std::min(delay_ms, static_cast<double>(config_.max_delay.count()));
    
    auto base_delay = std::chrono::milliseconds(static_cast<long long>(delay_ms));
    
    // EN: Apply jitter if enabled
    // FR: Applique le jitter si activé
    if (config_.enable_jitter) {
        return calculateDelayWithJitter(base_delay);
    }
    
    return base_delay;
}

std::chrono::milliseconds RetryContext::calculateDelayWithJitter(std::chrono::milliseconds base_delay) const {
    if (config_.jitter_factor <= 0.0) {
        return base_delay;
    }
    
    // EN: Calculate jitter range
    // FR: Calcule la plage de jitter
    double jitter_range = base_delay.count() * config_.jitter_factor;
    std::uniform_real_distribution<double> jitter_dist(-jitter_range, jitter_range);
    
    double jittered_delay = base_delay.count() + jitter_dist(jitter_generator_);
    
    // EN: Ensure delay is not negative
    // FR: S'assure que le délai n'est pas négatif
    jittered_delay = std::max(0.0, jittered_delay);
    
    return std::chrono::milliseconds(static_cast<long long>(jittered_delay));
}

void RetryContext::reset() {
    current_attempt_ = 0;
    attempts_.clear();
}

// EN: ErrorRecoveryManager implementation
// FR: Implémentation d'ErrorRecoveryManager
ErrorRecoveryManager& ErrorRecoveryManager::getInstance() {
    static std::once_flag once_flag;
    std::call_once(once_flag, []() {
        instance_ = new ErrorRecoveryManager();
        
        auto& logger = Logger::getInstance();
        logger.debug("ErrorRecoveryManager instance created", "error_recovery");
    });
    
    return *instance_;
}

ErrorRecoveryManager::ErrorRecoveryManager() {
    // EN: Initialize default configuration
    // FR: Initialise la configuration par défaut
    default_config_.max_attempts = 3;
    default_config_.initial_delay = std::chrono::milliseconds(100);
    default_config_.max_delay = std::chrono::milliseconds(30000);
    default_config_.backoff_multiplier = 2.0;
    default_config_.jitter_factor = 0.1;
    default_config_.enable_jitter = true;
    
    // EN: Set default recoverable errors
    // FR: Définit les erreurs récupérables par défaut
    default_config_.recoverable_errors = {
        RecoverableErrorType::NETWORK_TIMEOUT,
        RecoverableErrorType::CONNECTION_REFUSED,
        RecoverableErrorType::DNS_RESOLUTION,
        RecoverableErrorType::HTTP_5XX,
        RecoverableErrorType::HTTP_429,
        RecoverableErrorType::TEMPORARY_FAILURE
    };
    
    // EN: Initialize statistics
    // FR: Initialise les statistiques
    statistics_.created_at = std::chrono::system_clock::now();
    
    // EN: Add default error classifiers
    // FR: Ajoute les classificateurs d'erreur par défaut
    error_classifiers_.push_back([](const std::exception& e) -> RecoverableErrorType {
        std::string message = e.what();
        std::transform(message.begin(), message.end(), message.begin(), ::tolower);
        
        // EN: Network-related errors
        // FR: Erreurs liées au réseau
        if (message.find("timeout") != std::string::npos ||
            message.find("timed out") != std::string::npos) {
            return RecoverableErrorType::NETWORK_TIMEOUT;
        }
        
        if (message.find("connection refused") != std::string::npos ||
            message.find("connection reset") != std::string::npos) {
            return RecoverableErrorType::CONNECTION_REFUSED;
        }
        
        if (message.find("dns") != std::string::npos ||
            message.find("resolve") != std::string::npos ||
            message.find("host not found") != std::string::npos) {
            return RecoverableErrorType::DNS_RESOLUTION;
        }
        
        if (message.find("ssl") != std::string::npos ||
            message.find("tls") != std::string::npos ||
            message.find("handshake") != std::string::npos) {
            return RecoverableErrorType::SSL_HANDSHAKE;
        }
        
        if (message.find("socket") != std::string::npos) {
            return RecoverableErrorType::SOCKET_ERROR;
        }
        
        // EN: HTTP errors
        // FR: Erreurs HTTP
        if (message.find("5") != std::string::npos && 
            (message.find("50") != std::string::npos || message.find("error") != std::string::npos)) {
            return RecoverableErrorType::HTTP_5XX;
        }
        
        if (message.find("429") != std::string::npos ||
            message.find("rate limit") != std::string::npos ||
            message.find("too many requests") != std::string::npos) {
            return RecoverableErrorType::HTTP_429;
        }
        
        // EN: Temporary failures
        // FR: Échecs temporaires
        if (message.find("temporary") != std::string::npos ||
            message.find("unavailable") != std::string::npos ||
            message.find("service") != std::string::npos) {
            return RecoverableErrorType::TEMPORARY_FAILURE;
        }
        
        return RecoverableErrorType::CUSTOM;
    });
}

void ErrorRecoveryManager::configure(const RetryConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_config_ = config;
    
    auto& logger = Logger::getInstance();
    logger.info("ErrorRecoveryManager configured - Max attempts: " + std::to_string(config.max_attempts) +
                ", Initial delay: " + std::to_string(config.initial_delay.count()) + "ms" +
                ", Max delay: " + std::to_string(config.max_delay.count()) + "ms" +
                ", Backoff multiplier: " + std::to_string(config.backoff_multiplier), "error_recovery");
}

bool ErrorRecoveryManager::isRecoverable(RecoverableErrorType error_type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return default_config_.recoverable_errors.find(error_type) != default_config_.recoverable_errors.end();
}

bool ErrorRecoveryManager::isRecoverable(const std::exception& error) const {
    RecoverableErrorType error_type = classifyError(error);
    return isRecoverable(error_type);
}

void ErrorRecoveryManager::addErrorClassifier(std::function<RecoverableErrorType(const std::exception&)> classifier) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_classifiers_.push_back(classifier);
    
    auto& logger = Logger::getInstance();
    logger.debug("Custom error classifier added", "error_recovery");
}

RetryStatistics ErrorRecoveryManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // EN: Calculate average retry time
    // FR: Calcule le temps moyen de retry
    RetryStatistics stats = statistics_;
    if (stats.total_retries > 0) {
        stats.average_retry_time = std::chrono::milliseconds(
            stats.total_retry_time.count() / stats.total_retries
        );
    }
    
    return stats;
}

void ErrorRecoveryManager::resetStatistics() {
    std::lock_guard<std::mutex> lock(mutex_);
    statistics_ = RetryStatistics{};
    statistics_.created_at = std::chrono::system_clock::now();
    
    auto& logger = Logger::getInstance();
    logger.info("ErrorRecoveryManager statistics reset", "error_recovery");
}

void ErrorRecoveryManager::setDetailedLogging(bool enabled) {
    detailed_logging_.store(enabled);
    
    auto& logger = Logger::getInstance();
    logger.info("ErrorRecoveryManager detailed logging " + std::string(enabled ? "enabled" : "disabled"), "error_recovery");
}

void ErrorRecoveryManager::setCircuitBreakerThreshold(size_t threshold) {
    circuit_breaker_threshold_.store(threshold);
    
    auto& logger = Logger::getInstance();
    logger.info("ErrorRecoveryManager circuit breaker threshold set to " + std::to_string(threshold), "error_recovery");
}

bool ErrorRecoveryManager::isCircuitBreakerOpen() const {
    return circuit_breaker_open_.load();
}

void ErrorRecoveryManager::resetCircuitBreaker() {
    circuit_breaker_open_.store(false);
    consecutive_failures_.store(0);
    
    auto& logger = Logger::getInstance();
    logger.info("ErrorRecoveryManager circuit breaker reset", "error_recovery");
}

RecoverableErrorType ErrorRecoveryManager::classifyError(const std::exception& error) const {
    // EN: Try custom classifiers first
    // FR: Essaie les classificateurs personnalisés en premier
    for (const auto& classifier : error_classifiers_) {
        try {
            RecoverableErrorType type = classifier(error);
            if (type != RecoverableErrorType::CUSTOM) {
                return type;
            }
        } catch (...) {
            // EN: Ignore classifier errors
            // FR: Ignore les erreurs de classificateur
            continue;
        }
    }
    
    return RecoverableErrorType::CUSTOM;
}

void ErrorRecoveryManager::updateStatistics(const RetryContext& context, bool success) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    statistics_.total_operations++;
    
    if (success) {
        statistics_.successful_operations++;
    } else {
        statistics_.failed_operations++;
    }
    
    // EN: Update retry statistics
    // FR: Met à jour les statistiques de retry
    const auto& attempts = context.getAttempts();
    if (!attempts.empty()) {
        statistics_.total_retries += attempts.size();
        
        // EN: Calculate total retry time
        // FR: Calcule le temps total de retry
        auto start_time = attempts.front().timestamp;
        auto end_time = attempts.back().timestamp;
        auto retry_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        statistics_.total_retry_time += retry_duration;
        
        // EN: Update error counts
        // FR: Met à jour les comptes d'erreur
        for (const auto& attempt : attempts) {
            statistics_.error_counts[attempt.error_type]++;
        }
        
        // EN: Keep recent attempts for analysis (limit to last 100)
        // FR: Garde les tentatives récentes pour analyse (limite aux 100 dernières)
        for (const auto& attempt : attempts) {
            statistics_.recent_attempts.push_back(attempt);
        }
        
        if (statistics_.recent_attempts.size() > 100) {
            statistics_.recent_attempts.erase(statistics_.recent_attempts.begin(),
                                            statistics_.recent_attempts.begin() + 
                                            (statistics_.recent_attempts.size() - 100));
        }
    }
}

void ErrorRecoveryManager::logRetryAttempt(const RetryContext& context, const RetryAttempt& attempt) const {
    auto& logger = Logger::getInstance();
    
    std::ostringstream oss;
    oss << "Retry attempt " << attempt.attempt_number 
        << " for operation '" << context.getOperationName() 
        << "' after " << attempt.delay.count() << "ms delay"
        << " - Error: " << attempt.error_message;
    
    logger.warn(oss.str(), "error_recovery");
}

void ErrorRecoveryManager::interruptibleSleep(std::chrono::milliseconds duration) const {
    // EN: Sleep in small increments to allow interruption
    // FR: Dort par petits incréments pour permettre l'interruption
    const auto sleep_increment = std::chrono::milliseconds(100);
    auto remaining = duration;
    
    while (remaining > std::chrono::milliseconds(0)) {
        auto sleep_time = std::min(remaining, sleep_increment);
        std::this_thread::sleep_for(sleep_time);
        remaining -= sleep_time;
        
        // EN: Check if we should stop (could add interruption logic here)
        // FR: Vérifie si on doit s'arrêter (pourrait ajouter la logique d'interruption ici)
    }
}

// EN: AutoRetryGuard implementation
// FR: Implémentation d'AutoRetryGuard
AutoRetryGuard::AutoRetryGuard(const std::string& operation_name)
    : context_(operation_name), manager_(ErrorRecoveryManager::getInstance()) {
}

AutoRetryGuard::AutoRetryGuard(const std::string& operation_name, const RetryConfig& config)
    : context_(operation_name, config), manager_(ErrorRecoveryManager::getInstance()) {
}

AutoRetryGuard::~AutoRetryGuard() = default;

// EN: ErrorRecoveryUtils namespace implementation
// FR: Implémentation du namespace ErrorRecoveryUtils
namespace ErrorRecoveryUtils {

RetryConfig createNetworkRetryConfig() {
    RetryConfig config;
    config.max_attempts = 3;
    config.initial_delay = std::chrono::milliseconds(200);
    config.max_delay = std::chrono::milliseconds(10000);
    config.backoff_multiplier = 2.0;
    config.jitter_factor = 0.15;
    config.enable_jitter = true;
    
    config.recoverable_errors = {
        RecoverableErrorType::NETWORK_TIMEOUT,
        RecoverableErrorType::CONNECTION_REFUSED,
        RecoverableErrorType::DNS_RESOLUTION,
        RecoverableErrorType::SOCKET_ERROR,
        RecoverableErrorType::TEMPORARY_FAILURE
    };
    
    return config;
}

RetryConfig createHttpRetryConfig() {
    RetryConfig config;
    config.max_attempts = 5;
    config.initial_delay = std::chrono::milliseconds(500);
    config.max_delay = std::chrono::milliseconds(30000);
    config.backoff_multiplier = 2.5;
    config.jitter_factor = 0.2;
    config.enable_jitter = true;
    
    config.recoverable_errors = {
        RecoverableErrorType::NETWORK_TIMEOUT,
        RecoverableErrorType::CONNECTION_REFUSED,
        RecoverableErrorType::HTTP_5XX,
        RecoverableErrorType::HTTP_429,
        RecoverableErrorType::SSL_HANDSHAKE,
        RecoverableErrorType::TEMPORARY_FAILURE
    };
    
    return config;
}

RetryConfig createDatabaseRetryConfig() {
    RetryConfig config;
    config.max_attempts = 4;
    config.initial_delay = std::chrono::milliseconds(100);
    config.max_delay = std::chrono::milliseconds(5000);
    config.backoff_multiplier = 1.8;
    config.jitter_factor = 0.1;
    config.enable_jitter = true;
    
    config.recoverable_errors = {
        RecoverableErrorType::CONNECTION_REFUSED,
        RecoverableErrorType::TEMPORARY_FAILURE,
        RecoverableErrorType::SOCKET_ERROR
    };
    
    return config;
}

RecoverableErrorType classifyHttpError(int status_code) {
    // EN: 5xx server errors are generally recoverable
    // FR: Les erreurs serveur 5xx sont généralement récupérables
    if (status_code >= 500 && status_code < 600) {
        if (status_code == 502 || status_code == 503 || status_code == 504) {
            return RecoverableErrorType::TEMPORARY_FAILURE;
        }
        return RecoverableErrorType::HTTP_5XX;
    }
    
    // EN: Rate limiting
    // FR: Limitation de débit
    if (status_code == 429) {
        return RecoverableErrorType::HTTP_429;
    }
    
    // EN: Client errors (4xx) are generally not recoverable except for specific cases
    // FR: Les erreurs client (4xx) ne sont généralement pas récupérables sauf cas spécifiques
    return RecoverableErrorType::CUSTOM;
}

RecoverableErrorType classifyNetworkError(int errno_value) {
    switch (errno_value) {
        case ETIMEDOUT:
            return RecoverableErrorType::NETWORK_TIMEOUT;
        case ECONNREFUSED:
            return RecoverableErrorType::CONNECTION_REFUSED;
        case EHOSTUNREACH:
        case ENETUNREACH:
            return RecoverableErrorType::DNS_RESOLUTION;
        case ECONNRESET:
        case EPIPE:
            return RecoverableErrorType::CONNECTION_REFUSED;
        case EAGAIN:
#if EAGAIN != EWOULDBLOCK
        case EWOULDBLOCK:
#endif
            return RecoverableErrorType::TEMPORARY_FAILURE;
        default:
            return RecoverableErrorType::SOCKET_ERROR;
    }
}

RecoverableErrorType classifyException(const std::exception& error) {
    auto& manager = ErrorRecoveryManager::getInstance();
    return manager.classifyError(error);
}

} // namespace ErrorRecoveryUtils

} // namespace BBP