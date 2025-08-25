// EN: Implementation of the CacheSystem class. Provides HTTP caching with ETag/Last-Modified validation and TTL management.
// FR: Implémentation de la classe CacheSystem. Fournit la mise en cache HTTP avec validation ETag/Last-Modified et gestion TTL.

#include "../include/core/cache_system.hpp"
#include "../include/core/logger.hpp"
#include <algorithm>
#include <regex>
#include <sstream>
#include <thread>
#include <zlib.h>

namespace BBP {

// EN: Get the singleton cache system instance.
// FR: Obtient l'instance singleton du système de cache.
CacheSystem& CacheSystem::getInstance() {
    static CacheSystem instance;
    return instance;
}

// EN: Destructor stops cleanup thread.
// FR: Le destructeur arrête le thread de nettoyage.
CacheSystem::~CacheSystem() {
    stopCleanupThread();
}

// EN: Configure cache behavior and limits.
// FR: Configure le comportement et les limites du cache.
void CacheSystem::configure(const CacheConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    LOG_INFO("cache", "Cache configured - Max entries: " + std::to_string(config_.max_entries) + 
             ", Default TTL: " + std::to_string(config_.default_ttl.count()) + "s");
}

// EN: Store HTTP response in cache with validation headers.
// FR: Stocke la réponse HTTP en cache avec les headers de validation.
void CacheSystem::store(const std::string& url, 
                       const std::string& content,
                       const std::unordered_map<std::string, std::string>& headers) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // EN: Create new cache entry.
    // FR: Crée une nouvelle entrée de cache.
    auto entry = std::make_unique<CacheEntry>();
    entry->url = url;
    entry->content = config_.enable_compression ? compress(content) : content;
    entry->headers = headers;
    entry->created_at = std::chrono::system_clock::now();
    entry->last_accessed = entry->created_at;
    
    // EN: Extract validation headers.
    // FR: Extrait les headers de validation.
    auto etag_it = headers.find("etag");
    if (etag_it == headers.end()) {
        etag_it = headers.find("ETag");
    }
    if (etag_it != headers.end()) {
        entry->etag = etag_it->second;
    }
    
    auto last_modified_it = headers.find("last-modified");
    if (last_modified_it == headers.end()) {
        last_modified_it = headers.find("Last-Modified");
    }
    if (last_modified_it != headers.end()) {
        entry->last_modified = last_modified_it->second;
    }
    
    // EN: Calculate expiration time based on cache headers.
    // FR: Calcule le temps d'expiration basé sur les headers de cache.
    auto ttl = parseCacheControl(headers);
    entry->expires_at = entry->created_at + ttl;
    
    // EN: Evict old entries if cache is full.
    // FR: Évince les anciennes entrées si le cache est plein.
    if (cache_.size() >= config_.max_entries) {
        evictLRU();
    }
    
    // EN: Store entry and update statistics.
    // FR: Stocke l'entrée et met à jour les statistiques.
    cache_[url] = std::move(entry);
    stats_.entries_count = cache_.size();
    
    triggerEvent("store", url);
    LOG_DEBUG("cache", "Stored entry for URL: " + url);
}

// EN: Retrieve cached entry if available and valid.
// FR: Récupère l'entrée en cache si disponible et valide.
std::optional<CacheEntry> CacheSystem::get(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.total_requests++;
    
    auto it = cache_.find(url);
    if (it == cache_.end()) {
        stats_.cache_misses++;
        triggerEvent("miss", url);
        return std::nullopt;
    }
    
    auto& entry = it->second;
    entry->access_count++;
    entry->last_accessed = std::chrono::system_clock::now();
    
    // EN: Check if entry is expired.
    // FR: Vérifie si l'entrée est expirée.
    if (isExpired(*entry)) {
        if (config_.enable_stale_while_revalidate) {
            if (!isStale(*entry)) {
                // EN: Entry is expired but can be served stale while revalidating.
                // FR: L'entrée est expirée mais peut être servie comme obsolète pendant la revalidation.
                entry->is_stale = true;
                stats_.cache_hits++;
                triggerEvent("stale_hit", url);
                
                CacheEntry result = *entry;
                if (config_.enable_compression) {
                    result.content = decompress(result.content);
                }
                return result;
            }
        }
        
        // EN: Entry is expired and should not be served, remove it.
        // FR: L'entrée est expirée et ne doit pas être servie, la supprime.
        cache_.erase(it);
        stats_.entries_count = cache_.size();
        stats_.cache_misses++;
        triggerEvent("expired_miss", url);
        return std::nullopt;
    }
    
    // EN: Entry is fresh, return it.
    // FR: L'entrée est fraîche, la retourne.
    stats_.cache_hits++;
    triggerEvent("hit", url);
    
    CacheEntry result = *entry;
    if (config_.enable_compression) {
        result.content = decompress(result.content);
    }
    return result;
}

// EN: Check if URL has cached entry (regardless of freshness).
// FR: Vérifie si l'URL a une entrée en cache (peu importe la fraîcheur).
bool CacheSystem::has(const std::string& url) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.find(url) != cache_.end();
}

// EN: Validate cached entry against new response headers.
// FR: Valide l'entrée en cache contre les nouveaux headers de réponse.
ValidationResult CacheSystem::validate(const std::string& url,
                                       const std::unordered_map<std::string, std::string>& response_headers) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.validation_requests++;
    
    auto it = cache_.find(url);
    if (it == cache_.end()) {
        return ValidationResult::MODIFIED;
    }
    
    auto& entry = it->second;
    
    // EN: Check ETag validation.
    // FR: Vérifie la validation ETag.
    if (entry->etag) {
        auto etag_it = response_headers.find("etag");
        if (etag_it == response_headers.end()) {
            etag_it = response_headers.find("ETag");
        }
        
        if (etag_it != response_headers.end()) {
            if (*entry->etag == etag_it->second) {
                return ValidationResult::FRESH;
            } else {
                return ValidationResult::MODIFIED;
            }
        }
    }
    
    // EN: Check Last-Modified validation.
    // FR: Vérifie la validation Last-Modified.
    if (entry->last_modified) {
        auto last_modified_it = response_headers.find("last-modified");
        if (last_modified_it == response_headers.end()) {
            last_modified_it = response_headers.find("Last-Modified");
        }
        
        if (last_modified_it != response_headers.end()) {
            if (*entry->last_modified == last_modified_it->second) {
                return ValidationResult::FRESH;
            } else {
                return ValidationResult::MODIFIED;
            }
        }
    }
    
    // EN: If no validation headers, consider stale based on age.
    // FR: Si pas de headers de validation, considère obsolète basé sur l'âge.
    if (isExpired(*entry)) {
        return ValidationResult::STALE;
    }
    
    return ValidationResult::FRESH;
}

// EN: Generate conditional request headers for validation.
// FR: Génère les headers de requête conditionnelle pour la validation.
std::unordered_map<std::string, std::string> CacheSystem::getConditionalHeaders(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, std::string> conditional_headers;
    
    auto it = cache_.find(url);
    if (it == cache_.end()) {
        return conditional_headers;
    }
    
    auto& entry = it->second;
    
    // EN: Add If-None-Match header if ETag exists.
    // FR: Ajoute le header If-None-Match si ETag existe.
    if (entry->etag) {
        conditional_headers["If-None-Match"] = *entry->etag;
    }
    
    // EN: Add If-Modified-Since header if Last-Modified exists.
    // FR: Ajoute le header If-Modified-Since si Last-Modified existe.
    if (entry->last_modified) {
        conditional_headers["If-Modified-Since"] = *entry->last_modified;
    }
    
    return conditional_headers;
}

// EN: Update cached entry after successful validation.
// FR: Met à jour l'entrée en cache après validation réussie.
void CacheSystem::updateAfterValidation(const std::string& url,
                                        const std::unordered_map<std::string, std::string>& headers) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(url);
    if (it == cache_.end()) {
        return;
    }
    
    auto& entry = it->second;
    
    // EN: Update validation headers.
    // FR: Met à jour les headers de validation.
    entry->headers = headers;
    
    auto etag_it = headers.find("etag");
    if (etag_it == headers.end()) {
        etag_it = headers.find("ETag");
    }
    if (etag_it != headers.end()) {
        entry->etag = etag_it->second;
    }
    
    auto last_modified_it = headers.find("last-modified");
    if (last_modified_it == headers.end()) {
        last_modified_it = headers.find("Last-Modified");
    }
    if (last_modified_it != headers.end()) {
        entry->last_modified = last_modified_it->second;
    }
    
    // EN: Update expiration time.
    // FR: Met à jour le temps d'expiration.
    auto ttl = parseCacheControl(headers);
    entry->expires_at = std::chrono::system_clock::now() + ttl;
    entry->is_stale = false;
    
    triggerEvent("validated", url);
    LOG_DEBUG("cache", "Updated entry after validation for URL: " + url);
}

// EN: Remove specific entry from cache.
// FR: Supprime une entrée spécifique du cache.
void CacheSystem::remove(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(url);
    if (it != cache_.end()) {
        cache_.erase(it);
        stats_.entries_count = cache_.size();
        triggerEvent("removed", url);
        LOG_DEBUG("cache", "Removed entry for URL: " + url);
    }
}

// EN: Clear all cache entries.
// FR: Efface toutes les entrées du cache.
void CacheSystem::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t removed_count = cache_.size();
    cache_.clear();
    
    // EN: Reset all statistics.
    // FR: Remet à zéro toutes les statistiques.
    stats_.entries_count = 0;
    stats_.total_requests = 0;
    stats_.cache_hits = 0;
    stats_.cache_misses = 0;
    stats_.validation_requests = 0;
    stats_.memory_usage_bytes = 0;
    stats_.evictions = 0;
    stats_.hit_ratio = 0.0;
    
    triggerEvent("cleared", "all");
    LOG_INFO("cache", "Cleared cache, removed " + std::to_string(removed_count) + " entries");
}

// EN: Force cleanup of expired and stale entries.
// FR: Force le nettoyage des entrées expirées et obsolètes.
size_t CacheSystem::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t removed_count = 0;
    
    auto it = cache_.begin();
    while (it != cache_.end()) {
        if (isExpired(*it->second) && 
            (!config_.enable_stale_while_revalidate || isStale(*it->second))) {
            it = cache_.erase(it);
            removed_count++;
            stats_.evictions++;
        } else {
            ++it;
        }
    }
    
    stats_.entries_count = cache_.size();
    
    if (removed_count > 0) {
        triggerEvent("cleanup", std::to_string(removed_count) + " entries");
        LOG_INFO("cache", "Cleanup removed " + std::to_string(removed_count) + " expired entries");
    }
    
    return removed_count;
}

// EN: Get cache statistics for monitoring.
// FR: Obtient les statistiques du cache pour le monitoring.
CacheStats CacheSystem::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    CacheStats current_stats = stats_;
    current_stats.entries_count = cache_.size();
    current_stats.memory_usage_bytes = 0;
    
    // EN: Calculate total memory usage.
    // FR: Calcule l'utilisation mémoire totale.
    for (const auto& [url, entry] : cache_) {
        current_stats.memory_usage_bytes += calculateEntrySize(*entry);
    }
    
    // EN: Calculate hit ratio.
    // FR: Calcule le ratio de hit.
    if (current_stats.total_requests > 0) {
        current_stats.hit_ratio = static_cast<double>(current_stats.cache_hits) / 
                                  static_cast<double>(current_stats.total_requests);
    }
    
    return current_stats;
}

// EN: Set callback for cache events (hit, miss, eviction).
// FR: Définit le callback pour les événements de cache (hit, miss, éviction).
void CacheSystem::setEventCallback(std::function<void(const std::string&, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = callback;
}

// EN: Enable or disable automatic background cleanup.
// FR: Active ou désactive le nettoyage automatique en arrière-plan.
void CacheSystem::enableAutoCleanup(bool enabled, std::chrono::seconds interval) {
    if (enabled && !cleanup_enabled_) {
        cleanup_interval_ = interval;
        cleanup_enabled_ = true;
        startCleanupThread();
        LOG_INFO("cache", "Auto cleanup enabled with " + std::to_string(interval.count()) + "s interval");
    } else if (!enabled && cleanup_enabled_) {
        cleanup_enabled_ = false;
        stopCleanupThread();
        LOG_INFO("cache", "Auto cleanup disabled");
    }
}

// EN: Parse cache control headers to determine TTL.
// FR: Parse les headers cache-control pour déterminer le TTL.
std::chrono::seconds CacheSystem::parseCacheControl(const std::unordered_map<std::string, std::string>& headers) {
    auto cache_control_it = headers.find("cache-control");
    if (cache_control_it == headers.end()) {
        cache_control_it = headers.find("Cache-Control");
    }
    
    if (cache_control_it != headers.end()) {
        std::regex max_age_regex(R"(max-age\s*=\s*(\d+))");
        std::smatch match;
        
        if (std::regex_search(cache_control_it->second, match, max_age_regex)) {
            auto max_age = std::chrono::seconds(std::stoll(match[1].str()));
            // EN: Clamp TTL to configured min/max values.
            // FR: Limite le TTL aux valeurs min/max configurées.
            max_age = std::max(config_.min_ttl, std::min(config_.max_ttl, max_age));
            return max_age;
        }
    }
    
    // EN: Check Expires header as fallback.
    // FR: Vérifie le header Expires comme fallback.
    auto expires_it = headers.find("expires");
    if (expires_it == headers.end()) {
        expires_it = headers.find("Expires");
    }
    
    if (expires_it != headers.end()) {
        // EN: Simple expires parsing - in production, use proper HTTP date parsing.
        // FR: Parsing simple d'expires - en production, utiliser un parsing de date HTTP approprié.
        // For now, return default TTL
        return config_.default_ttl;
    }
    
    return config_.default_ttl;
}

// EN: Check if entry is expired based on TTL.
// FR: Vérifie si l'entrée est expirée basé sur le TTL.
bool CacheSystem::isExpired(const CacheEntry& entry) const {
    return std::chrono::system_clock::now() > entry.expires_at;
}

// EN: Check if entry is stale but still usable.
// FR: Vérifie si l'entrée est obsolète mais encore utilisable.
bool CacheSystem::isStale(const CacheEntry& entry) const {
    auto stale_threshold = entry.expires_at + config_.stale_max_age;
    return std::chrono::system_clock::now() > stale_threshold;
}

// EN: Evict least recently used entries when cache is full.
// FR: Évince les entrées les moins récemment utilisées quand le cache est plein.
void CacheSystem::evictLRU() {
    if (cache_.empty()) {
        return;
    }
    
    // EN: Find least recently used entry.
    // FR: Trouve l'entrée la moins récemment utilisée.
    auto lru_it = cache_.begin();
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second->last_accessed < lru_it->second->last_accessed) {
            lru_it = it;
        }
    }
    
    std::string evicted_url = lru_it->first;
    cache_.erase(lru_it);
    stats_.evictions++;
    
    triggerEvent("evicted", evicted_url);
    LOG_DEBUG("cache", "Evicted LRU entry for URL: " + evicted_url);
}

// EN: Calculate memory usage of cache entry.
// FR: Calcule l'utilisation mémoire d'une entrée de cache.
size_t CacheSystem::calculateEntrySize(const CacheEntry& entry) const {
    size_t size = entry.url.size() + entry.content.size();
    
    for (const auto& [key, value] : entry.headers) {
        size += key.size() + value.size();
    }
    
    if (entry.etag) {
        size += entry.etag->size();
    }
    if (entry.last_modified) {
        size += entry.last_modified->size();
    }
    
    return size + sizeof(CacheEntry);
}

// EN: Compress content for storage efficiency.
// FR: Compresse le contenu pour l'efficacité de stockage.
std::string CacheSystem::compress(const std::string& content) const {
    if (content.empty()) {
        return content;
    }
    
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK) {
        return content; // Return original on compression failure
    }
    
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(content.data()));
    zs.avail_in = content.size();
    
    std::string compressed;
    char buffer[32768];
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(buffer);
        zs.avail_out = sizeof(buffer);
        
        int result = deflate(&zs, Z_FINISH);
        if (result == Z_STREAM_ERROR) {
            deflateEnd(&zs);
            return content;
        }
        
        compressed.append(buffer, sizeof(buffer) - zs.avail_out);
    } while (zs.avail_out == 0);
    
    deflateEnd(&zs);
    return compressed;
}

// EN: Decompress content for retrieval.
// FR: Décompresse le contenu pour la récupération.
std::string CacheSystem::decompress(const std::string& compressed_content) const {
    if (compressed_content.empty()) {
        return compressed_content;
    }
    
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (inflateInit(&zs) != Z_OK) {
        return compressed_content;
    }
    
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed_content.data()));
    zs.avail_in = compressed_content.size();
    
    std::string decompressed;
    char buffer[32768];
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(buffer);
        zs.avail_out = sizeof(buffer);
        
        int result = inflate(&zs, Z_NO_FLUSH);
        if (result == Z_STREAM_ERROR || result == Z_DATA_ERROR) {
            inflateEnd(&zs);
            return compressed_content;
        }
        
        decompressed.append(buffer, sizeof(buffer) - zs.avail_out);
    } while (zs.avail_out == 0);
    
    inflateEnd(&zs);
    return decompressed;
}

// EN: Start background cleanup thread.
// FR: Démarre le thread de nettoyage en arrière-plan.
void CacheSystem::startCleanupThread() {
    should_stop_cleanup_ = false;
    cleanup_thread_ = std::make_unique<std::thread>([this]() {
        cleanupLoop();
    });
}

// EN: Stop background cleanup thread.
// FR: Arrête le thread de nettoyage en arrière-plan.
void CacheSystem::stopCleanupThread() {
    if (cleanup_thread_) {
        should_stop_cleanup_ = true;
        cleanup_thread_->join();
        cleanup_thread_.reset();
    }
}

// EN: Background cleanup function.
// FR: Fonction de nettoyage en arrière-plan.
void CacheSystem::cleanupLoop() {
    while (!should_stop_cleanup_) {
        std::this_thread::sleep_for(cleanup_interval_);
        
        if (!should_stop_cleanup_) {
            cleanup();
        }
    }
}

// EN: Trigger cache event callback.
// FR: Déclenche le callback d'événement de cache.
void CacheSystem::triggerEvent(const std::string& event, const std::string& url) {
    if (event_callback_) {
        event_callback_(event, url);
    }
}

} // namespace BBP