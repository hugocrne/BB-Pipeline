# Resume System - Système de Reprise

## English Documentation

### Overview

The Resume System is a sophisticated checkpoint and recovery mechanism designed for the BB-Pipeline framework. It provides intelligent crash recovery capabilities by automatically creating checkpoints during pipeline execution and enabling seamless resume operations after system failures or interruptions.

### Key Features

#### Intelligent Checkpointing
- **Multiple Strategies**: Time-based, progress-based, hybrid, manual, and adaptive checkpointing strategies
- **Granular Control**: Coarse, medium, and fine-grained checkpoint creation
- **Automatic Management**: Self-managing checkpoint lifecycle with configurable retention policies
- **Verification**: Built-in checkpoint integrity verification with hash-based validation

#### Crash Recovery
- **Automatic Detection**: Detects crashed operations and available recovery points
- **Multiple Resume Modes**: Full restart, last checkpoint, best checkpoint, and interactive recovery
- **State Preservation**: Complete pipeline state restoration including completed stages and intermediate results
- **Intelligent Selection**: Automatically selects optimal recovery points based on progress and timestamp

#### Data Security
- **Compression**: Optional checkpoint data compression to reduce storage footprint
- **Encryption**: Configurable encryption for sensitive checkpoint data
- **Verification**: Hash-based integrity verification to ensure data consistency
- **Safe Storage**: Atomic checkpoint operations with rollback capabilities

#### Monitoring & Statistics
- **Real-time Monitoring**: Live progress tracking and checkpoint creation events
- **Comprehensive Statistics**: Detailed metrics on checkpoint creation, recovery success rates, and performance
- **Event Callbacks**: Customizable callbacks for progress updates, checkpoint creation, and recovery events
- **Performance Metrics**: Overhead tracking and optimization recommendations

### Architecture

#### Core Components

1. **ResumeSystem**: Main class providing checkpoint and recovery functionality
2. **ResumeSystemManager**: Singleton manager for global resume operations
3. **AutoCheckpointGuard**: RAII helper for automatic checkpoint management
4. **CheckpointStorage**: Pluggable storage interface (file-based implementation included)

#### Configuration System

```cpp
struct CheckpointConfig {
    std::string checkpoint_dir;                    // Storage directory
    CheckpointStrategy strategy;                   // Checkpoint strategy
    CheckpointGranularity granularity;             // Checkpoint granularity level
    std::chrono::seconds time_interval;            // Time-based interval
    double progress_threshold;                     // Progress-based threshold
    size_t max_checkpoints;                        // Maximum checkpoints to keep
    bool enable_compression;                       // Enable data compression
    bool enable_encryption;                        // Enable data encryption
    std::string encryption_key;                    // Encryption key
    bool enable_verification;                      // Enable integrity verification
    size_t max_memory_threshold_mb;                // Memory threshold for adaptive strategy
    bool auto_cleanup;                             // Automatic cleanup of old checkpoints
    std::chrono::hours cleanup_age;                // Age threshold for cleanup
};
```

#### Checkpoint Strategies

1. **Time-Based**: Creates checkpoints at regular time intervals
2. **Progress-Based**: Creates checkpoints when progress thresholds are reached
3. **Hybrid**: Combines time and progress-based strategies
4. **Manual**: Checkpoints created only on explicit request
5. **Adaptive**: Intelligent checkpointing based on system load and memory usage

### Usage Examples

#### Basic Setup

```cpp
#include "orchestrator/resume_system.hpp"

// Configure checkpoint settings
BBP::CheckpointConfig config;
config.checkpoint_dir = "./checkpoints";
config.strategy = BBP::CheckpointStrategy::HYBRID;
config.time_interval = std::chrono::minutes(5);
config.progress_threshold = 10.0; // 10%
config.max_checkpoints = 10;
config.enable_compression = true;
config.enable_verification = true;

// Create resume system
BBP::ResumeSystem resume_system(config);
resume_system.initialize();
```

#### Creating Checkpoints

```cpp
// Start monitoring an operation
std::string operation_id = "my_pipeline_run";
resume_system.startMonitoring(operation_id, "/path/to/config.yaml");

// Create manual checkpoint
nlohmann::json pipeline_state = {
    {"current_stage", "data_processing"},
    {"completed_stages", {"init", "load_data"}},
    {"pending_stages", {"process", "analyze", "report"}},
    {"progress", 40.0}
};

std::map<std::string, std::string> metadata;
metadata["stage_type"] = "processing";
metadata["batch_size"] = "1000";

std::string checkpoint_id = resume_system.createCheckpoint(
    "data_processing", pipeline_state, metadata);

// Create automatic checkpoint with progress
checkpoint_id = resume_system.createAutomaticCheckpoint(
    "analysis_stage", pipeline_state, 75.0);
```

#### Resuming from Checkpoints

```cpp
// Check if resume is possible
if (resume_system.canResume(operation_id)) {
    // Get available resume points
    auto resume_points = resume_system.getAvailableResumePoints(operation_id);
    
    for (const auto& point : resume_points) {
        std::cout << "Checkpoint: " << point.checkpoint_id 
                  << ", Stage: " << point.stage_name
                  << ", Progress: " << point.progress_percentage << "%" << std::endl;
    }
    
    // Resume from specific checkpoint
    auto context = resume_system.resumeFromCheckpoint(
        checkpoint_id, BBP::ResumeMode::LAST_CHECKPOINT);
    
    if (context) {
        std::cout << "Resumed operation: " << context->operation_id << std::endl;
        std::cout << "Resume mode: " << static_cast<int>(context->resume_mode) << std::endl;
        
        // Access restored state
        for (const auto& stage : context->completed_stages) {
            std::cout << "Completed stage: " << stage << std::endl;
        }
    }
}

// Or use automatic resume (finds best checkpoint)
auto auto_context = resume_system.resumeAutomatically(operation_id);
```

#### Using AutoCheckpointGuard

```cpp
{
    // RAII checkpoint management
    BBP::AutoCheckpointGuard guard(operation_id, "critical_stage", resume_system);
    
    // Set initial pipeline state
    nlohmann::json state = {{"stage", "critical_processing"}};
    guard.setPipelineState(state);
    
    // Add custom metadata
    guard.addMetadata("process_type", "batch_processing");
    guard.addMetadata("input_size", "large");
    
    // Update progress (automatically creates checkpoints at significant milestones)
    for (int i = 0; i <= 100; i += 10) {
        // ... do processing work ...
        guard.updateProgress(i);
        
        // Update state as processing continues
        state["progress"] = i;
        guard.setPipelineState(state);
    }
    
    // Guard destructor automatically creates final checkpoint
}
```

#### Event Callbacks

```cpp
// Set progress callback
resume_system.setProgressCallback([](const std::string& operation_id, double progress) {
    std::cout << "Operation " << operation_id << " progress: " << progress << "%" << std::endl;
});

// Set checkpoint callback
resume_system.setCheckpointCallback([](const std::string& checkpoint_id, const BBP::CheckpointMetadata& metadata) {
    std::cout << "Checkpoint created: " << checkpoint_id 
              << " for stage: " << metadata.stage_name 
              << " at " << metadata.progress_percentage << "% progress" << std::endl;
});

// Set recovery callback
resume_system.setRecoveryCallback([](const std::string& checkpoint_id, bool success) {
    if (success) {
        std::cout << "Successfully recovered from checkpoint: " << checkpoint_id << std::endl;
    } else {
        std::cout << "Failed to recover from checkpoint: " << checkpoint_id << std::endl;
    }
});
```

### Global Manager Usage

```cpp
// Initialize global manager
auto& manager = BBP::ResumeSystemManager::getInstance();
manager.initialize(config);

// Register pipeline for automatic management
manager.registerPipeline("pipeline_1", &my_pipeline_engine);

// Detect crashed operations
auto crashed_ops = manager.detectCrashedOperations();
for (const auto& op_id : crashed_ops) {
    std::cout << "Detected crashed operation: " << op_id << std::endl;
    
    // Attempt automatic recovery
    if (manager.attemptAutomaticRecovery(op_id)) {
        std::cout << "Successfully recovered: " << op_id << std::endl;
    }
}

// Get global statistics
auto stats = manager.getGlobalStatistics();
std::cout << "Total checkpoints: " << stats.total_checkpoints_created << std::endl;
std::cout << "Successful resumes: " << stats.successful_resumes << std::endl;
std::cout << "Average recovery time: " << stats.average_recovery_time.count() << "ms" << std::endl;
```

### Utility Functions

```cpp
// Create predefined configurations
auto default_config = BBP::ResumeSystemUtils::createDefaultConfig();
auto high_freq_config = BBP::ResumeSystemUtils::createHighFrequencyConfig();
auto low_overhead_config = BBP::ResumeSystemUtils::createLowOverheadConfig();

// Validate configuration
if (!BBP::ResumeSystemUtils::validateConfig(config)) {
    std::cerr << "Invalid checkpoint configuration" << std::endl;
}

// Generate unique operation ID
std::string op_id = BBP::ResumeSystemUtils::generateOperationId();

// Parse resume context from command line
std::vector<std::string> args = {"--resume-operation", "op_123", "--resume-mode", "best"};
auto resume_context = BBP::ResumeSystemUtils::parseResumeContext(args);
```

### Performance Considerations

#### Checkpoint Frequency
- Balance between recovery granularity and performance overhead
- Use adaptive strategy for automatic optimization
- Monitor checkpoint overhead percentage in statistics

#### Storage Optimization
- Enable compression for large pipeline states
- Configure appropriate cleanup policies
- Use SSD storage for better I/O performance

#### Memory Management
- Set appropriate memory thresholds for adaptive checkpointing
- Monitor memory footprint in checkpoint metadata
- Consider checkpoint granularity based on available memory

#### Security Considerations
- Enable encryption for sensitive pipeline data
- Use secure storage locations with appropriate permissions
- Implement proper key management for encrypted checkpoints

---

## Documentation Française

### Aperçu

Le Système de Reprise est un mécanisme sophistiqué de checkpoint et de récupération conçu pour le framework BB-Pipeline. Il fournit des capacités intelligentes de récupération après crash en créant automatiquement des checkpoints pendant l'exécution du pipeline et en permettant des opérations de reprise seamless après des pannes système ou des interruptions.

### Fonctionnalités Principales

#### Checkpointing Intelligent
- **Stratégies Multiples**: Stratégies de checkpointing basées sur le temps, la progression, hybrides, manuelles et adaptatives
- **Contrôle Granulaire**: Création de checkpoints à grain grossier, moyen et fin
- **Gestion Automatique**: Cycle de vie auto-géré des checkpoints avec politiques de rétention configurables
- **Vérification**: Vérification intégrée de l'intégrité des checkpoints avec validation basée sur hash

#### Récupération après Crash
- **Détection Automatique**: Détecte les opérations crashées et les points de récupération disponibles
- **Modes de Reprise Multiples**: Redémarrage complet, dernier checkpoint, meilleur checkpoint et récupération interactive
- **Préservation d'État**: Restauration complète de l'état du pipeline incluant les étapes terminées et résultats intermédiaires
- **Sélection Intelligente**: Sélectionne automatiquement les points de récupération optimaux basés sur la progression et timestamp

#### Sécurité des Données
- **Compression**: Compression optionnelle des données de checkpoint pour réduire l'empreinte de stockage
- **Chiffrement**: Chiffrement configurable pour les données sensibles de checkpoint
- **Vérification**: Vérification d'intégrité basée sur hash pour assurer la cohérence des données
- **Stockage Sécurisé**: Opérations de checkpoint atomiques avec capacités de rollback

#### Monitoring et Statistiques
- **Monitoring Temps Réel**: Suivi live de la progression et événements de création de checkpoint
- **Statistiques Complètes**: Métriques détaillées sur la création de checkpoint, taux de succès de récupération et performance
- **Callbacks d'Événement**: Callbacks personnalisables pour mises à jour de progression, création de checkpoint et événements de récupération
- **Métriques de Performance**: Suivi de surcharge et recommandations d'optimisation

### Architecture

#### Composants Principaux

1. **ResumeSystem**: Classe principale fournissant les fonctionnalités de checkpoint et récupération
2. **ResumeSystemManager**: Gestionnaire singleton pour les opérations de reprise globales
3. **AutoCheckpointGuard**: Helper RAII pour la gestion automatique de checkpoint
4. **CheckpointStorage**: Interface de stockage pluggable (implémentation basée fichier incluse)

#### Système de Configuration

```cpp
struct CheckpointConfig {
    std::string checkpoint_dir;                    // Répertoire de stockage
    CheckpointStrategy strategy;                   // Stratégie de checkpoint
    CheckpointGranularity granularity;             // Niveau de granularité checkpoint
    std::chrono::seconds time_interval;            // Intervalle basé temps
    double progress_threshold;                     // Seuil basé progression
    size_t max_checkpoints;                        // Maximum checkpoints à garder
    bool enable_compression;                       // Active compression données
    bool enable_encryption;                        // Active chiffrement données
    std::string encryption_key;                    // Clé de chiffrement
    bool enable_verification;                      // Active vérification intégrité
    size_t max_memory_threshold_mb;                // Seuil mémoire pour stratégie adaptive
    bool auto_cleanup;                             // Nettoyage automatique anciens checkpoints
    std::chrono::hours cleanup_age;                // Seuil d'âge pour nettoyage
};
```

#### Stratégies de Checkpoint

1. **Basé Temps**: Crée des checkpoints à intervalles de temps réguliers
2. **Basé Progression**: Crée des checkpoints quand les seuils de progression sont atteints
3. **Hybride**: Combine les stratégies basées temps et progression
4. **Manuel**: Checkpoints créés seulement sur demande explicite
5. **Adaptatif**: Checkpointing intelligent basé sur la charge système et utilisation mémoire

### Exemples d'Usage

#### Configuration de Base

```cpp
#include "orchestrator/resume_system.hpp"

// Configure les paramètres de checkpoint
BBP::CheckpointConfig config;
config.checkpoint_dir = "./checkpoints";
config.strategy = BBP::CheckpointStrategy::HYBRID;
config.time_interval = std::chrono::minutes(5);
config.progress_threshold = 10.0; // 10%
config.max_checkpoints = 10;
config.enable_compression = true;
config.enable_verification = true;

// Crée le système de reprise
BBP::ResumeSystem resume_system(config);
resume_system.initialize();
```

#### Création de Checkpoints

```cpp
// Commence le monitoring d'une opération
std::string operation_id = "my_pipeline_run";
resume_system.startMonitoring(operation_id, "/path/to/config.yaml");

// Crée un checkpoint manuel
nlohmann::json pipeline_state = {
    {"current_stage", "data_processing"},
    {"completed_stages", {"init", "load_data"}},
    {"pending_stages", {"process", "analyze", "report"}},
    {"progress", 40.0}
};

std::map<std::string, std::string> metadata;
metadata["stage_type"] = "processing";
metadata["batch_size"] = "1000";

std::string checkpoint_id = resume_system.createCheckpoint(
    "data_processing", pipeline_state, metadata);

// Crée un checkpoint automatique avec progression
checkpoint_id = resume_system.createAutomaticCheckpoint(
    "analysis_stage", pipeline_state, 75.0);
```

#### Reprise depuis Checkpoints

```cpp
// Vérifie si la reprise est possible
if (resume_system.canResume(operation_id)) {
    // Obtient les points de reprise disponibles
    auto resume_points = resume_system.getAvailableResumePoints(operation_id);
    
    for (const auto& point : resume_points) {
        std::cout << "Checkpoint: " << point.checkpoint_id 
                  << ", Étape: " << point.stage_name
                  << ", Progression: " << point.progress_percentage << "%" << std::endl;
    }
    
    // Reprend depuis un checkpoint spécifique
    auto context = resume_system.resumeFromCheckpoint(
        checkpoint_id, BBP::ResumeMode::LAST_CHECKPOINT);
    
    if (context) {
        std::cout << "Opération reprise: " << context->operation_id << std::endl;
        std::cout << "Mode reprise: " << static_cast<int>(context->resume_mode) << std::endl;
        
        // Accède à l'état restauré
        for (const auto& stage : context->completed_stages) {
            std::cout << "Étape terminée: " << stage << std::endl;
        }
    }
}

// Ou utilise la reprise automatique (trouve le meilleur checkpoint)
auto auto_context = resume_system.resumeAutomatically(operation_id);
```

#### Utilisation d'AutoCheckpointGuard

```cpp
{
    // Gestion de checkpoint RAII
    BBP::AutoCheckpointGuard guard(operation_id, "critical_stage", resume_system);
    
    // Définit l'état initial du pipeline
    nlohmann::json state = {{"stage", "critical_processing"}};
    guard.setPipelineState(state);
    
    // Ajoute des métadonnées personnalisées
    guard.addMetadata("process_type", "batch_processing");
    guard.addMetadata("input_size", "large");
    
    // Met à jour la progression (crée automatiquement des checkpoints aux jalons significatifs)
    for (int i = 0; i <= 100; i += 10) {
        // ... fait le travail de traitement ...
        guard.updateProgress(i);
        
        // Met à jour l'état pendant que le traitement continue
        state["progress"] = i;
        guard.setPipelineState(state);
    }
    
    // Le destructeur du guard crée automatiquement le checkpoint final
}
```

#### Callbacks d'Événement

```cpp
// Définit le callback de progression
resume_system.setProgressCallback([](const std::string& operation_id, double progress) {
    std::cout << "Opération " << operation_id << " progression: " << progress << "%" << std::endl;
});

// Définit le callback de checkpoint
resume_system.setCheckpointCallback([](const std::string& checkpoint_id, const BBP::CheckpointMetadata& metadata) {
    std::cout << "Checkpoint créé: " << checkpoint_id 
              << " pour étape: " << metadata.stage_name 
              << " à " << metadata.progress_percentage << "% progression" << std::endl;
});

// Définit le callback de récupération
resume_system.setRecoveryCallback([](const std::string& checkpoint_id, bool success) {
    if (success) {
        std::cout << "Récupération réussie depuis checkpoint: " << checkpoint_id << std::endl;
    } else {
        std::cout << "Échec récupération depuis checkpoint: " << checkpoint_id << std::endl;
    }
});
```

### Usage du Gestionnaire Global

```cpp
// Initialise le gestionnaire global
auto& manager = BBP::ResumeSystemManager::getInstance();
manager.initialize(config);

// Enregistre le pipeline pour gestion automatique
manager.registerPipeline("pipeline_1", &my_pipeline_engine);

// Détecte les opérations crashées
auto crashed_ops = manager.detectCrashedOperations();
for (const auto& op_id : crashed_ops) {
    std::cout << "Opération crashée détectée: " << op_id << std::endl;
    
    // Tente la récupération automatique
    if (manager.attemptAutomaticRecovery(op_id)) {
        std::cout << "Récupération réussie: " << op_id << std::endl;
    }
}

// Obtient les statistiques globales
auto stats = manager.getGlobalStatistics();
std::cout << "Total checkpoints: " << stats.total_checkpoints_created << std::endl;
std::cout << "Reprises réussies: " << stats.successful_resumes << std::endl;
std::cout << "Temps récupération moyen: " << stats.average_recovery_time.count() << "ms" << std::endl;
```

### Fonctions Utilitaires

```cpp
// Crée des configurations prédéfinies
auto default_config = BBP::ResumeSystemUtils::createDefaultConfig();
auto high_freq_config = BBP::ResumeSystemUtils::createHighFrequencyConfig();
auto low_overhead_config = BBP::ResumeSystemUtils::createLowOverheadConfig();

// Valide la configuration
if (!BBP::ResumeSystemUtils::validateConfig(config)) {
    std::cerr << "Configuration de checkpoint invalide" << std::endl;
}

// Génère un ID d'opération unique
std::string op_id = BBP::ResumeSystemUtils::generateOperationId();

// Parse le contexte de reprise depuis la ligne de commande
std::vector<std::string> args = {"--resume-operation", "op_123", "--resume-mode", "best"};
auto resume_context = BBP::ResumeSystemUtils::parseResumeContext(args);
```

### Considérations de Performance

#### Fréquence de Checkpoint
- Équilibre entre granularité de récupération et surcharge de performance
- Utilise la stratégie adaptive pour optimisation automatique
- Monitor le pourcentage de surcharge de checkpoint dans les statistiques

#### Optimisation de Stockage
- Active la compression pour les gros états de pipeline
- Configure des politiques de nettoyage appropriées
- Utilise du stockage SSD pour de meilleures performances I/O

#### Gestion Mémoire
- Définit des seuils mémoire appropriés pour checkpointing adaptatif
- Monitor l'empreinte mémoire dans les métadonnées de checkpoint
- Considère la granularité de checkpoint basée sur la mémoire disponible

#### Considérations de Sécurité
- Active le chiffrement pour données sensibles de pipeline
- Utilise des emplacements de stockage sécurisés avec permissions appropriées
- Implémente une gestion de clés appropriée pour checkpoints chiffrés

### API Reference

#### ResumeSystem Class

**Constructor**
```cpp
explicit ResumeSystem(const CheckpointConfig& config = CheckpointConfig{});
```

**Core Methods**
```cpp
bool initialize();
void shutdown();
bool startMonitoring(const std::string& operation_id, const std::string& pipeline_config_path);
void stopMonitoring();
std::string createCheckpoint(const std::string& stage_name, const nlohmann::json& pipeline_state, const std::map<std::string, std::string>& custom_metadata = {});
std::string createAutomaticCheckpoint(const std::string& stage_name, const nlohmann::json& pipeline_state, double progress_percentage = 0.0);
std::optional<ResumeContext> resumeFromCheckpoint(const std::string& checkpoint_id, ResumeMode mode = ResumeMode::LAST_CHECKPOINT);
std::optional<ResumeContext> resumeAutomatically(const std::string& operation_id);
```

**Management Methods**
```cpp
bool canResume(const std::string& operation_id) const;
std::vector<CheckpointMetadata> getAvailableResumePoints(const std::string& operation_id) const;
bool verifyCheckpoint(const std::string& checkpoint_id) const;
std::vector<std::string> listCheckpoints(const std::string& operation_id = "") const;
bool deleteCheckpoint(const std::string& checkpoint_id);
size_t cleanupOldCheckpoints();
```

**Configuration & Statistics**
```cpp
ResumeState getCurrentState() const;
ResumeStatistics getStatistics() const;
void resetStatistics();
void updateConfig(const CheckpointConfig& config);
const CheckpointConfig& getConfig() const;
```

**Callbacks**
```cpp
void setProgressCallback(std::function<void(const std::string&, double)> callback);
void setCheckpointCallback(std::function<void(const std::string&, const CheckpointMetadata&)> callback);
void setRecoveryCallback(std::function<void(const std::string&, bool)> callback);
```

#### ResumeSystemManager Class

```cpp
static ResumeSystemManager& getInstance();
bool initialize(const CheckpointConfig& config = CheckpointConfig{});
void shutdown();
ResumeSystem& getResumeSystem();
bool registerPipeline(const std::string& pipeline_id, PipelineEngine* pipeline);
void unregisterPipeline(const std::string& pipeline_id);
std::vector<std::string> detectCrashedOperations();
bool attemptAutomaticRecovery(const std::string& operation_id);
ResumeStatistics getGlobalStatistics() const;
```

#### AutoCheckpointGuard Class

```cpp
AutoCheckpointGuard(const std::string& operation_id, const std::string& stage_name, ResumeSystem& resume_system);
void updateProgress(double percentage);
void setPipelineState(const nlohmann::json& state);
void addMetadata(const std::string& key, const std::string& value);
std::string forceCheckpoint();
```

### Error Handling

The Resume System provides comprehensive error handling through:

- **Exception Safety**: All operations are exception-safe with proper resource cleanup
- **Validation**: Input validation for all public methods
- **Logging**: Detailed logging of errors and recovery attempts
- **Graceful Degradation**: System continues to operate even if checkpointing fails
- **Recovery Verification**: Automatic verification of recovered state integrity

### Thread Safety

The Resume System is fully thread-safe:

- **Concurrent Access**: Multiple threads can safely access the same ResumeSystem instance
- **Atomic Operations**: All state changes are atomic
- **Lock-Free Paths**: Read-only operations use lock-free implementations where possible
- **Deadlock Prevention**: Careful lock ordering prevents deadlock scenarios

### Integration with BB-Pipeline

The Resume System integrates seamlessly with the BB-Pipeline framework:

- **Pipeline Engine Integration**: Direct integration with PipelineEngine for automatic checkpointing
- **CSV Module Compatibility**: Preserves CSV processing state and intermediate results
- **Configuration System**: Uses BB-Pipeline's configuration management
- **Logging Integration**: Uses BB-Pipeline's structured logging system

### Troubleshooting

#### Common Issues

**Checkpoint Creation Fails**
- Check disk space in checkpoint directory
- Verify directory permissions
- Ensure configuration is valid

**Resume Operation Fails**
- Verify checkpoint integrity
- Check for corrupted checkpoint files
- Ensure matching pipeline configuration

**High Memory Usage**
- Reduce checkpoint granularity
- Enable compression
- Adjust memory thresholds

**Slow Performance**
- Use SSD storage for checkpoints
- Reduce checkpoint frequency
- Monitor checkpoint overhead statistics

#### Debug Information

Enable detailed logging to get comprehensive debug information:

```cpp
resume_system.setDetailedLogging(true);
```

Check statistics for performance insights:

```cpp
auto stats = resume_system.getStatistics();
std::cout << "Checkpoint overhead: " << stats.checkpoint_overhead_percentage << "%" << std::endl;
std::cout << "Average recovery time: " << stats.average_recovery_time.count() << "ms" << std::endl;
```