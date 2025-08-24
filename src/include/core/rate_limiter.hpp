#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace BBP {

// EN: Thread-safe rate limiter with per-domain token buckets and adaptive backoff.
// FR: Limiteur de débit thread-safe avec token buckets par domaine et backoff adaptatif.
class RateLimiter {
public:
    // EN: Token bucket structure for rate limiting algorithm.
    // FR: Structure token bucket pour l'algorithme de limitation de débit.
    struct TokenBucket {
        double tokens;
        double capacity;
        double refill_rate;
        std::chrono::steady_clock::time_point last_refill;
        
        TokenBucket(double cap, double rate) 
            : tokens(cap), capacity(cap), refill_rate(rate), 
              last_refill(std::chrono::steady_clock::now()) {}
    };
    
    // EN: Configuration for adaptive backoff behavior.
    // FR: Configuration pour le comportement de backoff adaptatif.
    struct BackoffConfig {
        double initial_delay_ms = 1000.0;
        double max_delay_ms = 60000.0;
        double multiplier = 2.0;
        int max_retries = 5;
    };

    // EN: Get the singleton instance.
    // FR: Obtient l'instance singleton.
    static RateLimiter& getInstance();
    
    // EN: Configure rate limiting for a specific domain.
    // FR: Configure la limitation de débit pour un domaine spécifique.
    void setBucketConfig(const std::string& domain, double requests_per_second, double burst_capacity = 0);
    
    // EN: Set global rate limit across all domains.
    // FR: Définit la limite de débit globale pour tous les domaines.
    void setGlobalRateLimit(double requests_per_second);
    
    // EN: Configure adaptive backoff for a domain.
    // FR: Configure le backoff adaptatif pour un domaine.
    void setBackoffConfig(const std::string& domain, const BackoffConfig& config);
    
    // EN: Try to acquire tokens from the domain's bucket.
    // FR: Tente d'acquérir des tokens du bucket du domaine.
    bool tryAcquire(const std::string& domain, int tokens = 1);
    
    // EN: Calculate wait time needed before next request can be made.
    // FR: Calcule le temps d'attente nécessaire avant la prochaine requête.
    std::chrono::milliseconds getWaitTime(const std::string& domain, int tokens = 1);
    
    // EN: Wait for tokens to become available (blocking call).
    // FR: Attend que les tokens deviennent disponibles (appel bloquant).
    void waitForToken(const std::string& domain, int tokens = 1);
    
    // EN: Check if domain is currently in backoff state.
    // FR: Vérifie si le domaine est actuellement en état de backoff.
    bool isRateLimited(const std::string& domain);
    
    // EN: Report request failure to trigger backoff.
    // FR: Signale un échec de requête pour déclencher le backoff.
    void reportFailure(const std::string& domain);
    
    // EN: Report request success to reduce backoff.
    // FR: Signale un succès de requête pour réduire le backoff.
    void reportSuccess(const std::string& domain);
    
    // EN: Reset backoff state for a domain.
    // FR: Remet à zéro l'état de backoff pour un domaine.
    void resetBackoff(const std::string& domain);
    
    // EN: Get current backoff delay for a domain.
    // FR: Obtient le délai de backoff actuel pour un domaine.
    double getCurrentDelay(const std::string& domain);
    
    // EN: Statistics structure for rate limiter monitoring.
    // FR: Structure de statistiques pour le monitoring du limiteur de débit.
    struct Stats {
        size_t total_requests = 0;
        size_t denied_requests = 0;
        size_t backoff_triggered = 0;
        double current_tokens = 0.0;
        double current_delay_ms = 0.0;
    };
    
    // EN: Get statistics for a specific domain.
    // FR: Obtient les statistiques pour un domaine spécifique.
    Stats getStats(const std::string& domain);
    
    // EN: Reset all rate limiting state.
    // FR: Remet à zéro tout l'état de limitation de débit.
    void reset();

private:
    RateLimiter() = default;
    ~RateLimiter() = default;
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;
    
    // EN: Get or create token bucket for domain.
    // FR: Obtient ou crée le token bucket pour le domaine.
    TokenBucket* getBucket(const std::string& domain);
    
    // EN: Refill tokens based on elapsed time.
    // FR: Recharge les tokens basé sur le temps écoulé.
    void refillTokens(TokenBucket& bucket);
    
    // EN: Per-domain state tracking structure.
    // FR: Structure de suivi d'état par domaine.
    struct DomainState {
        std::unique_ptr<TokenBucket> bucket;
        BackoffConfig backoff_config;
        double current_delay_ms = 0.0;
        int consecutive_failures = 0;
        std::chrono::steady_clock::time_point last_failure;
        size_t total_requests = 0;
        size_t denied_requests = 0;
        size_t backoff_triggered = 0;
    };
    
    std::unordered_map<std::string, std::unique_ptr<DomainState>> domain_states_;
    std::unique_ptr<TokenBucket> global_bucket_;
    mutable std::mutex mutex_;
    
    static constexpr double DEFAULT_REQUESTS_PER_SECOND = 10.0;
    static constexpr double GLOBAL_REQUESTS_PER_SECOND = 100.0;
    static constexpr std::chrono::milliseconds BACKOFF_DECAY_INTERVAL{30000}; // 30 seconds
};

} // namespace BBP