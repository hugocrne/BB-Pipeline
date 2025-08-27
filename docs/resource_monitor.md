# Resource Monitor Documentation / Documentation du Moniteur de Ressources

## English Documentation

### Overview

The Resource Monitor is a comprehensive system monitoring and throttling solution designed for the BB-Pipeline framework. It provides real-time monitoring of system resources (CPU, memory, network, disk) with adaptive throttling capabilities to maintain optimal performance and prevent system overload during intensive reconnaissance operations.

### Key Features

#### Multi-Resource Monitoring
- **CPU Usage**: Real-time CPU utilization tracking with per-core monitoring
- **Memory Monitoring**: RAM usage, available memory, and memory pressure detection
- **Network Monitoring**: Bandwidth usage, connection counts, and network latency
- **Disk I/O**: Read/write operations, disk space, and I/O wait times
- **Process Monitoring**: Per-process resource consumption tracking
- **System Monitoring**: Overall system health and performance metrics
- **Custom Metrics**: Extensible framework for user-defined monitoring

#### Adaptive Throttling System
- **Multiple Strategies**: None, Linear, Exponential, Adaptive, Predictive, Aggressive
- **Dynamic Adjustment**: Real-time throttling based on current system load
- **Predictive Analysis**: Trend analysis to prevent resource exhaustion
- **Custom Thresholds**: Configurable limits for different resource types

#### Alert and Notification System
- **Multi-Level Alerts**: Debug, Info, Warning, Critical, Emergency
- **Real-time Notifications**: Immediate alerts when thresholds are exceeded
- **Historical Tracking**: Alert history and trend analysis
- **Custom Callbacks**: User-defined alert handlers

#### Performance Analysis
- **Statistical Tracking**: Moving averages, trend analysis, and performance metrics
- **Predictive Modeling**: ETA calculations and resource usage predictions
- **Self-Diagnostics**: Monitor performance impact and overhead analysis
- **Export Capabilities**: JSON export for external analysis tools

### Architecture

#### Core Components

##### ResourceMonitor
The main monitoring class that handles resource collection, analysis, and throttling decisions.

**Key Methods:**
- `start()`: Initialize and begin monitoring
- `stop()`: Stop monitoring and cleanup resources
- `getCurrentUsage(ResourceType)`: Get current usage for specific resource
- `setThrottlingStrategy()`: Configure throttling behavior
- `addAlert()`: Register custom alert conditions

##### ResourceMonitorManager
Singleton manager for coordinating multiple monitor instances across the pipeline.

**Key Methods:**
- `getInstance()`: Get singleton instance
- `registerMonitor()`: Add monitor to management
- `getGlobalStats()`: Aggregate statistics from all monitors
- `emergencyShutdown()`: System-wide emergency stop

##### Specialized Monitors
- **PipelineResourceMonitor**: Optimized for pipeline operations
- **NetworkResourceMonitor**: Focused on network resource management
- **MemoryResourceMonitor**: Specialized memory usage tracking

##### RAII Helpers
- **AutoResourceMonitor**: Automatic lifecycle management
- **ResourceGuard**: Scoped resource protection
- **ThrottleGuard**: Automatic throttling management

#### Resource Types

- **CPU**: Processor utilization, load average, core usage
- **MEMORY**: RAM usage, available memory, swap usage
- **NETWORK**: Bandwidth, connection count, latency
- **DISK**: I/O operations, space usage, queue depth
- **PROCESS**: Per-process metrics, thread count
- **SYSTEM**: Overall system health, temperature
- **CUSTOM**: User-defined metrics and collectors

#### Throttling Strategies

- **NONE**: No throttling applied
- **LINEAR**: Proportional reduction based on usage
- **EXPONENTIAL**: Aggressive reduction for high usage
- **ADAPTIVE**: Dynamic adjustment based on system behavior
- **PREDICTIVE**: Proactive throttling based on trends
- **AGGRESSIVE**: Maximum throttling for resource protection

### Configuration

```cpp
ResourceMonitorConfig config;
config.monitoring_interval = std::chrono::milliseconds(100);
config.cpu_threshold = 80.0;
config.memory_threshold = 85.0;
config.network_threshold = 90.0;
config.throttling_strategy = ThrottlingStrategy::ADAPTIVE;
config.enable_alerts = true;
config.enable_auto_scaling = true;
```

### Usage Examples

#### Basic Monitoring
```cpp
ResourceMonitorConfig config;
config.cpu_threshold = 75.0;
config.memory_threshold = 80.0;

ResourceMonitor monitor(config);
monitor.start();

// Get current CPU usage
auto cpu_usage = monitor.getCurrentUsage(ResourceType::CPU);
if (cpu_usage.has_value()) {
    std::cout << "CPU Usage: " << cpu_usage->current_value << "%" << std::endl;
}
```

#### Custom Alert Handling
```cpp
monitor.setAlertCallback([](const ResourceAlert& alert) {
    if (alert.severity >= AlertSeverity::CRITICAL) {
        std::cout << "Critical alert: " << alert.message << std::endl;
        // Implement custom emergency response
    }
});
```

#### Automatic Resource Management
```cpp
{
    AutoResourceMonitor auto_monitor(config);
    // Monitor automatically starts and stops with scope
    performIntensiveOperation();
} // Monitor automatically cleaned up
```

### Integration with BB-Pipeline

The Resource Monitor integrates seamlessly with the BB-Pipeline orchestrator:

- **Pipeline Stage Monitoring**: Each pipeline stage can have dedicated resource monitoring
- **Adaptive Rate Limiting**: Automatic adjustment of request rates based on system load
- **Emergency Shutdown**: Safe pipeline termination if resources are critically low
- **Performance Optimization**: Dynamic thread pool sizing based on available resources

### Performance Impact

The Resource Monitor is designed with minimal overhead:
- **Low CPU Impact**: < 1% CPU usage for monitoring operations
- **Memory Efficient**: < 10MB memory footprint for typical configurations
- **Non-Blocking**: Asynchronous operation with minimal impact on pipeline performance
- **Configurable Frequency**: Adjustable monitoring intervals to balance accuracy and performance

---

## Documentation Française

### Aperçu Général

Le Moniteur de Ressources est un système complet de surveillance et de limitation des ressources système conçu pour le framework BB-Pipeline. Il fournit une surveillance en temps réel des ressources système (CPU, mémoire, réseau, disque) avec des capacités de limitation adaptative pour maintenir des performances optimales et prévenir la surcharge du système pendant les opérations de reconnaissance intensives.

### Fonctionnalités Principales

#### Surveillance Multi-Ressources
- **Utilisation CPU**: Suivi en temps réel de l'utilisation du processeur avec surveillance par cœur
- **Surveillance Mémoire**: Utilisation RAM, mémoire disponible et détection de pression mémoire
- **Surveillance Réseau**: Utilisation de la bande passante, nombre de connexions et latence réseau
- **E/S Disque**: Opérations de lecture/écriture, espace disque et temps d'attente E/S
- **Surveillance Processus**: Suivi de la consommation de ressources par processus
- **Surveillance Système**: Métriques globales de santé et performance du système
- **Métriques Personnalisées**: Framework extensible pour la surveillance définie par l'utilisateur

#### Système de Limitation Adaptative
- **Stratégies Multiples**: Aucune, Linéaire, Exponentielle, Adaptative, Prédictive, Agressive
- **Ajustement Dynamique**: Limitation en temps réel basée sur la charge système actuelle
- **Analyse Prédictive**: Analyse des tendances pour prévenir l'épuisement des ressources
- **Seuils Personnalisés**: Limites configurables pour différents types de ressources

#### Système d'Alertes et Notifications
- **Alertes Multi-Niveaux**: Debug, Info, Avertissement, Critique, Urgence
- **Notifications Temps Réel**: Alertes immédiates lors du dépassement de seuils
- **Suivi Historique**: Historique des alertes et analyse des tendances
- **Callbacks Personnalisés**: Gestionnaires d'alertes définis par l'utilisateur

#### Analyse de Performance
- **Suivi Statistique**: Moyennes mobiles, analyse des tendances et métriques de performance
- **Modélisation Prédictive**: Calculs ETA et prédictions d'utilisation des ressources
- **Auto-Diagnostics**: Surveillance de l'impact sur les performances et analyse des surcoûts
- **Capacités d'Export**: Export JSON pour les outils d'analyse externes

### Architecture

#### Composants Principaux

##### ResourceMonitor
La classe principale de surveillance qui gère la collecte des ressources, l'analyse et les décisions de limitation.

**Méthodes Clés:**
- `start()`: Initialiser et commencer la surveillance
- `stop()`: Arrêter la surveillance et nettoyer les ressources
- `getCurrentUsage(ResourceType)`: Obtenir l'utilisation actuelle pour une ressource spécifique
- `setThrottlingStrategy()`: Configurer le comportement de limitation
- `addAlert()`: Enregistrer des conditions d'alerte personnalisées

##### ResourceMonitorManager
Gestionnaire singleton pour coordonner plusieurs instances de moniteurs à travers le pipeline.

**Méthodes Clés:**
- `getInstance()`: Obtenir l'instance singleton
- `registerMonitor()`: Ajouter un moniteur à la gestion
- `getGlobalStats()`: Statistiques agrégées de tous les moniteurs
- `emergencyShutdown()`: Arrêt d'urgence à l'échelle du système

##### Moniteurs Spécialisés
- **PipelineResourceMonitor**: Optimisé pour les opérations de pipeline
- **NetworkResourceMonitor**: Focalisé sur la gestion des ressources réseau
- **MemoryResourceMonitor**: Suivi spécialisé de l'utilisation mémoire

##### Helpers RAII
- **AutoResourceMonitor**: Gestion automatique du cycle de vie
- **ResourceGuard**: Protection des ressources avec portée
- **ThrottleGuard**: Gestion automatique de la limitation

#### Types de Ressources

- **CPU**: Utilisation processeur, charge moyenne, utilisation par cœur
- **MEMORY**: Utilisation RAM, mémoire disponible, utilisation swap
- **NETWORK**: Bande passante, nombre de connexions, latence
- **DISK**: Opérations E/S, utilisation espace, profondeur de queue
- **PROCESS**: Métriques par processus, nombre de threads
- **SYSTEM**: Santé globale du système, température
- **CUSTOM**: Métriques et collecteurs définis par l'utilisateur

#### Stratégies de Limitation

- **NONE**: Aucune limitation appliquée
- **LINEAR**: Réduction proportionnelle basée sur l'utilisation
- **EXPONENTIAL**: Réduction agressive pour haute utilisation
- **ADAPTIVE**: Ajustement dynamique basé sur le comportement système
- **PREDICTIVE**: Limitation proactive basée sur les tendances
- **AGGRESSIVE**: Limitation maximale pour protection des ressources

### Configuration

```cpp
ResourceMonitorConfig config;
config.monitoring_interval = std::chrono::milliseconds(100);
config.cpu_threshold = 80.0;
config.memory_threshold = 85.0;
config.network_threshold = 90.0;
config.throttling_strategy = ThrottlingStrategy::ADAPTIVE;
config.enable_alerts = true;
config.enable_auto_scaling = true;
```

### Exemples d'Utilisation

#### Surveillance de Base
```cpp
ResourceMonitorConfig config;
config.cpu_threshold = 75.0;
config.memory_threshold = 80.0;

ResourceMonitor monitor(config);
monitor.start();

// Obtenir l'utilisation CPU actuelle
auto cpu_usage = monitor.getCurrentUsage(ResourceType::CPU);
if (cpu_usage.has_value()) {
    std::cout << "Utilisation CPU: " << cpu_usage->current_value << "%" << std::endl;
}
```

#### Gestion d'Alertes Personnalisées
```cpp
monitor.setAlertCallback([](const ResourceAlert& alert) {
    if (alert.severity >= AlertSeverity::CRITICAL) {
        std::cout << "Alerte critique: " << alert.message << std::endl;
        // Implémenter une réponse d'urgence personnalisée
    }
});
```

#### Gestion Automatique des Ressources
```cpp
{
    AutoResourceMonitor auto_monitor(config);
    // Le moniteur démarre et s'arrête automatiquement avec la portée
    performIntensiveOperation();
} // Moniteur automatiquement nettoyé
```

### Intégration avec BB-Pipeline

Le Moniteur de Ressources s'intègre parfaitement avec l'orchestrateur BB-Pipeline:

- **Surveillance par Étape**: Chaque étape du pipeline peut avoir une surveillance dédiée des ressources
- **Limitation de Débit Adaptative**: Ajustement automatique des taux de requête basé sur la charge système
- **Arrêt d'Urgence**: Terminaison sécurisée du pipeline si les ressources sont critiquement faibles
- **Optimisation des Performances**: Dimensionnement dynamique du pool de threads basé sur les ressources disponibles

### Impact sur les Performances

Le Moniteur de Ressources est conçu avec un surcoût minimal:
- **Faible Impact CPU**: < 1% d'utilisation CPU pour les opérations de surveillance
- **Efficace en Mémoire**: < 10MB d'empreinte mémoire pour les configurations typiques
- **Non-Bloquant**: Fonctionnement asynchrone avec impact minimal sur les performances du pipeline
- **Fréquence Configurable**: Intervalles de surveillance ajustables pour équilibrer précision et performance

### Considérations Techniques

#### Thread Safety
Toutes les opérations du Moniteur de Ressources sont thread-safe, utilisant des primitives de synchronisation appropriées pour les accès concurrents.

#### Gestion d'Erreurs
Le système implémente une gestion d'erreurs robuste avec récupération automatique et signalement détaillé des problèmes.

#### Extensibilité
L'architecture modulaire permet l'ajout facile de nouveaux types de ressources et stratégies de limitation sans modifier le code existant.

#### Compatibilité Plateforme
Le système supporte à la fois Windows et Linux avec des implémentations spécifiques à la plateforme pour la collecte de métriques système.