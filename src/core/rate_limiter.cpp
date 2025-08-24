// EN: Implementation of the RateLimiter class. Provides per-domain token bucket rate limiting with adaptive backoff.
// FR: Implémentation de la classe RateLimiter. Fournit une limitation de débit par token bucket par domaine avec backoff adaptatif.

#include "../include/core/rate_limiter.hpp"
#include <algorithm>
#include <thread>

namespace BBP {

// EN: Get the singleton rate limiter instance.
// FR: Obtient l'instance singleton du limiteur de débit.
RateLimiter& RateLimiter::getInstance() {
    static RateLimiter instance;
    return instance;
}

// EN: Configure token bucket parameters for a specific domain.
// FR: Configure les paramètres de token bucket pour un domaine spécifique.
void RateLimiter::setBucketConfig(const std::string& domain, double requests_per_second, double burst_capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // EN: Default burst capacity to 2x rate if not specified.
    // FR: Capacité de burst par défaut à 2x le débit si non spécifiée.
    if (burst_capacity == 0) {
        burst_capacity = std::max(1.0, requests_per_second * 2.0);
    }
    
    auto it = domain_states_.find(domain);
    if (it == domain_states_.end()) {
        domain_states_[domain] = std::make_unique<DomainState>();
        it = domain_states_.find(domain);
    }
    
    it->second->bucket = std::make_unique<TokenBucket>(burst_capacity, requests_per_second);
}

// EN: Set global rate limit that applies across all domains.
// FR: Définit la limite de débit globale qui s'applique à tous les domaines.
void RateLimiter::setGlobalRateLimit(double requests_per_second) {
    std::lock_guard<std::mutex> lock(mutex_);
    double burst_capacity = std::max(1.0, requests_per_second * 2.0);
    global_bucket_ = std::make_unique<TokenBucket>(burst_capacity, requests_per_second);
}

// EN: Configure adaptive backoff behavior for a specific domain.
// FR: Configure le comportement de backoff adaptatif pour un domaine spécifique.
void RateLimiter::setBackoffConfig(const std::string& domain, const BackoffConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = domain_states_.find(domain);
    if (it == domain_states_.end()) {
        domain_states_[domain] = std::make_unique<DomainState>();
        it = domain_states_.find(domain);
    }
    
    it->second->backoff_config = config;
}

bool RateLimiter::tryAcquire(const std::string& domain, int tokens) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    TokenBucket* bucket = getBucket(domain);
    if (!bucket) {
        setBucketConfig(domain, DEFAULT_REQUESTS_PER_SECOND);
        bucket = getBucket(domain);
    }
    
    refillTokens(*bucket);
    
    if (global_bucket_) {
        refillTokens(*global_bucket_);
        if (global_bucket_->tokens < tokens) {
            auto& state = domain_states_[domain];
            state->denied_requests++;
            state->total_requests++;
            return false;
        }
    }
    
    auto& state = domain_states_[domain];
    state->total_requests++;
    
    auto now = std::chrono::steady_clock::now();
    if (state->current_delay_ms > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - state->last_failure).count();
        
        if (elapsed >= state->current_delay_ms) {
            state->current_delay_ms = std::max(0.0, 
                state->current_delay_ms - static_cast<double>(elapsed));
        }
        
        if (state->current_delay_ms > 0) {
            state->denied_requests++;
            return false;
        }
    }
    
    if (bucket->tokens >= tokens) {
        bucket->tokens -= tokens;
        if (global_bucket_) {
            global_bucket_->tokens -= tokens;
        }
        return true;
    }
    
    state->denied_requests++;
    return false;
}

std::chrono::milliseconds RateLimiter::getWaitTime(const std::string& domain, int tokens) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    TokenBucket* bucket = getBucket(domain);
    if (!bucket) {
        return std::chrono::milliseconds(0);
    }
    
    refillTokens(*bucket);
    
    if (bucket->tokens >= tokens) {
        return std::chrono::milliseconds(0);
    }
    
    double needed_tokens = tokens - bucket->tokens;
    double wait_time_ms = (needed_tokens / bucket->refill_rate) * 1000.0;
    
    auto it = domain_states_.find(domain);
    if (it != domain_states_.end() && it->second->current_delay_ms > 0) {
        wait_time_ms = std::max(wait_time_ms, it->second->current_delay_ms);
    }
    
    return std::chrono::milliseconds(static_cast<long long>(wait_time_ms));
}

void RateLimiter::waitForToken(const std::string& domain, int tokens) {
    auto wait_time = getWaitTime(domain, tokens);
    if (wait_time.count() > 0) {
        std::this_thread::sleep_for(wait_time);
    }
}

bool RateLimiter::isRateLimited(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = domain_states_.find(domain);
    if (it != domain_states_.end()) {
        return it->second->current_delay_ms > 0;
    }
    
    return false;
}

// EN: Report request failure to trigger exponential backoff.
// FR: Signale un échec de requête pour déclencher le backoff exponentiel.
void RateLimiter::reportFailure(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = domain_states_.find(domain);
    if (it == domain_states_.end()) {
        domain_states_[domain] = std::make_unique<DomainState>();
        it = domain_states_.find(domain);
    }
    
    auto& state = it->second;
    state->consecutive_failures++;
    state->last_failure = std::chrono::steady_clock::now();
    state->backoff_triggered++;
    
    // EN: Apply exponential backoff with configured limits.
    // FR: Applique un backoff exponentiel avec les limites configurées.
    if (state->current_delay_ms == 0.0) {
        state->current_delay_ms = state->backoff_config.initial_delay_ms;
    } else {
        state->current_delay_ms = std::min(
            state->current_delay_ms * state->backoff_config.multiplier,
            state->backoff_config.max_delay_ms
        );
    }
}

// EN: Report request success to reduce backoff delay.
// FR: Signale un succès de requête pour réduire le délai de backoff.
void RateLimiter::reportSuccess(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = domain_states_.find(domain);
    if (it != domain_states_.end()) {
        auto& state = it->second;
        state->consecutive_failures = 0;
        
        // EN: Gradually reduce backoff delay on success.
        // FR: Réduit progressivement le délai de backoff en cas de succès.
        if (state->current_delay_ms > 0) {
            state->current_delay_ms *= 0.5;
            if (state->current_delay_ms < state->backoff_config.initial_delay_ms) {
                state->current_delay_ms = 0.0;
            }
        }
    }
}

void RateLimiter::resetBackoff(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = domain_states_.find(domain);
    if (it != domain_states_.end()) {
        auto& state = it->second;
        state->current_delay_ms = 0.0;
        state->consecutive_failures = 0;
    }
}

double RateLimiter::getCurrentDelay(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = domain_states_.find(domain);
    if (it != domain_states_.end()) {
        return it->second->current_delay_ms;
    }
    
    return 0.0;
}

RateLimiter::Stats RateLimiter::getStats(const std::string& domain) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Stats stats;
    auto it = domain_states_.find(domain);
    if (it != domain_states_.end()) {
        const auto& state = it->second;
        stats.total_requests = state->total_requests;
        stats.denied_requests = state->denied_requests;
        stats.backoff_triggered = state->backoff_triggered;
        stats.current_delay_ms = state->current_delay_ms;
        
        if (state->bucket) {
            stats.current_tokens = state->bucket->tokens;
        }
    }
    
    return stats;
}

void RateLimiter::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    domain_states_.clear();
    global_bucket_.reset();
}

RateLimiter::TokenBucket* RateLimiter::getBucket(const std::string& domain) {
    auto it = domain_states_.find(domain);
    if (it != domain_states_.end() && it->second->bucket) {
        return it->second->bucket.get();
    }
    return nullptr;
}

void RateLimiter::refillTokens(RateLimiter::TokenBucket& bucket) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - bucket.last_refill).count();
    
    double tokens_to_add = elapsed * bucket.refill_rate;
    bucket.tokens = std::min(bucket.capacity, bucket.tokens + tokens_to_add);
    bucket.last_refill = now;
}

} // namespace BBP