# 📚 Documentation BB-Pipeline - Fonctionnalités Développées

## 🌐 Langues / Languages
- 🇫🇷 **Français** : Documentation détaillée en français
- 🇺🇸 **English** : Complete documentation in English

---

# 🇫🇷 DOCUMENTATION FRANÇAISE

## 📋 Vue d'Ensemble

BB-Pipeline est un framework de cybersécurité modulaire conçu pour l'automatisation de la reconnaissance éthique en bug bounty. Le framework suit une approche défensive stricte avec validation de scope et opérations safe-by-default.

### Architecture Modulaire

```
infrastructure/
├── logging/      - Système de logs structurés
├── networking/   - Rate limiting et cache HTTP
├── threading/    - Pool de threads haute performance  
├── config/       - Gestion de configuration
└── system/       - Gestionnaires système (en cours)
```

---

## 🚀 Fonctionnalités Développées

### 1. 📝 Système de Logging (Logger System)

#### Fonctionnalité
Système de logging thread-safe produisant des logs structurés au format NDJSON avec support des correlation IDs pour le tracing distribué.

#### Caractéristiques Techniques
- **Pattern** : Singleton thread-safe
- **Format** : NDJSON (Newline Delimited JSON)
- **Niveaux** : DEBUG, INFO, WARN, ERROR
- **Thread-Safety** : Mutex pour accès concurrent
- **Correlation IDs** : Identifiants uniques générés automatiquement
- **Timestamps** : ISO 8601 UTC

#### Utilisation
```cpp
#include "infrastructure/logging/logger.hpp"

auto& logger = BBP::Logger::getInstance();
logger.info("module", "Message d'information");
logger.warn("module", "Avertissement");
logger.error("module", "Erreur critique");
```

#### Format de Sortie
```json
{"timestamp":"2025-08-25T10:30:45.123Z","level":"INFO","message":"Démarrage du système","module":"core","correlation_id":"abc-123-def","thread_id":"0x7fff12345"}
```

#### Cas d'Usage
- Debugging des modules pipeline
- Monitoring des performances
- Audit trail pour conformité sécurité
- Corrélation des événements distribués

---

### 2. 🔄 Rate Limiter

#### Fonctionnalité
Système de limitation de débit par domaine utilisant l'algorithme token bucket avec backoff exponentiel adaptatif pour respecter les limites des serveurs cibles.

#### Caractéristiques Techniques
- **Algorithme** : Token Bucket par domaine
- **Backoff** : Exponentiel adaptatif (initial: 100ms, max: 60s, multiplicateur: 2.0)
- **Granularité** : Par domaine avec rate global optionnel
- **Persistance** : États en mémoire avec statistiques
- **Thread-Safety** : Mutex pour accès concurrent

#### Configuration
```cpp
BBP::RateLimiter::getInstance().setBucketConfig("example.com", 10.0, 20.0);
// 10 requêtes/seconde avec burst de 20 requêtes
```

#### Utilisation
```cpp
#include "infrastructure/networking/rate_limiter.hpp"

auto& limiter = BBP::RateLimiter::getInstance();

// Tentative d'acquisition de token
if (limiter.tryAcquire("example.com")) {
    // Faire la requête
} else {
    // Attendre ou reporter
}

// En cas d'erreur serveur (429, 503)
limiter.reportFailure("example.com");

// En cas de succès
limiter.reportSuccess("example.com");
```

#### Métriques
- Requêtes totales/refusées par domaine
- Délai de backoff actuel
- Tokens disponibles
- Nombre de déclenchements de backoff

#### Cas d'Usage
- Respect des rate limits des APIs
- Protection contre les 429 Too Many Requests
- Optimisation automatique du débit
- Éviter les blacklists IP

---

### 3. ⚙️ Gestionnaire de Configuration (Configuration Manager)

#### Fonctionnalité
Système de gestion centralisé des configurations YAML avec validation des schémas, support des templates et expansion des variables d'environnement.

#### Caractéristiques Techniques
- **Format** : YAML avec validation stricte
- **Templates** : Expansion de variables `${VAR}` et `${VAR:default}`
- **Types Supportés** : string, int, double, bool, arrays, objects
- **Validation** : Schémas avec contraintes (min/max, required, etc.)
- **Rechargement** : À chaud avec notification de changements
- **Thread-Safety** : Mutex pour modifications concurrentes

#### Structure Configuration
```yaml
# configs/default.yaml
http:
  timeout: ${HTTP_TIMEOUT:30}
  user_agent: "BB-Pipeline/1.0"
  max_redirects: 5

rate_limits:
  default_rps: 10
  burst_capacity: 20
  
logging:
  level: ${LOG_LEVEL:INFO}
  format: "json"
```

#### Utilisation
```cpp
#include "infrastructure/config/config_manager.hpp"

auto& config = BBP::ConfigManager::getInstance();
config.loadFromFile("configs/default.yaml");

// Accès aux valeurs
int timeout = config.get<int>("http.timeout");
string user_agent = config.get<string>("http.user_agent");
```

#### Validation
```cpp
// Définir des contraintes
config.setValidationRule("http.timeout", [](const auto& value) {
    return value.as<int>() >= 1 && value.as<int>() <= 300;
});
```

#### Cas d'Usage
- Configuration centralisée des modules
- Environnements multiples (dev/prod)
- Validation des paramètres critiques
- Rechargement sans redémarrage

---

### 4. 💾 Système de Cache HTTP (Cache System)

#### Fonctionnalité
Cache HTTP intelligent avec validation ETag/Last-Modified, gestion TTL sophistiquée et support stale-while-revalidate pour optimiser les performances réseau.

#### Caractéristiques Techniques
- **Validation** : ETag et Last-Modified selon RFC 7234
- **TTL** : Configurable avec min/max/default (défaut: 1h, max: 24h, min: 1min)
- **Éviction** : LRU (Least Recently Used)
- **Stale-While-Revalidate** : Servir du contenu stale pendant revalidation
- **Compression** : zlib pour optimisation mémoire
- **Cleanup** : Automatique en arrière-plan (optionnel, 5min)
- **Thread-Safety** : Mutex pour opérations concurrentes

#### Configuration
```cpp
BBP::CacheConfig config;
config.default_ttl = std::chrono::seconds(3600);  // 1 heure
config.max_entries = 10000;
config.enable_compression = true;
config.enable_stale_while_revalidate = true;
config.stale_max_age = std::chrono::seconds(7200);  // 2 heures

auto& cache = BBP::CacheSystem::getInstance();
cache.configure(config);
```

#### Utilisation
```cpp
#include "infrastructure/networking/cache_system.hpp"

auto& cache = BBP::CacheSystem::getInstance();

// Stockage
std::unordered_map<string, string> headers = {
    {"etag", "\"abc123\""},
    {"cache-control", "max-age=3600"}
};
cache.store("https://api.example.com/data", content, headers);

// Récupération
auto cached = cache.get("https://api.example.com/data");
if (cached.has_value()) {
    if (cached->is_stale) {
        // Contenu stale, lancer revalidation en arrière-plan
    }
    // Utiliser cached->content
}

// Headers conditionnels pour revalidation
auto conditional = cache.getConditionalHeaders("https://api.example.com/data");
// Retourne: {"If-None-Match": "\"abc123\""}
```

#### Métriques
```cpp
auto stats = cache.getStats();
// stats.hit_ratio, stats.cache_hits, stats.cache_misses
// stats.entries_count, stats.memory_usage_bytes
```

#### Cas d'Usage
- Réduction des requêtes redondantes
- Optimisation bande passante
- Respect des headers HTTP de cache
- Performance des scans répétés

---

### 5. 🧵 Pool de Threads (Thread Pool)

#### Fonctionnalité
Pool de threads haute performance avec queue de priorités, auto-scaling dynamique et gestion sophistiquée des tâches pour paralléliser efficacement les opérations.

#### Caractéristiques Techniques
- **Queue de Priorités** : LOW, NORMAL, HIGH, URGENT
- **Auto-Scaling** : Basé sur la charge (seuils: scale-up 80%, scale-down 20%)
- **Contrôle** : Pause/Resume, shutdown gracieux/forcé
- **Timeout** : Support timeout par tâche
- **Monitoring** : Statistiques temps réel
- **Thread-Safety** : Atomics et mutex pour coordination
- **Futures** : Support std::future pour récupération des résultats

#### Configuration
```cpp
BBP::ThreadPoolConfig config;
config.initial_threads = 4;
config.max_threads = 16;
config.min_threads = 2;
config.max_queue_size = 1000;
config.enable_auto_scaling = true;
config.idle_timeout = std::chrono::seconds(60);

BBP::ThreadPool pool(config);
```

#### Utilisation
```cpp
#include "infrastructure/threading/thread_pool.hpp"

BBP::ThreadPool pool;

// Tâche simple
auto future = pool.submit([]() {
    return 42;
});
int result = future.get();

// Tâche avec priorité
auto urgent_future = pool.submit(BBP::TaskPriority::URGENT, []() {
    // Tâche critique
});

// Tâche nommée pour debugging
auto named_future = pool.submitNamed("http_request", BBP::TaskPriority::HIGH, 
    [](const std::string& url) {
        return performHttpRequest(url);
    }, "https://example.com");

// Contrôle
pool.pause();   // Met en pause
pool.resume();  // Reprend
pool.waitForAll();  // Attend toutes les tâches
```

#### Callbacks et Monitoring
```cpp
// Callback sur completion des tâches
pool.setTaskCallback([](const std::string& name, bool success, std::chrono::milliseconds duration) {
    std::cout << "Tâche " << name << " terminée en " << duration.count() << "ms" << std::endl;
});

// Statistiques
auto stats = pool.getStats();
// stats.active_threads, stats.completed_tasks, stats.average_task_duration_ms
```

#### Cas d'Usage
- Parallélisation des requêtes HTTP
- Traitement concurrent des domaines
- Priorisation des tâches critiques (validation scope)
- Optimisation utilisation CPU/réseau

---

# 🇺🇸 ENGLISH DOCUMENTATION

## 📋 Overview

BB-Pipeline is a modular cybersecurity framework designed for ethical reconnaissance automation in bug bounty hunting. The framework follows a strict defensive approach with scope validation and safe-by-default operations.

### Modular Architecture

```
infrastructure/
├── logging/      - Structured logging system
├── networking/   - Rate limiting and HTTP cache
├── threading/    - High-performance thread pool  
├── config/       - Configuration management
└── system/       - System handlers (in progress)
```

---

## 🚀 Developed Features

### 1. 📝 Logger System

#### Functionality
Thread-safe logging system producing structured NDJSON logs with correlation ID support for distributed tracing.

#### Technical Characteristics
- **Pattern**: Thread-safe Singleton
- **Format**: NDJSON (Newline Delimited JSON)
- **Levels**: DEBUG, INFO, WARN, ERROR
- **Thread-Safety**: Mutex for concurrent access
- **Correlation IDs**: Auto-generated unique identifiers
- **Timestamps**: ISO 8601 UTC format

#### Usage
```cpp
#include "infrastructure/logging/logger.hpp"

auto& logger = BBP::Logger::getInstance();
logger.info("module", "Information message");
logger.warn("module", "Warning message");
logger.error("module", "Critical error");
```

#### Output Format
```json
{"timestamp":"2025-08-25T10:30:45.123Z","level":"INFO","message":"System startup","module":"core","correlation_id":"abc-123-def","thread_id":"0x7fff12345"}
```

#### Use Cases
- Pipeline module debugging
- Performance monitoring
- Security compliance audit trail
- Distributed event correlation

---

### 2. 🔄 Rate Limiter

#### Functionality
Per-domain rate limiting system using token bucket algorithm with adaptive exponential backoff to respect target server limits.

#### Technical Characteristics
- **Algorithm**: Token Bucket per domain
- **Backoff**: Adaptive exponential (initial: 100ms, max: 60s, multiplier: 2.0)
- **Granularity**: Per-domain with optional global rate
- **Persistence**: In-memory states with statistics
- **Thread-Safety**: Mutex for concurrent access

#### Configuration
```cpp
BBP::RateLimiter::getInstance().setBucketConfig("example.com", 10.0, 20.0);
// 10 requests/second with burst capacity of 20 requests
```

#### Usage
```cpp
#include "infrastructure/networking/rate_limiter.hpp"

auto& limiter = BBP::RateLimiter::getInstance();

// Try to acquire token
if (limiter.tryAcquire("example.com")) {
    // Make request
} else {
    // Wait or defer
}

// On server error (429, 503)
limiter.reportFailure("example.com");

// On success
limiter.reportSuccess("example.com");
```

#### Metrics
- Total/denied requests per domain
- Current backoff delay
- Available tokens
- Backoff trigger count

#### Use Cases
- API rate limit compliance
- 429 Too Many Requests protection
- Automatic throughput optimization
- IP blacklist avoidance

---

### 3. ⚙️ Configuration Manager

#### Functionality
Centralized YAML configuration management system with schema validation, template support, and environment variable expansion.

#### Technical Characteristics
- **Format**: YAML with strict validation
- **Templates**: Variable expansion `${VAR}` and `${VAR:default}`
- **Supported Types**: string, int, double, bool, arrays, objects
- **Validation**: Schemas with constraints (min/max, required, etc.)
- **Hot Reload**: Runtime reload with change notifications
- **Thread-Safety**: Mutex for concurrent modifications

#### Configuration Structure
```yaml
# configs/default.yaml
http:
  timeout: ${HTTP_TIMEOUT:30}
  user_agent: "BB-Pipeline/1.0"
  max_redirects: 5

rate_limits:
  default_rps: 10
  burst_capacity: 20
  
logging:
  level: ${LOG_LEVEL:INFO}
  format: "json"
```

#### Usage
```cpp
#include "infrastructure/config/config_manager.hpp"

auto& config = BBP::ConfigManager::getInstance();
config.loadFromFile("configs/default.yaml");

// Access values
int timeout = config.get<int>("http.timeout");
string user_agent = config.get<string>("http.user_agent");
```

#### Validation
```cpp
// Define constraints
config.setValidationRule("http.timeout", [](const auto& value) {
    return value.as<int>() >= 1 && value.as<int>() <= 300;
});
```

#### Use Cases
- Centralized module configuration
- Multi-environment support (dev/prod)
- Critical parameter validation
- Zero-downtime configuration updates

---

### 4. 💾 HTTP Cache System

#### Functionality
Intelligent HTTP cache with ETag/Last-Modified validation, sophisticated TTL management, and stale-while-revalidate support for network performance optimization.

#### Technical Characteristics
- **Validation**: ETag and Last-Modified per RFC 7234
- **TTL**: Configurable with min/max/default (default: 1h, max: 24h, min: 1min)
- **Eviction**: LRU (Least Recently Used)
- **Stale-While-Revalidate**: Serve stale content during revalidation
- **Compression**: zlib for memory optimization
- **Cleanup**: Automatic background cleanup (optional, 5min)
- **Thread-Safety**: Mutex for concurrent operations

#### Configuration
```cpp
BBP::CacheConfig config;
config.default_ttl = std::chrono::seconds(3600);  // 1 hour
config.max_entries = 10000;
config.enable_compression = true;
config.enable_stale_while_revalidate = true;
config.stale_max_age = std::chrono::seconds(7200);  // 2 hours

auto& cache = BBP::CacheSystem::getInstance();
cache.configure(config);
```

#### Usage
```cpp
#include "infrastructure/networking/cache_system.hpp"

auto& cache = BBP::CacheSystem::getInstance();

// Storage
std::unordered_map<string, string> headers = {
    {"etag", "\"abc123\""},
    {"cache-control", "max-age=3600"}
};
cache.store("https://api.example.com/data", content, headers);

// Retrieval
auto cached = cache.get("https://api.example.com/data");
if (cached.has_value()) {
    if (cached->is_stale) {
        // Stale content, trigger background revalidation
    }
    // Use cached->content
}

// Conditional headers for revalidation
auto conditional = cache.getConditionalHeaders("https://api.example.com/data");
// Returns: {"If-None-Match": "\"abc123\""}
```

#### Metrics
```cpp
auto stats = cache.getStats();
// stats.hit_ratio, stats.cache_hits, stats.cache_misses
// stats.entries_count, stats.memory_usage_bytes
```

#### Use Cases
- Redundant request reduction
- Bandwidth optimization
- HTTP cache header compliance
- Repeated scan performance

---

### 5. 🧵 Thread Pool

#### Functionality
High-performance thread pool with priority queue, dynamic auto-scaling, and sophisticated task management for efficient operation parallelization.

#### Technical Characteristics
- **Priority Queue**: LOW, NORMAL, HIGH, URGENT
- **Auto-Scaling**: Load-based (thresholds: scale-up 80%, scale-down 20%)
- **Control**: Pause/Resume, graceful/forced shutdown
- **Timeout**: Per-task timeout support
- **Monitoring**: Real-time statistics
- **Thread-Safety**: Atomics and mutex for coordination
- **Futures**: std::future support for result retrieval

#### Configuration
```cpp
BBP::ThreadPoolConfig config;
config.initial_threads = 4;
config.max_threads = 16;
config.min_threads = 2;
config.max_queue_size = 1000;
config.enable_auto_scaling = true;
config.idle_timeout = std::chrono::seconds(60);

BBP::ThreadPool pool(config);
```

#### Usage
```cpp
#include "infrastructure/threading/thread_pool.hpp"

BBP::ThreadPool pool;

// Simple task
auto future = pool.submit([]() {
    return 42;
});
int result = future.get();

// Task with priority
auto urgent_future = pool.submit(BBP::TaskPriority::URGENT, []() {
    // Critical task
});

// Named task for debugging
auto named_future = pool.submitNamed("http_request", BBP::TaskPriority::HIGH, 
    [](const std::string& url) {
        return performHttpRequest(url);
    }, "https://example.com");

// Control
pool.pause();       // Pause execution
pool.resume();      // Resume execution  
pool.waitForAll();  // Wait for all tasks
```

#### Callbacks and Monitoring
```cpp
// Task completion callback
pool.setTaskCallback([](const std::string& name, bool success, std::chrono::milliseconds duration) {
    std::cout << "Task " << name << " completed in " << duration.count() << "ms" << std::endl;
});

// Statistics
auto stats = pool.getStats();
// stats.active_threads, stats.completed_tasks, stats.average_task_duration_ms
```

#### Use Cases
- HTTP request parallelization
- Concurrent domain processing
- Critical task prioritization (scope validation)
- CPU/network utilization optimization

---

## 🔧 Intégration et Utilisation Globale / Integration and Global Usage

### Exemple d'Utilisation Combinée / Combined Usage Example
```cpp
#include "infrastructure/logging/logger.hpp"
#include "infrastructure/config/config_manager.hpp" 
#include "infrastructure/networking/rate_limiter.hpp"
#include "infrastructure/networking/cache_system.hpp"
#include "infrastructure/threading/thread_pool.hpp"

int main() {
    // 1. Configuration
    auto& config = BBP::ConfigManager::getInstance();
    config.loadFromFile("configs/default.yaml");
    
    // 2. Logging
    auto& logger = BBP::Logger::getInstance();
    logger.info("main", "Starting BB-Pipeline");
    
    // 3. Rate Limiter
    auto& limiter = BBP::RateLimiter::getInstance();
    double rps = config.get<double>("rate_limits.default_rps");
    limiter.setBucketConfig("example.com", rps);
    
    // 4. Cache
    auto& cache = BBP::CacheSystem::getInstance();
    
    // 5. Thread Pool
    BBP::ThreadPool pool;
    
    // Parallel scan task
    std::vector<std::string> domains = {"sub1.example.com", "sub2.example.com"};
    
    for (const auto& domain : domains) {
        pool.submit(BBP::TaskPriority::NORMAL, [&](const std::string& d) {
            if (limiter.tryAcquire(d)) {
                // Check cache
                auto cached = cache.get("https://" + d);
                if (!cached.has_value()) {
                    // Make HTTP request
                    logger.info("scan", "Scanning " + d);
                }
            }
        }, domain);
    }
    
    pool.waitForAll();
    logger.info("main", "Scan completed");
    return 0;
}
```

---

## 📊 Performance et Statistiques / Performance and Statistics

### Métriques Disponibles / Available Metrics

#### Logger System
- Messages par seconde par niveau / Messages per second by level
- Taille moyenne des messages / Average message size  
- Latence d'écriture / Write latency

#### Rate Limiter  
- Requêtes acceptées/refusées par domaine / Accepted/denied requests per domain
- Délai de backoff moyen / Average backoff delay
- Efficacité du token bucket / Token bucket efficiency

#### Cache System
- Taux de hit/miss / Hit/miss ratio
- Utilisation mémoire / Memory usage
- Latence de lookup / Lookup latency
- Nombre d'évictions LRU / LRU eviction count

#### Thread Pool
- Utilisation des threads (actifs/idle) / Thread utilization (active/idle)
- Durée moyenne des tâches / Average task duration
- Taille de la queue / Queue size
- Throughput (tâches/seconde) / Throughput (tasks/second)

### Recommandations d'Usage / Usage Recommendations

1. **Production** : Activer toutes les métriques pour monitoring / Enable all metrics for monitoring
2. **Development** : Logger en mode DEBUG pour troubleshooting / Logger in DEBUG mode for troubleshooting
3. **Performance** : Ajuster les pools selon la charge / Adjust pools based on load
4. **Sécurité** : Valider toutes les configurations avant déploiement / Validate all configurations before deployment

---

## 🔮 Prochaines Fonctionnalités / Next Features

- **Signal Handler** : Arrêt propre avec flush CSV garanti / Clean shutdown with guaranteed CSV flush
- **Memory Manager** : Pool allocator optimisé / Optimized pool allocator
- **Error Recovery** : Retry automatique avec backoff / Automatic retry with backoff
- **CSV Engine** : Parser/Writer haute performance / High-performance Parser/Writer
- **Pipeline Orchestrator** : Logique bbpctl complète / Complete bbpctl logic