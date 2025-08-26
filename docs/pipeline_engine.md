# Pipeline Engine Documentation / Documentation du Moteur de Pipeline

## English

### Overview

The Pipeline Engine is a comprehensive orchestration system designed for the BB-Pipeline framework. It provides advanced capabilities for executing sequential, parallel, and hybrid pipelines with sophisticated dependency management, error handling, and monitoring features.

### Architecture

The Pipeline Engine follows a modular architecture with several key components:

#### Core Components

1. **PipelineEngine** - Main orchestration class
2. **PipelineTask** - Individual stage execution unit
3. **PipelineDependencyResolver** - Dependency analysis and ordering
4. **PipelineExecutionContext** - Runtime execution environment
5. **PipelineUtils** - Utility functions for validation and I/O

#### Key Features

- **Multi-Mode Execution**: Sequential, parallel, and hybrid execution modes
- **Dependency Resolution**: Automatic dependency ordering with circular dependency detection
- **Error Strategies**: Fail-fast, continue, retry, and skip error handling
- **Progress Monitoring**: Real-time progress tracking with event callbacks
- **State Management**: Pipeline state persistence and recovery
- **Performance Monitoring**: Comprehensive statistics and metrics
- **Configuration Management**: Flexible configuration with YAML/JSON support

### Usage Examples

#### Basic Pipeline Creation

```cpp
#include \"orchestrator/pipeline_engine.hpp\"

using namespace BBP::Orchestrator;

// Create pipeline engine
PipelineEngine::Config config;
config.thread_pool_size = 8;
config.enable_metrics = true;
PipelineEngine engine(config);

// Create pipeline stages
std::vector<PipelineStageConfig> stages;

PipelineStageConfig stage1;
stage1.id = \"build\";
stage1.name = \"Build Stage\";
stage1.executable = \"make\";
stage1.arguments = {\"build\"};
stage1.timeout = std::chrono::seconds(300);
stages.push_back(stage1);

PipelineStageConfig stage2;
stage2.id = \"test\";
stage2.name = \"Test Stage\";
stage2.executable = \"make\";
stage2.arguments = {\"test\"};
stage2.dependencies = {\"build\"};
stage2.timeout = std::chrono::seconds(600);
stages.push_back(stage2);

// Create and execute pipeline
std::string pipeline_id = engine.createPipeline(stages);
auto stats = engine.executePipeline(pipeline_id);

std::cout << \"Execution completed: \" << stats.success_rate * 100 << \"% success rate\" << std::endl;
```

#### Advanced Pipeline Configuration

```cpp
// Configure execution parameters
PipelineExecutionConfig exec_config;
exec_config.execution_mode = PipelineExecutionMode::HYBRID;
exec_config.error_strategy = PipelineErrorStrategy::CONTINUE;
exec_config.max_concurrent_stages = 4;
exec_config.global_timeout = std::chrono::minutes(30);
exec_config.enable_progress_reporting = true;

// Execute with custom configuration
auto future = engine.executePipelineAsync(pipeline_id, exec_config);
auto stats = future.get();
```

#### Event Monitoring

```cpp
// Register event callback for monitoring
engine.registerEventCallback([](const PipelineEvent& event) {
    std::cout << \"[\" << PipelineUtils::formatTimestamp(event.timestamp) << \"] \"
              << \"Pipeline: \" << event.pipeline_id 
              << \", Stage: \" << event.stage_id
              << \", Event: \" << static_cast<int>(event.type) << std::endl;
});
```

#### Progress Tracking

```cpp
// Monitor pipeline progress
while (auto progress = engine.getPipelineProgress(pipeline_id)) {
    std::cout << \"Progress: \" << progress->completion_percentage << \"%\"
              << \" (\" << progress->completed_stages << \"/\" << progress->total_stages << \")\"
              << std::endl;
    
    if (progress->isComplete()) {
        break;
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

### Configuration Options

#### Pipeline Engine Configuration

- **thread_pool_size**: Number of worker threads (default: hardware_concurrency)
- **max_concurrent_pipelines**: Maximum simultaneous pipeline executions (default: 1)
- **enable_metrics**: Enable performance metrics collection (default: true)
- **enable_logging**: Enable detailed logging (default: true)
- **health_check_interval**: Health check frequency (default: 30 seconds)
- **max_pipeline_history**: Maximum stored execution history (default: 100)

#### Pipeline Execution Configuration

- **execution_mode**: Sequential, Parallel, or Hybrid execution
- **error_strategy**: Fail-fast, Continue, Retry, or Skip on errors
- **max_concurrent_stages**: Maximum parallel stage execution
- **global_timeout**: Overall pipeline timeout
- **enable_progress_reporting**: Enable progress updates
- **enable_checkpointing**: Enable state checkpointing
- **dry_run**: Simulate execution without running commands

#### Stage Configuration

- **id**: Unique stage identifier
- **name**: Human-readable stage name
- **description**: Stage description
- **executable**: Command to execute
- **arguments**: Command line arguments
- **dependencies**: List of prerequisite stages
- **environment**: Environment variables
- **working_directory**: Execution directory
- **priority**: Execution priority (LOW, NORMAL, HIGH, CRITICAL)
- **timeout**: Stage execution timeout
- **max_retries**: Maximum retry attempts
- **retry_delay**: Delay between retries
- **allow_failure**: Continue pipeline on stage failure

### Performance Considerations

#### Optimization Tips

1. **Parallel Execution**: Use PARALLEL or HYBRID modes for independent stages
2. **Thread Pool Sizing**: Configure thread_pool_size based on available CPU cores
3. **Timeout Management**: Set appropriate timeouts to prevent hanging
4. **Memory Management**: Monitor memory usage with large pipelines
5. **Dependency Optimization**: Minimize deep dependency chains

#### Scalability

- **Horizontal Scaling**: Multiple engine instances for distributed execution
- **Resource Management**: Automatic resource allocation and cleanup
- **Memory Efficiency**: Lazy loading and memory pooling
- **Network Optimization**: Efficient inter-stage communication

### Error Handling

#### Error Strategies

1. **FAIL_FAST**: Stop immediately on first error (default)
2. **CONTINUE**: Continue executing other stages despite failures
3. **RETRY**: Automatically retry failed stages
4. **SKIP**: Skip failed stages and continue with dependents

#### Retry Mechanisms

- Configurable retry attempts per stage
- Exponential backoff between retries
- Conditional retry based on exit codes
- Custom retry logic through callbacks

### Monitoring and Observability

#### Statistics Collection

- Execution time tracking
- Success/failure rates
- Resource utilization metrics
- Critical path analysis
- Performance bottleneck identification

#### Event System

- Real-time pipeline events
- Stage lifecycle notifications
- Progress updates
- Error notifications
- Custom event metadata

### Integration

#### File Format Support

- **YAML Configuration**: Human-readable pipeline definitions
- **JSON Configuration**: Machine-readable pipeline data
- **State Persistence**: Automatic state saving and loading
- **Export Formats**: Multiple output formats for statistics

#### External Tool Integration

- Process execution with environment management
- Working directory isolation
- Signal handling and cleanup
- Exit code interpretation

---

## Français

### Vue d'ensemble

Le Moteur de Pipeline est un système d'orchestration complet conçu pour le framework BB-Pipeline. Il fournit des capacités avancées pour exécuter des pipelines séquentiels, parallèles et hybrides avec une gestion sophistiquée des dépendances, la gestion d'erreurs et des fonctionnalités de surveillance.

### Architecture

Le Moteur de Pipeline suit une architecture modulaire avec plusieurs composants clés :

#### Composants Principaux

1. **PipelineEngine** - Classe d'orchestration principale
2. **PipelineTask** - Unité d'exécution d'étape individuelle
3. **PipelineDependencyResolver** - Analyse et ordonnancement des dépendances
4. **PipelineExecutionContext** - Environnement d'exécution runtime
5. **PipelineUtils** - Fonctions utilitaires pour la validation et E/S

#### Fonctionnalités Clés

- **Exécution Multi-Mode** : Modes d'exécution séquentiel, parallèle et hybride
- **Résolution de Dépendances** : Ordonnancement automatique des dépendances avec détection de dépendances circulaires
- **Stratégies d'Erreur** : Gestion d'erreur échec-rapide, continuer, réessayer et ignorer
- **Surveillance de Progression** : Suivi de progression en temps réel avec callbacks d'événements
- **Gestion d'État** : Persistance et récupération d'état de pipeline
- **Surveillance de Performance** : Statistiques et métriques complètes
- **Gestion de Configuration** : Configuration flexible avec support YAML/JSON

### Exemples d'Utilisation

#### Création de Pipeline Basique

```cpp
#include \"orchestrator/pipeline_engine.hpp\"

using namespace BBP::Orchestrator;

// Créer le moteur de pipeline
PipelineEngine::Config config;
config.thread_pool_size = 8;
config.enable_metrics = true;
PipelineEngine engine(config);

// Créer les étapes du pipeline
std::vector<PipelineStageConfig> stages;

PipelineStageConfig stage1;
stage1.id = \"build\";
stage1.name = \"Étape de Construction\";
stage1.executable = \"make\";
stage1.arguments = {\"build\"};
stage1.timeout = std::chrono::seconds(300);
stages.push_back(stage1);

PipelineStageConfig stage2;
stage2.id = \"test\";
stage2.name = \"Étape de Test\";
stage2.executable = \"make\";
stage2.arguments = {\"test\"};
stage2.dependencies = {\"build\"};
stage2.timeout = std::chrono::seconds(600);
stages.push_back(stage2);

// Créer et exécuter le pipeline
std::string pipeline_id = engine.createPipeline(stages);
auto stats = engine.executePipeline(pipeline_id);

std::cout << \"Exécution terminée : \" << stats.success_rate * 100 << \"% de taux de réussite\" << std::endl;
```

#### Configuration Avancée de Pipeline

```cpp
// Configurer les paramètres d'exécution
PipelineExecutionConfig exec_config;
exec_config.execution_mode = PipelineExecutionMode::HYBRID;
exec_config.error_strategy = PipelineErrorStrategy::CONTINUE;
exec_config.max_concurrent_stages = 4;
exec_config.global_timeout = std::chrono::minutes(30);
exec_config.enable_progress_reporting = true;

// Exécuter avec configuration personnalisée
auto future = engine.executePipelineAsync(pipeline_id, exec_config);
auto stats = future.get();
```

#### Surveillance d'Événements

```cpp
// Enregistrer un callback d'événement pour la surveillance
engine.registerEventCallback([](const PipelineEvent& event) {
    std::cout << \"[\" << PipelineUtils::formatTimestamp(event.timestamp) << \"] \"
              << \"Pipeline : \" << event.pipeline_id 
              << \", Étape : \" << event.stage_id
              << \", Événement : \" << static_cast<int>(event.type) << std::endl;
});
```

#### Suivi de Progression

```cpp
// Surveiller la progression du pipeline
while (auto progress = engine.getPipelineProgress(pipeline_id)) {
    std::cout << \"Progression : \" << progress->completion_percentage << \"%\"
              << \" (\" << progress->completed_stages << \"/\" << progress->total_stages << \")\"
              << std::endl;
    
    if (progress->isComplete()) {
        break;
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

### Options de Configuration

#### Configuration du Moteur de Pipeline

- **thread_pool_size** : Nombre de threads workers (défaut : hardware_concurrency)
- **max_concurrent_pipelines** : Exécutions simultanées maximum de pipelines (défaut : 1)
- **enable_metrics** : Activer la collecte de métriques de performance (défaut : true)
- **enable_logging** : Activer la journalisation détaillée (défaut : true)
- **health_check_interval** : Fréquence de vérification de santé (défaut : 30 secondes)
- **max_pipeline_history** : Historique d'exécution stocké maximum (défaut : 100)

#### Configuration d'Exécution de Pipeline

- **execution_mode** : Exécution Séquentielle, Parallèle ou Hybride
- **error_strategy** : Échec-rapide, Continuer, Réessayer ou Ignorer sur erreurs
- **max_concurrent_stages** : Exécution d'étapes parallèles maximum
- **global_timeout** : Délai d'expiration global du pipeline
- **enable_progress_reporting** : Activer les mises à jour de progression
- **enable_checkpointing** : Activer les points de contrôle d'état
- **dry_run** : Simuler l'exécution sans lancer les commandes

#### Configuration d'Étape

- **id** : Identifiant unique d'étape
- **name** : Nom d'étape lisible par l'humain
- **description** : Description de l'étape
- **executable** : Commande à exécuter
- **arguments** : Arguments de ligne de commande
- **dependencies** : Liste des étapes prérequises
- **environment** : Variables d'environnement
- **working_directory** : Répertoire d'exécution
- **priority** : Priorité d'exécution (LOW, NORMAL, HIGH, CRITICAL)
- **timeout** : Délai d'expiration d'exécution d'étape
- **max_retries** : Tentatives de répétition maximum
- **retry_delay** : Délai entre les tentatives
- **allow_failure** : Continuer le pipeline en cas d'échec d'étape

### Considérations de Performance

#### Conseils d'Optimisation

1. **Exécution Parallèle** : Utiliser les modes PARALLEL ou HYBRID pour les étapes indépendantes
2. **Dimensionnement du Pool de Threads** : Configurer thread_pool_size basé sur les cœurs CPU disponibles
3. **Gestion des Délais d'Expiration** : Définir des délais appropriés pour éviter les blocages
4. **Gestion de Mémoire** : Surveiller l'utilisation mémoire avec des pipelines larges
5. **Optimisation des Dépendances** : Minimiser les chaînes de dépendances profondes

#### Évolutivité

- **Mise à l'Échelle Horizontale** : Instances multiples du moteur pour exécution distribuée
- **Gestion des Ressources** : Allocation et nettoyage automatiques des ressources
- **Efficacité Mémoire** : Chargement paresseux et mise en commun de mémoire
- **Optimisation Réseau** : Communication inter-étapes efficace

### Gestion d'Erreurs

#### Stratégies d'Erreur

1. **FAIL_FAST** : Arrêter immédiatement à la première erreur (défaut)
2. **CONTINUE** : Continuer l'exécution d'autres étapes malgré les échecs
3. **RETRY** : Réessayer automatiquement les étapes échouées
4. **SKIP** : Ignorer les étapes échouées et continuer avec les dépendantes

#### Mécanismes de Répétition

- Tentatives de répétition configurables par étape
- Backoff exponentiel entre les tentatives
- Répétition conditionnelle basée sur les codes de sortie
- Logique de répétition personnalisée via callbacks

### Surveillance et Observabilité

#### Collecte de Statistiques

- Suivi du temps d'exécution
- Taux de succès/échec
- Métriques d'utilisation des ressources
- Analyse du chemin critique
- Identification des goulots d'étranglement de performance

#### Système d'Événements

- Événements de pipeline en temps réel
- Notifications de cycle de vie d'étape
- Mises à jour de progression
- Notifications d'erreur
- Métadonnées d'événements personnalisées

### Intégration

#### Support de Formats de Fichier

- **Configuration YAML** : Définitions de pipeline lisibles par l'humain
- **Configuration JSON** : Données de pipeline lisibles par machine
- **Persistance d'État** : Sauvegarde et chargement automatiques d'état
- **Formats d'Export** : Formats de sortie multiples pour les statistiques

#### Intégration d'Outils Externes

- Exécution de processus avec gestion d'environnement
- Isolation de répertoire de travail
- Gestion de signaux et nettoyage
- Interprétation de codes de sortie

### API Reference / Référence API

#### Core Classes / Classes Principales

##### PipelineEngine

Main orchestration class providing pipeline management and execution capabilities.
Classe d'orchestration principale fournissant les capacités de gestion et d'exécution de pipeline.

**Key Methods / Méthodes Principales :**
- `createPipeline()` - Create new pipeline / Créer un nouveau pipeline
- `executePipeline()` - Execute pipeline synchronously / Exécuter le pipeline de manière synchrone  
- `executePipelineAsync()` - Execute pipeline asynchronously / Exécuter le pipeline de manière asynchrone
- `getPipelineProgress()` - Get execution progress / Obtenir la progression d'exécution
- `validatePipeline()` - Validate pipeline configuration / Valider la configuration du pipeline

##### PipelineStageConfig

Configuration structure for individual pipeline stages.
Structure de configuration pour les étapes individuelles du pipeline.

**Key Fields / Champs Principaux :**
- `id` - Stage identifier / Identifiant d'étape
- `executable` - Command to execute / Commande à exécuter
- `dependencies` - Stage dependencies / Dépendances d'étape
- `timeout` - Execution timeout / Délai d'expiration d'exécution
- `priority` - Execution priority / Priorité d'exécution

##### PipelineExecutionConfig

Configuration for pipeline execution behavior.
Configuration pour le comportement d'exécution du pipeline.

**Key Fields / Champs Principaux :**
- `execution_mode` - Execution mode / Mode d'exécution
- `error_strategy` - Error handling strategy / Stratégie de gestion d'erreur
- `max_concurrent_stages` - Parallel execution limit / Limite d'exécution parallèle
- `global_timeout` - Overall timeout / Délai d'expiration global

### Best Practices / Meilleures Pratiques

#### Pipeline Design / Conception de Pipeline

1. **Minimize Dependencies / Minimiser les Dépendances** : Keep dependency chains short for better parallelization / Garder les chaînes de dépendances courtes pour une meilleure parallélisation
2. **Idempotent Stages / Étapes Idempotentes** : Design stages to be safely re-runnable / Concevoir les étapes pour être réexécutables en sécurité
3. **Resource Isolation / Isolation des Ressources** : Use working directories and environment variables / Utiliser les répertoires de travail et variables d'environnement
4. **Timeout Management / Gestion des Délais** : Set realistic timeouts based on expected execution times / Définir des délais réalistes basés sur les temps d'exécution attendus

#### Error Handling / Gestion d'Erreurs

1. **Fail-Safe Design / Conception Fail-Safe** : Handle errors gracefully without corrupting state / Gérer les erreurs gracieusement sans corrompre l'état
2. **Retry Logic / Logique de Répétition** : Implement retry for transient failures / Implémenter la répétition pour les échecs transitoires
3. **Monitoring / Surveillance** : Use event callbacks for real-time error tracking / Utiliser les callbacks d'événements pour le suivi d'erreurs en temps réel

#### Performance / Performance

1. **Parallel Execution / Exécution Parallèle** : Leverage parallel modes for independent stages / Exploiter les modes parallèles pour les étapes indépendantes
2. **Resource Optimization / Optimisation des Ressources** : Monitor and tune thread pool and memory usage / Surveiller et ajuster l'utilisation du pool de threads et de la mémoire
3. **Profiling / Profilage** : Use statistics to identify bottlenecks / Utiliser les statistiques pour identifier les goulots d'étranglement

### Troubleshooting / Dépannage

#### Common Issues / Problèmes Courants

1. **Circular Dependencies / Dépendances Circulaires** : Use validation to detect and fix dependency cycles / Utiliser la validation pour détecter et corriger les cycles de dépendances
2. **Timeout Issues / Problèmes de Délai** : Adjust timeouts based on actual execution times / Ajuster les délais basés sur les temps d'exécution réels
3. **Resource Exhaustion / Épuisement des Ressources** : Monitor memory and CPU usage, adjust thread pool size / Surveiller l'utilisation mémoire et CPU, ajuster la taille du pool de threads
4. **Failed Dependencies / Dépendances Échouées** : Check dependency ordering and execution status / Vérifier l'ordonnancement des dépendances et le statut d'exécution

#### Debugging / Débogage

1. **Enable Logging / Activer la Journalisation** : Use detailed logging for execution tracking / Utiliser la journalisation détaillée pour le suivi d'exécution
2. **Dry Run Mode / Mode Exécution à Blanc** : Test pipeline logic without actual execution / Tester la logique du pipeline sans exécution réelle
3. **Event Monitoring / Surveillance d'Événements** : Register event callbacks for real-time debugging / Enregistrer les callbacks d'événements pour le débogage en temps réel
4. **Statistics Analysis / Analyse des Statistiques** : Use execution statistics to identify performance issues / Utiliser les statistiques d'exécution pour identifier les problèmes de performance

### Conclusion

The Pipeline Engine provides a robust, scalable, and feature-rich orchestration system for the BB-Pipeline framework. Its comprehensive feature set, flexible configuration options, and extensive monitoring capabilities make it suitable for both simple automation tasks and complex multi-stage workflows.

Le Moteur de Pipeline fournit un système d'orchestration robuste, évolutif et riche en fonctionnalités pour le framework BB-Pipeline. Son ensemble de fonctionnalités complet, ses options de configuration flexibles et ses capacités de surveillance étendues le rendent adapté tant pour des tâches d'automatisation simples que pour des workflows multi-étapes complexes.