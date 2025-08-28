// EN: Stage Selector Implementation for BB-Pipeline - Individual module execution with validation
// FR: Implémentation du Sélecteur d'Étapes pour BB-Pipeline - Exécution de modules individuels avec validation

#include "orchestrator/stage_selector.hpp"
#include "infrastructure/logging/logger.hpp"
#include "infrastructure/threading/thread_pool.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <unordered_set>

namespace BBP {
namespace Orchestrator {

// EN: Internal utility functions
// FR: Fonctions utilitaires internes
namespace {
    // EN: Generate unique ID for selection operations
    // FR: Générer un ID unique pour les opérations de sélection
    std::string generateSelectionId() {
        static std::atomic<uint64_t> counter{0};
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
        return "sel_" + std::to_string(timestamp) + "_" + std::to_string(++counter);
    }

    // EN: Calculate hash for cache key
    // FR: Calculer le hash pour la clé de cache
    std::string calculateCacheKey(const std::vector<PipelineStageConfig>& stages,
                                 const StageSelectionConfig& config) {
        std::hash<std::string> hasher;
        std::string combined;
        
        for (const auto& stage : stages) {
            combined += stage.id + stage.name + stage.executable;
        }
        
        combined += std::to_string(static_cast<int>(config.validation_level));
        combined += std::to_string(config.include_dependencies);
        combined += std::to_string(config.max_selected_stages);
        
        return "stage_sel_" + std::to_string(hasher(combined));
    }

    // EN: Check if stage matches filter criteria
    // FR: Vérifier si l'étape correspond aux critères de filtre
    bool matchesFilter(const PipelineStageConfig& stage, const StageSelectionFilter& filter) {
        switch (filter.criteria) {
            case StageSelectionCriteria::BY_ID:
                if (filter.exact_match) {
                    return filter.case_sensitive ? 
                        stage.id == filter.value : 
                        std::equal(stage.id.begin(), stage.id.end(),
                                  filter.value.begin(), filter.value.end(),
                                  [](char a, char b) { return std::tolower(a) == std::tolower(b); });
                } else {
                    std::string id = filter.case_sensitive ? stage.id : stage.id;
                    std::string val = filter.case_sensitive ? filter.value : filter.value;
                    if (!filter.case_sensitive) {
                        std::transform(id.begin(), id.end(), id.begin(), ::tolower);
                        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
                    }
                    return id.find(val) != std::string::npos;
                }
                
            case StageSelectionCriteria::BY_NAME:
                if (filter.exact_match) {
                    return filter.case_sensitive ? 
                        stage.name == filter.value : 
                        std::equal(stage.name.begin(), stage.name.end(),
                                  filter.value.begin(), filter.value.end(),
                                  [](char a, char b) { return std::tolower(a) == std::tolower(b); });
                } else {
                    std::string name = filter.case_sensitive ? stage.name : stage.name;
                    std::string val = filter.case_sensitive ? filter.value : filter.value;
                    if (!filter.case_sensitive) {
                        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
                    }
                    return name.find(val) != std::string::npos;
                }
                
            case StageSelectionCriteria::BY_PATTERN:
                if (filter.pattern) {
                    return std::regex_match(stage.id, *filter.pattern) || 
                           std::regex_match(stage.name, *filter.pattern);
                }
                return false;
                
            case StageSelectionCriteria::BY_TAG: {
                // EN: Check if stage has any of the required tags in metadata
                // FR: Vérifier si l'étape a un des tags requis dans les métadonnées
                auto tags_it = stage.metadata.find("tags");
                if (tags_it == stage.metadata.end()) return false;
                
                std::string stage_tags = tags_it->second;
                for (const auto& tag : filter.tags) {
                    if (stage_tags.find(tag) != std::string::npos) {
                        return true;
                    }
                }
                return false;
            }
                
            case StageSelectionCriteria::BY_PRIORITY:
                return stage.priority >= filter.min_priority && stage.priority <= filter.max_priority;
                
            case StageSelectionCriteria::BY_EXECUTION_TIME: {
                // EN: Check if estimated execution time is within range
                // FR: Vérifier si le temps d'exécution estimé est dans la plage
                auto timeout = stage.timeout;
                return timeout >= filter.min_execution_time && timeout <= filter.max_execution_time;
            }
                
            case StageSelectionCriteria::BY_CUSTOM:
                return filter.custom_filter ? filter.custom_filter(stage) : true;
                
            default:
                return true;
        }
    }
}

// EN: Stage Dependency Analyzer Implementation
// FR: Implémentation de l'Analyseur de Dépendances d'Étapes
class StageDependencyAnalyzer::DependencyAnalyzerImpl {
public:
    explicit DependencyAnalyzerImpl(const std::vector<PipelineStageConfig>& stages) 
        : stages_map_() {
        // EN: Build stage lookup map and dependency graph
        // FR: Construire la carte de recherche d'étapes et le graphe de dépendances
        for (const auto& stage : stages) {
            stages_map_[stage.id] = stage;
            dependency_graph_[stage.id] = stage.dependencies;
            
            // EN: Build reverse dependency graph
            // FR: Construire le graphe de dépendances inversé
            for (const auto& dep : stage.dependencies) {
                reverse_dependency_graph_[dep].push_back(stage.id);
            }
        }
    }

    std::vector<std::string> getDirectDependencies(const std::string& stage_id) const {
        auto it = dependency_graph_.find(stage_id);
        return it != dependency_graph_.end() ? it->second : std::vector<std::string>{};
    }

    std::vector<std::string> getTransitiveDependencies(const std::string& stage_id) const {
        std::vector<std::string> result;
        std::unordered_set<std::string> visited;
        std::function<void(const std::string&)> dfs = [&](const std::string& id) {
            if (visited.count(id)) return;
            visited.insert(id);
            
            auto it = dependency_graph_.find(id);
            if (it != dependency_graph_.end()) {
                for (const auto& dep : it->second) {
                    result.push_back(dep);
                    dfs(dep);
                }
            }
        };
        
        dfs(stage_id);
        return result;
    }

    std::vector<std::string> getDependents(const std::string& stage_id) const {
        auto it = reverse_dependency_graph_.find(stage_id);
        return it != reverse_dependency_graph_.end() ? it->second : std::vector<std::string>{};
    }

    std::vector<std::string> getTransitiveDependents(const std::string& stage_id) const {
        std::vector<std::string> result;
        std::unordered_set<std::string> visited;
        std::function<void(const std::string&)> dfs = [&](const std::string& id) {
            if (visited.count(id)) return;
            visited.insert(id);
            
            auto it = reverse_dependency_graph_.find(id);
            if (it != reverse_dependency_graph_.end()) {
                for (const auto& dep : it->second) {
                    result.push_back(dep);
                    dfs(dep);
                }
            }
        };
        
        dfs(stage_id);
        return result;
    }

    bool hasCycle() const {
        // EN: Use DFS with coloring to detect cycles
        // FR: Utiliser DFS avec coloration pour détecter les cycles
        std::unordered_map<std::string, int> colors; // 0=white, 1=gray, 2=black
        for (const auto& pair : stages_map_) {
            colors[pair.first] = 0;
        }
        
        std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool {
            colors[node] = 1; // Mark as gray (visiting)
            
            auto it = dependency_graph_.find(node);
            if (it != dependency_graph_.end()) {
                for (const auto& dep : it->second) {
                    if (colors[dep] == 1) { // Back edge found (cycle)
                        return true;
                    }
                    if (colors[dep] == 0 && dfs(dep)) {
                        return true;
                    }
                }
            }
            
            colors[node] = 2; // Mark as black (visited)
            return false;
        };
        
        for (const auto& pair : stages_map_) {
            if (colors[pair.first] == 0) {
                if (dfs(pair.first)) {
                    return true;
                }
            }
        }
        return false;
    }

    std::vector<std::string> findCycles() const {
        std::vector<std::string> cycles;
        std::unordered_map<std::string, int> colors;
        std::vector<std::string> path;
        
        for (const auto& pair : stages_map_) {
            colors[pair.first] = 0;
        }
        
        std::function<void(const std::string&)> dfs = [&](const std::string& node) {
            colors[node] = 1;
            path.push_back(node);
            
            auto it = dependency_graph_.find(node);
            if (it != dependency_graph_.end()) {
                for (const auto& dep : it->second) {
                    if (colors[dep] == 1) {
                        // EN: Found cycle, extract it from path
                        // FR: Cycle trouvé, l'extraire du chemin
                        auto cycle_start = std::find(path.begin(), path.end(), dep);
                        if (cycle_start != path.end()) {
                            std::string cycle_str;
                            for (auto it = cycle_start; it != path.end(); ++it) {
                                if (!cycle_str.empty()) cycle_str += " -> ";
                                cycle_str += *it;
                            }
                            cycle_str += " -> " + dep;
                            cycles.push_back(cycle_str);
                        }
                    } else if (colors[dep] == 0) {
                        dfs(dep);
                    }
                }
            }
            
            path.pop_back();
            colors[node] = 2;
        };
        
        for (const auto& pair : stages_map_) {
            if (colors[pair.first] == 0) {
                dfs(pair.first);
            }
        }
        
        return cycles;
    }

    std::vector<std::string> topologicalSort() const {
        std::vector<std::string> result;
        std::unordered_map<std::string, int> in_degree;
        
        // EN: Calculate in-degrees
        // FR: Calculer les degrés entrants
        for (const auto& pair : stages_map_) {
            in_degree[pair.first] = 0;
        }
        for (const auto& pair : dependency_graph_) {
            for (const auto& dep [[maybe_unused]] : pair.second) {
                in_degree[pair.first]++;
            }
        }
        
        // EN: Use Kahn's algorithm
        // FR: Utiliser l'algorithme de Kahn
        std::queue<std::string> queue;
        for (const auto& pair : in_degree) {
            if (pair.second == 0) {
                queue.push(pair.first);
            }
        }
        
        while (!queue.empty()) {
            std::string current = queue.front();
            queue.pop();
            result.push_back(current);
            
            auto it = reverse_dependency_graph_.find(current);
            if (it != reverse_dependency_graph_.end()) {
                for (const auto& dependent : it->second) {
                    in_degree[dependent]--;
                    if (in_degree[dependent] == 0) {
                        queue.push(dependent);
                    }
                }
            }
        }
        
        return result;
    }

    std::map<std::string, int> calculateDependencyDepths() const {
        std::map<std::string, int> depths;
        std::unordered_set<std::string> visited;
        
        std::function<int(const std::string&)> calculateDepth = [&](const std::string& stage_id) -> int {
            if (visited.count(stage_id)) {
                return depths[stage_id];
            }
            
            visited.insert(stage_id);
            int max_depth = 0;
            
            auto it = dependency_graph_.find(stage_id);
            if (it != dependency_graph_.end()) {
                for (const auto& dep : it->second) {
                    max_depth = std::max(max_depth, calculateDepth(dep) + 1);
                }
            }
            
            depths[stage_id] = max_depth;
            return max_depth;
        };
        
        for (const auto& pair : stages_map_) {
            calculateDepth(pair.first);
        }
        
        return depths;
    }

    std::vector<std::string> getCriticalPath() const {
        // EN: Find the longest path in the dependency graph
        // FR: Trouver le chemin le plus long dans le graphe de dépendances
        auto depths = calculateDependencyDepths();
        
        // EN: Find stage with maximum depth
        // FR: Trouver l'étape avec la profondeur maximale
        auto max_it = std::max_element(depths.begin(), depths.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        
        if (max_it == depths.end()) return {};
        
        // EN: Reconstruct path from leaf to root
        // FR: Reconstruire le chemin de la feuille à la racine
        std::vector<std::string> path;
        std::string current = max_it->first;
        
        while (!current.empty()) {
            path.push_back(current);
            
            // EN: Find dependency with highest depth
            // FR: Trouver la dépendance avec la profondeur la plus élevée
            std::string next_dep;
            int max_dep_depth = -1;
            
            auto it = dependency_graph_.find(current);
            if (it != dependency_graph_.end()) {
                for (const auto& dep : it->second) {
                    if (depths[dep] > max_dep_depth) {
                        max_dep_depth = depths[dep];
                        next_dep = dep;
                    }
                }
            }
            
            current = next_dep;
        }
        
        std::reverse(path.begin(), path.end());
        return path;
    }

    double calculateParallelismPotential() const {
        if (stages_map_.empty()) return 0.0;
        
        // EN: Calculate parallelism based on dependency structure
        // FR: Calculer le parallélisme basé sur la structure des dépendances
        auto topo_order = topologicalSort();
        std::map<std::string, int> levels;
        
        // EN: Assign levels based on dependencies
        // FR: Assigner des niveaux basés sur les dépendances
        for (const auto& stage_id : topo_order) {
            int max_dep_level = 0;
            auto it = dependency_graph_.find(stage_id);
            if (it != dependency_graph_.end()) {
                for (const auto& dep : it->second) {
                    max_dep_level = std::max(max_dep_level, levels[dep]);
                }
            }
            levels[stage_id] = max_dep_level + 1;
        }
        
        // EN: Count stages at each level
        // FR: Compter les étapes à chaque niveau
        std::map<int, int> level_counts;
        for (const auto& pair : levels) {
            level_counts[pair.second]++;
        }
        
        // EN: Calculate average parallelism
        // FR: Calculer le parallélisme moyen
        double total_parallel = 0.0;
        for (const auto& pair : level_counts) {
            total_parallel += pair.second;
        }
        
        return level_counts.empty() ? 0.0 : total_parallel / level_counts.size();
    }

private:
    std::unordered_map<std::string, PipelineStageConfig> stages_map_;
    std::unordered_map<std::string, std::vector<std::string>> dependency_graph_;
    std::unordered_map<std::string, std::vector<std::string>> reverse_dependency_graph_;
};

// EN: Stage Constraint Validator Implementation
// FR: Implémentation du Validateur de Contraintes d'Étapes
class StageConstraintValidator::ConstraintValidatorImpl {
public:
    ConstraintValidatorImpl() {
        // EN: Register default constraint validators
        // FR: Enregistrer les validateurs de contraintes par défaut
        registerDefaultValidators();
    }

    bool validateConstraint(const PipelineStageConfig& stage, StageExecutionConstraint constraint) const {
        auto it = validators_.find(constraint);
        if (it != validators_.end()) {
            return it->second(stage);
        }
        
        // EN: Default validation logic if no custom validator
        // FR: Logique de validation par défaut si pas de validateur personnalisé
        return defaultValidateConstraint(stage, constraint);
    }

    std::vector<StageExecutionConstraint> findViolatedConstraints(
        const PipelineStageConfig& stage,
        const std::vector<StageExecutionConstraint>& constraints) const {
        
        std::vector<StageExecutionConstraint> violations;
        for (auto constraint : constraints) {
            if (!validateConstraint(stage, constraint)) {
                violations.push_back(constraint);
            }
        }
        return violations;
    }

    bool checkConstraintCompatibility(const std::vector<StageExecutionConstraint>& constraints) const {
        // EN: Check for conflicting constraints
        // FR: Vérifier les contraintes conflictuelles
        std::set<StageExecutionConstraint> constraint_set(constraints.begin(), constraints.end());
        
        for (auto constraint : constraints) {
            auto conflicts = getConflictingConstraints(constraint);
            for (auto conflict : conflicts) {
                if (constraint_set.count(conflict)) {
                    return false;
                }
            }
        }
        return true;
    }

    void registerCustomValidator(StageExecutionConstraint constraint,
                               std::function<bool(const PipelineStageConfig&)> validator) {
        validators_[constraint] = validator;
    }

    std::vector<StageExecutionConstraint> inferConstraintsFromConfig(const PipelineStageConfig& stage) const {
        std::vector<StageExecutionConstraint> inferred;
        
        // EN: Infer constraints based on stage configuration
        // FR: Inférer les contraintes basées sur la configuration d'étape
        
        // EN: Check for network dependency
        // FR: Vérifier la dépendance réseau
        if (stage.executable.find("http") != std::string::npos ||
            stage.executable.find("network") != std::string::npos ||
            std::find_if(stage.arguments.begin(), stage.arguments.end(),
                        [](const std::string& arg) { 
                            return arg.find("--url") != std::string::npos || 
                                   arg.find("--host") != std::string::npos; 
                        }) != stage.arguments.end()) {
            inferred.push_back(StageExecutionConstraint::NETWORK_DEPENDENT);
        }
        
        // EN: Check for filesystem dependency
        // FR: Vérifier la dépendance système de fichiers
        if (std::find_if(stage.arguments.begin(), stage.arguments.end(),
                        [](const std::string& arg) { 
                            return arg.find("--input") != std::string::npos || 
                                   arg.find("--output") != std::string::npos ||
                                   arg.find(".csv") != std::string::npos; 
                        }) != stage.arguments.end()) {
            inferred.push_back(StageExecutionConstraint::FILESYSTEM_DEPENDENT);
        }
        
        // EN: Check for resource intensity based on timeout
        // FR: Vérifier l'intensité des ressources basée sur le timeout
        if (stage.timeout.count() > 300) { // More than 5 minutes
            inferred.push_back(StageExecutionConstraint::CPU_INTENSIVE);
        }
        
        // EN: Check for exclusive access needs
        // FR: Vérifier les besoins d'accès exclusif
        if (stage.executable.find("exclusive") != std::string::npos) {
            inferred.push_back(StageExecutionConstraint::EXCLUSIVE_ACCESS);
        }
        
        return inferred;
    }

private:
    std::unordered_map<StageExecutionConstraint, std::function<bool(const PipelineStageConfig&)>> validators_;

    void registerDefaultValidators() {
        validators_[StageExecutionConstraint::NETWORK_DEPENDENT] = [](const PipelineStageConfig& stage [[maybe_unused]]) {
            // EN: Basic network dependency check
            // FR: Vérification de base de la dépendance réseau
            return true; // Assume network is available
        };
        
        validators_[StageExecutionConstraint::FILESYSTEM_DEPENDENT] = [](const PipelineStageConfig& stage) {
            // EN: Basic filesystem dependency check
            // FR: Vérification de base de la dépendance système de fichiers
            return !stage.working_directory.empty();
        };
        
        validators_[StageExecutionConstraint::CPU_INTENSIVE] = [](const PipelineStageConfig& stage [[maybe_unused]]) {
            // EN: Check if system has sufficient CPU resources
            // FR: Vérifier si le système a suffisamment de ressources CPU
            return std::thread::hardware_concurrency() >= 2;
        };
        
        validators_[StageExecutionConstraint::MEMORY_INTENSIVE] = [](const PipelineStageConfig& stage [[maybe_unused]]) {
            // EN: Basic memory availability check
            // FR: Vérification de base de la disponibilité mémoire
            return true; // Simplified check
        };
    }

    bool defaultValidateConstraint(const PipelineStageConfig& stage, StageExecutionConstraint constraint) const {
        switch (constraint) {
            case StageExecutionConstraint::NONE:
                return true;
            case StageExecutionConstraint::SEQUENTIAL_ONLY:
                return true; // Always valid
            case StageExecutionConstraint::PARALLEL_SAFE:
                return stage.allow_failure || stage.max_retries == 0;
            default:
                return true;
        }
    }

    std::vector<StageExecutionConstraint> getConflictingConstraints(StageExecutionConstraint constraint) const {
        switch (constraint) {
            case StageExecutionConstraint::SEQUENTIAL_ONLY:
                return {StageExecutionConstraint::PARALLEL_SAFE};
            case StageExecutionConstraint::PARALLEL_SAFE:
                return {StageExecutionConstraint::SEQUENTIAL_ONLY, StageExecutionConstraint::EXCLUSIVE_ACCESS};
            case StageExecutionConstraint::EXCLUSIVE_ACCESS:
                return {StageExecutionConstraint::PARALLEL_SAFE};
            default:
                return {};
        }
    }
};

// EN: Stage Execution Planner Implementation
// FR: Implémentation du Planificateur d'Exécution d'Étapes
class StageExecutionPlanner::ExecutionPlannerImpl {
public:
    explicit ExecutionPlannerImpl(const StageSelectorConfig& config) : config_(config) {}

    StageExecutionPlan createPlan(const std::vector<PipelineStageConfig>& stages,
                                 const PipelineExecutionConfig& execution_config) {
        StageExecutionPlan plan;
        plan.plan_id = generateSelectionId();
        plan.stages = stages;
        plan.execution_config = execution_config;
        plan.created_at = std::chrono::system_clock::now();
        
        // EN: Analyze dependencies
        // FR: Analyser les dépendances
        StageDependencyAnalyzer analyzer(stages);
        plan.execution_order = analyzer.topologicalSort();
        plan.parallel_groups = identifyParallelGroups(stages);
        plan.critical_path = analyzer.getCriticalPath();
        
        // EN: Build dependency map
        // FR: Construire la carte des dépendances
        for (const auto& stage : stages) {
            for (const auto& dep : stage.dependencies) {
                plan.dependencies[stage.id].insert(dep);
            }
        }
        
        // EN: Estimate execution times
        // FR: Estimer les temps d'exécution
        plan.estimated_total_time = estimateTotalExecutionTime(stages, false);
        plan.estimated_parallel_time = estimateTotalExecutionTime(stages, true);
        
        // EN: Calculate resource requirements
        // FR: Calculer les exigences de ressources
        plan.resource_requirements = estimateResourceRequirements(stages);
        plan.peak_resource_usage = calculatePeakResourceUsage(plan.resource_requirements);
        
        // EN: Generate optimization suggestions
        // FR: Générer des suggestions d'optimisation
        plan.optimization_suggestions = generateOptimizationSuggestions(stages, analyzer);
        
        plan.is_valid = validatePlan(plan);
        return plan;
    }

    std::vector<std::string> optimizeExecutionOrder(const std::vector<PipelineStageConfig>& stages) {
        StageDependencyAnalyzer analyzer(stages);
        auto topo_order = analyzer.topologicalSort();
        
        // EN: Optimize based on priorities and estimated execution times
        // FR: Optimiser basé sur les priorités et les temps d'exécution estimés
        std::stable_sort(topo_order.begin(), topo_order.end(),
            [&stages](const std::string& a, const std::string& b) {
                auto stage_a = std::find_if(stages.begin(), stages.end(),
                    [&a](const auto& s) { return s.id == a; });
                auto stage_b = std::find_if(stages.begin(), stages.end(),
                    [&b](const auto& s) { return s.id == b; });
                
                if (stage_a != stages.end() && stage_b != stages.end()) {
                    // EN: Higher priority stages first
                    // FR: Étapes de priorité plus élevée en premier
                    if (stage_a->priority != stage_b->priority) {
                        return stage_a->priority > stage_b->priority;
                    }
                    // EN: Shorter stages first within same priority
                    // FR: Étapes plus courtes en premier dans la même priorité
                    return stage_a->timeout < stage_b->timeout;
                }
                return false;
            });
        
        return topo_order;
    }

    std::vector<std::vector<std::string>> identifyParallelGroups(const std::vector<PipelineStageConfig>& stages) {
        StageDependencyAnalyzer analyzer(stages);
        auto topo_order = analyzer.topologicalSort();
        
        // EN: Group stages by dependency level
        // FR: Grouper les étapes par niveau de dépendance
        std::map<int, std::vector<std::string>> levels;
        std::map<std::string, int> stage_levels;
        
        for (const auto& stage_id : topo_order) {
            int max_dep_level = 0;
            auto stage_it = std::find_if(stages.begin(), stages.end(),
                [&stage_id](const auto& s) { return s.id == stage_id; });
            
            if (stage_it != stages.end()) {
                for (const auto& dep : stage_it->dependencies) {
                    if (stage_levels.count(dep)) {
                        max_dep_level = std::max(max_dep_level, stage_levels[dep]);
                    }
                }
            }
            
            stage_levels[stage_id] = max_dep_level + 1;
            levels[max_dep_level + 1].push_back(stage_id);
        }
        
        // EN: Convert to vector of parallel groups
        // FR: Convertir en vecteur de groupes parallèles
        std::vector<std::vector<std::string>> parallel_groups;
        for (const auto& level : levels) {
            if (!level.second.empty()) {
                parallel_groups.push_back(level.second);
            }
        }
        
        return parallel_groups;
    }

    std::chrono::milliseconds estimateTotalExecutionTime(const std::vector<PipelineStageConfig>& stages,
                                                        bool consider_parallelism) const {
        if (stages.empty()) return std::chrono::milliseconds{0};
        
        if (!consider_parallelism) {
            // EN: Sequential execution time
            // FR: Temps d'exécution séquentiel
            auto total = std::chrono::milliseconds{0};
            for (const auto& stage : stages) {
                total += std::chrono::duration_cast<std::chrono::milliseconds>(stage.timeout);
            }
            return total;
        } else {
            // EN: Parallel execution time (critical path)
            // FR: Temps d'exécution parallèle (chemin critique)
            StageDependencyAnalyzer analyzer(stages);
            auto critical_path = analyzer.getCriticalPath();
            
            auto total = std::chrono::milliseconds{0};
            for (const auto& stage_id : critical_path) {
                auto stage_it = std::find_if(stages.begin(), stages.end(),
                    [&stage_id](const auto& s) { return s.id == stage_id; });
                if (stage_it != stages.end()) {
                    total += std::chrono::duration_cast<std::chrono::milliseconds>(stage_it->timeout);
                }
            }
            return total;
        }
    }

    std::map<std::string, double> estimateResourceRequirements(const std::vector<PipelineStageConfig>& stages) const {
        std::map<std::string, double> requirements;
        requirements["cpu"] = 0.0;
        requirements["memory"] = 0.0;
        requirements["network"] = 0.0;
        requirements["disk"] = 0.0;
        
        for (const auto& stage : stages) {
            // EN: Basic resource estimation based on stage configuration
            // FR: Estimation de base des ressources basée sur la configuration d'étape
            
            // EN: CPU estimation based on timeout and priority
            // FR: Estimation CPU basée sur le timeout et la priorité
            double cpu_factor = 1.0;
            switch (stage.priority) {
                case PipelineStagePriority::HIGH:
                    cpu_factor = 1.5;
                    break;
                case PipelineStagePriority::CRITICAL:
                    cpu_factor = 2.0;
                    break;
                default:
                    cpu_factor = 1.0;
                    break;
            }
            
            requirements["cpu"] += cpu_factor * (stage.timeout.count() / 60.0); // CPU-minutes
            requirements["memory"] += 100.0; // MB per stage (basic estimate)
            
            // EN: Network estimation based on executable type
            // FR: Estimation réseau basée sur le type d'exécutable
            if (stage.executable.find("http") != std::string::npos ||
                stage.executable.find("network") != std::string::npos) {
                requirements["network"] += 50.0; // MB network transfer
            }
            
            // EN: Disk estimation based on I/O operations
            // FR: Estimation disque basée sur les opérations I/O
            if (!stage.arguments.empty()) {
                requirements["disk"] += 10.0; // MB disk I/O
            }
        }
        
        return requirements;
    }

private:
    StageSelectorConfig config_;
    
    double calculatePeakResourceUsage(const std::map<std::string, double>& requirements) const {
        double peak = 0.0;
        for (const auto& req : requirements) {
            peak = std::max(peak, req.second);
        }
        return peak;
    }
    
    std::vector<std::string> generateOptimizationSuggestions(const std::vector<PipelineStageConfig>& stages,
                                                           const StageDependencyAnalyzer& analyzer) const {
        std::vector<std::string> suggestions;
        
        // EN: Check for optimization opportunities
        // FR: Vérifier les opportunités d'optimisation
        
        if (analyzer.calculateParallelismPotential() > 2.0) {
            suggestions.push_back("Consider increasing parallel execution to utilize available parallelism");
        }
        
        if (stages.size() > 20) {
            suggestions.push_back("Large number of stages detected - consider breaking into smaller pipelines");
        }
        
        auto critical_path = analyzer.getCriticalPath();
        if (critical_path.size() > stages.size() * 0.7) {
            suggestions.push_back("Critical path is long - consider optimizing stage dependencies");
        }
        
        return suggestions;
    }
    
    bool validatePlan(const StageExecutionPlan& plan) const {
        return !plan.stages.empty() && 
               !plan.execution_order.empty() && 
               plan.execution_order.size() == plan.stages.size();
    }
};

// EN: Stage Selector Main Implementation
// FR: Implémentation Principale du Sélecteur d'Étapes
class StageSelector::StageSelectorImpl {
public:
    explicit StageSelectorImpl(const StageSelectorConfig& config) 
        : config_(config)
        , thread_pool_(std::make_unique<ThreadPool>(ThreadPoolConfig{}))
        , constraint_validator_(std::make_unique<StageConstraintValidator>())
        , execution_planner_(std::make_unique<StageExecutionPlanner>(config))
        , statistics_{}
        , cache_()
        , event_callback_()
        , running_selections_()
        , selection_mutex_()
        , cache_mutex_()
        , stats_mutex_() {
        
        statistics_.last_reset = std::chrono::system_clock::now();
        
        // EN: Initialize constraint usage statistics
        // FR: Initialiser les statistiques d'utilisation des contraintes
        for (int i = 0; i <= static_cast<int>(StageSelectionCriteria::BY_CUSTOM); ++i) {
            statistics_.criteria_usage[static_cast<StageSelectionCriteria>(i)] = 0;
        }
        
        for (int i = 0; i <= static_cast<int>(StageValidationLevel::COMPREHENSIVE); ++i) {
            statistics_.validation_level_usage[static_cast<StageValidationLevel>(i)] = 0;
        }
    }

    ~StageSelectorImpl() {
        // EN: Cancel all running selections
        // FR: Annuler toutes les sélections en cours
        std::unique_lock<std::mutex> lock(selection_mutex_);
        for (auto& selection : running_selections_) {
            selection.second = true; // Mark as cancelled
        }
    }

    StageSelectionResult selectStages(const std::vector<PipelineStageConfig>& available_stages,
                                    const StageSelectionConfig& selection_config) {
        auto start_time = std::chrono::high_resolution_clock::now();
        std::string selection_id = generateSelectionId();
        
        // EN: Check cache first if enabled
        // FR: Vérifier le cache d'abord si activé
        if (config_.enable_caching && selection_config.enable_caching) {
            std::string cache_key = calculateCacheKey(available_stages, selection_config);
            auto cached_result = getCachedResult(cache_key);
            if (cached_result) {
                updateStatistics(true, std::chrono::milliseconds{0}, true);
                emitEvent(StageSelectorEventType::CACHE_HIT, selection_id);
                return *cached_result;
            } else {
                emitEvent(StageSelectorEventType::CACHE_MISS, selection_id);
            }
        }
        
        emitEvent(StageSelectorEventType::SELECTION_STARTED, selection_id);
        
        // EN: Register running selection
        // FR: Enregistrer la sélection en cours
        {
            std::unique_lock<std::mutex> lock(selection_mutex_);
            running_selections_[selection_id] = false;
        }
        
        StageSelectionResult result;
        result.selection_timestamp = std::chrono::system_clock::now();
        result.total_available_stages = available_stages.size();
        
        try {
            // EN: Perform stage selection
            // FR: Effectuer la sélection d'étapes
            result = performSelection(available_stages, selection_config, selection_id);
            
            // EN: Cache result if successful and caching enabled
            // FR: Mettre en cache le résultat si succès et cache activé
            if (config_.enable_caching && selection_config.enable_caching && 
                result.status == StageSelectionStatus::SUCCESS) {
                std::string cache_key = calculateCacheKey(available_stages, selection_config);
                result.cache_key = cache_key;
                cacheResult(cache_key, result);
            }
            
            emitEvent(StageSelectorEventType::SELECTION_COMPLETED, selection_id);
            
        } catch (const std::exception& e) {
            result.status = StageSelectionStatus::CONFIGURATION_ERROR;
            result.errors.push_back("Selection failed: " + std::string(e.what()));
            emitEvent(StageSelectorEventType::SELECTION_FAILED, selection_id, "", e.what());
        }
        
        // EN: Unregister running selection
        // FR: Désenregistrer la sélection en cours
        {
            std::unique_lock<std::mutex> lock(selection_mutex_);
            running_selections_.erase(selection_id);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.selection_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        updateStatistics(result.status == StageSelectionStatus::SUCCESS, result.selection_time, false);
        
        return result;
    }

    std::future<StageSelectionResult> selectStagesAsync(const std::vector<PipelineStageConfig>& available_stages,
                                                       const StageSelectionConfig& selection_config) {
        return thread_pool_->submit(TaskPriority::NORMAL, [this, available_stages, selection_config]() {
            return selectStages(available_stages, selection_config);
        });
    }

    StageSelectionResult selectStagesByIds(const std::vector<PipelineStageConfig>& available_stages,
                                          const std::vector<std::string>& stage_ids,
                                          StageValidationLevel validation_level) {
        StageSelectionConfig config;
        config.validation_level = validation_level;
        
        // EN: Create ID-based filters
        // FR: Créer des filtres basés sur les ID
        for (const auto& id : stage_ids) {
            StageSelectionFilter filter;
            filter.criteria = StageSelectionCriteria::BY_ID;
            filter.mode = StageFilterMode::INCLUDE;
            filter.value = id;
            filter.exact_match = true;
            config.filters.push_back(filter);
        }
        
        return selectStages(available_stages, config);
    }

    StageSelectionResult selectStagesByPattern(const std::vector<PipelineStageConfig>& available_stages,
                                              const std::string& pattern,
                                              bool include_dependencies) {
        StageSelectionConfig config;
        config.include_dependencies = include_dependencies;
        
        StageSelectionFilter filter;
        filter.criteria = StageSelectionCriteria::BY_PATTERN;
        filter.mode = StageFilterMode::INCLUDE;
        filter.value = pattern;
        
        try {
            filter.pattern = std::regex(pattern);
        } catch (const std::regex_error& e) {
            StageSelectionResult result;
            result.status = StageSelectionStatus::CONFIGURATION_ERROR;
            result.errors.push_back("Invalid regex pattern: " + std::string(e.what()));
            return result;
        }
        
        config.filters.push_back(filter);
        return selectStages(available_stages, config);
    }

    std::vector<PipelineStageConfig> filterStages(const std::vector<PipelineStageConfig>& stages,
                                                 const std::vector<StageSelectionFilter>& filters) {
        std::vector<PipelineStageConfig> result;
        
        for (const auto& stage : stages) {
            bool include = true;
            bool exclude = false;
            bool all_required_met = true;
            
            for (const auto& filter : filters) {
                bool matches = matchesFilter(stage, filter);
                
                switch (filter.mode) {
                    case StageFilterMode::INCLUDE:
                        if (matches) {
                            include = true;
                        }
                        break;
                    case StageFilterMode::EXCLUDE:
                        if (matches) {
                            exclude = true;
                        }
                        break;
                    case StageFilterMode::REQUIRE:
                        if (!matches) {
                            all_required_met = false;
                        }
                        break;
                }
            }
            
            if (include && !exclude && all_required_met) {
                result.push_back(stage);
            }
        }
        
        return result;
    }

    bool validateStageSelection(const std::vector<PipelineStageConfig>& stages, StageValidationLevel level) {
        switch (level) {
            case StageValidationLevel::NONE:
                return true;
                
            case StageValidationLevel::BASIC: {
                // EN: Basic validation - check stage IDs and executables
                // FR: Validation de base - vérifier les ID d'étapes et exécutables
                std::set<std::string> ids;
                for (const auto& stage : stages) {
                    if (stage.id.empty() || stage.executable.empty()) {
                        return false;
                    }
                    if (ids.count(stage.id)) {
                        return false; // Duplicate ID
                    }
                    ids.insert(stage.id);
                }
                return true;
            }
            
            case StageValidationLevel::DEPENDENCIES: {
                // EN: Validate dependencies
                // FR: Valider les dépendances
                if (!validateStageSelection(stages, StageValidationLevel::BASIC)) {
                    return false;
                }
                
                StageDependencyAnalyzer analyzer(stages);
                return !analyzer.hasCycle();
            }
            
            case StageValidationLevel::RESOURCES:
            case StageValidationLevel::COMPATIBILITY:
            case StageValidationLevel::COMPREHENSIVE: {
                // EN: Comprehensive validation
                // FR: Validation complète
                if (!validateStageSelection(stages, StageValidationLevel::DEPENDENCIES)) {
                    return false;
                }
                
                auto compatibility = analyzeStageCompatibility(stages);
                return compatibility.are_compatible;
            }
        }
        
        return true;
    }

    StageCompatibilityResult analyzeStageCompatibility(const std::vector<PipelineStageConfig>& stages) {
        StageCompatibilityResult result;
        result.are_compatible = true;
        result.compatibility_score = 1.0;
        
        // EN: Check for conflicting constraints
        // FR: Vérifier les contraintes conflictuelles
        std::map<std::string, std::vector<StageExecutionConstraint>> stage_constraints;
        
        for (const auto& stage : stages) {
            stage_constraints[stage.id] = constraint_validator_->inferConstraintsFromConfig(stage);
        }
        
        // EN: Analyze constraint compatibility
        // FR: Analyser la compatibilité des contraintes
        for (const auto& stage1 : stages) {
            for (const auto& stage2 : stages) {
                if (stage1.id >= stage2.id) continue; // Avoid duplicate comparisons
                
                auto constraints1 = stage_constraints[stage1.id];
                auto constraints2 = stage_constraints[stage2.id];
                
                // EN: Check for conflicting constraints
                // FR: Vérifier les contraintes conflictuelles
                for (auto c1 : constraints1) {
                    for (auto c2 : constraints2) {
                        if (!areConstraintsCompatible(c1, c2)) {
                            result.are_compatible = false;
                            result.incompatible_stages.push_back(stage1.id);
                            result.incompatible_stages.push_back(stage2.id);
                            result.conflicts[stage1.id].push_back(
                                "Constraint conflict with " + stage2.id + 
                                ": " + StageSelectorUtils::constraintToString(c1) + 
                                " vs " + StageSelectorUtils::constraintToString(c2)
                            );
                        }
                    }
                }
            }
        }
        
        // EN: Calculate compatibility scores
        // FR: Calculer les scores de compatibilité
        for (const auto& stage : stages) {
            if (std::find(result.incompatible_stages.begin(), result.incompatible_stages.end(), stage.id) 
                == result.incompatible_stages.end()) {
                result.compatible_stages.push_back(stage.id);
                result.stage_compatibility_scores[stage.id] = 1.0;
            } else {
                result.stage_compatibility_scores[stage.id] = 0.5;
            }
        }
        
        // EN: Calculate overall compatibility score
        // FR: Calculer le score de compatibilité global
        if (!result.stage_compatibility_scores.empty()) {
            double total_score = 0.0;
            for (const auto& score : result.stage_compatibility_scores) {
                total_score += score.second;
            }
            result.compatibility_score = total_score / result.stage_compatibility_scores.size();
        }
        
        return result;
    }

    std::vector<std::string> resolveDependencies(const std::vector<PipelineStageConfig>& all_stages,
                                               const std::vector<std::string>& selected_stage_ids,
                                               bool include_transitive) {
        std::set<std::string> resolved_deps;
        std::set<std::string> selected_set(selected_stage_ids.begin(), selected_stage_ids.end());
        
        StageDependencyAnalyzer analyzer(all_stages);
        
        for (const auto& stage_id : selected_stage_ids) {
            std::vector<std::string> deps = include_transitive ? 
                analyzer.getTransitiveDependencies(stage_id) :
                analyzer.getDirectDependencies(stage_id);
            
            for (const auto& dep : deps) {
                if (selected_set.find(dep) == selected_set.end()) {
                    resolved_deps.insert(dep);
                }
            }
        }
        
        return std::vector<std::string>(resolved_deps.begin(), resolved_deps.end());
    }

    std::vector<std::string> findDependents(const std::vector<PipelineStageConfig>& all_stages,
                                          const std::vector<std::string>& selected_stage_ids) {
        std::set<std::string> dependents;
        std::set<std::string> selected_set(selected_stage_ids.begin(), selected_stage_ids.end());
        
        StageDependencyAnalyzer analyzer(all_stages);
        
        for (const auto& stage_id : selected_stage_ids) {
            auto stage_dependents = analyzer.getTransitiveDependents(stage_id);
            for (const auto& dep : stage_dependents) {
                if (selected_set.find(dep) == selected_set.end()) {
                    dependents.insert(dep);
                }
            }
        }
        
        return std::vector<std::string>(dependents.begin(), dependents.end());
    }

    std::vector<std::string> detectCircularDependencies(const std::vector<PipelineStageConfig>& stages) {
        StageDependencyAnalyzer analyzer(stages);
        return analyzer.findCycles();
    }

    StageExecutionPlan createExecutionPlan(const std::vector<PipelineStageConfig>& stages,
                                          const PipelineExecutionConfig& execution_config) {
        return execution_planner_->createPlan(stages, execution_config);
    }

    std::vector<std::string> optimizeExecutionOrder(const std::vector<PipelineStageConfig>& stages) {
        return execution_planner_->optimizeExecutionOrder(stages);
    }

    std::vector<std::vector<std::string>> identifyParallelExecutionGroups(const std::vector<PipelineStageConfig>& stages) {
        return execution_planner_->identifyParallelGroups(stages);
    }

    bool checkStageConstraints(const PipelineStageConfig& stage,
                              const std::vector<StageExecutionConstraint>& allowed_constraints,
                              const std::vector<StageExecutionConstraint>& forbidden_constraints) {
        auto inferred_constraints = constraint_validator_->inferConstraintsFromConfig(stage);
        
        // EN: Check if any inferred constraints are forbidden
        // FR: Vérifier si des contraintes inférées sont interdites
        for (auto constraint : inferred_constraints) {
            if (std::find(forbidden_constraints.begin(), forbidden_constraints.end(), constraint) 
                != forbidden_constraints.end()) {
                return false;
            }
        }
        
        // EN: Check if all inferred constraints are allowed (if allowed list is specified)
        // FR: Vérifier si toutes les contraintes inférées sont autorisées (si liste autorisée spécifiée)
        if (!allowed_constraints.empty()) {
            for (auto constraint : inferred_constraints) {
                if (std::find(allowed_constraints.begin(), allowed_constraints.end(), constraint) 
                    == allowed_constraints.end()) {
                    return false;
                }
            }
        }
        
        return true;
    }

    std::vector<StageExecutionConstraint> inferStageConstraints(const PipelineStageConfig& stage) {
        return constraint_validator_->inferConstraintsFromConfig(stage);
    }

    void registerConstraintValidator(StageExecutionConstraint constraint,
                                   std::function<bool(const PipelineStageConfig&)> validator) {
        constraint_validator_->registerCustomValidator(constraint, validator);
    }

    // EN: Additional methods for statistics, caching, and configuration management
    // FR: Méthodes supplémentaires pour statistiques, cache et gestion configuration
    StageSelectorConfig getConfig() const {
        return config_;
    }
    
    void updateConfig(const StageSelectorConfig& new_config) {
        std::unique_lock<std::mutex> lock(selection_mutex_);
        config_ = new_config;
    }
    
    StageSelectorStatistics getStatistics() const {
        std::unique_lock<std::mutex> lock(stats_mutex_);
        return statistics_;
    }
    
    void resetStatistics() {
        std::unique_lock<std::mutex> lock(stats_mutex_);
        statistics_ = StageSelectorStatistics{};
        statistics_.last_reset = std::chrono::system_clock::now();
    }
    
    void setEventCallback(StageSelectorEventCallback callback) {
        std::unique_lock<std::mutex> lock(selection_mutex_); // Using selection_mutex since callback_mutex_ not in member list
        event_callback_ = callback;
    }
    
    void clearCache() {
        std::unique_lock<std::mutex> lock(cache_mutex_);
        cache_.clear();
    }
    
    size_t getCacheSize() const {
        std::unique_lock<std::mutex> lock(cache_mutex_);
        return cache_.size();
    }
    
    void updateStatistics(bool success, std::chrono::milliseconds selection_time, bool was_cached) {
        std::unique_lock<std::mutex> lock(stats_mutex_);
        
        statistics_.total_selections++;
        if (success) {
            statistics_.successful_selections++;
        } else {
            statistics_.failed_selections++;
        }
        
        if (was_cached) {
            statistics_.cached_selections++;
        }
        
        statistics_.total_selection_time += selection_time;
        
        if (statistics_.total_selections > 0) {
            statistics_.avg_selection_time = std::chrono::milliseconds(
                statistics_.total_selection_time.count() / statistics_.total_selections
            );
        }
        
        if (statistics_.min_selection_time.count() == 0 || selection_time < statistics_.min_selection_time) {
            statistics_.min_selection_time = selection_time;
        }
        
        if (selection_time > statistics_.max_selection_time) {
            statistics_.max_selection_time = selection_time;
        }
    }

    void emitEvent(StageSelectorEventType type, const std::string& selection_id,
                   const std::string& stage_id = "", const std::string& message = "") {
        if (!event_callback_) return;
        
        StageSelectorEvent event;
        event.type = type;
        event.timestamp = std::chrono::system_clock::now();
        event.selection_id = selection_id;
        event.stage_id = stage_id;
        event.message = message;
        event.success = (type != StageSelectorEventType::SELECTION_FAILED);
        
        event_callback_(event);
    }

    std::optional<StageSelectionResult> getCachedResult(const std::string& cache_key) {
        std::unique_lock<std::mutex> lock(cache_mutex_);
        auto it = cache_.find(cache_key);
        if (it != cache_.end()) {
            auto now = std::chrono::system_clock::now();
            if (now - it->second.first < config_.cache_ttl) {
                return it->second.second;
            } else {
                cache_.erase(it);
            }
        }
        return std::nullopt;
    }

    void cacheResult(const std::string& cache_key, const StageSelectionResult& result) {
        std::unique_lock<std::mutex> lock(cache_mutex_);
        
        // EN: Enforce cache size limit
        // FR: Appliquer la limite de taille du cache
        if (cache_.size() >= config_.max_cache_entries) {
            // EN: Remove oldest entry
            // FR: Supprimer l'entrée la plus ancienne
            auto oldest = cache_.begin();
            for (auto it = cache_.begin(); it != cache_.end(); ++it) {
                if (it->second.first < oldest->second.first) {
                    oldest = it;
                }
            }
            cache_.erase(oldest);
        }
        
        cache_[cache_key] = {std::chrono::system_clock::now(), result};
    }

    bool areConstraintsCompatible(StageExecutionConstraint c1, StageExecutionConstraint c2) const {
        // EN: Define incompatible constraint pairs
        // FR: Définir les paires de contraintes incompatibles
        static const std::map<StageExecutionConstraint, std::vector<StageExecutionConstraint>> incompatible_pairs = {
            {StageExecutionConstraint::SEQUENTIAL_ONLY, {StageExecutionConstraint::PARALLEL_SAFE}},
            {StageExecutionConstraint::PARALLEL_SAFE, {StageExecutionConstraint::SEQUENTIAL_ONLY, StageExecutionConstraint::EXCLUSIVE_ACCESS}},
            {StageExecutionConstraint::EXCLUSIVE_ACCESS, {StageExecutionConstraint::PARALLEL_SAFE}}
        };
        
        auto it = incompatible_pairs.find(c1);
        if (it != incompatible_pairs.end()) {
            return std::find(it->second.begin(), it->second.end(), c2) == it->second.end();
        }
        
        return true; // No known incompatibilities
    }

    StageSelectionResult performSelection(const std::vector<PipelineStageConfig>& available_stages,
                                        const StageSelectionConfig& selection_config,
                                        const std::string& selection_id) {
        StageSelectionResult result;
        result.total_available_stages = available_stages.size();
        result.status = StageSelectionStatus::SUCCESS;
        
        // EN: Apply filters to select stages
        // FR: Appliquer les filtres pour sélectionner les étapes
        auto filtered_stages = filterStages(available_stages, selection_config.filters);
        result.filtered_stages = filtered_stages.size();
        
        if (filtered_stages.empty()) {
            result.status = StageSelectionStatus::EMPTY_SELECTION;
            result.warnings.push_back("No stages matched the selection criteria");
            return result;
        }
        
        // EN: Include dependencies if requested
        // FR: Inclure les dépendances si demandé
        if (selection_config.include_dependencies) {
            std::vector<std::string> filtered_ids;
            for (const auto& stage : filtered_stages) {
                filtered_ids.push_back(stage.id);
            }
            
            auto dependencies = resolveDependencies(available_stages, filtered_ids, true);
            
            // EN: Add dependency stages to selection
            // FR: Ajouter les étapes de dépendance à la sélection
            for (const auto& dep_id : dependencies) {
                auto dep_stage = std::find_if(available_stages.begin(), available_stages.end(),
                    [&dep_id](const auto& s) { return s.id == dep_id; });
                if (dep_stage != available_stages.end()) {
                    // EN: Check if not already included
                    // FR: Vérifier si pas déjà inclus
                    if (std::find_if(filtered_stages.begin(), filtered_stages.end(),
                                   [&dep_id](const auto& s) { return s.id == dep_id; }) == filtered_stages.end()) {
                        filtered_stages.push_back(*dep_stage);
                    }
                }
            }
        }
        
        // EN: Validate selection
        // FR: Valider la sélection
        emitEvent(StageSelectorEventType::VALIDATION_STARTED, selection_id);
        
        if (!validateStageSelection(filtered_stages, selection_config.validation_level)) {
            result.status = StageSelectionStatus::VALIDATION_FAILED;
            result.errors.push_back("Stage selection validation failed");
            return result;
        }
        
        emitEvent(StageSelectorEventType::VALIDATION_COMPLETED, selection_id);
        
        // EN: Check for circular dependencies
        // FR: Vérifier les dépendances circulaires
        auto cycles = detectCircularDependencies(filtered_stages);
        if (!cycles.empty()) {
            result.status = StageSelectionStatus::CIRCULAR_DEPENDENCY;
            result.errors.push_back("Circular dependencies detected: " + cycles[0]);
            return result;
        }
        
        // EN: Analyze compatibility
        // FR: Analyser la compatibilité
        result.compatibility = analyzeStageCompatibility(filtered_stages);
        if (!result.compatibility.are_compatible && !selection_config.allow_partial_selection) {
            result.status = StageSelectionStatus::INCOMPATIBLE_STAGES;
            result.errors.push_back("Selected stages are incompatible");
            return result;
        }
        
        // EN: Generate execution plan
        // FR: Générer le plan d'exécution
        result.selected_stages = filtered_stages;
        for (const auto& stage : filtered_stages) {
            result.selected_stage_ids.push_back(stage.id);
        }
        
        result.execution_order = optimizeExecutionOrder(filtered_stages);
        result.execution_levels = identifyParallelExecutionGroups(filtered_stages);
        
        // EN: Build dependency chain
        // FR: Construire la chaîne de dépendances
        StageDependencyAnalyzer analyzer(filtered_stages);
        result.dependency_chain = analyzer.topologicalSort();
        
        // EN: Calculate estimates
        // FR: Calculer les estimations
        result.estimated_execution_time = execution_planner_->estimateTotalExecutionTime(filtered_stages, true);
        result.resource_estimates = execution_planner_->estimateResourceRequirements(filtered_stages);
        
        // EN: Per-stage estimates
        // FR: Estimations par étape
        for (const auto& stage : filtered_stages) {
            result.stage_execution_estimates[stage.id] = 
                std::chrono::duration_cast<std::chrono::milliseconds>(stage.timeout);
        }
        
        // EN: Calculate selection ratio
        // FR: Calculer le ratio de sélection
        result.selection_ratio = static_cast<double>(result.selected_stages.size()) / result.total_available_stages;
        
        // EN: Add informational messages
        // FR: Ajouter des messages informatifs
        result.information.push_back("Selected " + std::to_string(result.selected_stages.size()) + 
                                   " stages out of " + std::to_string(result.total_available_stages) + " available");
        
        if (result.compatibility.compatibility_score < 1.0) {
            result.warnings.push_back("Some stages have compatibility issues (score: " + 
                                    std::to_string(result.compatibility.compatibility_score) + ")");
        }
        
        return result;
    }

private:
    StageSelectorConfig config_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<StageConstraintValidator> constraint_validator_;
    std::unique_ptr<StageExecutionPlanner> execution_planner_;
    StageSelectorStatistics statistics_;
    std::map<std::string, std::pair<std::chrono::system_clock::time_point, StageSelectionResult>> cache_;
    StageSelectorEventCallback event_callback_;
    std::map<std::string, bool> running_selections_; // selection_id -> cancelled
    
    mutable std::mutex selection_mutex_;
    mutable std::mutex cache_mutex_;
    mutable std::mutex stats_mutex_;
};

// EN: Stage Selector Public Interface Implementation
// FR: Implémentation de l'Interface Publique du Sélecteur d'Étapes

StageSelector::StageSelector(const StageSelectorConfig& config) 
    : impl_(std::make_unique<StageSelectorImpl>(config)) {}

StageSelector::~StageSelector() = default;

StageSelectionResult StageSelector::selectStages(const std::vector<PipelineStageConfig>& available_stages,
                                                const StageSelectionConfig& selection_config) {
    return impl_->selectStages(available_stages, selection_config);
}

std::future<StageSelectionResult> StageSelector::selectStagesAsync(
    const std::vector<PipelineStageConfig>& available_stages,
    const StageSelectionConfig& selection_config) {
    return impl_->selectStagesAsync(available_stages, selection_config);
}

StageSelectionResult StageSelector::selectStagesByIds(
    const std::vector<PipelineStageConfig>& available_stages,
    const std::vector<std::string>& stage_ids,
    StageValidationLevel validation_level) {
    return impl_->selectStagesByIds(available_stages, stage_ids, validation_level);
}

StageSelectionResult StageSelector::selectStagesByPattern(
    const std::vector<PipelineStageConfig>& available_stages,
    const std::string& pattern,
    bool include_dependencies) {
    return impl_->selectStagesByPattern(available_stages, pattern, include_dependencies);
}

std::vector<PipelineStageConfig> StageSelector::filterStages(
    const std::vector<PipelineStageConfig>& stages,
    const std::vector<StageSelectionFilter>& filters) {
    return impl_->filterStages(stages, filters);
}

bool StageSelector::validateStageSelection(const std::vector<PipelineStageConfig>& stages,
                                          StageValidationLevel level) {
    return impl_->validateStageSelection(stages, level);
}

StageCompatibilityResult StageSelector::analyzeStageCompatibility(const std::vector<PipelineStageConfig>& stages) {
    return impl_->analyzeStageCompatibility(stages);
}

std::vector<std::string> StageSelector::resolveDependencies(
    const std::vector<PipelineStageConfig>& all_stages,
    const std::vector<std::string>& selected_stage_ids,
    bool include_transitive) {
    return impl_->resolveDependencies(all_stages, selected_stage_ids, include_transitive);
}

std::vector<std::string> StageSelector::findDependents(
    const std::vector<PipelineStageConfig>& all_stages,
    const std::vector<std::string>& selected_stage_ids) {
    return impl_->findDependents(all_stages, selected_stage_ids);
}

std::vector<std::string> StageSelector::detectCircularDependencies(const std::vector<PipelineStageConfig>& stages) {
    return impl_->detectCircularDependencies(stages);
}

StageExecutionPlan StageSelector::createExecutionPlan(const std::vector<PipelineStageConfig>& stages,
                                                     const PipelineExecutionConfig& execution_config) {
    return impl_->createExecutionPlan(stages, execution_config);
}

std::vector<std::string> StageSelector::optimizeExecutionOrder(const std::vector<PipelineStageConfig>& stages) {
    return impl_->optimizeExecutionOrder(stages);
}

std::vector<std::vector<std::string>> StageSelector::identifyParallelExecutionGroups(
    const std::vector<PipelineStageConfig>& stages) {
    return impl_->identifyParallelExecutionGroups(stages);
}

bool StageSelector::checkStageConstraints(const PipelineStageConfig& stage,
                                         const std::vector<StageExecutionConstraint>& allowed_constraints,
                                         const std::vector<StageExecutionConstraint>& forbidden_constraints) {
    return impl_->checkStageConstraints(stage, allowed_constraints, forbidden_constraints);
}

std::vector<StageExecutionConstraint> StageSelector::inferStageConstraints(const PipelineStageConfig& stage) {
    return impl_->inferStageConstraints(stage);
}

void StageSelector::registerConstraintValidator(StageExecutionConstraint constraint,
                                               std::function<bool(const PipelineStageConfig&)> validator) {
    impl_->registerConstraintValidator(constraint, validator);
}

std::chrono::milliseconds StageSelector::estimateStageExecutionTime(const PipelineStageConfig& stage) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(stage.timeout);
}

double StageSelector::estimateStageResourceUsage(const PipelineStageConfig& stage, const std::string& resource_type) {
    // EN: Basic resource usage estimation based on stage configuration
    // FR: Estimation de base de l'usage des ressources basée sur la configuration d'étape
    if (resource_type == "cpu") {
        double cpu_factor = 1.0;
        switch (stage.priority) {
            case PipelineStagePriority::HIGH: cpu_factor = 1.5; break;
            case PipelineStagePriority::CRITICAL: cpu_factor = 2.0; break;
            default: cpu_factor = 1.0; break;
        }
        return cpu_factor * (stage.timeout.count() / 60.0); // CPU-minutes
    } else if (resource_type == "memory") {
        return 100.0; // MB per stage (basic estimate)
    } else if (resource_type == "network") {
        if (stage.executable.find("http") != std::string::npos ||
            stage.executable.find("network") != std::string::npos) {
            return 50.0; // MB network transfer
        }
        return 0.0;
    } else if (resource_type == "disk") {
        return stage.arguments.empty() ? 0.0 : 10.0; // MB disk I/O
    }
    return 0.0;
}

double StageSelector::calculateStageSuccessRate(const std::string& stage_id [[maybe_unused]]) {
    // EN: Basic success rate calculation (would normally use historical data)
    // FR: Calcul de base du taux de succès (utiliserait normalement des données historiques)
    return 0.95; // 95% success rate by default
}

std::map<std::string, std::string> StageSelector::extractStageMetadata(const PipelineStageConfig& stage) {
    std::map<std::string, std::string> metadata = stage.metadata;
    
    // EN: Add computed metadata
    // FR: Ajouter des métadonnées calculées
    metadata["estimated_duration"] = std::to_string(stage.timeout.count()) + "s";
    metadata["priority"] = std::to_string(static_cast<int>(stage.priority));
    metadata["dependencies_count"] = std::to_string(stage.dependencies.size());
    metadata["has_retries"] = stage.max_retries > 0 ? "true" : "false";
    metadata["allow_failure"] = stage.allow_failure ? "true" : "false";
    
    return metadata;
}

void StageSelector::enableCaching(bool enable) {
    StageSelectorConfig config = impl_->getConfig();
    config.enable_caching = enable;
    impl_->updateConfig(config);
}

void StageSelector::clearCache() {
    impl_->clearCache();
}

void StageSelector::setCacheTTL(std::chrono::seconds ttl) {
    StageSelectorConfig config = impl_->getConfig();
    config.cache_ttl = ttl;
    impl_->updateConfig(config);
}

size_t StageSelector::getCacheSize() const {
    return impl_->getCacheSize();
}

double StageSelector::getCacheHitRatio() const {
    auto stats = impl_->getStatistics();
    if (stats.total_selections == 0) return 0.0;
    return static_cast<double>(stats.cached_selections) / stats.total_selections;
}

void StageSelector::setEventCallback(StageSelectorEventCallback callback) {
    impl_->setEventCallback(callback);
}

void StageSelector::removeEventCallback() {
    setEventCallback(nullptr);
}

void StageSelector::emitEvent(StageSelectorEventType type [[maybe_unused]], const std::string& selection_id [[maybe_unused]],
                             const std::string& stage_id [[maybe_unused]], const std::string& message [[maybe_unused]]) {
    // EN: Implementation handled by impl_
    // FR: Implémentation gérée par impl_
}

StageSelectorStatistics StageSelector::getStatistics() const {
    return impl_->getStatistics();
}

void StageSelector::resetStatistics() {
    impl_->resetStatistics();
}

bool StageSelector::isHealthy() const {
    return true; // EN: Basic health check / FR: Vérification de santé de base
}

std::string StageSelector::getStatus() const {
    auto stats = getStatistics();
    std::ostringstream status;
    status << "StageSelector: " << stats.total_selections << " selections, "
           << stats.successful_selections << " successful, "
           << "avg time: " << stats.avg_selection_time.count() << "ms";
    return status.str();
}

void StageSelector::updateConfig(const StageSelectorConfig& new_config) {
    impl_->updateConfig(new_config);
}

StageSelectorConfig StageSelector::getConfig() const {
    return impl_->getConfig();
}

bool StageSelector::exportSelectionResult(const StageSelectionResult& result, const std::string& filepath) const {
    try {
        auto json = StageSelectorUtils::selectionResultToJson(result);
        std::ofstream file(filepath);
        file << json.dump(2);
        return file.good();
    } catch (const std::exception& e) {
        return false;
    }
}

std::optional<StageSelectionResult> StageSelector::importSelectionResult(const std::string& filepath) const {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) return std::nullopt;
        
        nlohmann::json json;
        file >> json;
        return StageSelectorUtils::selectionResultFromJson(json);
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

bool StageSelector::exportExecutionPlan(const StageExecutionPlan& plan, const std::string& filepath) const {
    try {
        auto json = StageSelectorUtils::executionPlanToJson(plan);
        std::ofstream file(filepath);
        file << json.dump(2);
        return file.good();
    } catch (const std::exception& e) {
        return false;
    }
}

std::optional<StageExecutionPlan> StageSelector::importExecutionPlan(const std::string& filepath) const {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) return std::nullopt;
        
        nlohmann::json json;
        file >> json;
        return StageSelectorUtils::executionPlanFromJson(json);
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

// EN: Helper class implementations
// FR: Implémentations des classes d'aide

StageDependencyAnalyzer::StageDependencyAnalyzer(const std::vector<PipelineStageConfig>& stages)
    : impl_(std::make_unique<DependencyAnalyzerImpl>(stages)) {}

std::vector<std::string> StageDependencyAnalyzer::getDirectDependencies(const std::string& stage_id) const {
    return impl_->getDirectDependencies(stage_id);
}

std::vector<std::string> StageDependencyAnalyzer::getTransitiveDependencies(const std::string& stage_id) const {
    return impl_->getTransitiveDependencies(stage_id);
}

std::vector<std::string> StageDependencyAnalyzer::getDependents(const std::string& stage_id) const {
    return impl_->getDependents(stage_id);
}

std::vector<std::string> StageDependencyAnalyzer::getTransitiveDependents(const std::string& stage_id) const {
    return impl_->getTransitiveDependents(stage_id);
}

bool StageDependencyAnalyzer::hasCycle() const {
    return impl_->hasCycle();
}

std::vector<std::string> StageDependencyAnalyzer::findCycles() const {
    return impl_->findCycles();
}

std::vector<std::string> StageDependencyAnalyzer::topologicalSort() const {
    return impl_->topologicalSort();
}

std::map<std::string, int> StageDependencyAnalyzer::calculateDependencyDepths() const {
    return impl_->calculateDependencyDepths();
}

std::vector<std::string> StageDependencyAnalyzer::getCriticalPath() const {
    return impl_->getCriticalPath();
}

double StageDependencyAnalyzer::calculateParallelismPotential() const {
    return impl_->calculateParallelismPotential();
}

StageConstraintValidator::StageConstraintValidator()
    : impl_(std::make_unique<ConstraintValidatorImpl>()) {}

StageConstraintValidator::~StageConstraintValidator() = default;

bool StageConstraintValidator::validateConstraint(const PipelineStageConfig& stage, 
                                                 StageExecutionConstraint constraint) const {
    return impl_->validateConstraint(stage, constraint);
}

std::vector<StageExecutionConstraint> StageConstraintValidator::findViolatedConstraints(
    const PipelineStageConfig& stage,
    const std::vector<StageExecutionConstraint>& constraints) const {
    return impl_->findViolatedConstraints(stage, constraints);
}

bool StageConstraintValidator::checkConstraintCompatibility(
    const std::vector<StageExecutionConstraint>& constraints) const {
    return impl_->checkConstraintCompatibility(constraints);
}

void StageConstraintValidator::registerCustomValidator(
    StageExecutionConstraint constraint,
    std::function<bool(const PipelineStageConfig&)> validator) {
    impl_->registerCustomValidator(constraint, validator);
}

std::vector<StageExecutionConstraint> StageConstraintValidator::inferConstraintsFromConfig(
    const PipelineStageConfig& stage) const {
    return impl_->inferConstraintsFromConfig(stage);
}

StageExecutionPlanner::StageExecutionPlanner(const StageSelectorConfig& config)
    : impl_(std::make_unique<ExecutionPlannerImpl>(config)) {}

StageExecutionPlan StageExecutionPlanner::createPlan(const std::vector<PipelineStageConfig>& stages,
                                                    const PipelineExecutionConfig& execution_config) {
    return impl_->createPlan(stages, execution_config);
}

std::vector<std::string> StageExecutionPlanner::optimizeExecutionOrder(const std::vector<PipelineStageConfig>& stages) {
    return impl_->optimizeExecutionOrder(stages);
}

std::vector<std::vector<std::string>> StageExecutionPlanner::identifyParallelGroups(
    const std::vector<PipelineStageConfig>& stages) {
    return impl_->identifyParallelGroups(stages);
}

std::chrono::milliseconds StageExecutionPlanner::estimateTotalExecutionTime(
    const std::vector<PipelineStageConfig>& stages,
    bool consider_parallelism) const {
    return impl_->estimateTotalExecutionTime(stages, consider_parallelism);
}

std::map<std::string, double> StageExecutionPlanner::estimateResourceRequirements(
    const std::vector<PipelineStageConfig>& stages) const {
    return impl_->estimateResourceRequirements(stages);
}

// EN: Utility Functions Implementation
// FR: Implémentation des Fonctions Utilitaires

namespace StageSelectorUtils {

std::string criteriaToString(StageSelectionCriteria criteria) {
    switch (criteria) {
        case StageSelectionCriteria::BY_ID: return "BY_ID";
        case StageSelectionCriteria::BY_NAME: return "BY_NAME";
        case StageSelectionCriteria::BY_PATTERN: return "BY_PATTERN";
        case StageSelectionCriteria::BY_TAG: return "BY_TAG";
        case StageSelectionCriteria::BY_PRIORITY: return "BY_PRIORITY";
        case StageSelectionCriteria::BY_DEPENDENCY: return "BY_DEPENDENCY";
        case StageSelectionCriteria::BY_EXECUTION_TIME: return "BY_EXECUTION_TIME";
        case StageSelectionCriteria::BY_RESOURCE_USAGE: return "BY_RESOURCE_USAGE";
        case StageSelectionCriteria::BY_SUCCESS_RATE: return "BY_SUCCESS_RATE";
        case StageSelectionCriteria::BY_CUSTOM: return "BY_CUSTOM";
        default: return "UNKNOWN";
    }
}

StageSelectionCriteria stringToCriteria(const std::string& str) {
    if (str == "BY_ID") return StageSelectionCriteria::BY_ID;
    if (str == "BY_NAME") return StageSelectionCriteria::BY_NAME;
    if (str == "BY_PATTERN") return StageSelectionCriteria::BY_PATTERN;
    if (str == "BY_TAG") return StageSelectionCriteria::BY_TAG;
    if (str == "BY_PRIORITY") return StageSelectionCriteria::BY_PRIORITY;
    if (str == "BY_DEPENDENCY") return StageSelectionCriteria::BY_DEPENDENCY;
    if (str == "BY_EXECUTION_TIME") return StageSelectionCriteria::BY_EXECUTION_TIME;
    if (str == "BY_RESOURCE_USAGE") return StageSelectionCriteria::BY_RESOURCE_USAGE;
    if (str == "BY_SUCCESS_RATE") return StageSelectionCriteria::BY_SUCCESS_RATE;
    if (str == "BY_CUSTOM") return StageSelectionCriteria::BY_CUSTOM;
    return StageSelectionCriteria::BY_ID; // default
}

std::string filterModeToString(StageFilterMode mode) {
    switch (mode) {
        case StageFilterMode::INCLUDE: return "INCLUDE";
        case StageFilterMode::EXCLUDE: return "EXCLUDE";
        case StageFilterMode::REQUIRE: return "REQUIRE";
        default: return "UNKNOWN";
    }
}

StageFilterMode stringToFilterMode(const std::string& str) {
    if (str == "INCLUDE") return StageFilterMode::INCLUDE;
    if (str == "EXCLUDE") return StageFilterMode::EXCLUDE;
    if (str == "REQUIRE") return StageFilterMode::REQUIRE;
    return StageFilterMode::INCLUDE; // default
}

std::string validationLevelToString(StageValidationLevel level) {
    switch (level) {
        case StageValidationLevel::NONE: return "NONE";
        case StageValidationLevel::BASIC: return "BASIC";
        case StageValidationLevel::DEPENDENCIES: return "DEPENDENCIES";
        case StageValidationLevel::RESOURCES: return "RESOURCES";
        case StageValidationLevel::COMPATIBILITY: return "COMPATIBILITY";
        case StageValidationLevel::COMPREHENSIVE: return "COMPREHENSIVE";
        default: return "UNKNOWN";
    }
}

StageValidationLevel stringToValidationLevel(const std::string& str) {
    if (str == "NONE") return StageValidationLevel::NONE;
    if (str == "BASIC") return StageValidationLevel::BASIC;
    if (str == "DEPENDENCIES") return StageValidationLevel::DEPENDENCIES;
    if (str == "RESOURCES") return StageValidationLevel::RESOURCES;
    if (str == "COMPATIBILITY") return StageValidationLevel::COMPATIBILITY;
    if (str == "COMPREHENSIVE") return StageValidationLevel::COMPREHENSIVE;
    return StageValidationLevel::BASIC; // default
}

std::string constraintToString(StageExecutionConstraint constraint) {
    switch (constraint) {
        case StageExecutionConstraint::NONE: return "NONE";
        case StageExecutionConstraint::SEQUENTIAL_ONLY: return "SEQUENTIAL_ONLY";
        case StageExecutionConstraint::PARALLEL_SAFE: return "PARALLEL_SAFE";
        case StageExecutionConstraint::RESOURCE_INTENSIVE: return "RESOURCE_INTENSIVE";
        case StageExecutionConstraint::NETWORK_DEPENDENT: return "NETWORK_DEPENDENT";
        case StageExecutionConstraint::FILESYSTEM_DEPENDENT: return "FILESYSTEM_DEPENDENT";
        case StageExecutionConstraint::MEMORY_INTENSIVE: return "MEMORY_INTENSIVE";
        case StageExecutionConstraint::CPU_INTENSIVE: return "CPU_INTENSIVE";
        case StageExecutionConstraint::EXCLUSIVE_ACCESS: return "EXCLUSIVE_ACCESS";
        case StageExecutionConstraint::TIME_SENSITIVE: return "TIME_SENSITIVE";
        case StageExecutionConstraint::STATEFUL: return "STATEFUL";
        default: return "UNKNOWN";
    }
}

StageExecutionConstraint stringToConstraint(const std::string& str) {
    if (str == "NONE") return StageExecutionConstraint::NONE;
    if (str == "SEQUENTIAL_ONLY") return StageExecutionConstraint::SEQUENTIAL_ONLY;
    if (str == "PARALLEL_SAFE") return StageExecutionConstraint::PARALLEL_SAFE;
    if (str == "RESOURCE_INTENSIVE") return StageExecutionConstraint::RESOURCE_INTENSIVE;
    if (str == "NETWORK_DEPENDENT") return StageExecutionConstraint::NETWORK_DEPENDENT;
    if (str == "FILESYSTEM_DEPENDENT") return StageExecutionConstraint::FILESYSTEM_DEPENDENT;
    if (str == "MEMORY_INTENSIVE") return StageExecutionConstraint::MEMORY_INTENSIVE;
    if (str == "CPU_INTENSIVE") return StageExecutionConstraint::CPU_INTENSIVE;
    if (str == "EXCLUSIVE_ACCESS") return StageExecutionConstraint::EXCLUSIVE_ACCESS;
    if (str == "TIME_SENSITIVE") return StageExecutionConstraint::TIME_SENSITIVE;
    if (str == "STATEFUL") return StageExecutionConstraint::STATEFUL;
    return StageExecutionConstraint::NONE; // default
}

std::string selectionStatusToString(StageSelectionStatus status) {
    switch (status) {
        case StageSelectionStatus::SUCCESS: return "SUCCESS";
        case StageSelectionStatus::PARTIAL_SUCCESS: return "PARTIAL_SUCCESS";
        case StageSelectionStatus::VALIDATION_FAILED: return "VALIDATION_FAILED";
        case StageSelectionStatus::DEPENDENCY_ERROR: return "DEPENDENCY_ERROR";
        case StageSelectionStatus::CONSTRAINT_VIOLATION: return "CONSTRAINT_VIOLATION";
        case StageSelectionStatus::RESOURCE_UNAVAILABLE: return "RESOURCE_UNAVAILABLE";
        case StageSelectionStatus::CONFIGURATION_ERROR: return "CONFIGURATION_ERROR";
        case StageSelectionStatus::EMPTY_SELECTION: return "EMPTY_SELECTION";
        case StageSelectionStatus::CIRCULAR_DEPENDENCY: return "CIRCULAR_DEPENDENCY";
        case StageSelectionStatus::INCOMPATIBLE_STAGES: return "INCOMPATIBLE_STAGES";
        default: return "UNKNOWN";
    }
}

// EN: Filter creation utilities
// FR: Utilitaires de création de filtres

StageSelectionFilter createIdFilter(const std::string& stage_id, bool exact_match) {
    StageSelectionFilter filter;
    filter.criteria = StageSelectionCriteria::BY_ID;
    filter.mode = StageFilterMode::INCLUDE;
    filter.value = stage_id;
    filter.exact_match = exact_match;
    filter.case_sensitive = true;
    return filter;
}

StageSelectionFilter createNameFilter(const std::string& name, bool case_sensitive) {
    StageSelectionFilter filter;
    filter.criteria = StageSelectionCriteria::BY_NAME;
    filter.mode = StageFilterMode::INCLUDE;
    filter.value = name;
    filter.exact_match = false;
    filter.case_sensitive = case_sensitive;
    return filter;
}

StageSelectionFilter createPatternFilter(const std::string& pattern) {
    StageSelectionFilter filter;
    filter.criteria = StageSelectionCriteria::BY_PATTERN;
    filter.mode = StageFilterMode::INCLUDE;
    filter.value = pattern;
    
    try {
        filter.pattern = std::regex(pattern);
    } catch (const std::regex_error& e) {
        // EN: Invalid pattern, will be caught during selection
        // FR: Motif invalide, sera capturé pendant la sélection
    }
    
    return filter;
}

StageSelectionFilter createTagFilter(const std::set<std::string>& tags) {
    StageSelectionFilter filter;
    filter.criteria = StageSelectionCriteria::BY_TAG;
    filter.mode = StageFilterMode::INCLUDE;
    filter.tags = tags;
    return filter;
}

StageSelectionFilter createPriorityFilter(PipelineStagePriority min_priority, 
                                          PipelineStagePriority max_priority) {
    StageSelectionFilter filter;
    filter.criteria = StageSelectionCriteria::BY_PRIORITY;
    filter.mode = StageFilterMode::INCLUDE;
    filter.min_priority = min_priority;
    filter.max_priority = max_priority;
    return filter;
}

// EN: Validation utilities
// FR: Utilitaires de validation

bool isValidStageId(const std::string& id) {
    if (id.empty() || id.length() > 255) return false;
    
    // EN: Check for valid characters (alphanumeric, underscore, dash)
    // FR: Vérifier les caractères valides (alphanumériques, underscore, tiret)
    return std::all_of(id.begin(), id.end(), [](char c) {
        return std::isalnum(c) || c == '_' || c == '-' || c == '.';
    });
}

bool isValidPattern(const std::string& pattern) {
    try {
        std::regex test_regex(pattern);
        return true;
    } catch (const std::regex_error& e) {
        return false;
    }
}

std::vector<std::string> validateSelectionConfig(const StageSelectionConfig& config) {
    std::vector<std::string> errors;
    
    if (config.max_selected_stages == 0) {
        errors.push_back("max_selected_stages cannot be zero");
    }
    
    if (config.selection_timeout.count() <= 0) {
        errors.push_back("selection_timeout must be positive");
    }
    
    // EN: Validate filters
    // FR: Valider les filtres
    for (size_t i = 0; i < config.filters.size(); ++i) {
        const auto& filter = config.filters[i];
        
        if (filter.criteria == StageSelectionCriteria::BY_PATTERN) {
            if (!isValidPattern(filter.value)) {
                errors.push_back("Filter " + std::to_string(i) + " has invalid regex pattern: " + filter.value);
            }
        }
        
        if (filter.criteria == StageSelectionCriteria::BY_PRIORITY) {
            if (filter.min_priority > filter.max_priority) {
                errors.push_back("Filter " + std::to_string(i) + " has min_priority > max_priority");
            }
        }
        
        if (filter.criteria == StageSelectionCriteria::BY_EXECUTION_TIME) {
            if (filter.min_execution_time > filter.max_execution_time) {
                errors.push_back("Filter " + std::to_string(i) + " has min_execution_time > max_execution_time");
            }
        }
    }
    
    return errors;
}

std::vector<std::string> validateExecutionPlan(const StageExecutionPlan& plan) {
    std::vector<std::string> errors;
    
    if (plan.plan_id.empty()) {
        errors.push_back("plan_id cannot be empty");
    }
    
    if (plan.stages.empty()) {
        errors.push_back("stages cannot be empty");
    }
    
    if (plan.execution_order.size() != plan.stages.size()) {
        errors.push_back("execution_order size doesn't match stages size");
    }
    
    // EN: Validate stage IDs consistency
    // FR: Valider la cohérence des ID d'étapes
    std::set<std::string> stage_ids;
    for (const auto& stage : plan.stages) {
        if (stage_ids.count(stage.id)) {
            errors.push_back("Duplicate stage ID: " + stage.id);
        }
        stage_ids.insert(stage.id);
    }
    
    for (const auto& stage_id : plan.execution_order) {
        if (!stage_ids.count(stage_id)) {
            errors.push_back("execution_order contains unknown stage ID: " + stage_id);
        }
    }
    
    return errors;
}

// EN: Constraint utilities
// FR: Utilitaires de contraintes

bool areConstraintsCompatible(StageExecutionConstraint c1, StageExecutionConstraint c2) {
    // EN: Define incompatible constraint pairs
    // FR: Définir les paires de contraintes incompatibles
    static const std::map<StageExecutionConstraint, std::vector<StageExecutionConstraint>> incompatible = {
        {StageExecutionConstraint::SEQUENTIAL_ONLY, {StageExecutionConstraint::PARALLEL_SAFE}},
        {StageExecutionConstraint::PARALLEL_SAFE, {StageExecutionConstraint::SEQUENTIAL_ONLY, StageExecutionConstraint::EXCLUSIVE_ACCESS}},
        {StageExecutionConstraint::EXCLUSIVE_ACCESS, {StageExecutionConstraint::PARALLEL_SAFE}}
    };
    
    auto it = incompatible.find(c1);
    if (it != incompatible.end()) {
        return std::find(it->second.begin(), it->second.end(), c2) == it->second.end();
    }
    
    return true;
}

std::vector<StageExecutionConstraint> getConflictingConstraints(StageExecutionConstraint constraint) {
    switch (constraint) {
        case StageExecutionConstraint::SEQUENTIAL_ONLY:
            return {StageExecutionConstraint::PARALLEL_SAFE};
        case StageExecutionConstraint::PARALLEL_SAFE:
            return {StageExecutionConstraint::SEQUENTIAL_ONLY, StageExecutionConstraint::EXCLUSIVE_ACCESS};
        case StageExecutionConstraint::EXCLUSIVE_ACCESS:
            return {StageExecutionConstraint::PARALLEL_SAFE};
        default:
            return {};
    }
}

std::vector<StageExecutionConstraint> getDependentConstraints(StageExecutionConstraint constraint) {
    switch (constraint) {
        case StageExecutionConstraint::RESOURCE_INTENSIVE:
            return {StageExecutionConstraint::CPU_INTENSIVE, StageExecutionConstraint::MEMORY_INTENSIVE};
        case StageExecutionConstraint::NETWORK_DEPENDENT:
            return {StageExecutionConstraint::TIME_SENSITIVE};
        default:
            return {};
    }
}

// EN: JSON serialization utilities (basic implementation)
// FR: Utilitaires de sérialisation JSON (implémentation de base)

nlohmann::json selectionResultToJson(const StageSelectionResult& result) {
    nlohmann::json j;
    j["status"] = selectionStatusToString(result.status);
    j["selected_stage_ids"] = result.selected_stage_ids;
    j["execution_order"] = result.execution_order;
    j["errors"] = result.errors;
    j["warnings"] = result.warnings;
    j["information"] = result.information;
    j["selection_time_ms"] = result.selection_time.count();
    j["estimated_execution_time_ms"] = result.estimated_execution_time.count();
    j["total_available_stages"] = result.total_available_stages;
    j["filtered_stages"] = result.filtered_stages;
    j["selection_ratio"] = result.selection_ratio;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        result.selection_timestamp.time_since_epoch()).count();
    return j;
}

StageSelectionResult selectionResultFromJson(const nlohmann::json& json) {
    StageSelectionResult result;
    
    if (json.contains("status")) {
        std::string status_str = json["status"];
        // EN: Parse status from string (simplified)
        // FR: Parser le statut depuis la chaîne (simplifié)
        if (status_str == "SUCCESS") result.status = StageSelectionStatus::SUCCESS;
        else if (status_str == "VALIDATION_FAILED") result.status = StageSelectionStatus::VALIDATION_FAILED;
        // Add other status mappings as needed
    }
    
    if (json.contains("selected_stage_ids")) {
        result.selected_stage_ids = json["selected_stage_ids"];
    }
    
    if (json.contains("execution_order")) {
        result.execution_order = json["execution_order"];
    }
    
    if (json.contains("errors")) {
        result.errors = json["errors"];
    }
    
    if (json.contains("warnings")) {
        result.warnings = json["warnings"];
    }
    
    if (json.contains("information")) {
        result.information = json["information"];
    }
    
    if (json.contains("selection_time_ms")) {
        result.selection_time = std::chrono::milliseconds(json["selection_time_ms"]);
    }
    
    if (json.contains("estimated_execution_time_ms")) {
        result.estimated_execution_time = std::chrono::milliseconds(json["estimated_execution_time_ms"]);
    }
    
    if (json.contains("total_available_stages")) {
        result.total_available_stages = json["total_available_stages"];
    }
    
    if (json.contains("filtered_stages")) {
        result.filtered_stages = json["filtered_stages"];
    }
    
    if (json.contains("selection_ratio")) {
        result.selection_ratio = json["selection_ratio"];
    }
    
    if (json.contains("timestamp")) {
        auto timestamp_ms = std::chrono::milliseconds(json["timestamp"]);
        result.selection_timestamp = std::chrono::system_clock::time_point(timestamp_ms);
    }
    
    return result;
}

nlohmann::json executionPlanToJson(const StageExecutionPlan& plan) {
    nlohmann::json j;
    j["plan_id"] = plan.plan_id;
    j["execution_order"] = plan.execution_order;
    j["estimated_total_time_ms"] = plan.estimated_total_time.count();
    j["estimated_parallel_time_ms"] = plan.estimated_parallel_time.count();
    j["peak_resource_usage"] = plan.peak_resource_usage;
    j["critical_path"] = plan.critical_path;
    j["optimization_suggestions"] = plan.optimization_suggestions;
    j["is_valid"] = plan.is_valid;
    j["created_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        plan.created_at.time_since_epoch()).count();
    return j;
}

StageExecutionPlan executionPlanFromJson(const nlohmann::json& json) {
    StageExecutionPlan plan;
    
    if (json.contains("plan_id")) {
        plan.plan_id = json["plan_id"];
    }
    
    if (json.contains("execution_order")) {
        plan.execution_order = json["execution_order"];
    }
    
    if (json.contains("estimated_total_time_ms")) {
        plan.estimated_total_time = std::chrono::milliseconds(json["estimated_total_time_ms"]);
    }
    
    if (json.contains("estimated_parallel_time_ms")) {
        plan.estimated_parallel_time = std::chrono::milliseconds(json["estimated_parallel_time_ms"]);
    }
    
    if (json.contains("peak_resource_usage")) {
        plan.peak_resource_usage = json["peak_resource_usage"];
    }
    
    if (json.contains("critical_path")) {
        plan.critical_path = json["critical_path"];
    }
    
    if (json.contains("optimization_suggestions")) {
        plan.optimization_suggestions = json["optimization_suggestions"];
    }
    
    if (json.contains("is_valid")) {
        plan.is_valid = json["is_valid"];
    }
    
    if (json.contains("created_at")) {
        auto timestamp_ms = std::chrono::milliseconds(json["created_at"]);
        plan.created_at = std::chrono::system_clock::time_point(timestamp_ms);
    }
    
    return plan;
}

nlohmann::json configToJson(const StageSelectorConfig& config) {
    nlohmann::json j;
    j["max_concurrent_selections"] = config.max_concurrent_selections;
    j["enable_caching"] = config.enable_caching;
    j["cache_ttl_seconds"] = config.cache_ttl.count();
    j["max_cache_entries"] = config.max_cache_entries;
    j["enable_statistics"] = config.enable_statistics;
    j["enable_detailed_logging"] = config.enable_detailed_logging;
    j["default_selection_timeout_seconds"] = config.default_selection_timeout.count();
    j["max_dependency_depth"] = config.max_dependency_depth;
    j["auto_include_dependencies"] = config.auto_include_dependencies;
    j["auto_resolve_conflicts"] = config.auto_resolve_conflicts;
    j["compatibility_threshold"] = config.compatibility_threshold;
    j["default_log_level"] = config.default_log_level;
    j["custom_settings"] = config.custom_settings;
    return j;
}

StageSelectorConfig configFromJson(const nlohmann::json& json) {
    StageSelectorConfig config;
    
    if (json.contains("max_concurrent_selections")) {
        config.max_concurrent_selections = json["max_concurrent_selections"];
    }
    
    if (json.contains("enable_caching")) {
        config.enable_caching = json["enable_caching"];
    }
    
    if (json.contains("cache_ttl_seconds")) {
        config.cache_ttl = std::chrono::seconds(json["cache_ttl_seconds"]);
    }
    
    if (json.contains("max_cache_entries")) {
        config.max_cache_entries = json["max_cache_entries"];
    }
    
    if (json.contains("enable_statistics")) {
        config.enable_statistics = json["enable_statistics"];
    }
    
    if (json.contains("enable_detailed_logging")) {
        config.enable_detailed_logging = json["enable_detailed_logging"];
    }
    
    if (json.contains("default_selection_timeout_seconds")) {
        config.default_selection_timeout = std::chrono::seconds(json["default_selection_timeout_seconds"]);
    }
    
    if (json.contains("max_dependency_depth")) {
        config.max_dependency_depth = json["max_dependency_depth"];
    }
    
    if (json.contains("auto_include_dependencies")) {
        config.auto_include_dependencies = json["auto_include_dependencies"];
    }
    
    if (json.contains("auto_resolve_conflicts")) {
        config.auto_resolve_conflicts = json["auto_resolve_conflicts"];
    }
    
    if (json.contains("compatibility_threshold")) {
        config.compatibility_threshold = json["compatibility_threshold"];
    }
    
    if (json.contains("default_log_level")) {
        config.default_log_level = json["default_log_level"];
    }
    
    if (json.contains("custom_settings")) {
        config.custom_settings = json["custom_settings"];
    }
    
    return config;
}

// EN: Performance utilities
// FR: Utilitaires de performance

std::chrono::milliseconds measureSelectionTime(std::function<StageSelectionResult()> selection_func) {
    auto start = std::chrono::high_resolution_clock::now();
    selection_func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
}

double calculateSelectionEfficiency(const StageSelectionResult& result) {
    if (result.total_available_stages == 0) return 0.0;
    
    // EN: Calculate efficiency based on selection ratio and time
    // FR: Calculer l'efficacité basée sur le ratio de sélection et le temps
    double ratio_score = result.selection_ratio;
    double time_score = 1.0 / (1.0 + result.selection_time.count() / 1000.0); // Normalize by seconds
    
    return (ratio_score + time_score) / 2.0;
}

std::vector<std::string> identifyBottleneckStages(const std::vector<PipelineStageConfig>& stages) {
    std::vector<std::string> bottlenecks;
    
    // EN: Identify stages with long execution times or complex dependencies
    // FR: Identifier les étapes avec des temps d'exécution longs ou des dépendances complexes
    for (const auto& stage : stages) {
        if (stage.timeout.count() > 300) { // More than 5 minutes
            bottlenecks.push_back(stage.id + " (long execution time: " + std::to_string(stage.timeout.count()) + "s)");
        }
        
        if (stage.dependencies.size() > 3) {
            bottlenecks.push_back(stage.id + " (many dependencies: " + std::to_string(stage.dependencies.size()) + ")");
        }
    }
    
    return bottlenecks;
}

// EN: Debugging and diagnostics utilities
// FR: Utilitaires de débogage et diagnostics

std::string generateSelectionReport(const StageSelectionResult& result) {
    std::ostringstream report;
    
    report << "=== Stage Selection Report ===\n";
    report << "Status: " << selectionStatusToString(result.status) << "\n";
    report << "Selected Stages: " << result.selected_stage_ids.size() << " / " << result.total_available_stages << "\n";
    report << "Selection Ratio: " << std::fixed << std::setprecision(2) << (result.selection_ratio * 100) << "%\n";
    report << "Selection Time: " << result.selection_time.count() << " ms\n";
    report << "Estimated Execution Time: " << result.estimated_execution_time.count() << " ms\n";
    
    if (!result.selected_stage_ids.empty()) {
        report << "\nSelected Stages:\n";
        for (const auto& stage_id : result.selected_stage_ids) {
            report << "  - " << stage_id << "\n";
        }
    }
    
    if (!result.execution_order.empty()) {
        report << "\nExecution Order:\n";
        for (size_t i = 0; i < result.execution_order.size(); ++i) {
            report << "  " << (i + 1) << ". " << result.execution_order[i] << "\n";
        }
    }
    
    if (!result.errors.empty()) {
        report << "\nErrors:\n";
        for (const auto& error : result.errors) {
            report << "  - " << error << "\n";
        }
    }
    
    if (!result.warnings.empty()) {
        report << "\nWarnings:\n";
        for (const auto& warning : result.warnings) {
            report << "  - " << warning << "\n";
        }
    }
    
    if (!result.information.empty()) {
        report << "\nInformation:\n";
        for (const auto& info : result.information) {
            report << "  - " << info << "\n";
        }
    }
    
    report << "\nCompatibility Score: " << std::fixed << std::setprecision(2) 
           << (result.compatibility.compatibility_score * 100) << "%\n";
    
    return report.str();
}

std::string generateExecutionPlanReport(const StageExecutionPlan& plan) {
    std::ostringstream report;
    
    report << "=== Execution Plan Report ===\n";
    report << "Plan ID: " << plan.plan_id << "\n";
    report << "Number of Stages: " << plan.stages.size() << "\n";
    report << "Is Valid: " << (plan.is_valid ? "Yes" : "No") << "\n";
    report << "Estimated Total Time: " << plan.estimated_total_time.count() << " ms\n";
    report << "Estimated Parallel Time: " << plan.estimated_parallel_time.count() << " ms\n";
    report << "Peak Resource Usage: " << std::fixed << std::setprecision(2) << plan.peak_resource_usage << "\n";
    
    if (!plan.execution_order.empty()) {
        report << "\nExecution Order:\n";
        for (size_t i = 0; i < plan.execution_order.size(); ++i) {
            report << "  " << (i + 1) << ". " << plan.execution_order[i] << "\n";
        }
    }
    
    if (!plan.critical_path.empty()) {
        report << "\nCritical Path:\n";
        for (size_t i = 0; i < plan.critical_path.size(); ++i) {
            report << "  " << (i + 1) << ". " << plan.critical_path[i];
            if (i < plan.critical_path.size() - 1) {
                report << " -> ";
            }
            report << "\n";
        }
    }
    
    if (!plan.parallel_groups.empty()) {
        report << "\nParallel Execution Groups:\n";
        for (size_t i = 0; i < plan.parallel_groups.size(); ++i) {
            report << "  Group " << (i + 1) << ": ";
            for (size_t j = 0; j < plan.parallel_groups[i].size(); ++j) {
                report << plan.parallel_groups[i][j];
                if (j < plan.parallel_groups[i].size() - 1) {
                    report << ", ";
                }
            }
            report << "\n";
        }
    }
    
    if (!plan.optimization_suggestions.empty()) {
        report << "\nOptimization Suggestions:\n";
        for (const auto& suggestion : plan.optimization_suggestions) {
            report << "  - " << suggestion << "\n";
        }
    }
    
    return report.str();
}

void dumpSelectionDebugInfo(const StageSelectionResult& result, const std::string& filepath) {
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << generateSelectionReport(result);
        
        // EN: Add detailed debug information
        // FR: Ajouter des informations de débogage détaillées
        file << "\n=== Debug Information ===\n";
        file << "Timestamp: " << std::chrono::duration_cast<std::chrono::milliseconds>(
            result.selection_timestamp.time_since_epoch()).count() << " ms since epoch\n";
        file << "Cache Key: " << result.cache_key << "\n";
        
        if (!result.resource_estimates.empty()) {
            file << "\nResource Estimates:\n";
            for (const auto& resource : result.resource_estimates) {
                file << "  " << resource.first << ": " << resource.second << "\n";
            }
        }
        
        if (!result.stage_execution_estimates.empty()) {
            file << "\nStage Execution Estimates:\n";
            for (const auto& estimate : result.stage_execution_estimates) {
                file << "  " << estimate.first << ": " << estimate.second.count() << " ms\n";
            }
        }
        
        file.close();
    }
}

} // namespace StageSelectorUtils

} // namespace Orchestrator
} // namespace BBP