# Kill Switch System / Système Kill Switch

## English

### Overview

The Kill Switch system provides comprehensive emergency shutdown capabilities for the BB-Pipeline framework. It ensures graceful termination of all running operations while preserving partial results and maintaining data integrity during unexpected shutdowns.

### Key Features

#### Emergency Shutdown
- **Multi-phase shutdown process**: Gracefully terminates operations in ordered phases
- **Configurable timeouts**: Each phase has customizable timeout limits
- **Force immediate mode**: Bypass graceful shutdown when critical situations arise
- **Signal integration**: Works with system signals (SIGTERM, SIGINT) through SignalHandler

#### State Preservation
- **Automatic state saving**: Preserves partial results and current execution state
- **JSON-based storage**: Human-readable state files with optional compression
- **Data integrity**: Built-in checksum validation and corruption detection
- **Configurable retention**: Automatic cleanup of old state files
- **Recovery support**: State can be loaded for resumption on restart

#### Callback System
- **State preservation callbacks**: Components register to save their state
- **Task termination callbacks**: Clean shutdown of running tasks
- **Cleanup callbacks**: Final resource cleanup operations
- **Notification callbacks**: External system integration

#### Thread Safety
- **Singleton pattern**: Thread-safe instance management
- **Mutex protection**: All operations are thread-safe
- **Concurrent shutdown**: Handles multiple trigger attempts gracefully

### Architecture

#### Core Components

1. **KillSwitch Class**: Main singleton managing shutdown process
2. **StateSnapshot**: Serializable state containers with metadata
3. **KillSwitchConfig**: Comprehensive configuration structure
4. **KillSwitchStats**: Execution statistics and monitoring
5. **Utility Functions**: Helper functions for triggers and phases

#### Shutdown Phases

1. **TRIGGERED**: Initial trigger received, preparing shutdown
2. **STOPPING_TASKS**: Gracefully stopping all running tasks
3. **SAVING_STATE**: Preserving current state to persistent storage
4. **CLEANUP**: Executing cleanup operations and resource deallocation
5. **FINALIZING**: Final system cleanup and SignalHandler integration
6. **COMPLETED**: Shutdown process finished successfully

### Configuration

#### Timeout Settings
```cpp
struct KillSwitchConfig {
    std::chrono::milliseconds trigger_timeout{500};
    std::chrono::milliseconds task_stop_timeout{5000};
    std::chrono::milliseconds state_save_timeout{10000};
    std::chrono::milliseconds cleanup_timeout{3000};
    std::chrono::milliseconds total_shutdown_timeout{30000};
    // ... other settings
};
```

#### State Management
- `state_directory`: Location for state files (default: "./.kill_switch_state")
- `max_state_files`: Maximum number of preserved state files
- `compress_state_data`: Enable/disable state compression
- `preserve_partial_results`: Save partial results during shutdown

### Usage Examples

#### Basic Setup
```cpp
auto& kill_switch = KillSwitch::getInstance();

KillSwitchConfig config;
config.task_stop_timeout = 5000ms;
config.state_save_timeout = 10000ms;
config.preserve_partial_results = true;

kill_switch.configure(config);
kill_switch.initialize();
```

#### Registering Callbacks
```cpp
// State preservation
kill_switch.registerStatePreservationCallback("my_component",
    [](const std::string& component_id) -> std::optional<StateSnapshot> {
        StateSnapshot snapshot;
        snapshot.component_id = component_id;
        snapshot.state_data = serialize_my_state();
        return snapshot;
    });

// Task termination
kill_switch.registerTaskTerminationCallback("http_requests",
    [](const std::string& task_type, std::chrono::milliseconds timeout) -> bool {
        return stop_http_requests(timeout);
    });
```

#### Triggering Shutdown
```cpp
// Manual trigger
kill_switch.trigger(KillSwitchTrigger::USER_REQUEST, "Manual shutdown requested");

// Wait for completion
bool completed = kill_switch.waitForCompletion(30000ms);
if (completed) {
    std::cout << "Shutdown completed successfully" << std::endl;
}
```

### Integration with BB-Pipeline

The Kill Switch integrates seamlessly with:
- **PipelineEngine**: Graceful pipeline termination
- **SignalHandler**: System signal handling
- **Logger**: Comprehensive bilingual logging
- **ConfigManager**: Configuration management
- **All BB-Pipeline modules**: Through callback registration

---

## Français

### Vue d'ensemble

Le système Kill Switch fournit des capacités d'arrêt d'urgence complètes pour le framework BB-Pipeline. Il assure la terminaison propre de toutes les opérations en cours tout en préservant les résultats partiels et maintenant l'intégrité des données lors d'arrêts inattendus.

### Fonctionnalités clés

#### Arrêt d'urgence
- **Processus d'arrêt multi-phases** : Termine proprement les opérations dans des phases ordonnées
- **Timeouts configurables** : Chaque phase a des limites de timeout personnalisables
- **Mode immédiat forcé** : Contourne l'arrêt propre lors de situations critiques
- **Intégration des signaux** : Fonctionne avec les signaux système (SIGTERM, SIGINT) via SignalHandler

#### Préservation d'état
- **Sauvegarde d'état automatique** : Préserve les résultats partiels et l'état d'exécution actuel
- **Stockage basé JSON** : Fichiers d'état lisibles avec compression optionnelle
- **Intégrité des données** : Validation par somme de contrôle intégrée et détection de corruption
- **Rétention configurable** : Nettoyage automatique des anciens fichiers d'état
- **Support de récupération** : L'état peut être chargé pour reprendre au redémarrage

#### Système de callbacks
- **Callbacks de préservation d'état** : Les composants s'enregistrent pour sauvegarder leur état
- **Callbacks de terminaison de tâches** : Arrêt propre des tâches en cours
- **Callbacks de nettoyage** : Opérations finales de nettoyage des ressources
- **Callbacks de notification** : Intégration avec des systèmes externes

#### Sécurité des threads
- **Pattern singleton** : Gestion d'instance thread-safe
- **Protection par mutex** : Toutes les opérations sont thread-safe
- **Arrêt concurrent** : Gère proprement les tentatives de déclenchement multiples

### Architecture

#### Composants principaux

1. **Classe KillSwitch** : Singleton principal gérant le processus d'arrêt
2. **StateSnapshot** : Conteneurs d'état sérialisables avec métadonnées
3. **KillSwitchConfig** : Structure de configuration complète
4. **KillSwitchStats** : Statistiques d'exécution et surveillance
5. **Fonctions utilitaires** : Fonctions d'aide pour déclencheurs et phases

#### Phases d'arrêt

1. **TRIGGERED** : Déclencheur initial reçu, préparation de l'arrêt
2. **STOPPING_TASKS** : Arrêt propre de toutes les tâches en cours
3. **SAVING_STATE** : Préservation de l'état actuel vers stockage persistant
4. **CLEANUP** : Exécution des opérations de nettoyage et désallocation des ressources
5. **FINALIZING** : Nettoyage final du système et intégration SignalHandler
6. **COMPLETED** : Processus d'arrêt terminé avec succès

### Configuration

#### Paramètres de timeout
```cpp
struct KillSwitchConfig {
    std::chrono::milliseconds trigger_timeout{500};
    std::chrono::milliseconds task_stop_timeout{5000};
    std::chrono::milliseconds state_save_timeout{10000};
    std::chrono::milliseconds cleanup_timeout{3000};
    std::chrono::milliseconds total_shutdown_timeout{30000};
    // ... autres paramètres
};
```

#### Gestion d'état
- `state_directory` : Emplacement pour les fichiers d'état (défaut : "./.kill_switch_state")
- `max_state_files` : Nombre maximum de fichiers d'état préservés
- `compress_state_data` : Activer/désactiver la compression d'état
- `preserve_partial_results` : Sauvegarder les résultats partiels lors de l'arrêt

### Exemples d'utilisation

#### Configuration de base
```cpp
auto& kill_switch = KillSwitch::getInstance();

KillSwitchConfig config;
config.task_stop_timeout = 5000ms;
config.state_save_timeout = 10000ms;
config.preserve_partial_results = true;

kill_switch.configure(config);
kill_switch.initialize();
```

#### Enregistrement de callbacks
```cpp
// Préservation d'état
kill_switch.registerStatePreservationCallback("mon_composant",
    [](const std::string& component_id) -> std::optional<StateSnapshot> {
        StateSnapshot snapshot;
        snapshot.component_id = component_id;
        snapshot.state_data = serialiser_mon_etat();
        return snapshot;
    });

// Terminaison de tâches
kill_switch.registerTaskTerminationCallback("requetes_http",
    [](const std::string& task_type, std::chrono::milliseconds timeout) -> bool {
        return arreter_requetes_http(timeout);
    });
```

#### Déclenchement d'arrêt
```cpp
// Déclenchement manuel
kill_switch.trigger(KillSwitchTrigger::USER_REQUEST, "Arrêt manuel demandé");

// Attente de completion
bool completed = kill_switch.waitForCompletion(30000ms);
if (completed) {
    std::cout << "Arrêt terminé avec succès" << std::endl;
}
```

### Intégration avec BB-Pipeline

Le Kill Switch s'intègre parfaitement avec :
- **PipelineEngine** : Terminaison propre du pipeline
- **SignalHandler** : Gestion des signaux système
- **Logger** : Journalisation bilingue complète
- **ConfigManager** : Gestion de la configuration
- **Tous les modules BB-Pipeline** : Via l'enregistrement de callbacks

### Sécurité et fiabilité

- **Validation des données** : Vérification d'intégrité par sommes de contrôle
- **Gestion des erreurs** : Récupération propre en cas d'échec
- **Logging bilingue** : Messages en anglais et français
- **Tests complets** : Couverture de test à 100% avec scénarios d'erreur
- **Thread safety** : Toutes les opérations sont thread-safe