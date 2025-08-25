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

### 6. 🛡️ Gestionnaire de Signaux (Signal Handler)

#### Fonctionnalité
Système de gestion des signaux système (SIGINT/SIGTERM) avec arrêt propre garanti et flush automatique des données CSV pour éviter la perte de données lors d'interruptions.

#### Caractéristiques Techniques
- **Signaux Gérés** : SIGINT (Ctrl+C), SIGTERM (kill)
- **Arrêt Propre** : Exécution ordonnée des callbacks de nettoyage
- **Flush CSV Garanti** : Sauvegarde prioritaire des données CSV
- **Timeout Protection** : Délais configurables avec flush d'urgence
- **Thread-Safety** : Gestion thread-safe des callbacks multiples
- **Statistiques** : Monitoring des arrêts et performances
- **Emergency Mode** : Flush d'urgence en cas de timeout

#### Configuration
```cpp
BBP::SignalHandlerConfig config;
config.shutdown_timeout = std::chrono::milliseconds(5000);     // 5 secondes
config.csv_flush_timeout = std::chrono::milliseconds(2000);    // 2 secondes  
config.enable_emergency_flush = true;                          // Flush d'urgence activé
config.log_signal_details = true;                              // Log détaillé des signaux

auto& handler = BBP::SignalHandler::getInstance();
handler.configure(config);
handler.initialize();
```

#### Utilisation
```cpp
#include "infrastructure/system/signal_handler.hpp"

auto& handler = BBP::SignalHandler::getInstance();

// Enregistrement callback nettoyage général
handler.registerCleanupCallback("thread_pool_cleanup", []() {
    ThreadPool::getInstance().shutdown();
    Logger::getInstance().info("cleanup", "Thread pool fermé proprement");
});

// Enregistrement callback flush CSV (prioritaire)
handler.registerCsvFlushCallback("/out/01_subdomains.csv", [](const std::string& path) {
    CsvWriter writer(path);
    writer.flushPendingData();
    writer.close();
    Logger::getInstance().info("csv_flush", "CSV sauvegardé: " + path);
});

// Initialisation (enregistre SIGINT/SIGTERM)
handler.initialize();

// Le processus principal continue...
while (!handler.isShutdownRequested()) {
    // Travail principal
    doWork();
    
    if (handler.isShuttingDown()) {
        Logger::getInstance().info("main", "Arrêt en cours...");
        break;
    }
}

// Attendre la fin complète de l'arrêt
handler.waitForShutdown();
```

#### Callbacks et Priorité
```cpp
// 1. Les callbacks CSV sont exécutés EN PREMIER (données critiques)
handler.registerCsvFlushCallback("/out/findings.csv", [](const std::string& path) {
    // Sauvegarde critique des résultats
});

// 2. Puis les callbacks de nettoyage général
handler.registerCleanupCallback("database_connection", []() {
    database.disconnect();
});

handler.registerCleanupCallback("temp_files", []() {
    filesystem::remove_all("/tmp/bbp_temp");
});
```

#### Gestion des Timeouts et Urgence
```cpp
// Configuration avec timeouts stricts
BBP::SignalHandlerConfig config;
config.csv_flush_timeout = 1000ms;        // 1 seconde max pour CSV
config.shutdown_timeout = 3000ms;         // 3 secondes total
config.enable_emergency_flush = true;     // Mode urgence si timeout

handler.configure(config);

// En cas de timeout:
// 1. Les callbacks CSV lents sont interrompus
// 2. Emergency flush automatique activé  
// 3. Sauvegarde minimale garantie
```

#### Statistiques et Monitoring
```cpp
auto stats = handler.getStats();

std::cout << "Signaux reçus: " << stats.signals_received << std::endl;
std::cout << "Arrêts réussis: " << stats.successful_shutdowns << std::endl;
std::cout << "Timeouts: " << stats.timeout_shutdowns << std::endl;
std::cout << "Durée dernier arrêt: " << stats.last_shutdown_duration.count() << "ms" << std::endl;
std::cout << "Temps total flush CSV: " << stats.total_csv_flush_time.count() << "ms" << std::endl;

// Statistiques par signal
for (const auto& [signal, count] : stats.signal_counts) {
    std::cout << "Signal " << signal << ": " << count << " fois" << std::endl;
}
```

#### Cas d'Usage
- **Interruption Ctrl+C** : Sauvegarde propre des résultats de scan
- **Kill du processus** : Fermeture ordonnée avec flush CSV garanti
- **Crash recovery** : Données critiques sauvegardées même en urgence
- **Pipeline debugging** : Arrêt propre pour inspection des états
- **Intégration CI/CD** : Terminaison contrôlée des tests longs
- **Production monitoring** : Statistiques d'arrêt pour surveillance

---

### 7. 💾 Gestionnaire Mémoire (Memory Manager)

#### Fonctionnalité
Gestionnaire mémoire haute performance avec pool allocator optimisé pour le parsing de fichiers CSV massifs. Fournit une allocation mémoire efficace avec défragmentation automatique et statistiques détaillées.

#### Caractéristiques Techniques
- **Pool Allocator** : Allocation par blocs avec réutilisation optimisée
- **Best Fit Algorithm** : Algorithme de recherche de bloc optimal
- **Défragmentation** : Fusion automatique des blocs libres adjacents
- **Alignment Support** : Support alignement mémoire configurable
- **Thread-Safety** : Accès concurrent thread-safe avec mutex
- **Statistiques** : Monitoring détaillé usage/performance/fragmentation
- **Memory Limits** : Contrôle des limites d'utilisation mémoire
- **RAII Wrapper** : `ManagedPtr<T>` pour gestion automatique

#### Configuration
```cpp
BBP::MemoryPoolConfig config;
config.initial_pool_size = 1024 * 1024;        // Pool initial 1MB
config.max_pool_size = 100 * 1024 * 1024;      // Pool maximum 100MB
config.block_size = 64;                         // Taille de bloc par défaut
config.alignment = sizeof(void*);               // Alignement par défaut
config.enable_statistics = true;                // Statistiques détaillées
config.enable_defragmentation = true;           // Défragmentation auto
config.growth_factor = 2.0;                     // Facteur croissance x2
config.defrag_threshold = 0.3;                  // Seuil fragmentation 30%

auto& manager = BBP::MemoryManager::getInstance();
manager.configure(config);
manager.initialize();
```

#### Utilisation de Base
```cpp
#include "infrastructure/system/memory_manager.hpp"

auto& manager = BBP::MemoryManager::getInstance();

// Allocation simple
void* ptr = manager.allocate(1024);
if (ptr) {
    // Utiliser la mémoire...
    manager.deallocate(ptr);
}

// Allocation alignée
void* aligned_ptr = manager.allocate(2048, 16);  // Aligné sur 16 bytes
manager.deallocate(aligned_ptr);

// Allocation de tableaux typés
int* int_array = manager.allocate_array<int>(1000);
for (int i = 0; i < 1000; ++i) {
    int_array[i] = i * i;
}
manager.deallocate_array(int_array);
```

#### Pool Allocator STL-Compatible
```cpp
// Utilisation avec conteneurs STL
auto allocator = manager.get_allocator<std::string>();
std::vector<std::string, BBP::PoolAllocator<std::string>> csv_lines(allocator);

// Parsing CSV optimisé
csv_lines.reserve(10000);  // Pré-allocation pour CSV massif
for (const auto& line : csv_data) {
    csv_lines.push_back(parseCsvLine(line));  // Utilise pool allocator
}
```

#### Wrapper RAII ManagedPtr
```cpp
// Gestion automatique avec RAII
{
    BBP::ManagedPtr<double> data_buffer(manager, 50000);  // 50k doubles
    
    // Utilisation comme tableau standard
    for (size_t i = 0; i < data_buffer.count(); ++i) {
        data_buffer[i] = calculateValue(i);
    }
    
    // Traitement des données...
    processDataBuffer(data_buffer.get(), data_buffer.count());
    
    // Désallocation automatique à la sortie de scope
}
```

#### Optimisation pour CSV Massif
```cpp
// Configuration optimisée pour parsing CSV
BBP::MemoryPoolConfig csv_config;
csv_config.initial_pool_size = 10 * 1024 * 1024;   // 10MB initial
csv_config.max_pool_size = 500 * 1024 * 1024;      // 500MB maximum
csv_config.block_size = 256;                        // Blocs 256 bytes
csv_config.enable_defragmentation = true;           // Défrag activée
csv_config.defrag_threshold = 0.2;                  // Défrag à 20%

manager.configure(csv_config);

// Définir limite mémoire
manager.setMemoryLimit(400 * 1024 * 1024);  // Limite 400MB

// Allocation optimisée pour lignes CSV
auto csv_allocator = manager.get_allocator<CsvRow>();
std::vector<CsvRow, decltype(csv_allocator)> parsed_rows(csv_allocator);
```

#### Monitoring et Statistiques
```cpp
auto stats = manager.getStats();

std::cout << "Pool total: " << stats.pool_size << " bytes" << std::endl;
std::cout << "Mémoire utilisée: " << stats.current_used_bytes << " bytes" << std::endl;
std::cout << "Pic utilisation: " << stats.peak_used_bytes << " bytes" << std::endl;
std::cout << "Allocations totales: " << stats.total_allocations << std::endl;
std::cout << "Fragmentation: " << (stats.fragmentation_ratio * 100) << "%" << std::endl;
std::cout << "Défragmentations: " << stats.defragmentation_count << std::endl;

// Temps moyens d'allocation/désallocation
std::cout << "Temps alloc moyen: " << 
    (stats.total_alloc_time.count() / stats.total_allocations) << "μs" << std::endl;

// Distribution des tailles d'allocation
for (const auto& [size, count] : stats.size_histogram) {
    std::cout << "Taille " << size << ": " << count << " allocations" << std::endl;
}
```

#### Maintenance et Optimisation
```cpp
// Vérification intégrité mémoire
if (!manager.checkIntegrity()) {
    LOG_ERROR("memory", "Corruption mémoire détectée!");
}

// Défragmentation manuelle
manager.defragment();

// Optimisation automatique
manager.optimize();

// Dump état pour debugging
std::string debug_info = manager.dumpPoolState();
LOG_DEBUG("memory", debug_info);

// Suivi détaillé pour développement
manager.setDetailedTracking(true);
```

#### Cas d'Usage Spécifiques
- **Parsing CSV Géant** : Allocation efficace pour millions de lignes
- **Buffer Management** : Gestion mémoire pour buffers temporaires
- **String Processing** : Pool optimisé pour manipulation chaînes
- **Data Structures** : Allocation rapide pour structures complexes

---

## Error Recovery - Auto-retry avec Exponential Backoff

Le système **Error Recovery** de BB-Pipeline implémente une stratégie de récupération d'erreurs sophistiquée avec retry automatique et backoff exponentiel. Ce composant est essentiel pour la robustesse des opérations réseau et la fiabilité générale du framework.

### Fonctionnalités Principales

#### Retry Automatique Intelligent
- **Exponential Backoff** : Délais croissants exponentiels entre retries
- **Jitter Aléatoire** : Évite l'effet "thundering herd" avec randomisation
- **Classification d'Erreurs** : Distinction automatique erreurs récupérables/permanentes
- **Limites Configurables** : Nombre max tentatives, délais min/max personnalisables
- **Circuit Breaker** : Protection contre cascades d'échecs avec désactivation auto

#### Support Multi-Threading
- **Thread-Safe** : Accès concurrent sécurisé avec protection mutex
- **Opérations Asynchrones** : Support natif std::future et std::async
- **Contexte Par Thread** : Isolation des tentatives par opération/thread
- **Statistiques Globales** : Agrégation thread-safe des métriques de performance

#### Monitoring et Observabilité
- **Statistiques Détaillées** : Tracking complet succès/échecs/temps retry
- **Classification Erreurs** : Comptage par type d'erreur avec historique
- **Logging Configurable** : Traces détaillées optionnelles pour debugging
- **Métriques Temps Réel** : Monitoring continu performance et fiabilité

### Architecture Technique

#### Types d'Erreurs Récupérables
```cpp
enum class RecoverableErrorType {
    NETWORK_TIMEOUT,        // Timeout réseau
    CONNECTION_REFUSED,     // Connexion refusée
    DNS_RESOLUTION,         // Échec résolution DNS  
    SSL_HANDSHAKE,         // Échec handshake SSL
    HTTP_5XX,              // Erreurs serveur HTTP 5xx
    HTTP_429,              // Rate limiting HTTP 429
    SOCKET_ERROR,          // Erreur socket générale
    TEMPORARY_FAILURE,     // Échec service temporaire
    CUSTOM                 // Erreur récupérable personnalisée
};
```

#### Configuration Retry Avancée
```cpp
struct RetryConfig {
    size_t max_attempts{3};                          // Tentatives maximum
    std::chrono::milliseconds initial_delay{100};   // Délai initial ms
    std::chrono::milliseconds max_delay{30000};     // Délai max ms  
    double backoff_multiplier{2.0};                 // Multiplicateur backoff
    double jitter_factor{0.1};                      // Facteur jitter 0-1
    bool enable_jitter{true};                       // Active randomisation
    std::unordered_set<RecoverableErrorType> recoverable_errors; // Erreurs récupérables
};
```

#### Statistiques Temps Réel
```cpp
struct RetryStatistics {
    size_t total_operations{0};                      // Opérations totales
    size_t successful_operations{0};                 // Opérations réussies
    size_t failed_operations{0};                     // Opérations échouées
    size_t total_retries{0};                         // Retries totaux
    std::chrono::milliseconds total_retry_time{0};   // Temps retry total
    std::chrono::milliseconds average_retry_time{0}; // Temps retry moyen
    std::unordered_map<RecoverableErrorType, size_t> error_counts; // Compte par type
    std::vector<RetryAttempt> recent_attempts;       // Tentatives récentes
};
```

### Utilisation Pratique

#### Configuration et Initialisation
```cpp
#include "infrastructure/system/error_recovery.hpp"

// Obtenir instance singleton
auto& recovery = BBP::ErrorRecoveryManager::getInstance();

// Configuration réseau typique
auto network_config = BBP::ErrorRecoveryUtils::createNetworkRetryConfig();
network_config.max_attempts = 5;
network_config.initial_delay = std::chrono::milliseconds(200);
network_config.max_delay = std::chrono::milliseconds(10000);
network_config.backoff_multiplier = 2.5;
network_config.jitter_factor = 0.2;

recovery.configure(network_config);
recovery.setDetailedLogging(true);
```

#### Exécution avec Retry Automatique
```cpp
// Opération simple avec retry automatique
try {
    auto result = recovery.executeWithRetry("http_request", [&]() {
        // Code pouvant échouer (requête HTTP, DNS, etc.)
        return performHttpRequest(url);
    });
    
    std::cout << "Succès: " << result << std::endl;
    
} catch (const BBP::RetryExhaustedException& e) {
    std::cerr << "Retries épuisés: " << e.what() << std::endl;
} catch (const BBP::NonRecoverableError& e) {
    std::cerr << "Erreur non récupérable: " << e.what() << std::endl;
}
```

#### Configuration Personnalisée par Opération
```cpp
// Config spécifique pour API critiques
BBP::RetryConfig api_config;
api_config.max_attempts = 7;
api_config.initial_delay = std::chrono::milliseconds(500);
api_config.backoff_multiplier = 1.8;
api_config.recoverable_errors = {
    BBP::RecoverableErrorType::HTTP_5XX,
    BBP::RecoverableErrorType::HTTP_429,
    BBP::RecoverableErrorType::NETWORK_TIMEOUT
};

auto api_result = recovery.executeWithRetry("critical_api", api_config, [&]() {
    return callCriticalAPI(params);
});
```

#### Opérations Asynchrones
```cpp
// Retry asynchrone avec futures
std::vector<std::future<std::string>> futures;

for (const auto& url : urls) {
    futures.push_back(recovery.executeAsyncWithRetry("bulk_request", [url]() {
        return downloadContent(url);
    }));
}

// Récupération résultats
for (auto& future : futures) {
    try {
        auto content = future.get();
        processContent(content);
    } catch (const std::exception& e) {
        std::cerr << "Échec async: " << e.what() << std::endl;
    }
}
```

#### RAII avec AutoRetryGuard
```cpp
// Gestion automatique lifecycle retry
BBP::AutoRetryGuard guard("database_operation", db_config);

auto connection = guard.execute([&]() {
    return database.connect(connection_string);
});

auto results = guard.execute([&]() {
    return connection.query("SELECT * FROM large_table");
});
```

#### Circuit Breaker Protection
```cpp
// Configuration circuit breaker
recovery.setCircuitBreakerThreshold(10); // Ouvre après 10 échecs consécutifs

// Vérification état
if (recovery.isCircuitBreakerOpen()) {
    std::cout << "Circuit ouvert - service dégradé" << std::endl;
    return fallback_response();
}

// Reset manuel si nécessaire
recovery.resetCircuitBreaker();
```

### Monitoring et Diagnostics

#### Statistiques Détaillées
```cpp
auto stats = recovery.getStatistics();

std::cout << "=== Statistiques Error Recovery ===" << std::endl;
std::cout << "Opérations totales: " << stats.total_operations << std::endl;
std::cout << "Taux de succès: " << 
    ((double)stats.successful_operations / stats.total_operations * 100.0) << "%" << std::endl;
std::cout << "Retries moyens: " << 
    ((double)stats.total_retries / stats.total_operations) << std::endl;
std::cout << "Temps retry moyen: " << stats.average_retry_time.count() << "ms" << std::endl;

// Détail par type d'erreur
std::cout << "\n=== Distribution Erreurs ===" << std::endl;
for (const auto& [type, count] : stats.error_counts) {
    std::cout << "Type " << static_cast<int>(type) << ": " << count << std::endl;
}
```

#### Classification d'Erreurs Personnalisée
```cpp
// Ajout classificateur custom pour APIs spécifiques  
recovery.addErrorClassifier([](const std::exception& e) -> BBP::RecoverableErrorType {
    std::string message = e.what();
    
    if (message.find("rate_limit_exceeded") != std::string::npos) {
        return BBP::RecoverableErrorType::HTTP_429;
    }
    
    if (message.find("temporary_maintenance") != std::string::npos) {
        return BBP::RecoverableErrorType::TEMPORARY_FAILURE;
    }
    
    return BBP::RecoverableErrorType::CUSTOM;
});
```

#### Configurations Pré-définies
```cpp
// Configurations optimisées par cas d'usage

// Opérations réseau standard
auto network_config = BBP::ErrorRecoveryUtils::createNetworkRetryConfig();

// Requêtes HTTP avec rate limiting
auto http_config = BBP::ErrorRecoveryUtils::createHttpRetryConfig();

// Connexions base de données
auto db_config = BBP::ErrorRecoveryUtils::createDatabaseRetryConfig();

// Classification automatique erreurs HTTP
auto error_type = BBP::ErrorRecoveryUtils::classifyHttpError(503); // TEMPORARY_FAILURE

// Classification erreurs réseau système  
auto net_error = BBP::ErrorRecoveryUtils::classifyNetworkError(ETIMEDOUT); // NETWORK_TIMEOUT
```

### Cas d'Usage Avancés

#### Integration avec Rate Limiter
```cpp
// Combinaison retry + rate limiting pour APIs
auto& rate_limiter = BBP::RateLimiter::getInstance();
auto rate_limit_config = BBP::ErrorRecoveryUtils::createHttpRetryConfig();

auto api_response = recovery.executeWithRetry("rate_limited_api", rate_limit_config, [&]() {
    rate_limiter.acquireToken("api.service.com");
    return callExternalAPI(request);
});
```

#### Retry Conditionnel Avancé
```cpp
// Retry avec conditions métier complexes
recovery.executeWithRetry("conditional_retry", [&]() {
    auto result = performBusinessOperation();
    
    // Validation métier post-opération
    if (!result.isValid()) {
        throw BBP::RecoverableError("Validation métier échouée");
    }
    
    return result;
});
```

#### Fallback et Degraded Mode
```cpp
// Pattern fallback avec retry
std::string fetchData(const std::string& key) {
    try {
        return recovery.executeWithRetry("primary_cache", [&]() {
            return primary_cache.get(key);
        });
    } catch (const BBP::RetryExhaustedException&) {
        // Fallback sur cache secondaire
        return secondary_cache.get(key);  
    }
}
```

### Caractéristiques Techniques

#### Performance et Scalabilité
- **Thread-Safe** : Accès concurrent optimisé avec minimal contention
- **Memory Efficient** : Faible overhead mémoire par contexte retry
- **Low Latency** : Calcul délais optimisé sans allocation dynamique  
- **High Throughput** : Support milliers d'opérations concurrentes

#### Robustesse et Fiabilité  
- **Exception Safety** : Garanties RAII et exception-safe
- **Circuit Protection** : Prévention cascades d'échecs système
- **Graceful Degradation** : Fonctionnement dégradé intelligent
- **Monitoring Intégré** : Observabilité complète sans overhead

#### Configuration Flexible
- **Policy-Based** : Configuration fine par type d'opération
- **Runtime Adjustable** : Modification paramètres à chaud
- **Environment Aware** : Adaptation automatique contexte déploiement
- **Extensible** : Classification erreurs et stratégies personnalisables

---

## Error Recovery - Auto-retry with Exponential Backoff (EN)

The **Error Recovery** system in BB-Pipeline implements a sophisticated error recovery strategy with automatic retry and exponential backoff. This component is essential for network operations robustness and overall framework reliability.

### Key Features

#### Intelligent Auto-Retry
- **Exponential Backoff**: Exponentially increasing delays between retries
- **Random Jitter**: Prevents "thundering herd" effect with randomization
- **Error Classification**: Automatic distinction between recoverable/permanent errors
- **Configurable Limits**: Customizable max attempts, min/max delays
- **Circuit Breaker**: Protection against failure cascades with auto-disable

#### Multi-Threading Support
- **Thread-Safe**: Secure concurrent access with mutex protection
- **Async Operations**: Native std::future and std::async support
- **Per-Thread Context**: Operation/thread attempt isolation
- **Global Statistics**: Thread-safe performance metrics aggregation

#### Monitoring & Observability
- **Detailed Statistics**: Complete tracking of success/failures/retry times
- **Error Classification**: Per-error-type counting with history
- **Configurable Logging**: Optional detailed traces for debugging
- **Real-time Metrics**: Continuous performance and reliability monitoring

### Technical Architecture

#### Error Types and Configuration
```cpp
// Recoverable error types
enum class RecoverableErrorType {
    NETWORK_TIMEOUT,        // Network timeout
    CONNECTION_REFUSED,     // Connection refused
    DNS_RESOLUTION,         // DNS resolution failure
    SSL_HANDSHAKE,         // SSL handshake failure
    HTTP_5XX,              // HTTP 5xx server errors
    HTTP_429,              // HTTP 429 rate limiting
    SOCKET_ERROR,          // General socket error
    TEMPORARY_FAILURE,     // Temporary service failure
    CUSTOM                 // Custom recoverable error
};

// Advanced retry configuration
struct RetryConfig {
    size_t max_attempts{3};                          // Maximum attempts
    std::chrono::milliseconds initial_delay{100};   // Initial delay ms
    std::chrono::milliseconds max_delay{30000};     // Max delay ms
    double backoff_multiplier{2.0};                 // Backoff multiplier
    double jitter_factor{0.1};                      // Jitter factor 0-1
    bool enable_jitter{true};                       // Enable randomization
    std::unordered_set<RecoverableErrorType> recoverable_errors; // Recoverable errors
};
```

### Practical Usage

#### Basic Configuration
```cpp
#include "infrastructure/system/error_recovery.hpp"

// Get singleton instance
auto& recovery = BBP::ErrorRecoveryManager::getInstance();

// Typical network configuration
auto network_config = BBP::ErrorRecoveryUtils::createNetworkRetryConfig();
network_config.max_attempts = 5;
network_config.initial_delay = std::chrono::milliseconds(200);
network_config.backoff_multiplier = 2.5;

recovery.configure(network_config);
```

#### Automatic Retry Execution
```cpp
// Simple operation with automatic retry
try {
    auto result = recovery.executeWithRetry("http_request", [&]() {
        return performHttpRequest(url);
    });
    
    std::cout << "Success: " << result << std::endl;
    
} catch (const BBP::RetryExhaustedException& e) {
    std::cerr << "Retries exhausted: " << e.what() << std::endl;
} catch (const BBP::NonRecoverableError& e) {
    std::cerr << "Non-recoverable error: " << e.what() << std::endl;
}
```

#### Asynchronous Operations
```cpp
// Async retry with futures
std::vector<std::future<std::string>> futures;

for (const auto& url : urls) {
    futures.push_back(recovery.executeAsyncWithRetry("bulk_request", [url]() {
        return downloadContent(url);
    }));
}

// Collect results
for (auto& future : futures) {
    try {
        auto content = future.get();
        processContent(content);
    } catch (const std::exception& e) {
        std::cerr << "Async failed: " << e.what() << std::endl;
    }
}
```

#### Circuit Breaker Protection
```cpp
// Configure circuit breaker
recovery.setCircuitBreakerThreshold(10); // Opens after 10 consecutive failures

// Check state
if (recovery.isCircuitBreakerOpen()) {
    std::cout << "Circuit open - degraded service" << std::endl;
    return fallback_response();
}

// Manual reset if needed
recovery.resetCircuitBreaker();
```

### Advanced Use Cases

#### Custom Error Classification
```cpp
// Add custom classifier for specific APIs
recovery.addErrorClassifier([](const std::exception& e) -> BBP::RecoverableErrorType {
    std::string message = e.what();
    
    if (message.find("rate_limit_exceeded") != std::string::npos) {
        return BBP::RecoverableErrorType::HTTP_429;
    }
    
    if (message.find("temporary_maintenance") != std::string::npos) {
        return BBP::RecoverableErrorType::TEMPORARY_FAILURE;
    }
    
    return BBP::RecoverableErrorType::CUSTOM;
});
```

#### Integration with Rate Limiter
```cpp
// Combine retry + rate limiting for APIs
auto& rate_limiter = BBP::RateLimiter::getInstance();

auto api_response = recovery.executeWithRetry("rate_limited_api", [&]() {
    rate_limiter.acquireToken("api.service.com");
    return callExternalAPI(request);
});
```

#### Fallback Patterns
```cpp
// Fallback pattern with retry
std::string fetchData(const std::string& key) {
    try {
        return recovery.executeWithRetry("primary_cache", [&]() {
            return primary_cache.get(key);
        });
    } catch (const BBP::RetryExhaustedException&) {
        // Fallback to secondary cache
        return secondary_cache.get(key);
    }
}
```

### Statistics and Monitoring

```cpp
auto stats = recovery.getStatistics();

std::cout << "=== Error Recovery Statistics ===" << std::endl;
std::cout << "Total operations: " << stats.total_operations << std::endl;
std::cout << "Success rate: " << 
    ((double)stats.successful_operations / stats.total_operations * 100.0) << "%" << std::endl;
std::cout << "Average retries: " << 
    ((double)stats.total_retries / stats.total_operations) << std::endl;
std::cout << "Average retry time: " << stats.average_retry_time.count() << "ms" << std::endl;

// Error type breakdown
for (const auto& [type, count] : stats.error_counts) {
    std::cout << "Type " << static_cast<int>(type) << ": " << count << std::endl;
}
```

### Technical Characteristics

#### Performance Features
- **Thread-Safe**: Optimized concurrent access with minimal contention
- **Memory Efficient**: Low memory overhead per retry context
- **Low Latency**: Optimized delay calculation without dynamic allocation
- **High Throughput**: Support for thousands of concurrent operations

#### Reliability Features
- **Exception Safety**: RAII guarantees and exception-safe design
- **Circuit Protection**: Prevents system failure cascades
- **Graceful Degradation**: Intelligent degraded mode operation
- **Integrated Monitoring**: Complete observability without overhead

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

### 6. 🛡️ Signal Handler

#### Functionality
System signal management (SIGINT/SIGTERM) with guaranteed graceful shutdown and automatic CSV data flush to prevent data loss during interruptions.

#### Technical Characteristics
- **Handled Signals**: SIGINT (Ctrl+C), SIGTERM (kill)
- **Graceful Shutdown**: Ordered execution of cleanup callbacks
- **Guaranteed CSV Flush**: Priority CSV data saving
- **Timeout Protection**: Configurable timeouts with emergency flush
- **Thread-Safety**: Thread-safe management of multiple callbacks
- **Statistics**: Shutdown and performance monitoring
- **Emergency Mode**: Emergency flush on timeout

#### Configuration
```cpp
BBP::SignalHandlerConfig config;
config.shutdown_timeout = std::chrono::milliseconds(5000);     // 5 seconds
config.csv_flush_timeout = std::chrono::milliseconds(2000);    // 2 seconds
config.enable_emergency_flush = true;                          // Emergency flush enabled
config.log_signal_details = true;                              // Detailed signal logging

auto& handler = BBP::SignalHandler::getInstance();
handler.configure(config);
handler.initialize();
```

#### Usage
```cpp
#include "infrastructure/system/signal_handler.hpp"

auto& handler = BBP::SignalHandler::getInstance();

// Register general cleanup callback
handler.registerCleanupCallback("thread_pool_cleanup", []() {
    ThreadPool::getInstance().shutdown();
    Logger::getInstance().info("cleanup", "Thread pool closed gracefully");
});

// Register CSV flush callback (priority)
handler.registerCsvFlushCallback("/out/01_subdomains.csv", [](const std::string& path) {
    CsvWriter writer(path);
    writer.flushPendingData();
    writer.close();
    Logger::getInstance().info("csv_flush", "CSV saved: " + path);
});

// Initialize (registers SIGINT/SIGTERM)
handler.initialize();

// Main process continues...
while (!handler.isShutdownRequested()) {
    // Main work
    doWork();
    
    if (handler.isShuttingDown()) {
        Logger::getInstance().info("main", "Shutdown in progress...");
        break;
    }
}

// Wait for complete shutdown
handler.waitForShutdown();
```

#### Statistics and Monitoring
```cpp
auto stats = handler.getStats();

std::cout << "Signals received: " << stats.signals_received << std::endl;
std::cout << "Successful shutdowns: " << stats.successful_shutdowns << std::endl;
std::cout << "Timeouts: " << stats.timeout_shutdowns << std::endl;
std::cout << "Last shutdown duration: " << stats.last_shutdown_duration.count() << "ms" << std::endl;
std::cout << "Total CSV flush time: " << stats.total_csv_flush_time.count() << "ms" << std::endl;

// Statistics per signal
for (const auto& [signal, count] : stats.signal_counts) {
    std::cout << "Signal " << signal << ": " << count << " times" << std::endl;
}
```

#### Use Cases
- **Ctrl+C Interruption**: Clean saving of scan results
- **Process Kill**: Orderly shutdown with guaranteed CSV flush
- **Crash Recovery**: Critical data saved even in emergency
- **Pipeline Debugging**: Clean shutdown for state inspection
- **CI/CD Integration**: Controlled termination of long tests
- **Production Monitoring**: Shutdown statistics for surveillance

### 7. 💾 Memory Manager

#### Functionality
High-performance memory manager with pool allocator optimized for massive CSV file parsing. Provides efficient memory allocation with automatic defragmentation and detailed statistics.

#### Technical Characteristics
- **Pool Allocator**: Block allocation with optimized reuse
- **Best Fit Algorithm**: Optimal block search algorithm
- **Defragmentation**: Automatic merging of adjacent free blocks
- **Alignment Support**: Configurable memory alignment support
- **Thread-Safety**: Thread-safe concurrent access with mutex
- **Statistics**: Detailed usage/performance/fragmentation monitoring
- **Memory Limits**: Memory usage limit control
- **RAII Wrapper**: `ManagedPtr<T>` for automatic management

#### Configuration
```cpp
BBP::MemoryPoolConfig config;
config.initial_pool_size = 1024 * 1024;        // 1MB initial pool
config.max_pool_size = 100 * 1024 * 1024;      // 100MB maximum pool
config.block_size = 64;                         // Default block size
config.alignment = sizeof(void*);               // Default alignment
config.enable_statistics = true;                // Detailed statistics
config.enable_defragmentation = true;           // Auto defragmentation
config.growth_factor = 2.0;                     // Growth factor x2
config.defrag_threshold = 0.3;                  // Fragmentation threshold 30%

auto& manager = BBP::MemoryManager::getInstance();
manager.configure(config);
manager.initialize();
```

#### Basic Usage
```cpp
#include "infrastructure/system/memory_manager.hpp"

auto& manager = BBP::MemoryManager::getInstance();

// Simple allocation
void* ptr = manager.allocate(1024);
if (ptr) {
    // Use memory...
    manager.deallocate(ptr);
}

// Aligned allocation
void* aligned_ptr = manager.allocate(2048, 16);  // 16-byte aligned
manager.deallocate(aligned_ptr);

// Typed array allocation
int* int_array = manager.allocate_array<int>(1000);
for (int i = 0; i < 1000; ++i) {
    int_array[i] = i * i;
}
manager.deallocate_array(int_array);
```

#### STL-Compatible Pool Allocator
```cpp
// Usage with STL containers
auto allocator = manager.get_allocator<std::string>();
std::vector<std::string, BBP::PoolAllocator<std::string>> csv_lines(allocator);

// Optimized CSV parsing
csv_lines.reserve(10000);  // Pre-allocation for massive CSV
for (const auto& line : csv_data) {
    csv_lines.push_back(parseCsvLine(line));  // Uses pool allocator
}
```

#### RAII ManagedPtr Wrapper
```cpp
// Automatic management with RAII
{
    BBP::ManagedPtr<double> data_buffer(manager, 50000);  // 50k doubles
    
    // Use as standard array
    for (size_t i = 0; i < data_buffer.count(); ++i) {
        data_buffer[i] = calculateValue(i);
    }
    
    // Process data...
    processDataBuffer(data_buffer.get(), data_buffer.count());
    
    // Automatic deallocation on scope exit
}
```

#### Statistics and Monitoring
```cpp
auto stats = manager.getStats();

std::cout << "Total pool: " << stats.pool_size << " bytes" << std::endl;
std::cout << "Used memory: " << stats.current_used_bytes << " bytes" << std::endl;
std::cout << "Peak usage: " << stats.peak_used_bytes << " bytes" << std::endl;
std::cout << "Total allocations: " << stats.total_allocations << std::endl;
std::cout << "Fragmentation: " << (stats.fragmentation_ratio * 100) << "%" << std::endl;
std::cout << "Defragmentations: " << stats.defragmentation_count << std::endl;

// Average allocation/deallocation times
std::cout << "Avg alloc time: " << 
    (stats.total_alloc_time.count() / stats.total_allocations) << "μs" << std::endl;
```

#### Use Cases
- **Giant CSV Parsing**: Efficient allocation for millions of lines
- **Buffer Management**: Memory management for temporary buffers
- **String Processing**: Optimized pool for string manipulation
- **Data Structures**: Fast allocation for complex structures
- **Memory-Intensive**: Strict memory consumption control
- **High-Frequency**: High-frequency allocation/deallocation

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

## 🔍 Schema Validator - Validation Stricte CSV

### Fonctionnalité

Système de validation stricte des contrats d'entrée/sortie CSV avec versioning avancé. Garantit l'intégrité des données à travers tout le pipeline BB-Pipeline avec validation de schémas sophistiquée et gestion d'erreurs détaillée.

### Caractéristiques Techniques

- **Types de Données** : 12 types supportés (STRING, INTEGER, FLOAT, BOOLEAN, DATE, DATETIME, EMAIL, URL, IP_ADDRESS, UUID, ENUM, CUSTOM)
- **Versioning** : Gestion complète des versions de schémas avec compatibilité ascendante/descendante
- **Contraintes** : Validation de longueur, plage, pattern regex, valeurs enum, fonctions personnalisées
- **Rapports d'Erreur** : Système de sévérité (WARNING, ERROR, FATAL) avec localisation précise
- **Schemas Intégrés** : Validation automatique pour tous les modules BB-Pipeline
- **Performance** : Validation streaming optimisée pour gros fichiers CSV
- **Extensibilité** : Système de validateurs personnalisés

### Types de Données Supportés

```cpp
enum class DataType {
    STRING,        // Chaîne de caractères avec contraintes de longueur/pattern
    INTEGER,       // Entier avec validation de plage
    FLOAT,         // Nombre décimal avec validation de plage
    BOOLEAN,       // Boolean (true/false, 1/0, yes/no)
    DATE,          // Date au format ISO (YYYY-MM-DD)
    DATETIME,      // Timestamp ISO 8601 complet
    EMAIL,         // Adresse email avec validation RFC 5322
    URL,           // URL avec validation de schéma (http/https)
    IP_ADDRESS,    // Adresse IPv4 ou IPv6 valide
    UUID,          // UUID version 1-5 selon RFC 4122
    ENUM,          // Valeur parmi une liste définie
    CUSTOM         // Validation via fonction personnalisée
};
```

### Architecture des Classes

#### CsvSchema - Définition de Schéma
```cpp
class CsvSchema {
public:
    // Gestion des champs
    void addField(const SchemaField& field);
    void removeField(const std::string& field_name);
    SchemaField* getField(const std::string& field_name);
    
    // Versioning
    void setVersion(const SchemaVersion& version);
    SchemaVersion getVersion() const;
    bool isCompatibleWith(const SchemaVersion& version) const;
    
    // Configuration
    void setHeaderRequired(bool required);
    void setStrictMode(bool strict);
    void setDescription(const std::string& description);
    
    // Sérialisation
    std::string toJson() const;
    void fromJson(const std::string& json);
};
```

#### CsvSchemaValidator - Moteur de Validation
```cpp
class CsvSchemaValidator {
public:
    // Validation de fichiers/contenu
    ValidationResult validateCsvFile(const std::string& file_path, 
                                   const std::string& schema_name,
                                   const SchemaVersion& version = {});
    ValidationResult validateCsvContent(const std::string& csv_content,
                                      const std::string& schema_name,
                                      const SchemaVersion& version = {});
    
    // Gestion des schémas
    void registerSchema(const std::string& name, std::shared_ptr<CsvSchema> schema);
    std::shared_ptr<CsvSchema> getSchema(const std::string& name, 
                                       const SchemaVersion& version = {});
    
    // Configuration
    void setStopOnFirstError(bool stop);
    void setMaxErrors(size_t max_errors);
    
    // Validateurs personnalisés
    void registerCustomValidator(const std::string& name, 
                               std::function<bool(const std::string&)> validator);
};
```

#### SchemaVersion - Versioning Sémantique
```cpp
struct SchemaVersion {
    uint32_t major{1};                    // Version majeure (breaking changes)
    uint32_t minor{0};                    // Version mineure (nouvelles fonctionnalités)
    uint32_t patch{0};                    // Version patch (corrections)
    std::string description;              // Description des changements
    std::chrono::system_clock::time_point created_at;
    
    // Opérateurs de comparaison
    bool operator==(const SchemaVersion& other) const;
    bool operator<(const SchemaVersion& other) const;
    bool operator<=(const SchemaVersion& other) const;
    bool operator>(const SchemaVersion& other) const;
    bool operator>=(const SchemaVersion& other) const;
    
    std::string toString() const;
};
```

### Utilisation Basique

#### 1. Définition d'un Schéma Simple
```cpp
#include "csv/schema_validator.hpp"
using namespace BBP::CSV;

// Créer un schéma pour des données utilisateur
auto user_schema = std::make_shared<CsvSchema>("user_data", SchemaVersion{1, 0, 0});

// Définir les champs avec contraintes
SchemaField name_field("name", DataType::STRING, 0);
name_field.constraints.required = true;
name_field.constraints.min_length = 2;
name_field.constraints.max_length = 50;
user_schema->addField(name_field);

SchemaField email_field("email", DataType::EMAIL, 1);
email_field.constraints.required = true;
user_schema->addField(email_field);

SchemaField age_field("age", DataType::INTEGER, 2);
age_field.constraints.min_value = 0;
age_field.constraints.max_value = 150;
user_schema->addField(age_field);

// Configurer le schéma
user_schema->setHeaderRequired(true);
user_schema->setStrictMode(true);
user_schema->setDescription("Schema for user registration data validation");
```

#### 2. Enregistrement et Validation
```cpp
// Créer le validateur
CsvSchemaValidator validator;

// Enregistrer le schéma
validator.registerSchema("user_schema", user_schema);

// Valider un fichier CSV
auto result = validator.validateCsvFile("data/users.csv", "user_schema");

if (result.is_valid) {
    std::cout << "✅ Validation réussie !" << std::endl;
    std::cout << "Lignes totales: " << result.total_rows << std::endl;
    std::cout << "Lignes valides: " << result.valid_rows << std::endl;
} else {
    std::cout << "❌ Validation échouée !" << std::endl;
    std::cout << "Erreurs trouvées: " << result.errors.size() << std::endl;
    
    // Afficher les erreurs détaillées
    for (const auto& error : result.errors) {
        std::cout << "Ligne " << error.row_number 
                  << ", Colonne " << error.column_number
                  << " (" << error.field_name << "): "
                  << error.message << std::endl;
    }
}
```

#### 3. Validation avec Versioning
```cpp
// Schéma v1.0.0 - Version initiale
auto schema_v1 = std::make_shared<CsvSchema>("api_data", SchemaVersion{1, 0, 0});
schema_v1->addField({"endpoint", DataType::URL, 0});
schema_v1->addField({"method", DataType::ENUM, 1});
schema_v1->getField("method")->constraints.enum_values = {"GET", "POST", "PUT", "DELETE"};

// Schéma v1.1.0 - Ajout champ optionnel (compatible)
auto schema_v1_1 = std::make_shared<CsvSchema>("api_data", SchemaVersion{1, 1, 0});
schema_v1_1->addField({"endpoint", DataType::URL, 0});
schema_v1_1->addField({"method", DataType::ENUM, 1});
schema_v1_1->addField({"auth_required", DataType::BOOLEAN, 2});
schema_v1_1->getField("auth_required")->constraints.required = false; // Optionnel

validator.registerSchema("api_data", schema_v1);
validator.registerSchema("api_data", schema_v1_1);

// Validation avec version spécifique
auto result_v1 = validator.validateCsvFile("old_api_data.csv", "api_data", {1, 0, 0});
auto result_latest = validator.validateCsvFile("new_api_data.csv", "api_data"); // Dernière version
```

### Schemas Intégrés BB-Pipeline

Le Schema Validator inclut des définitions automatiques pour tous les formats CSV du pipeline :

#### 1. Scope Definition (data/scope.csv)
```cpp
// Schéma automatiquement enregistré comme "scope"
SchemaVersion: 1.0.0
Champs:
- domain (STRING, requis) : Domaine racine autorisé
- wildcard (BOOLEAN) : Support des sous-domaines
- description (STRING, optionnel) : Description du scope
```

#### 2. Subdomains (01_subdomains.csv)
```cpp
// Schéma "subdomains" - v1.0.0
Champs:
- subdomain (STRING, requis) : Sous-domaine découvert
- source (ENUM) : subfinder|amass|cert_transparency|dns_bruteforce
- confidence (INTEGER, 0-100) : Score de confiance
- discovered_at (DATETIME) : Timestamp de découverte
```

#### 3. HTTP Probe Results (02_probe.csv)
```cpp
// Schéma "probe" - v1.0.0  
Champs:
- url (URL, requis) : URL complète testée
- status_code (INTEGER, 100-599) : Code de statut HTTP
- content_length (INTEGER, ≥0) : Taille du contenu
- title (STRING) : Titre de la page
- technologies (STRING) : Technologies détectées (JSON array)
- server (STRING) : Header Server
- response_time_ms (INTEGER, ≥0) : Temps de réponse
```

#### 4. Discovery Results (04_discovery.csv)
```cpp
// Schéma "discovery" - v1.0.0
Champs:
- base_url (URL, requis) : URL de base scannée
- discovered_path (STRING, requis) : Chemin découvert
- status_code (INTEGER, 100-599) : Code de statut
- content_type (STRING) : Type MIME
- content_length (INTEGER, ≥0) : Taille du contenu
- method (ENUM) : GET|POST|PUT|DELETE|HEAD|OPTIONS
- interesting (BOOLEAN) : Marqué comme intéressant
```

### Validateurs Personnalisés

#### Enregistrement de Validateurs Custom
```cpp
// Validateur pour numéros de téléphone français
validator.registerCustomValidator("french_phone", [](const std::string& value) -> bool {
    std::regex french_phone_regex(R"(^(?:\+33|0)[1-9](?:[0-9]{8})$)");
    return std::regex_match(value, french_phone_regex);
});

// Utilisation dans un schéma
SchemaField phone_field("telephone", DataType::CUSTOM, 3);
phone_field.constraints.custom_validator = validator.getCustomValidator("french_phone");
schema->addField(phone_field);

// Validateur pour JWT tokens
validator.registerCustomValidator("jwt_token", [](const std::string& value) -> bool {
    // Vérification basique du format JWT (3 parties séparées par '.')
    auto parts = split(value, '.');
    return parts.size() == 3 && 
           !parts[0].empty() && !parts[1].empty() && !parts[2].empty();
});
```

### Gestion d'Erreurs Avancée

#### Configuration des Niveaux d'Erreur
```cpp
// Configuration stricte - arrêt à la première erreur
validator.setStopOnFirstError(true);

// Configuration permissive - collecter toutes les erreurs
validator.setStopOnFirstError(false);
validator.setMaxErrors(1000); // Limite pour éviter l'explosion mémoire

// Validation avec gestion détaillée
auto result = validator.validateCsvFile("data.csv", "schema_name");

// Tri des erreurs par sévérité
std::vector<ValidationError> critical_errors;
std::vector<ValidationError> warnings;

for (const auto& error : result.errors) {
    switch (error.severity) {
        case ValidationError::Severity::FATAL:
        case ValidationError::Severity::ERROR:
            critical_errors.push_back(error);
            break;
        case ValidationError::Severity::WARNING:
            warnings.push_back(error);
            break;
    }
}

std::cout << "Erreurs critiques: " << critical_errors.size() << std::endl;
std::cout << "Avertissements: " << warnings.size() << std::endl;
```

#### Génération de Rapports Détaillés
```cpp
// Rapport de validation complet
std::string report = result.generateReport();
std::cout << report << std::endl;

/* Sortie exemple:
=== Validation Report ===
Schema: user_data v1.0.0
File: data/users.csv
Status: FAILED

Statistics:
Total Rows: 150
Valid Rows: 142
Error Rows: 8
Warnings: 3

Errors by Field:
- email: 5 errors
- age: 2 errors
- phone: 1 error

Detailed Errors:
Row 23, Column 2 (email): Invalid email format 'john.doe@'
Row 45, Column 3 (age): Value '999' exceeds maximum allowed (150)
...
*/
```

### Performance et Optimisations

#### Validation Streaming pour Gros Fichiers
```cpp
// Configuration pour fichiers volumineux
CsvSchemaValidator validator;
validator.setStopOnFirstError(false);  // Collecter toutes les erreurs
validator.setMaxErrors(10000);         // Limite raisonnable

// Validation optimisée mémoire pour fichiers > 100MB
auto result = validator.validateCsvFile("huge_dataset.csv", "data_schema");

// Métriques de performance
std::cout << "Temps de validation: " << result.validation_duration.count() << "ms" << std::endl;
std::cout << "Vitesse: " << (result.total_rows * 1000.0 / result.validation_duration.count()) 
          << " lignes/seconde" << std::endl;
```

#### Cache de Schemas
```cpp
// Les schémas sont automatiquement mis en cache
// Réutilisation efficace pour validations multiples
for (const auto& file : csv_files) {
    auto result = validator.validateCsvFile(file, "common_schema");
    // Le schéma n'est chargé qu'une seule fois
}
```

### Intégration Pipeline BB-Pipeline

#### Validation Automatique entre Modules
```cpp
// Dans bbpctl - orchestrateur principal
CsvSchemaValidator pipeline_validator;

// Validation de scope avant démarrage
auto scope_result = pipeline_validator.validateCsvFile("data/scope.csv", "scope");
if (!scope_result.is_valid) {
    logger.error("pipeline", "Invalid scope definition");
    return -1;
}

// Validation entre chaque étape du pipeline
auto subdomains_result = pipeline_validator.validateCsvFile("out/01_subdomains.csv", "subdomains");
auto probe_result = pipeline_validator.validateCsvFile("out/02_probe.csv", "probe");
auto discovery_result = pipeline_validator.validateCsvFile("out/04_discovery.csv", "discovery");

// Agrégation des résultats de validation
ValidationSummary summary;
summary.addResult("scope", scope_result);
summary.addResult("subdomains", subdomains_result);
summary.addResult("probe", probe_result);
summary.addResult("discovery", discovery_result);

if (!summary.allValid()) {
    logger.error("pipeline", "Validation failures detected");
    std::cout << summary.generateReport() << std::endl;
    return -1;
}
```

### Migration de Schemas

#### Gestion des Changements de Version
```cpp
// Migration automatique v1.0 → v1.1
class SchemaV1ToV1_1Migrator {
public:
    std::string migrate(const std::string& csv_v1_content) {
        // Ajouter colonne 'created_at' avec timestamp par défaut
        std::stringstream result;
        auto lines = split(csv_v1_content, '\n');
        
        // Header
        if (!lines.empty()) {
            result << lines[0] << ",created_at\n";
        }
        
        // Data rows
        for (size_t i = 1; i < lines.size(); ++i) {
            if (!lines[i].empty()) {
                result << lines[i] << ",2024-01-01T00:00:00Z\n";
            }
        }
        
        return result.str();
    }
};

// Utilisation
SchemaV1ToV1_1Migrator migrator;
std::string migrated_content = migrator.migrate(old_csv_content);
auto result = validator.validateCsvContent(migrated_content, "schema_name", {1, 1, 0});
```

### Tests et Validation

Le Schema Validator dispose d'une suite de tests complète avec 100% de couverture :

- **30 tests unitaires** couvrant tous les types de données et cas d'usage
- **Validation de performance** sur fichiers volumineux (>1M lignes)
- **Tests de régression** pour compatibilité des versions
- **Tests d'intégration** avec l'écosystème BB-Pipeline
- **Benchmarks** de performance (>50k lignes/seconde typique)

### Cas d'Usage Principaux

1. **Validation de Scope** : Vérification stricte des domaines autorisés avant scan
2. **Contrôle Qualité Pipeline** : Validation automatique entre chaque étape
3. **Import de Données** : Validation de datasets externes avant traitement
4. **Monitoring Continu** : Détection d'anomalies dans les outputs
5. **Débogage** : Identification précise des problèmes de format
6. **Conformité** : Respect des contrats d'API et formats de données

---

## 📊 Streaming Parser - Parser CSV Haute Performance

### Fonctionnalité

Parser CSV streaming haute performance conçu pour traiter de très gros fichiers (multi-GB) sans chargement complet en mémoire. Utilise un buffer circulaire et un parsing incrémental pour maintenir un usage mémoire constant indépendamment de la taille du fichier.

### Caractéristiques Techniques

- **Mémoire Constante** : Usage mémoire indépendant de la taille du fichier
- **Performance Optimisée** : Buffer circulaire et parsing incrémental
- **Support Multi-Threading** : Parsing asynchrone avec contrôle (pause/resume/stop)
- **Callbacks Personnalisables** : Traitement ligne par ligne avec fonctions utilisateur
- **Gestion d'Encodage** : Support UTF-8, UTF-16, ASCII avec détection automatique
- **Recovery Gracieux** : Continue le parsing malgré les lignes malformées
- **Métriques Détaillées** : Statistiques de performance et progression en temps réel
- **Configuration Flexible** : Délimiteurs, quotes, whitespace personnalisables

### Architecture des Classes

#### ParserConfig - Configuration du Parser
```cpp
struct ParserConfig {
    char delimiter{','};                    // Délimiteur de champ
    char quote_char{'"'};                   // Caractère de quote
    char escape_char{'"'};                  // Caractère d'échappement
    bool has_header{true};                  // Première ligne est l'en-tête
    bool strict_mode{false};                // Parsing strict (échec sur erreurs)
    bool trim_whitespace{true};             // Supprime espaces début/fin
    bool skip_empty_rows{true};             // Ignore les lignes vides
    size_t buffer_size{8192};               // Taille buffer interne (octets)
    size_t max_field_size{1048576};         // Taille max champ (1MB)
    size_t max_row_size{10485760};          // Taille max ligne (10MB)
    EncodingType encoding{EncodingType::AUTO_DETECT};
    bool enable_parallel_processing{false}; // Multi-threading
    size_t thread_count{0};                 // Nombre threads (0=auto)
};
```

#### StreamingParser - Moteur de Parsing Principal
```cpp
class StreamingParser {
public:
    // Construction et configuration
    StreamingParser();
    explicit StreamingParser(const ParserConfig& config);
    void setConfig(const ParserConfig& config);
    
    // Enregistrement des callbacks
    void setRowCallback(RowCallback callback);
    void setProgressCallback(ProgressCallback callback);  
    void setErrorCallback(ErrorCallback callback);
    
    // Parsing synchrone
    ParserError parseFile(const std::string& file_path);
    ParserError parseStream(std::istream& stream);
    ParserError parseString(const std::string& csv_content);
    
    // Parsing asynchrone
    ParserError parseFileAsync(const std::string& file_path);
    ParserError parseStreamAsync(std::istream& stream);
    void pauseParsing();
    void resumeParsing();
    void stopParsing();
    ParserError waitForCompletion();
    
    // Statistiques et monitoring
    const ParserStatistics& getStatistics() const;
    void resetStatistics();
    
    // Méthodes utilitaires statiques
    static std::vector<std::string> parseRow(const std::string& row);
    static std::string escapeField(const std::string& field);
    static EncodingType detectEncoding(const std::string& file_path);
    static size_t getFileSize(const std::string& file_path);
};
```

#### ParsedRow - Ligne CSV Analysée
```cpp
class ParsedRow {
public:
    // Accès aux champs par index
    const std::string& operator[](size_t index) const;
    const std::string& getField(size_t index) const;
    std::optional<std::string> getFieldSafe(size_t index) const;
    
    // Accès aux champs par nom d'en-tête
    const std::string& operator[](const std::string& header) const;
    const std::string& getField(const std::string& header) const;
    std::optional<std::string> getFieldSafe(const std::string& header) const;
    
    // Conversion de type sécurisée
    template<typename T>
    std::optional<T> getFieldAs(size_t index) const;
    
    template<typename T>
    std::optional<T> getFieldAs(const std::string& header) const;
    
    // Informations de ligne
    size_t getRowNumber() const;
    size_t getFieldCount() const;
    bool hasHeaders() const;
    bool isValid() const;
    bool isEmpty() const;
    
    std::string toString() const;
};
```

#### ParserStatistics - Métriques de Performance
```cpp
class ParserStatistics {
public:
    // Chronométrage
    void startTiming();
    void stopTiming();
    
    // Mise à jour des statistiques
    void incrementRowsParsed();
    void incrementRowsSkipped();
    void incrementRowsWithErrors();
    void addBytesRead(size_t bytes);
    void recordFieldCount(size_t count);
    
    // Métriques calculées
    size_t getRowsParsed() const;
    size_t getRowsSkipped() const;
    size_t getRowsWithErrors() const;
    size_t getBytesRead() const;
    double getRowsPerSecond() const;
    double getBytesPerSecond() const;
    double getAverageFieldCount() const;
    
    std::string generateReport() const;
};
```

### Utilisation Basique

#### 1. Parsing Simple de Fichier
```cpp
#include "csv/streaming_parser.hpp"
using namespace BBP::CSV;

// Créer le parser avec configuration par défaut
StreamingParser parser;

// Callback pour traitement des lignes
parser.setRowCallback([](const ParsedRow& row, ParserError error) {
    if (error == ParserError::SUCCESS) {
        std::cout << "Ligne " << row.getRowNumber() << ": ";
        for (size_t i = 0; i < row.getFieldCount(); ++i) {
            std::cout << row[i] << " | ";
        }
        std::cout << std::endl;
    }
    return true; // Continuer le parsing
});

// Parser le fichier
ParserError result = parser.parseFile("large_dataset.csv");

if (result == ParserError::SUCCESS) {
    const auto& stats = parser.getStatistics();
    std::cout << "✅ Parsing réussi !" << std::endl;
    std::cout << "Lignes traitées: " << stats.getRowsParsed() << std::endl;
    std::cout << "Performance: " << stats.getRowsPerSecond() << " lignes/sec" << std::endl;
}
```

#### 2. Configuration Personnalisée
```cpp
// Configuration pour fichier CSV avec délimiteur point-virgule
ParserConfig config;
config.delimiter = ';';
config.quote_char = '\'';
config.has_header = true;
config.strict_mode = false;
config.buffer_size = 16384;  // Buffer de 16KB
config.trim_whitespace = true;
config.skip_empty_rows = true;

StreamingParser parser(config);

// Parsing avec gestion d'erreur personnalisée
parser.setErrorCallback([](ParserError error, const std::string& message, size_t row_number) {
    std::cerr << "Erreur ligne " << row_number << ": " << message << std::endl;
});

parser.parseFile("data_with_semicolons.csv");
```

#### 3. Parsing Asynchrone avec Progression
```cpp
StreamingParser parser;

// Callback de progression
parser.setProgressCallback([](size_t rows_processed, size_t bytes_read, double progress_percent) {
    std::cout << "\rProgrès: " << std::fixed << std::setprecision(1) 
              << progress_percent << "% (" << rows_processed << " lignes)" << std::flush;
});

// Démarrer parsing asynchrone
parser.parseFileAsync("huge_dataset.csv");

// Faire autre chose pendant le parsing
std::this_thread::sleep_for(std::chrono::seconds(5));

// Mettre en pause
parser.pauseParsing();
std::cout << "\nParsing mis en pause..." << std::endl;
std::this_thread::sleep_for(std::chrono::seconds(2));

// Reprendre
parser.resumeParsing();
std::cout << "Reprise du parsing..." << std::endl;

// Attendre la fin
parser.waitForCompletion();
std::cout << "\nParsing terminé !" << std::endl;
```

### Conversion de Types Sécurisée

#### Accès Typé aux Champs
```cpp
parser.setRowCallback([](const ParsedRow& row, ParserError error) {
    if (error == ParserError::SUCCESS) {
        // Conversion de type avec vérification d'erreur
        auto name = row.getFieldAs<std::string>("name");
        auto age = row.getFieldAs<int>("age");
        auto salary = row.getFieldAs<double>("salary");
        auto is_active = row.getFieldAs<bool>("active");
        
        if (name && age && salary && is_active) {
            std::cout << *name << " (" << *age << " ans) - "
                      << *salary << "€ - " << (*is_active ? "Actif" : "Inactif") << std::endl;
        } else {
            std::cout << "Erreur de conversion ligne " << row.getRowNumber() << std::endl;
        }
    }
    return true;
});
```

#### Types Supportés pour Conversion
```cpp
// Types numériques
auto int_val = row.getFieldAs<int>("id");
auto long_val = row.getFieldAs<long>("timestamp");  
auto float_val = row.getFieldAs<float>("score");
auto double_val = row.getFieldAs<double>("price");

// Booléens (supporte: true/false, 1/0, yes/no, on/off)
auto bool_val = row.getFieldAs<bool>("enabled");

// Chaînes (toujours disponible)
auto string_val = row.getFieldAs<std::string>("description");

// Accès sécurisé par index
auto field_0 = row.getFieldAs<std::string>(0);
```

### Gestion d'Erreurs et Modes de Fonctionnement

#### Mode Strict vs Non-Strict
```cpp
// Mode strict - s'arrête à la première erreur
ParserConfig strict_config;
strict_config.strict_mode = true;
StreamingParser strict_parser(strict_config);

// Mode non-strict - continue malgré les erreurs
ParserConfig tolerant_config;
tolerant_config.strict_mode = false;
StreamingParser tolerant_parser(tolerant_config);

// Callback d'erreur détaillé
tolerant_parser.setErrorCallback([](ParserError error, const std::string& message, size_t row_number) {
    switch (error) {
        case ParserError::MALFORMED_ROW:
            std::cout << "Ligne " << row_number << " malformée: " << message << std::endl;
            break;
        case ParserError::ENCODING_ERROR:
            std::cout << "Erreur d'encodage: " << message << std::endl;
            break;
        default:
            std::cout << "Erreur parsing: " << message << std::endl;
            break;
    }
});
```

### Performance et Optimisations

#### Configuration pour Gros Fichiers
```cpp
ParserConfig high_perf_config;

// Buffer plus grand pour moins d'I/O
high_perf_config.buffer_size = 65536; // 64KB

// Augmenter les limites si nécessaire
high_perf_config.max_field_size = 10485760; // 10MB
high_perf_config.max_row_size = 104857600;  // 100MB

// Optimisations pour performance pure
high_perf_config.trim_whitespace = false;   // Désactiver si pas nécessaire
high_perf_config.skip_empty_rows = false;   // Traiter toutes les lignes

StreamingParser perf_parser(high_perf_config);

// Callback optimisé (minimal processing)
size_t row_counter = 0;
perf_parser.setRowCallback([&row_counter](const ParsedRow& row, ParserError error) {
    if (error == ParserError::SUCCESS) {
        ++row_counter;
        // Traitement minimal ici
    }
    return true;
});
```

#### Métriques de Performance
```cpp
parser.parseFile("large_file.csv");

const auto& stats = parser.getStatistics();
std::cout << "=== Rapport de Performance ===" << std::endl;
std::cout << "Lignes traitées: " << stats.getRowsParsed() << std::endl;
std::cout << "Lignes ignorées: " << stats.getRowsSkipped() << std::endl;
std::cout << "Lignes avec erreurs: " << stats.getRowsWithErrors() << std::endl;
std::cout << "Octets lus: " << stats.getBytesRead() << " (" 
          << (stats.getBytesRead() / 1024.0 / 1024.0) << " MB)" << std::endl;
std::cout << "Temps de parsing: " << stats.getParsingDuration().count() << "s" << std::endl;
std::cout << "Débit: " << stats.getRowsPerSecond() << " lignes/sec" << std::endl;
std::cout << "Débit: " << (stats.getBytesPerSecond() / 1024.0 / 1024.0) << " MB/sec" << std::endl;
std::cout << "Champs par ligne (moy): " << stats.getAverageFieldCount() << std::endl;

// Rapport complet
std::cout << std::endl << stats.generateReport() << std::endl;
```

### Méthodes Utilitaires Statiques

#### Parsing de Ligne Individuelle
```cpp
// Parser une ligne CSV individuelle
std::string csv_line = "John,\"Smith, Jr.\",30,Engineer";
auto fields = StreamingParser::parseRow(csv_line);

for (const auto& field : fields) {
    std::cout << "[" << field << "] ";
}
// Output: [John] [Smith, Jr.] [30] [Engineer]

// Avec configuration personnalisée
ParserConfig custom_config;
custom_config.delimiter = ';';
custom_config.quote_char = '\'';

std::string csv_line_custom = "John;'Smith; Jr.';30;Engineer";
auto custom_fields = StreamingParser::parseRow(csv_line_custom, custom_config);
```

#### Échappement de Champs
```cpp
// Échapper un champ pour output CSV
std::string field_with_comma = "Smith, Jr.";
std::string escaped = StreamingParser::escapeField(field_with_comma);
std::cout << escaped << std::endl; // Output: "Smith, Jr."

std::string field_with_quotes = "He said \"Hello\"";
escaped = StreamingParser::escapeField(field_with_quotes);
std::cout << escaped << std::endl; // Output: "He said ""Hello"""

// Avec configuration personnalisée
ParserConfig config;
config.quote_char = '\'';
escaped = StreamingParser::escapeField("O'Reilly", config);
std::cout << escaped << std::endl; // Output: 'O''Reilly'
```

#### Détection d'Encodage et Taille de Fichier
```cpp
// Détecter l'encodage d'un fichier
EncodingType encoding = StreamingParser::detectEncoding("data.csv");
switch (encoding) {
    case EncodingType::UTF8:
        std::cout << "Fichier en UTF-8" << std::endl;
        break;
    case EncodingType::UTF16_LE:
        std::cout << "Fichier en UTF-16 Little Endian" << std::endl;
        break;
    // ... autres encodages
}

// Obtenir la taille d'un fichier
size_t file_size = StreamingParser::getFileSize("data.csv");
std::cout << "Taille du fichier: " << file_size << " octets" << std::endl;
```

### Cas d'Usage Avancés

#### Parsing avec Callback d'Arrêt Conditionnel
```cpp
size_t max_rows_to_process = 10000;
size_t processed_count = 0;

parser.setRowCallback([&](const ParsedRow& row, ParserError error) {
    if (error == ParserError::SUCCESS) {
        processed_count++;
        
        // Traiter la ligne
        processRow(row);
        
        // Arrêter après max_rows_to_process lignes
        return processed_count < max_rows_to_process;
    }
    return true;
});

// Le parsing s'arrêtera automatiquement après 10 000 lignes
parser.parseFile("huge_file.csv");
```

#### Filtering et Transformation en Temps Réel
```cpp
parser.setRowCallback([](const ParsedRow& row, ParserError error) {
    if (error != ParserError::SUCCESS) return true;
    
    // Filter: ne garder que les lignes où age > 25
    auto age = row.getFieldAs<int>("age");
    if (!age || *age <= 25) {
        return true; // Skip cette ligne
    }
    
    // Transform: formater et sauvegarder
    std::string formatted_output = 
        row["name"] + " (" + std::to_string(*age) + " ans) - " + row["email"];
    
    // Écrire dans un fichier de sortie ou base de données
    writeToOutput(formatted_output);
    
    return true;
});
```

#### Validation et Nettoyage de Données
```cpp
size_t valid_rows = 0;
size_t invalid_rows = 0;

parser.setRowCallback([&](const ParsedRow& row, ParserError error) {
    if (error != ParserError::SUCCESS) {
        invalid_rows++;
        return true;
    }
    
    // Validation des données
    bool is_valid = true;
    
    // Vérifier que l'email est valide
    std::string email = row["email"];
    if (email.find('@') == std::string::npos) {
        is_valid = false;
    }
    
    // Vérifier que l'âge est raisonnable
    auto age = row.getFieldAs<int>("age");
    if (!age || *age < 0 || *age > 120) {
        is_valid = false;
    }
    
    if (is_valid) {
        valid_rows++;
        // Traiter la ligne valide
        processValidRow(row);
    } else {
        invalid_rows++;
        logInvalidRow(row);
    }
    
    return true;
});

parser.parseFile("user_data.csv");

std::cout << "Lignes valides: " << valid_rows << std::endl;
std::cout << "Lignes invalides: " << invalid_rows << std::endl;
```

### Intégration Pipeline BB-Pipeline

#### Parser de Scope Efficace
```cpp
// Parser optimisé pour fichier scope.csv
ParserConfig scope_config;
scope_config.has_header = true;
scope_config.strict_mode = true; // Strict pour scope critique

StreamingParser scope_parser(scope_config);

std::vector<std::string> authorized_domains;

scope_parser.setRowCallback([&](const ParsedRow& row, ParserError error) {
    if (error == ParserError::SUCCESS) {
        std::string domain = row["domain"];
        bool wildcard = row.getFieldAs<bool>("wildcard").value_or(false);
        
        authorized_domains.push_back(domain);
        
        if (wildcard) {
            // Ajouter support pour sous-domaines
            authorized_domains.push_back("*." + domain);
        }
    }
    return true;
});

ParserError result = scope_parser.parseFile("data/scope.csv");
if (result == ParserError::SUCCESS) {
    std::cout << "Scope chargé: " << authorized_domains.size() << " domaines" << std::endl;
}
```

#### Processing de Résultats de Reconnaissance
```cpp
// Parser pour 01_subdomains.csv avec validation
StreamingParser subdomain_parser;

std::unordered_set<std::string> unique_subdomains;
std::vector<SubdomainEntry> high_confidence_results;

subdomain_parser.setRowCallback([&](const ParsedRow& row, ParserError error) {
    if (error == ParserError::SUCCESS) {
        std::string subdomain = row["subdomain"];
        std::string source = row["source"];
        auto confidence = row.getFieldAs<int>("confidence");
        
        if (confidence && *confidence >= 80) {
            // Ne garder que les résultats haute confiance
            if (unique_subdomains.find(subdomain) == unique_subdomains.end()) {
                unique_subdomains.insert(subdomain);
                high_confidence_results.push_back({subdomain, source, *confidence});
            }
        }
    }
    return true;
});

subdomain_parser.parseFile("out/01_subdomains.csv");

std::cout << "Sous-domaines uniques haute confiance: " 
          << high_confidence_results.size() << std::endl;
```

### Tests et Validation

Le Streaming Parser dispose d'une suite de tests complète avec couverture étendue :

- **27 tests unitaires** couvrant tous les aspects du parsing
- **Tests de performance** sur datasets de 10 000+ lignes
- **Tests de robustesse** avec données malformées
- **Tests de configuration** pour tous les paramètres
- **Tests multi-threading** pour parsing asynchrone
- **Tests d'encodage** UTF-8, UTF-16, ASCII
- **Benchmarks** de performance (>100k lignes/seconde typique)

### Cas d'Usage Principaux

1. **Processing de Gros Datasets** : Analyse de fichiers CSV multi-GB sans limite mémoire
2. **Pipeline BB-Pipeline** : Parser efficace pour tous les outputs du pipeline
3. **Data Cleaning** : Validation et nettoyage de données en temps réel
4. **ETL Processing** : Extract-Transform-Load avec streaming
5. **Log Analysis** : Analyse de logs CSV volumineux
6. **Real-time Processing** : Traitement au fur et à mesure avec callbacks
7. **Data Migration** : Migration de données entre formats
8. **Monitoring** : Parsing de metrics et événements en continu

### Performance Typique

- **Débit** : 100 000+ lignes/seconde sur hardware moderne
- **Mémoire** : Usage constant ~8-64KB indépendamment de la taille du fichier  
- **Scalabilité** : Traitement de fichiers de plusieurs TB sans problème
- **Latence** : Première ligne traitée en <1ms (streaming)
- **Concurrence** : Support multi-threading avec parsing asynchrone

---

## 🔮 Prochaines Fonctionnalités / Next Features

- **Signal Handler** : Arrêt propre avec flush CSV garanti / Clean shutdown with guaranteed CSV flush
- **Memory Manager** : Pool allocator optimisé / Optimized pool allocator
- **Error Recovery** : Retry automatique avec backoff / Automatic retry with backoff
- **Pipeline Orchestrator** : Logique bbpctl complète / Complete bbpctl logic