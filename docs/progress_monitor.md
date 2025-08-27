# Progress Monitor System / Système de Moniteur de Progression

## English Documentation

### Overview

The Progress Monitor System provides real-time progress tracking with ETA (Estimated Time of Arrival) calculation for the BB-Pipeline framework. This comprehensive system enables visual progress reporting, intelligent time estimation, and flexible monitoring capabilities for all pipeline operations.

### Key Features

- **Real-time Progress Tracking**: Live updates of task completion status with configurable update frequencies
- **Intelligent ETA Calculation**: Multiple algorithms for time estimation including linear, moving average, exponential smoothing, and adaptive strategies
- **Multiple Display Modes**: Support for simple bars, detailed displays, percentage-only, compact, verbose, JSON, and custom formats
- **Task Dependency Management**: Advanced dependency resolution with automatic ready-task detection
- **Event-driven Architecture**: Comprehensive callback system for progress events and milestones
- **State Persistence**: Save and restore monitoring state across application restarts
- **Performance Optimization**: Efficient handling of large task counts with throttled updates
- **Specialized Monitors**: Pre-built monitors for file transfers, network operations, and batch processing
- **RAII Support**: Automatic cleanup with RAII helper classes
- **Thread-safe Operations**: Full thread safety for concurrent access
- **Extensible Design**: Pluggable formatters and ETA predictors for customization

### Architecture

#### Core Components

1. **ProgressMonitor**: Main monitoring class with PIMPL design pattern
2. **ProgressMonitorManager**: Singleton manager for coordinating multiple monitors
3. **Specialized Monitors**: FileTransferProgressMonitor, NetworkProgressMonitor, BatchProcessingProgressMonitor
4. **AutoProgressMonitor**: RAII helper for automatic lifecycle management
5. **Utility Functions**: Helper functions for configuration, task generation, and formatting

#### Design Patterns

- **PIMPL (Pointer to Implementation)**: Encapsulates implementation details and provides stable ABI
- **Singleton Pattern**: Centralized manager for global coordination
- **Strategy Pattern**: Pluggable ETA calculation strategies
- **Observer Pattern**: Event-driven callbacks for progress notifications
- **RAII (Resource Acquisition Is Initialization)**: Automatic resource management

### Configuration Options

#### Progress Update Modes

- `REAL_TIME`: Update as frequently as possible for immediate feedback
- `THROTTLED`: Limit update frequency to prevent UI spam (recommended)
- `ON_DEMAND`: Update only when explicitly requested
- `MILESTONE`: Update only on significant progress milestones

#### Display Modes

- `SIMPLE_BAR`: Basic ASCII progress bar with percentage
- `DETAILED_BAR`: Enhanced bar with speed, ETA, and statistics
- `PERCENTAGE`: Percentage-only display for minimal UI
- `COMPACT`: Single-line compact format
- `VERBOSE`: Multi-line detailed display with full statistics
- `JSON`: Machine-readable JSON format for integration
- `CUSTOM`: User-defined format with custom formatter callbacks

#### ETA Calculation Strategies

- `LINEAR`: Simple linear extrapolation based on current progress
- `MOVING_AVERAGE`: Moving average of recent progress rates
- `EXPONENTIAL`: Exponential smoothing for stable predictions
- `ADAPTIVE`: Intelligent combination of multiple strategies
- `WEIGHTED`: Task weight-based calculations for complex workflows
- `HISTORICAL`: Historical data-based predictions (extensible)

### Usage Examples

#### Basic Progress Monitoring

```cpp
#include "orchestrator/progress_monitor.hpp"
using namespace BBP::Orchestrator;

// Create configuration
auto config = ProgressUtils::createDefaultConfig();
config.show_eta = true;
config.show_speed = true;
config.enable_colors = true;

// Create monitor
ProgressMonitor monitor(config);

// Add tasks
ProgressTaskConfig task;
task.id = "data_processing";
task.name = "Data Processing";
task.total_units = 1000;
task.weight = 1.0;

monitor.addTask(task);
monitor.start();

// Update progress
for (size_t i = 0; i <= 1000; i += 100) {
    monitor.updateProgress("data_processing", i);
    std::this_thread::sleep_for(100ms);
}

monitor.stop();
```

#### Multiple Task Coordination

```cpp
// Create multiple tasks with different weights
std::vector<ProgressTaskConfig> tasks;

ProgressTaskConfig task1;
task1.id = "download";
task1.name = "Downloading Files";
task1.total_units = 500;
task1.weight = 2.0; // Higher priority

ProgressTaskConfig task2;
task2.id = "process";
task2.name = "Processing Data";
task2.total_units = 300;
task2.weight = 1.0;

tasks.push_back(task1);
tasks.push_back(task2);

ProgressMonitor monitor(config);
monitor.start(tasks);

// Update multiple tasks
monitor.updateProgress("download", 250);
monitor.updateProgress("process", 100);

// Batch operations
std::map<std::string, size_t> updates;
updates["download"] = 500;
updates["process"] = 200;
monitor.updateMultipleProgress(updates);
```

#### Event Handling

```cpp
// Setup event callback
monitor.setEventCallback([](const ProgressEvent& event) {
    switch (event.type) {
        case ProgressEventType::STARTED:
            std::cout << "Progress monitoring started" << std::endl;
            break;
        case ProgressEventType::STAGE_COMPLETED:
            std::cout << "Task " << event.task_id << " completed" << std::endl;
            break;
        case ProgressEventType::ETA_UPDATED:
            std::cout << "ETA updated: " << event.statistics.estimated_remaining_time.count() << "ms" << std::endl;
            break;
        case ProgressEventType::COMPLETED:
            std::cout << "All tasks completed!" << std::endl;
            break;
    }
});
```

#### Custom Formatting

```cpp
// Custom formatter for specialized display
monitor.setCustomFormatter([](const ProgressStatistics& stats, const ProgressMonitorConfig& config) {
    std::ostringstream oss;
    oss << "🚀 Progress: " << std::fixed << std::setprecision(1) << stats.current_progress << "% ";
    oss << "(" << stats.completed_units << "/" << stats.total_units << ") ";
    if (stats.estimated_remaining_time.count() > 0) {
        oss << "⏱️ ETA: " << formatDuration(stats.estimated_remaining_time);
    }
    return oss.str();
});

config.display_mode = ProgressDisplayMode::CUSTOM;
monitor.updateConfig(config);
```

#### Specialized Monitors

```cpp
// File transfer monitoring
FileTransferProgressMonitor file_monitor(config);
file_monitor.startTransfer("large_file.zip", 104857600); // 100MB

// Update during transfer
file_monitor.updateTransferred(52428800); // 50MB transferred
file_monitor.setTransferRate(1048576); // 1MB/s

std::string info = file_monitor.getCurrentTransferInfo();
std::cout << info << std::endl;

// Network operation monitoring
NetworkProgressMonitor network_monitor(config);
network_monitor.startNetworkOperation("API Scan", 10000);
network_monitor.updateCompletedRequests(5000);
network_monitor.updateNetworkStats(120.5, 83.3); // latency, throughput

// Batch processing monitoring
BatchProcessingProgressMonitor batch_monitor(config);
batch_monitor.startBatch("Log Analysis", 50000);
batch_monitor.updateBatchProgress(35000, 1500); // processed, failed

std::map<std::string, size_t> categories;
categories["info"] = 25000;
categories["warning"] = 8500;
categories["error"] = 1500;
batch_monitor.reportBatchStats(categories);
```

#### RAII Helper Usage

```cpp
// Automatic lifecycle management
{
    std::vector<ProgressTaskConfig> tasks = createSomeTasks();
    AutoProgressMonitor auto_monitor("scan_operation", tasks, config);
    
    // Use monitor through RAII wrapper
    auto_monitor.updateProgress("task1", 50);
    auto_monitor.incrementProgress("task2", 25);
    auto_monitor.setTaskCompleted("task1");
    
    // Automatic cleanup when going out of scope
}
```

#### State Persistence

```cpp
// Save state during operation
monitor.saveState("/tmp/progress_state.json");

// Later, restore state
ProgressMonitor new_monitor(config);
new_monitor.loadState("/tmp/progress_state.json");
```

### Integration with Pipeline Engine

The Progress Monitor integrates seamlessly with the Pipeline Engine to provide automatic progress tracking:

```cpp
// Attach to pipeline for automatic monitoring
std::shared_ptr<PipelineEngine> pipeline = std::make_shared<PipelineEngine>(engine_config);
monitor.attachToPipeline(pipeline);

// Pipeline progress will automatically update the monitor
pipeline.executePipeline("my_pipeline");
```

### Performance Considerations

- **Update Throttling**: Use `THROTTLED` mode for high-frequency updates to prevent UI flooding
- **Memory Management**: History size is automatically limited to prevent excessive memory usage
- **Thread Safety**: All operations are thread-safe but avoid unnecessary concurrent access
- **Batch Operations**: Use batch update methods for multiple simultaneous changes
- **Display Optimization**: JSON mode for machine processing, SIMPLE_BAR for low-resource environments

### Error Handling

The system gracefully handles various error conditions:

- Invalid task IDs are ignored without throwing exceptions
- Out-of-bounds progress values are automatically clamped
- Missing dependencies are detected and reported
- Circular dependencies are prevented and detected
- Resource allocation failures are handled gracefully

### Best Practices

1. **Configuration**: Choose appropriate update modes based on your use case
2. **Task Design**: Use meaningful task IDs and weights for complex workflows
3. **Event Handling**: Implement event callbacks for important progress milestones
4. **Performance**: Enable batch mode for high-frequency updates
5. **Error Handling**: Always check return values for critical operations
6. **Cleanup**: Use RAII helpers or ensure proper stop() calls
7. **Testing**: Use the comprehensive test suite as examples for implementation

---

## Documentation Française

### Vue d'ensemble

Le Système de Moniteur de Progression fournit un suivi de progression en temps réel avec calcul d'ETA (Temps d'Arrivée Estimé) pour le framework BB-Pipeline. Ce système complet permet des rapports de progression visuels, une estimation intelligente du temps, et des capacités de surveillance flexibles pour toutes les opérations de pipeline.

### Fonctionnalités Clés

- **Suivi de Progression en Temps Réel**: Mises à jour en direct du statut de completion des tâches avec fréquences de mise à jour configurables
- **Calcul d'ETA Intelligent**: Algorithmes multiples pour l'estimation de temps incluant linéaire, moyenne mobile, lissage exponentiel, et stratégies adaptatives
- **Modes d'Affichage Multiples**: Support pour barres simples, affichages détaillés, pourcentage uniquement, compact, verbeux, JSON, et formats personnalisés
- **Gestion des Dépendances de Tâches**: Résolution avancée des dépendances avec détection automatique des tâches prêtes
- **Architecture Événementielle**: Système de callback complet pour les événements de progression et jalons
- **Persistance d'État**: Sauvegarder et restaurer l'état de surveillance entre les redémarrages d'application
- **Optimisation des Performances**: Gestion efficace de grands nombres de tâches avec mises à jour throttlées
- **Moniteurs Spécialisés**: Moniteurs pré-construits pour transferts de fichiers, opérations réseau, et traitement par lots
- **Support RAII**: Nettoyage automatique avec classes d'aide RAII
- **Opérations Thread-safe**: Sécurité thread complète pour accès concurrent
- **Design Extensible**: Formateurs et prédicteurs ETA pluggables pour personnalisation

### Architecture

#### Composants Principaux

1. **ProgressMonitor**: Classe de surveillance principale avec motif de design PIMPL
2. **ProgressMonitorManager**: Gestionnaire singleton pour coordonner plusieurs moniteurs
3. **Moniteurs Spécialisés**: FileTransferProgressMonitor, NetworkProgressMonitor, BatchProcessingProgressMonitor
4. **AutoProgressMonitor**: Assistant RAII pour la gestion automatique du cycle de vie
5. **Fonctions Utilitaires**: Fonctions d'aide pour configuration, génération de tâches, et formatage

#### Motifs de Design

- **PIMPL (Pointer to Implementation)**: Encapsule les détails d'implémentation et fournit une ABI stable
- **Motif Singleton**: Gestionnaire centralisé pour coordination globale
- **Motif Stratégie**: Stratégies de calcul ETA pluggables
- **Motif Observateur**: Callbacks événementiels pour notifications de progression
- **RAII (Resource Acquisition Is Initialization)**: Gestion automatique des ressources

### Options de Configuration

#### Modes de Mise à Jour de Progression

- `REAL_TIME`: Mettre à jour aussi fréquemment que possible pour feedback immédiat
- `THROTTLED`: Limiter la fréquence de mise à jour pour éviter le spam UI (recommandé)
- `ON_DEMAND`: Mettre à jour uniquement sur demande explicite
- `MILESTONE`: Mettre à jour uniquement sur des jalons de progression significatifs

#### Modes d'Affichage

- `SIMPLE_BAR`: Barre de progression ASCII basique avec pourcentage
- `DETAILED_BAR`: Barre améliorée avec vitesse, ETA, et statistiques
- `PERCENTAGE`: Affichage pourcentage uniquement pour UI minimale
- `COMPACT`: Format compact sur une ligne
- `VERBOSE`: Affichage détaillé multi-lignes avec statistiques complètes
- `JSON`: Format JSON lisible par machine pour intégration
- `CUSTOM`: Format défini par utilisateur avec callbacks de formateur personnalisé

#### Stratégies de Calcul ETA

- `LINEAR`: Extrapolation linéaire simple basée sur la progression actuelle
- `MOVING_AVERAGE`: Moyenne mobile des taux de progression récents
- `EXPONENTIAL`: Lissage exponentiel pour prédictions stables
- `ADAPTIVE`: Combinaison intelligente de stratégies multiples
- `WEIGHTED`: Calculs basés sur le poids des tâches pour workflows complexes
- `HISTORICAL`: Prédictions basées sur données historiques (extensible)

### Exemples d'Utilisation

#### Surveillance de Progression de Base

```cpp
#include "orchestrator/progress_monitor.hpp"
using namespace BBP::Orchestrator;

// Créer la configuration
auto config = ProgressUtils::createDefaultConfig();
config.show_eta = true;
config.show_speed = true;
config.enable_colors = true;

// Créer le moniteur
ProgressMonitor monitor(config);

// Ajouter des tâches
ProgressTaskConfig task;
task.id = "data_processing";
task.name = "Traitement de Données";
task.total_units = 1000;
task.weight = 1.0;

monitor.addTask(task);
monitor.start();

// Mettre à jour la progression
for (size_t i = 0; i <= 1000; i += 100) {
    monitor.updateProgress("data_processing", i);
    std::this_thread::sleep_for(100ms);
}

monitor.stop();
```

#### Coordination de Tâches Multiples

```cpp
// Créer plusieurs tâches avec différents poids
std::vector<ProgressTaskConfig> tasks;

ProgressTaskConfig task1;
task1.id = "download";
task1.name = "Téléchargement de Fichiers";
task1.total_units = 500;
task1.weight = 2.0; // Priorité plus élevée

ProgressTaskConfig task2;
task2.id = "process";
task2.name = "Traitement de Données";
task2.total_units = 300;
task2.weight = 1.0;

tasks.push_back(task1);
tasks.push_back(task2);

ProgressMonitor monitor(config);
monitor.start(tasks);

// Mettre à jour plusieurs tâches
monitor.updateProgress("download", 250);
monitor.updateProgress("process", 100);

// Opérations par lot
std::map<std::string, size_t> updates;
updates["download"] = 500;
updates["process"] = 200;
monitor.updateMultipleProgress(updates);
```

#### Gestion d'Événements

```cpp
// Configurer le callback d'événement
monitor.setEventCallback([](const ProgressEvent& event) {
    switch (event.type) {
        case ProgressEventType::STARTED:
            std::cout << "Surveillance de progression démarrée" << std::endl;
            break;
        case ProgressEventType::STAGE_COMPLETED:
            std::cout << "Tâche " << event.task_id << " terminée" << std::endl;
            break;
        case ProgressEventType::ETA_UPDATED:
            std::cout << "ETA mis à jour: " << event.statistics.estimated_remaining_time.count() << "ms" << std::endl;
            break;
        case ProgressEventType::COMPLETED:
            std::cout << "Toutes les tâches terminées!" << std::endl;
            break;
    }
});
```

#### Formatage Personnalisé

```cpp
// Formateur personnalisé pour affichage spécialisé
monitor.setCustomFormatter([](const ProgressStatistics& stats, const ProgressMonitorConfig& config) {
    std::ostringstream oss;
    oss << "🚀 Progression: " << std::fixed << std::setprecision(1) << stats.current_progress << "% ";
    oss << "(" << stats.completed_units << "/" << stats.total_units << ") ";
    if (stats.estimated_remaining_time.count() > 0) {
        oss << "⏱️ ETA: " << formatDuration(stats.estimated_remaining_time);
    }
    return oss.str();
});

config.display_mode = ProgressDisplayMode::CUSTOM;
monitor.updateConfig(config);
```

#### Moniteurs Spécialisés

```cpp
// Surveillance de transfert de fichier
FileTransferProgressMonitor file_monitor(config);
file_monitor.startTransfer("gros_fichier.zip", 104857600); // 100MB

// Mettre à jour pendant le transfert
file_monitor.updateTransferred(52428800); // 50MB transféré
file_monitor.setTransferRate(1048576); // 1MB/s

std::string info = file_monitor.getCurrentTransferInfo();
std::cout << info << std::endl;

// Surveillance d'opération réseau
NetworkProgressMonitor network_monitor(config);
network_monitor.startNetworkOperation("Scan API", 10000);
network_monitor.updateCompletedRequests(5000);
network_monitor.updateNetworkStats(120.5, 83.3); // latence, débit

// Surveillance de traitement par lot
BatchProcessingProgressMonitor batch_monitor(config);
batch_monitor.startBatch("Analyse de Logs", 50000);
batch_monitor.updateBatchProgress(35000, 1500); // traités, échoués

std::map<std::string, size_t> categories;
categories["info"] = 25000;
categories["warning"] = 8500;
categories["error"] = 1500;
batch_monitor.reportBatchStats(categories);
```

#### Utilisation d'Assistant RAII

```cpp
// Gestion automatique du cycle de vie
{
    std::vector<ProgressTaskConfig> tasks = createSomeTasks();
    AutoProgressMonitor auto_monitor("operation_scan", tasks, config);
    
    // Utiliser le moniteur via l'enveloppe RAII
    auto_monitor.updateProgress("task1", 50);
    auto_monitor.incrementProgress("task2", 25);
    auto_monitor.setTaskCompleted("task1");
    
    // Nettoyage automatique en sortant du scope
}
```

#### Persistance d'État

```cpp
// Sauvegarder l'état pendant l'opération
monitor.saveState("/tmp/progress_state.json");

// Plus tard, restaurer l'état
ProgressMonitor new_monitor(config);
new_monitor.loadState("/tmp/progress_state.json");
```

### Intégration avec le Moteur de Pipeline

Le Moniteur de Progression s'intègre parfaitement avec le Moteur de Pipeline pour fournir un suivi de progression automatique:

```cpp
// Attacher au pipeline pour surveillance automatique
std::shared_ptr<PipelineEngine> pipeline = std::make_shared<PipelineEngine>(engine_config);
monitor.attachToPipeline(pipeline);

// La progression du pipeline mettra automatiquement à jour le moniteur
pipeline.executePipeline("mon_pipeline");
```

### Considérations de Performance

- **Throttling de Mises à Jour**: Utiliser le mode `THROTTLED` pour mises à jour haute fréquence pour éviter l'inondation UI
- **Gestion Mémoire**: La taille d'historique est automatiquement limitée pour éviter l'usage mémoire excessif
- **Sécurité Thread**: Toutes les opérations sont thread-safe mais éviter l'accès concurrent inutile
- **Opérations par Lot**: Utiliser les méthodes de mise à jour par lot pour changements simultanés multiples
- **Optimisation d'Affichage**: Mode JSON pour traitement machine, SIMPLE_BAR pour environnements à ressources limitées

### Gestion d'Erreurs

Le système gère gracieusement diverses conditions d'erreur:

- Les IDs de tâche invalides sont ignorés sans lever d'exceptions
- Les valeurs de progression hors limites sont automatiquement écrêtées
- Les dépendances manquantes sont détectées et rapportées
- Les dépendances circulaires sont prévenues et détectées
- Les échecs d'allocation de ressources sont gérés gracieusement

### Bonnes Pratiques

1. **Configuration**: Choisir les modes de mise à jour appropriés selon votre cas d'usage
2. **Design de Tâches**: Utiliser des IDs de tâche significatifs et des poids pour workflows complexes
3. **Gestion d'Événements**: Implémenter des callbacks d'événement pour jalons de progression importants
4. **Performance**: Activer le mode batch pour mises à jour haute fréquence
5. **Gestion d'Erreurs**: Toujours vérifier les valeurs de retour pour opérations critiques
6. **Nettoyage**: Utiliser les assistants RAII ou assurer des appels stop() appropriés
7. **Test**: Utiliser la suite de tests comprehensive comme exemples pour l'implémentation

### API Reference / Référence API

#### Main Classes / Classes Principales

##### ProgressMonitor

**Constructor / Constructeur**
```cpp
explicit ProgressMonitor(const ProgressMonitorConfig& config = {});
```

**Task Management / Gestion des Tâches**
```cpp
bool addTask(const ProgressTaskConfig& task);
bool removeTask(const std::string& task_id);
bool updateTask(const std::string& task_id, const ProgressTaskConfig& task);
std::vector<std::string> getTaskIds() const;
std::optional<ProgressTaskConfig> getTask(const std::string& task_id) const;
void clearTasks();
```

**Lifecycle Control / Contrôle du Cycle de Vie**
```cpp
bool start();
bool start(const std::vector<ProgressTaskConfig>& tasks);
void stop();
void pause();
void resume();
bool isRunning() const;
bool isPaused() const;
```

**Progress Updates / Mises à Jour de Progression**
```cpp
void updateProgress(const std::string& task_id, size_t completed_units);
void updateProgress(const std::string& task_id, double percentage);
void incrementProgress(const std::string& task_id, size_t units = 1);
void setTaskCompleted(const std::string& task_id);
void setTaskFailed(const std::string& task_id, const std::string& error_message = "");
void reportMilestone(const std::string& task_id, const std::string& milestone_name);
```

**Statistics and Information / Statistiques et Informations**
```cpp
ProgressStatistics getOverallStatistics() const;
ProgressStatistics getTaskStatistics(const std::string& task_id) const;
std::map<std::string, ProgressStatistics> getAllTaskStatistics() const;
std::string getETAString(bool include_confidence = false) const;
std::string getCurrentDisplayString() const;
```

#### Configuration Structures / Structures de Configuration

##### ProgressMonitorConfig

```cpp
struct ProgressMonitorConfig {
    ProgressUpdateMode update_mode = ProgressUpdateMode::THROTTLED;
    ProgressDisplayMode display_mode = ProgressDisplayMode::DETAILED_BAR;
    ETACalculationStrategy eta_strategy = ETACalculationStrategy::ADAPTIVE;
    
    std::chrono::milliseconds update_interval{100};
    std::chrono::milliseconds refresh_interval{50};
    size_t moving_average_window = 10;
    double eta_confidence_threshold = 0.7;
    size_t max_history_size = 1000;
    
    bool enable_colors = true;
    bool show_eta = true;
    bool show_speed = true;
    bool show_statistics = false;
    bool auto_hide_on_complete = true;
    
    std::string progress_bar_chars = "█▇▆▅▄▃▂▁ ";
    size_t progress_bar_width = 50;
    std::ostream* output_stream = &std::cout;
    std::string log_file_path;
    bool enable_file_logging = false;
};
```

##### ProgressTaskConfig

```cpp
struct ProgressTaskConfig {
    std::string id;
    std::string name;
    std::string description;
    size_t total_units = 1;
    double weight = 1.0;
    std::chrono::milliseconds estimated_duration{0};
    std::map<std::string, std::string> metadata;
    std::vector<std::string> dependencies;
    bool allow_parallel = true;
    double complexity_factor = 1.0;
};
```

#### Utility Functions / Fonctions Utilitaires

##### Configuration Helpers / Assistants de Configuration

```cpp
ProgressMonitorConfig createDefaultConfig();
ProgressMonitorConfig createQuietConfig();
ProgressMonitorConfig createVerboseConfig();
ProgressMonitorConfig createFileTransferConfig();
ProgressMonitorConfig createNetworkConfig();
ProgressMonitorConfig createBatchProcessingConfig();
```

##### Task Generation / Génération de Tâches

```cpp
std::vector<ProgressTaskConfig> createTasksFromFileList(const std::vector<std::string>& filenames);
std::vector<ProgressTaskConfig> createTasksFromRange(const std::string& base_name, size_t count);
ProgressTaskConfig createSimpleTask(const std::string& name, size_t total_units);
```

##### Display Utilities / Utilitaires d'Affichage

```cpp
std::string createProgressBar(double percentage, size_t width = 50, char filled = '#', char empty = '-');
std::string createColoredProgressBar(double percentage, size_t width = 50);
std::string formatBytes(size_t bytes);
std::string formatRate(double rate, const std::string& unit);
```

##### ETA Calculations / Calculs ETA

```cpp
std::chrono::milliseconds calculateLinearETA(double current_progress, std::chrono::milliseconds elapsed);
std::chrono::milliseconds calculateMovingAverageETA(const std::vector<double>& progress_history,
                                                  const std::vector<std::chrono::milliseconds>& time_history);
double calculateETAConfidence(const std::vector<double>& eta_errors);
```

### Threading and Concurrency / Threading et Concurrence

The Progress Monitor system is designed to be fully thread-safe:

Le système de Moniteur de Progression est conçu pour être entièrement thread-safe:

- **Thread-safe Operations / Opérations Thread-safe**: All public methods can be called from multiple threads
- **Internal Synchronization / Synchronisation Interne**: Uses mutexes and atomic operations for data protection
- **Display Thread / Thread d'Affichage**: Separate background thread for display updates
- **Event Callbacks / Callbacks d'Événements**: Events are dispatched from a single thread context
- **Performance / Performance**: Minimal locking overhead with optimized critical sections

### Memory Management / Gestion Mémoire

- **PIMPL Pattern / Motif PIMPL**: Stable ABI and reduced compilation dependencies
- **Smart Pointers / Pointeurs Intelligents**: Automatic memory management with std::unique_ptr and std::shared_ptr
- **History Limits / Limites d'Historique**: Automatic cleanup of old data to prevent memory leaks
- **RAII Helpers / Assistants RAII**: Automatic resource cleanup on scope exit
- **Exception Safety / Sécurité d'Exception**: Strong exception safety guarantee for all operations

### Extension Points / Points d'Extension

The system provides several extension mechanisms:

Le système fournit plusieurs mécanismes d'extension:

- **Custom Formatters / Formateurs Personnalisés**: Implement custom display formats
- **ETA Predictors / Prédicteurs ETA**: Plug in custom ETA calculation algorithms
- **Event Callbacks / Callbacks d'Événements**: React to progress events and milestones
- **Specialized Monitors / Moniteurs Spécialisés**: Extend base class for domain-specific monitoring
- **Configuration Options / Options de Configuration**: Extensive configuration for behavior customization

This comprehensive system provides all the necessary tools for professional progress monitoring in high-performance applications while maintaining ease of use and flexibility for various use cases.

Ce système complet fournit tous les outils nécessaires pour la surveillance de progression professionnelle dans les applications haute performance tout en maintenant la facilité d'utilisation et la flexibilité pour divers cas d'usage.