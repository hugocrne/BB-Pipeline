#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <functional>
#include <thread>
#include <atomic>

namespace BBP {

// EN: Cache entry containing HTTP response data with validation metadata.
// FR: Entrée de cache contenant les données de réponse HTTP avec métadonnées de validation.
struct CacheEntry {
    std::string url;
    std::string content;
    std::unordered_map<std::string, std::string> headers;
    std::optional<std::string> etag;
    std::optional<std::string> last_modified;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
    size_t access_count = 0;
    std::chrono::system_clock::time_point last_accessed;
    bool is_stale = false;
};

// EN: Configuration for cache behavior and validation.
// FR: Configuration pour le comportement du cache et la validation.
struct CacheConfig {
    // EN: Default TTL for entries without explicit cache headers.
    // FR: TTL par défaut pour les entrées sans headers de cache explicites.
    std::chrono::seconds default_ttl{3600}; // 1 hour
    
    // EN: Maximum TTL to respect from cache headers.
    // FR: TTL maximum à respecter des headers de cache.
    std::chrono::seconds max_ttl{86400}; // 24 hours
    
    // EN: Minimum TTL to enforce even with short cache headers.
    // FR: TTL minimum à imposer même avec des headers de cache courts.
    std::chrono::seconds min_ttl{60}; // 1 minute
    
    // EN: Maximum number of entries to keep in cache.
    // FR: Nombre maximum d'entrées à garder en cache.
    size_t max_entries = 10000;
    
    // EN: Enable automatic stale-while-revalidate behavior.
    // FR: Active le comportement automatique stale-while-revalidate.
    bool enable_stale_while_revalidate = true;
    
    // EN: Maximum age for stale entries before forced removal.
    // FR: Âge maximum pour les entrées obsolètes avant suppression forcée.
    std::chrono::seconds stale_max_age{7200}; // 2 hours
    
    // EN: Enable compression for cached content.
    // FR: Active la compression pour le contenu en cache.
    bool enable_compression = true;
};

// EN: Statistics for cache monitoring and optimization.
// FR: Statistiques pour le monitoring et l'optimisation du cache.
struct CacheStats {
    size_t total_requests = 0;
    size_t cache_hits = 0;
    size_t cache_misses = 0;
    size_t validation_requests = 0;
    size_t entries_count = 0;
    size_t memory_usage_bytes = 0;
    size_t evictions = 0;
    double hit_ratio = 0.0;
};

// EN: Validation result for conditional HTTP requests.
// FR: Résultat de validation pour les requêtes HTTP conditionnelles.
enum class ValidationResult {
    FRESH,      // Content is fresh, use cache
    STALE,      // Content is stale, needs revalidation
    MODIFIED    // Content has been modified, cache invalid
};

// EN: Thread-safe HTTP cache with ETag/Last-Modified validation and TTL management.
// FR: Cache HTTP thread-safe avec validation ETag/Last-Modified et gestion TTL.
class CacheSystem {
public:
    // EN: Get the singleton cache system instance.
    // FR: Obtient l'instance singleton du système de cache.
    static CacheSystem& getInstance();
    
    // EN: Configure cache behavior and limits.
    // FR: Configure le comportement et les limites du cache.
    void configure(const CacheConfig& config);
    
    // EN: Get cache configuration.
    // FR: Obtient la configuration du cache.
    const CacheConfig& getConfig() const { return config_; }
    
    // EN: Store HTTP response in cache with validation headers.
    // FR: Stocke la réponse HTTP en cache avec les headers de validation.
    void store(const std::string& url, 
               const std::string& content,
               const std::unordered_map<std::string, std::string>& headers);
    
    // EN: Retrieve cached entry if available and valid.
    // FR: Récupère l'entrée en cache si disponible et valide.
    std::optional<CacheEntry> get(const std::string& url);
    
    // EN: Check if URL has cached entry (regardless of freshness).
    // FR: Vérifie si l'URL a une entrée en cache (peu importe la fraîcheur).
    bool has(const std::string& url) const;
    
    // EN: Validate cached entry against new response headers.
    // FR: Valide l'entrée en cache contre les nouveaux headers de réponse.
    ValidationResult validate(const std::string& url,
                             const std::unordered_map<std::string, std::string>& response_headers);
    
    // EN: Generate conditional request headers for validation.
    // FR: Génère les headers de requête conditionnelle pour la validation.
    std::unordered_map<std::string, std::string> getConditionalHeaders(const std::string& url);
    
    // EN: Update cached entry after successful validation.
    // FR: Met à jour l'entrée en cache après validation réussie.
    void updateAfterValidation(const std::string& url,
                              const std::unordered_map<std::string, std::string>& headers);
    
    // EN: Remove specific entry from cache.
    // FR: Supprime une entrée spécifique du cache.
    void remove(const std::string& url);
    
    // EN: Clear all cache entries.
    // FR: Efface toutes les entrées du cache.
    void clear();
    
    // EN: Force cleanup of expired and stale entries.
    // FR: Force le nettoyage des entrées expirées et obsolètes.
    size_t cleanup();
    
    // EN: Get cache statistics for monitoring.
    // FR: Obtient les statistiques du cache pour le monitoring.
    CacheStats getStats() const;
    
    // EN: Set callback for cache events (hit, miss, eviction).
    // FR: Définit le callback pour les événements de cache (hit, miss, éviction).
    void setEventCallback(std::function<void(const std::string&, const std::string&)> callback);
    
    // EN: Enable or disable automatic background cleanup.
    // FR: Active ou désactive le nettoyage automatique en arrière-plan.
    void enableAutoCleanup(bool enabled, std::chrono::seconds interval = std::chrono::seconds{300});

private:
    CacheSystem() = default;
    ~CacheSystem();
    CacheSystem(const CacheSystem&) = delete;
    CacheSystem& operator=(const CacheSystem&) = delete;
    
    // EN: Parse cache control headers to determine TTL.
    // FR: Parse les headers cache-control pour déterminer le TTL.
    std::chrono::seconds parseCacheControl(const std::unordered_map<std::string, std::string>& headers);
    
    // EN: Check if entry is expired based on TTL.
    // FR: Vérifie si l'entrée est expirée basé sur le TTL.
    bool isExpired(const CacheEntry& entry) const;
    
    // EN: Check if entry is stale but still usable.
    // FR: Vérifie si l'entrée est obsolète mais encore utilisable.
    bool isStale(const CacheEntry& entry) const;
    
    // EN: Evict least recently used entries when cache is full.
    // FR: Évince les entrées les moins récemment utilisées quand le cache est plein.
    void evictLRU();
    
    // EN: Calculate memory usage of cache entry.
    // FR: Calcule l'utilisation mémoire d'une entrée de cache.
    size_t calculateEntrySize(const CacheEntry& entry) const;
    
    // EN: Compress content for storage efficiency.
    // FR: Compresse le contenu pour l'efficacité de stockage.
    std::string compress(const std::string& content) const;
    
    // EN: Decompress content for retrieval.
    // FR: Décompresse le contenu pour la récupération.
    std::string decompress(const std::string& compressed_content) const;
    
    // EN: Start background cleanup thread.
    // FR: Démarre le thread de nettoyage en arrière-plan.
    void startCleanupThread();
    
    // EN: Stop background cleanup thread.
    // FR: Arrête le thread de nettoyage en arrière-plan.
    void stopCleanupThread();
    
    // EN: Background cleanup function.
    // FR: Fonction de nettoyage en arrière-plan.
    void cleanupLoop();
    
    // EN: Trigger cache event callback.
    // FR: Déclenche le callback d'événement de cache.
    void triggerEvent(const std::string& event, const std::string& url);
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<CacheEntry>> cache_;
    CacheConfig config_;
    
    // EN: Statistics tracking.
    // FR: Suivi des statistiques.
    mutable CacheStats stats_;
    
    // EN: Event callback for monitoring.
    // FR: Callback d'événement pour le monitoring.
    std::function<void(const std::string&, const std::string&)> event_callback_;
    
    // EN: Background cleanup thread management.
    // FR: Gestion du thread de nettoyage en arrière-plan.
    std::unique_ptr<std::thread> cleanup_thread_;
    std::atomic<bool> cleanup_enabled_{false};
    std::atomic<bool> should_stop_cleanup_{false};
    std::chrono::seconds cleanup_interval_{300}; // 5 minutes
};

} // namespace BBP