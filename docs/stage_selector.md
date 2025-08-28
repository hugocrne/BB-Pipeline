# Stage Selector Documentation / Documentation du Sélecteur d'Étapes

## English Documentation

### Overview

The Stage Selector is a sophisticated system for selecting and validating individual pipeline modules within the BB-Pipeline framework. It provides advanced filtering, dependency resolution, constraint validation, and execution planning capabilities to enable precise control over which stages are executed and in what order.

### Key Features

#### Advanced Selection Criteria
- **ID-based Selection**: Select stages by exact ID or ID patterns
- **Name-based Selection**: Select stages by name with case-sensitive/insensitive matching
- **Pattern Matching**: Use regular expressions to match stage IDs or names
- **Tag-based Selection**: Select stages based on metadata tags
- **Priority Filtering**: Filter stages by priority levels (LOW, NORMAL, HIGH, CRITICAL)
- **Resource-based Selection**: Select stages based on estimated resource usage
- **Execution Time Filtering**: Filter by estimated execution duration
- **Custom Filtering**: Define custom filter functions for complex selection logic

#### Dependency Resolution and Analysis
- **Automatic Dependency Inclusion**: Automatically include required dependencies
- **Transitive Dependencies**: Resolve dependencies recursively through the entire chain
- **Circular Dependency Detection**: Detect and report circular dependencies
- **Dependency Analysis**: Analyze dependency depth, critical paths, and parallelism potential
- **Dependent Stage Discovery**: Find stages that depend on selected stages

#### Constraint Validation System
- **Execution Constraints**: Validate stages against execution constraints (sequential, parallel, exclusive, etc.)
- **Resource Constraints**: Ensure stages meet CPU, memory, network, and disk requirements
- **Compatibility Analysis**: Analyze compatibility between selected stages
- **Constraint Inference**: Automatically infer constraints from stage configuration
- **Custom Validators**: Register custom constraint validation functions

#### Execution Planning
- **Optimized Execution Order**: Generate optimal execution sequences respecting dependencies
- **Parallel Execution Groups**: Identify stages that can be executed in parallel
- **Critical Path Analysis**: Determine the longest execution path through dependencies
- **Resource Planning**: Estimate total resource requirements and peak usage
- **Performance Optimization**: Suggest optimizations to improve execution efficiency

#### Caching and Performance
- **Selection Result Caching**: Cache selection results to improve performance
- **Configurable TTL**: Set cache time-to-live for different scenarios
- **Cache Statistics**: Monitor cache hit ratios and performance metrics
- **Concurrent Operations**: Support multiple simultaneous selection operations
- **Background Processing**: Asynchronous selection with future-based results

### Architecture

#### Core Components

##### StageSelector
The main class providing the public API for stage selection and validation.

**Key Methods:**
- `selectStages()`: General-purpose stage selection with flexible configuration
- `selectStagesByIds()`: Select specific stages by their IDs
- `selectStagesByPattern()`: Select stages matching a regex pattern
- `filterStages()`: Apply filters to a set of stages
- `validateStageSelection()`: Validate selected stages against various criteria
- `analyzeStageCompatibility()`: Analyze compatibility between stages
- `createExecutionPlan()`: Generate optimized execution plan

##### StageDependencyAnalyzer
Specialized component for analyzing and resolving stage dependencies.

**Capabilities:**
- Direct and transitive dependency resolution
- Circular dependency detection using depth-first search
- Topological sorting for execution ordering
- Critical path calculation for performance analysis
- Parallelism potential assessment

##### StageConstraintValidator
Component responsible for validating execution constraints.

**Features:**
- Built-in constraint validators for common scenarios
- Custom constraint registration
- Constraint compatibility checking
- Automatic constraint inference from configuration
- Constraint conflict resolution

##### StageExecutionPlanner
Advanced component for creating optimized execution plans.

**Functions:**
- Execution order optimization
- Parallel execution group identification
- Resource requirement estimation
- Performance bottleneck identification
- Optimization suggestion generation

#### Selection Process

1. **Configuration Validation**: Validate selection configuration and filters
2. **Filter Application**: Apply all specified filters to available stages
3. **Dependency Resolution**: Include required dependencies if configured
4. **Constraint Validation**: Validate selected stages against constraints
5. **Compatibility Analysis**: Check compatibility between selected stages
6. **Execution Planning**: Generate optimized execution order and parallel groups
7. **Result Generation**: Create comprehensive selection result with metadata

### Configuration

#### StageSelectionConfig
```cpp
StageSelectionConfig config;
config.validation_level = StageValidationLevel::DEPENDENCIES;
config.include_dependencies = true;
config.include_dependents = false;
config.resolve_conflicts = true;
config.optimize_execution_order = true;
config.allow_partial_selection = false;
config.max_selected_stages = 100;
config.selection_timeout = std::chrono::seconds(30);
```

#### StageSelectorConfig
```cpp
StageSelectorConfig selector_config;
selector_config.enable_caching = true;
selector_config.cache_ttl = std::chrono::seconds(300);
selector_config.max_cache_entries = 1000;
selector_config.enable_statistics = true;
selector_config.max_concurrent_selections = 4;
selector_config.auto_include_dependencies = true;
```

### Usage Examples

#### Basic Stage Selection
```cpp
StageSelector selector;
StageSelectionConfig config;

// Select stages by ID
std::vector<std::string> stage_ids = {"subhunter", "httpxpp"};
auto result = selector.selectStagesByIds(available_stages, stage_ids);

if (result.status == StageSelectionStatus::SUCCESS) {
    std::cout << "Selected " << result.selected_stages.size() << " stages" << std::endl;
    for (const auto& stage_id : result.execution_order) {
        std::cout << "Execute: " << stage_id << std::endl;
    }
}
```

#### Advanced Filtering
```cpp
StageSelectionConfig config;

// Filter by tags
auto tag_filter = StageSelectorUtils::createTagFilter({"reconnaissance", "active"});
config.filters.push_back(tag_filter);

// Filter by priority
auto priority_filter = StageSelectorUtils::createPriorityFilter(
    PipelineStagePriority::HIGH, PipelineStagePriority::CRITICAL);
config.filters.push_back(priority_filter);

// Apply filters
auto result = selector.selectStages(available_stages, config);
```

#### Execution Plan Creation
```cpp
auto selected_stages = result.selected_stages;
PipelineExecutionConfig exec_config;
exec_config.execution_mode = PipelineExecutionMode::HYBRID;

auto execution_plan = selector.createExecutionPlan(selected_stages, exec_config);

std::cout << "Execution Plan ID: " << execution_plan.plan_id << std::endl;
std::cout << "Estimated Time: " << execution_plan.estimated_parallel_time.count() << "ms" << std::endl;
std::cout << "Critical Path: ";
for (const auto& stage : execution_plan.critical_path) {
    std::cout << stage << " -> ";
}
std::cout << std::endl;
```

#### Asynchronous Selection
```cpp
auto future_result = selector.selectStagesAsync(available_stages, config);

// Do other work while selection is running
performOtherTasks();

// Get result when ready
auto result = future_result.get();
```

### Integration with BB-Pipeline

The Stage Selector integrates seamlessly with the BB-Pipeline orchestrator:

#### Command Line Integration
- `--stage <stage_id>`: Execute specific stage by ID
- `--stages <pattern>`: Execute stages matching pattern
- `--include-deps`: Include dependencies automatically
- `--validate-only`: Validate selection without execution
- `--dry-run-selection`: Preview selection results

#### Pipeline Engine Integration
- **Stage Filtering**: Pre-filter available stages before execution
- **Dependency Validation**: Ensure all dependencies are satisfied
- **Execution Optimization**: Optimize stage execution order
- **Resource Planning**: Plan resource allocation based on selected stages

#### Configuration File Support
```yaml
stage_selection:
  validation_level: "dependencies"
  include_dependencies: true
  optimize_execution_order: true
  
  filters:
    - criteria: "by_tag"
      tags: ["reconnaissance", "active"]
    - criteria: "by_priority"
      min_priority: "high"
```

### Error Handling and Validation

The Stage Selector provides comprehensive error handling:

#### Selection Status Types
- `SUCCESS`: Selection completed successfully
- `PARTIAL_SUCCESS`: Some stages selected with warnings
- `VALIDATION_FAILED`: Stage validation failed
- `DEPENDENCY_ERROR`: Dependency resolution failed
- `CONSTRAINT_VIOLATION`: Stage constraints violated
- `CIRCULAR_DEPENDENCY`: Circular dependencies detected
- `EMPTY_SELECTION`: No stages matched selection criteria

#### Validation Levels
- `NONE`: No validation performed
- `BASIC`: Basic syntax and configuration validation
- `DEPENDENCIES`: Validate dependencies and resolve conflicts
- `RESOURCES`: Validate resource requirements
- `COMPATIBILITY`: Check stage compatibility
- `COMPREHENSIVE`: Full validation with all checks

### Performance and Scalability

#### Caching System
- **Result Caching**: Cache selection results for identical configurations
- **Intelligent Cache Keys**: Generate cache keys based on stages and configuration
- **TTL Management**: Automatic cache expiration and cleanup
- **Cache Statistics**: Monitor hit ratios and performance impact

#### Concurrent Operations
- **Thread-Safe**: All operations are thread-safe for concurrent access
- **Async Support**: Asynchronous operations with future-based results
- **Resource Pooling**: Efficient resource utilization for multiple selections
- **Load Balancing**: Distribute concurrent selections across available threads

#### Performance Optimization
- **Lazy Evaluation**: Compute results only when needed
- **Early Termination**: Stop processing when errors are detected
- **Efficient Algorithms**: Use optimized algorithms for dependency resolution
- **Memory Management**: Efficient memory usage for large stage sets

---

## Documentation Française

### Aperçu Général

Le Sélecteur d'Étapes est un système sophistiqué pour sélectionner et valider les modules individuels du pipeline dans le framework BB-Pipeline. Il fournit des capacités avancées de filtrage, résolution de dépendances, validation de contraintes et planification d'exécution pour permettre un contrôle précis sur quelles étapes sont exécutées et dans quel ordre.

### Fonctionnalités Principales

#### Critères de Sélection Avancés
- **Sélection par ID**: Sélectionner les étapes par ID exact ou motifs d'ID
- **Sélection par Nom**: Sélectionner les étapes par nom avec correspondance sensible/insensible à la casse
- **Correspondance de Motifs**: Utiliser des expressions régulières pour correspondre aux ID ou noms d'étapes
- **Sélection par Tags**: Sélectionner les étapes basées sur les tags de métadonnées
- **Filtrage par Priorité**: Filtrer les étapes par niveaux de priorité (LOW, NORMAL, HIGH, CRITICAL)
- **Sélection par Ressources**: Sélectionner les étapes basées sur l'utilisation estimée des ressources
- **Filtrage par Temps d'Exécution**: Filtrer par durée d'exécution estimée
- **Filtrage Personnalisé**: Définir des fonctions de filtre personnalisées pour une logique de sélection complexe

#### Résolution et Analyse des Dépendances
- **Inclusion Automatique des Dépendances**: Inclure automatiquement les dépendances requises
- **Dépendances Transitives**: Résoudre les dépendances récursivement à travers toute la chaîne
- **Détection de Dépendances Circulaires**: Détecter et signaler les dépendances circulaires
- **Analyse des Dépendances**: Analyser la profondeur des dépendances, chemins critiques et potentiel de parallélisme
- **Découverte d'Étapes Dépendantes**: Trouver les étapes qui dépendent des étapes sélectionnées

#### Système de Validation des Contraintes
- **Contraintes d'Exécution**: Valider les étapes contre les contraintes d'exécution (séquentiel, parallèle, exclusif, etc.)
- **Contraintes de Ressources**: S'assurer que les étapes répondent aux exigences CPU, mémoire, réseau et disque
- **Analyse de Compatibilité**: Analyser la compatibilité entre les étapes sélectionnées
- **Inférence de Contraintes**: Inférer automatiquement les contraintes de la configuration d'étape
- **Validateurs Personnalisés**: Enregistrer des fonctions de validation de contraintes personnalisées

#### Planification d'Exécution
- **Ordre d'Exécution Optimisé**: Générer des séquences d'exécution optimales respectant les dépendances
- **Groupes d'Exécution Parallèle**: Identifier les étapes qui peuvent être exécutées en parallèle
- **Analyse du Chemin Critique**: Déterminer le chemin d'exécution le plus long à travers les dépendances
- **Planification des Ressources**: Estimer les exigences totales de ressources et l'utilisation maximale
- **Optimisation de Performance**: Suggérer des optimisations pour améliorer l'efficacité d'exécution

#### Cache et Performance
- **Cache des Résultats de Sélection**: Mettre en cache les résultats de sélection pour améliorer les performances
- **TTL Configurable**: Définir la durée de vie du cache pour différents scénarios
- **Statistiques de Cache**: Surveiller les ratios de succès du cache et les métriques de performance
- **Opérations Concurrentes**: Supporter plusieurs opérations de sélection simultanées
- **Traitement en Arrière-plan**: Sélection asynchrone avec des résultats basés sur les futures

### Architecture

#### Composants Principaux

##### StageSelector
La classe principale fournissant l'API publique pour la sélection et validation d'étapes.

**Méthodes Clés:**
- `selectStages()`: Sélection d'étapes généraliste avec configuration flexible
- `selectStagesByIds()`: Sélectionner des étapes spécifiques par leurs IDs
- `selectStagesByPattern()`: Sélectionner des étapes correspondant à un motif regex
- `filterStages()`: Appliquer des filtres à un ensemble d'étapes
- `validateStageSelection()`: Valider les étapes sélectionnées contre divers critères
- `analyzeStageCompatibility()`: Analyser la compatibilité entre étapes
- `createExecutionPlan()`: Générer un plan d'exécution optimisé

##### StageDependencyAnalyzer
Composant spécialisé pour analyser et résoudre les dépendances d'étapes.

**Capacités:**
- Résolution de dépendances directes et transitives
- Détection de dépendances circulaires utilisant la recherche en profondeur
- Tri topologique pour l'ordonnancement d'exécution
- Calcul du chemin critique pour l'analyse de performance
- Évaluation du potentiel de parallélisme

##### StageConstraintValidator
Composant responsable de la validation des contraintes d'exécution.

**Fonctionnalités:**
- Validateurs de contraintes intégrés pour les scénarios communs
- Enregistrement de contraintes personnalisées
- Vérification de compatibilité des contraintes
- Inférence automatique des contraintes de la configuration
- Résolution des conflits de contraintes

##### StageExecutionPlanner
Composant avancé pour créer des plans d'exécution optimisés.

**Fonctions:**
- Optimisation de l'ordre d'exécution
- Identification des groupes d'exécution parallèle
- Estimation des exigences de ressources
- Identification des goulots d'étranglement de performance
- Génération de suggestions d'optimisation

#### Processus de Sélection

1. **Validation de Configuration**: Valider la configuration de sélection et les filtres
2. **Application de Filtres**: Appliquer tous les filtres spécifiés aux étapes disponibles
3. **Résolution de Dépendances**: Inclure les dépendances requises si configuré
4. **Validation de Contraintes**: Valider les étapes sélectionnées contre les contraintes
5. **Analyse de Compatibilité**: Vérifier la compatibilité entre les étapes sélectionnées
6. **Planification d'Exécution**: Générer l'ordre d'exécution optimisé et les groupes parallèles
7. **Génération de Résultats**: Créer un résultat de sélection complet avec métadonnées

### Configuration

#### StageSelectionConfig
```cpp
StageSelectionConfig config;
config.validation_level = StageValidationLevel::DEPENDENCIES;
config.include_dependencies = true;
config.include_dependents = false;
config.resolve_conflicts = true;
config.optimize_execution_order = true;
config.allow_partial_selection = false;
config.max_selected_stages = 100;
config.selection_timeout = std::chrono::seconds(30);
```

#### StageSelectorConfig
```cpp
StageSelectorConfig selector_config;
selector_config.enable_caching = true;
selector_config.cache_ttl = std::chrono::seconds(300);
selector_config.max_cache_entries = 1000;
selector_config.enable_statistics = true;
selector_config.max_concurrent_selections = 4;
selector_config.auto_include_dependencies = true;
```

### Exemples d'Utilisation

#### Sélection d'Étapes de Base
```cpp
StageSelector selector;
StageSelectionConfig config;

// Sélectionner des étapes par ID
std::vector<std::string> stage_ids = {"subhunter", "httpxpp"};
auto result = selector.selectStagesByIds(available_stages, stage_ids);

if (result.status == StageSelectionStatus::SUCCESS) {
    std::cout << "Sélectionné " << result.selected_stages.size() << " étapes" << std::endl;
    for (const auto& stage_id : result.execution_order) {
        std::cout << "Exécuter: " << stage_id << std::endl;
    }
}
```

#### Filtrage Avancé
```cpp
StageSelectionConfig config;

// Filtrer par tags
auto tag_filter = StageSelectorUtils::createTagFilter({"reconnaissance", "active"});
config.filters.push_back(tag_filter);

// Filtrer par priorité
auto priority_filter = StageSelectorUtils::createPriorityFilter(
    PipelineStagePriority::HIGH, PipelineStagePriority::CRITICAL);
config.filters.push_back(priority_filter);

// Appliquer les filtres
auto result = selector.selectStages(available_stages, config);
```

#### Création de Plan d'Exécution
```cpp
auto selected_stages = result.selected_stages;
PipelineExecutionConfig exec_config;
exec_config.execution_mode = PipelineExecutionMode::HYBRID;

auto execution_plan = selector.createExecutionPlan(selected_stages, exec_config);

std::cout << "ID Plan d'Exécution: " << execution_plan.plan_id << std::endl;
std::cout << "Temps Estimé: " << execution_plan.estimated_parallel_time.count() << "ms" << std::endl;
std::cout << "Chemin Critique: ";
for (const auto& stage : execution_plan.critical_path) {
    std::cout << stage << " -> ";
}
std::cout << std::endl;
```

#### Sélection Asynchrone
```cpp
auto future_result = selector.selectStagesAsync(available_stages, config);

// Faire d'autres tâches pendant que la sélection s'exécute
performOtherTasks();

// Obtenir le résultat quand prêt
auto result = future_result.get();
```

### Intégration avec BB-Pipeline

Le Sélecteur d'Étapes s'intègre parfaitement avec l'orchestrateur BB-Pipeline:

#### Intégration Ligne de Commande
- `--stage <stage_id>`: Exécuter une étape spécifique par ID
- `--stages <pattern>`: Exécuter les étapes correspondant au motif
- `--include-deps`: Inclure les dépendances automatiquement
- `--validate-only`: Valider la sélection sans exécution
- `--dry-run-selection`: Prévisualiser les résultats de sélection

#### Intégration Moteur de Pipeline
- **Filtrage d'Étapes**: Pré-filtrer les étapes disponibles avant l'exécution
- **Validation de Dépendances**: S'assurer que toutes les dépendances sont satisfaites
- **Optimisation d'Exécution**: Optimiser l'ordre d'exécution des étapes
- **Planification des Ressources**: Planifier l'allocation des ressources basée sur les étapes sélectionnées

#### Support des Fichiers de Configuration
```yaml
stage_selection:
  validation_level: "dependencies"
  include_dependencies: true
  optimize_execution_order: true
  
  filters:
    - criteria: "by_tag"
      tags: ["reconnaissance", "active"]
    - criteria: "by_priority"
      min_priority: "high"
```

### Gestion d'Erreurs et Validation

Le Sélecteur d'Étapes fournit une gestion d'erreurs complète:

#### Types de Statut de Sélection
- `SUCCESS`: Sélection terminée avec succès
- `PARTIAL_SUCCESS`: Certaines étapes sélectionnées avec avertissements
- `VALIDATION_FAILED`: Validation d'étape échouée
- `DEPENDENCY_ERROR`: Résolution de dépendances échouée
- `CONSTRAINT_VIOLATION`: Contraintes d'étapes violées
- `CIRCULAR_DEPENDENCY`: Dépendances circulaires détectées
- `EMPTY_SELECTION`: Aucune étape ne correspond aux critères de sélection

#### Niveaux de Validation
- `NONE`: Aucune validation effectuée
- `BASIC`: Validation de syntaxe et configuration de base
- `DEPENDENCIES`: Valider les dépendances et résoudre les conflits
- `RESOURCES`: Valider les exigences de ressources
- `COMPATIBILITY`: Vérifier la compatibilité des étapes
- `COMPREHENSIVE`: Validation complète avec toutes les vérifications

### Performance et Évolutivité

#### Système de Cache
- **Cache des Résultats**: Mettre en cache les résultats de sélection pour des configurations identiques
- **Clés de Cache Intelligentes**: Générer des clés de cache basées sur les étapes et la configuration
- **Gestion TTL**: Expiration automatique du cache et nettoyage
- **Statistiques de Cache**: Surveiller les ratios de succès et l'impact sur les performances

#### Opérations Concurrentes
- **Thread-Safe**: Toutes les opérations sont thread-safe pour l'accès concurrent
- **Support Async**: Opérations asynchrones avec résultats basés sur les futures
- **Pooling de Ressources**: Utilisation efficace des ressources pour plusieurs sélections
- **Équilibrage de Charge**: Distribuer les sélections concurrentes à travers les threads disponibles

#### Optimisation de Performance
- **Évaluation Paresseuse**: Calculer les résultats seulement quand nécessaire
- **Terminaison Précoce**: Arrêter le traitement quand des erreurs sont détectées
- **Algorithmes Efficaces**: Utiliser des algorithmes optimisés pour la résolution de dépendances
- **Gestion Mémoire**: Utilisation efficace de la mémoire pour de grands ensembles d'étapes

### Cas d'Usage Avancés

#### Sélection Conditionnelle
```cpp
// Sélectionner seulement les étapes qui peuvent s'exécuter en parallèle
StageSelectionConfig config;
config.allowed_constraints = {StageExecutionConstraint::PARALLEL_SAFE};

auto result = selector.selectStages(available_stages, config);
```

#### Planification de Ressources
```cpp
auto execution_plan = selector.createExecutionPlan(selected_stages);

std::cout << "Ressources CPU requises: " << execution_plan.resource_requirements["cpu"] << std::endl;
std::cout << "Utilisation mémoire maximale: " << execution_plan.peak_resource_usage << "MB" << std::endl;

for (const auto& suggestion : execution_plan.optimization_suggestions) {
    std::cout << "Suggestion: " << suggestion << std::endl;
}
```

#### Analyse de Performance
```cpp
auto bottlenecks = StageSelectorUtils::identifyBottleneckStages(available_stages);
for (const auto& bottleneck : bottlenecks) {
    std::cout << "Goulot d'étranglement détecté: " << bottleneck << std::endl;
}

auto efficiency = StageSelectorUtils::calculateSelectionEfficiency(result);
std::cout << "Efficacité de sélection: " << (efficiency * 100) << "%" << std::endl;
```

### Extensibilité et Personnalisation

#### Contraintes Personnalisées
```cpp
// Enregistrer un validateur de contrainte personnalisé
selector.registerConstraintValidator(
    StageExecutionConstraint::CUSTOM,
    [](const PipelineStageConfig& stage) -> bool {
        // Logique de validation personnalisée
        return stage.metadata.find("custom_requirement") != stage.metadata.end();
    }
);
```

#### Filtres Personnalisés
```cpp
StageSelectionFilter custom_filter;
custom_filter.criteria = StageSelectionCriteria::BY_CUSTOM;
custom_filter.custom_filter = [](const PipelineStageConfig& stage) -> bool {
    // Logique de filtre personnalisée complexe
    return stage.timeout.count() < 300 && stage.priority >= PipelineStagePriority::HIGH;
};

config.filters.push_back(custom_filter);
```

### Surveillance et Diagnostics

#### Statistiques Détaillées
```cpp
auto stats = selector.getStatistics();
std::cout << "Sélections totales: " << stats.total_selections << std::endl;
std::cout << "Taux de succès: " << (stats.successful_selections * 100.0 / stats.total_selections) << "%" << std::endl;
std::cout << "Temps moyen: " << stats.avg_selection_time.count() << "ms" << std::endl;
std::cout << "Ratio cache: " << selector.getCacheHitRatio() << std::endl;
```

#### Rapports de Débogage
```cpp
auto report = StageSelectorUtils::generateSelectionReport(result);
std::cout << report << std::endl;

// Sauvegarder les informations de débogage détaillées
StageSelectorUtils::dumpSelectionDebugInfo(result, "debug_selection.txt");
```

### Bonnes Pratiques

1. **Validation de Configuration**: Toujours valider la configuration de sélection avant utilisation
2. **Gestion des Erreurs**: Vérifier le statut de sélection et gérer les erreurs appropriés
3. **Optimisation du Cache**: Utiliser le cache pour les sélections répétitives
4. **Surveillance des Performances**: Surveiller les statistiques pour identifier les goulots d'étranglement
5. **Documentation des Contraintes**: Documenter les contraintes personnalisées pour la maintenabilité