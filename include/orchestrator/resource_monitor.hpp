// EN: Resource Monitor for BB-Pipeline - CPU/RAM/Network monitoring with adaptive throttling
// FR: Moniteur de Ressources pour BB-Pipeline - Surveillance CPU/RAM/Réseau avec throttling adaptatif

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <sstream>
#include <fstream>
#include <queue>
#include <deque>
#include <algorithm>
#include <cmath>
#include <future>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")
#else
#include <sys/resource.h>
#include <unistd.h>
#include <fstream>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif
#endif

namespace BBP {
namespace Orchestrator {

// EN: Forward declarations for integration with other systems
// FR: Déclarations avancées pour l'intégration avec d'autres systèmes
class PipelineEngine;
class Logger;
class ThreadPool;

// EN: Resource types monitored by the system
// FR: Types de ressources surveillées par le système
enum class ResourceType {
    CPU = 0,           // EN: CPU usage monitoring / FR: Surveillance de l'usage CPU
    MEMORY = 1,        // EN: RAM usage monitoring / FR: Surveillance de l'usage RAM
    NETWORK = 2,       // EN: Network I/O monitoring / FR: Surveillance des E/S réseau
    DISK = 3,          // EN: Disk I/O monitoring / FR: Surveillance des E/S disque
    PROCESS = 4,       // EN: Process-specific monitoring / FR: Surveillance spécifique au processus
    SYSTEM = 5,        // EN: System-wide monitoring / FR: Surveillance à l'échelle du système
    CUSTOM = 6         // EN: Custom user-defined metrics / FR: Métriques personnalisées définies par l'utilisateur
};

// EN: Severity levels for resource alerts
// FR: Niveaux de sévérité pour les alertes de ressources
enum class ResourceAlertSeverity {
    DEBUG = 0,         // EN: Debug information / FR: Informations de débogage
    INFO = 1,          // EN: Informational alerts / FR: Alertes informationnelles
    WARNING = 2,       // EN: Warning level alerts / FR: Alertes de niveau avertissement
    CRITICAL = 3,      // EN: Critical alerts requiring action / FR: Alertes critiques nécessitant une action
    EMERGENCY = 4      // EN: Emergency alerts requiring immediate action / FR: Alertes d'urgence nécessitant une action immédiate
};

// EN: Monitoring frequency modes
// FR: Modes de fréquence de surveillance
enum class MonitoringFrequency {
    LOW = 0,           // EN: Low frequency monitoring (every 5-10 seconds) / FR: Surveillance basse fréquence (toutes les 5-10 secondes)
    NORMAL = 1,        // EN: Normal frequency (every 1-2 seconds) / FR: Fréquence normale (toutes les 1-2 secondes)
    HIGH = 2,          // EN: High frequency (every 100-500ms) / FR: Haute fréquence (toutes les 100-500ms)
    REALTIME = 3,      // EN: Real-time monitoring (every 10-50ms) / FR: Surveillance temps réel (toutes les 10-50ms)
    ADAPTIVE = 4       // EN: Adaptive frequency based on load / FR: Fréquence adaptative basée sur la charge
};

// EN: Throttling strategies for resource management
// FR: Stratégies de throttling pour la gestion des ressources
enum class ThrottlingStrategy {
    NONE = 0,          // EN: No throttling applied / FR: Aucun throttling appliqué
    LINEAR = 1,        // EN: Linear reduction of operations / FR: Réduction linéaire des opérations
    EXPONENTIAL = 2,   // EN: Exponential backoff / FR: Backoff exponentiel
    ADAPTIVE = 3,      // EN: Adaptive throttling based on resource state / FR: Throttling adaptatif basé sur l'état des ressources
    PREDICTIVE = 4,    // EN: Predictive throttling using ML algorithms / FR: Throttling prédictif utilisant des algorithmes ML
    AGGRESSIVE = 5     // EN: Aggressive throttling for critical situations / FR: Throttling agressif pour situations critiques
};

// EN: Resource measurement units
// FR: Unités de mesure des ressources
enum class ResourceUnit {
    PERCENTAGE,        // EN: Percentage (0-100%) / FR: Pourcentage (0-100%)
    BYTES,            // EN: Bytes / FR: Octets
    BYTES_PER_SECOND, // EN: Bytes per second / FR: Octets par seconde
    COUNT,            // EN: Simple count / FR: Comptage simple
    MILLISECONDS,     // EN: Time in milliseconds / FR: Temps en millisecondes
    HERTZ,            // EN: Frequency in Hz / FR: Fréquence en Hz
    CUSTOM            // EN: Custom unit / FR: Unité personnalisée
};

// EN: Current resource usage snapshot
// FR: Instantané de l'usage actuel des ressources
struct ResourceUsage {
    ResourceType type;
    double current_value = 0.0;       // EN: Current resource value / FR: Valeur actuelle de la ressource
    double average_value = 0.0;       // EN: Average over monitoring window / FR: Moyenne sur la fenêtre de surveillance
    double peak_value = 0.0;          // EN: Peak value observed / FR: Valeur de pic observée
    double minimum_value = 0.0;       // EN: Minimum value observed / FR: Valeur minimale observée
    ResourceUnit unit = ResourceUnit::PERCENTAGE;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds collection_duration{0}; // EN: Time to collect this measurement / FR: Temps pour collecter cette mesure
    bool is_valid = true;             // EN: Whether the measurement is valid / FR: Si la mesure est valide
    std::string error_message;        // EN: Error message if invalid / FR: Message d'erreur si invalide
    std::map<std::string, double> metadata; // EN: Additional resource-specific data / FR: Données supplémentaires spécifiques à la ressource
};

// EN: Resource threshold configuration for alerts and throttling
// FR: Configuration des seuils de ressources pour les alertes et le throttling
struct ResourceThreshold {
    ResourceType resource_type;
    double warning_threshold = 75.0;  // EN: Warning threshold (e.g., 75% CPU) / FR: Seuil d'avertissement (ex: 75% CPU)
    double critical_threshold = 90.0; // EN: Critical threshold (e.g., 90% CPU) / FR: Seuil critique (ex: 90% CPU)
    double emergency_threshold = 98.0; // EN: Emergency threshold (e.g., 98% CPU) / FR: Seuil d'urgence (ex: 98% CPU)
    std::chrono::seconds duration_before_alert{30}; // EN: Duration threshold must be exceeded / FR: Durée que le seuil doit être dépassé
    bool enable_throttling = true;    // EN: Enable throttling when threshold exceeded / FR: Activer le throttling quand le seuil est dépassé
    ThrottlingStrategy throttling_strategy = ThrottlingStrategy::ADAPTIVE;
    double throttling_factor = 0.5;   // EN: Factor to reduce operations (0.5 = 50% reduction) / FR: Facteur de réduction des opérations (0.5 = 50% de réduction)
    std::string custom_action;        // EN: Custom action to execute / FR: Action personnalisée à exécuter
};

// EN: Resource monitoring configuration
// FR: Configuration de surveillance des ressources
struct ResourceMonitorConfig {
    MonitoringFrequency frequency = MonitoringFrequency::NORMAL;
    std::chrono::milliseconds collection_interval{1000}; // EN: Base collection interval / FR: Intervalle de collecte de base
    std::chrono::milliseconds adaptive_min_interval{100}; // EN: Minimum interval for adaptive mode / FR: Intervalle minimum pour mode adaptatif
    std::chrono::milliseconds adaptive_max_interval{5000}; // EN: Maximum interval for adaptive mode / FR: Intervalle maximum pour mode adaptatif
    size_t history_size = 300;        // EN: Number of historical samples to keep / FR: Nombre d'échantillons historiques à conserver
    bool enable_predictive_analysis = true; // EN: Enable ML-based predictions / FR: Activer les prédictions basées ML
    bool enable_system_monitoring = true;   // EN: Monitor system-wide resources / FR: Surveiller les ressources système
    bool enable_process_monitoring = true;  // EN: Monitor current process / FR: Surveiller le processus actuel
    bool enable_network_monitoring = true;  // EN: Monitor network interfaces / FR: Surveiller les interfaces réseau
    bool enable_disk_monitoring = true;     // EN: Monitor disk I/O / FR: Surveiller les E/S disque
    bool enable_alerts = true;        // EN: Enable alert system / FR: Activer le système d'alertes
    bool enable_throttling = true;    // EN: Enable automatic throttling / FR: Activer le throttling automatique
    bool enable_logging = true;       // EN: Enable detailed logging / FR: Activer la journalisation détaillée
    std::string log_file_path;        // EN: Path for resource monitoring logs / FR: Chemin pour les logs de surveillance de ressources
    std::vector<ResourceThreshold> thresholds; // EN: Resource thresholds configuration / FR: Configuration des seuils de ressources
    std::map<std::string, std::string> custom_metrics; // EN: Custom metric definitions / FR: Définitions de métriques personnalisées
};

// EN: Resource alert event data
// FR: Données d'événement d'alerte de ressource
struct ResourceAlert {
    ResourceType resource_type;
    ResourceAlertSeverity severity;
    std::chrono::system_clock::time_point timestamp;
    double current_value;
    double threshold_value;
    ResourceUnit unit;
    std::string message;
    std::string recommended_action;
    std::chrono::milliseconds duration_exceeded{0}; // EN: How long threshold has been exceeded / FR: Durée de dépassement du seuil
    bool throttling_applied = false;  // EN: Whether throttling was applied / FR: Si le throttling a été appliqué
    double throttling_factor = 0.0;   // EN: Applied throttling factor / FR: Facteur de throttling appliqué
    std::map<std::string, std::string> context; // EN: Additional context information / FR: Informations de contexte supplémentaires
};

// EN: Historical resource statistics for analysis
// FR: Statistiques historiques des ressources pour analyse
struct ResourceStatistics {
    ResourceType resource_type;
    std::chrono::system_clock::time_point period_start;
    std::chrono::system_clock::time_point period_end;
    std::chrono::milliseconds total_duration{0};
    
    // EN: Statistical measures / FR: Mesures statistiques
    double mean_value = 0.0;
    double median_value = 0.0;
    double standard_deviation = 0.0;
    double variance = 0.0;
    double minimum_value = 0.0;
    double maximum_value = 0.0;
    double percentile_95 = 0.0;
    double percentile_99 = 0.0;
    
    // EN: Time-based analysis / FR: Analyse basée sur le temps
    size_t sample_count = 0;
    std::chrono::milliseconds time_above_warning{0};
    std::chrono::milliseconds time_above_critical{0};
    std::chrono::milliseconds time_above_emergency{0};
    size_t alert_count = 0;
    size_t throttling_events = 0;
    
    // EN: Trend analysis / FR: Analyse de tendance
    double trend_slope = 0.0;          // EN: Linear trend slope / FR: Pente de tendance linéaire
    double trend_correlation = 0.0;    // EN: Trend correlation coefficient / FR: Coefficient de corrélation de tendance
    bool is_increasing_trend = false;  // EN: Whether trend is increasing / FR: Si la tendance est croissante
    bool is_stable = true;             // EN: Whether resource usage is stable / FR: Si l'usage des ressources est stable
    
    std::vector<double> raw_samples;   // EN: Raw sample data for detailed analysis / FR: Données d'échantillon brutes pour analyse détaillée
};

// EN: System resource information
// FR: Informations des ressources système
struct SystemResourceInfo {
    // EN: CPU Information / FR: Informations CPU
    size_t cpu_core_count = 0;
    size_t cpu_logical_count = 0;
    double cpu_frequency_mhz = 0.0;
    std::string cpu_model;
    std::string cpu_architecture;
    
    // EN: Memory Information / FR: Informations mémoire
    size_t total_physical_memory = 0;
    size_t available_physical_memory = 0;
    size_t total_virtual_memory = 0;
    size_t available_virtual_memory = 0;
    size_t page_size = 0;
    
    // EN: Network Information / FR: Informations réseau
    std::vector<std::string> network_interfaces;
    std::map<std::string, uint64_t> interface_speeds; // EN: Speed in bps / FR: Vitesse en bps
    std::map<std::string, bool> interface_status;
    
    // EN: System Information / FR: Informations système
    std::string operating_system;
    std::string kernel_version;
    std::chrono::system_clock::time_point boot_time;
    std::chrono::milliseconds uptime{0};
    size_t process_count = 0;
    size_t thread_count = 0;
    double system_load_1min = 0.0;
    double system_load_5min = 0.0;
    double system_load_15min = 0.0;
};

// EN: Callback function types for resource monitoring events
// FR: Types de fonctions de callback pour les événements de surveillance des ressources
using ResourceAlertCallback = std::function<void(const ResourceAlert&)>;
using ResourceUpdateCallback = std::function<void(const ResourceUsage&)>;
using ThrottlingCallback = std::function<void(ResourceType, double, bool)>; // resource, factor, enabled
using ResourceStatisticsCallback = std::function<void(const ResourceStatistics&)>;

// EN: Main resource monitor class for system resource surveillance
// FR: Classe principale du moniteur de ressources pour la surveillance des ressources système
class ResourceMonitor {
public:
    // EN: Constructor and destructor / FR: Constructeur et destructeur
    explicit ResourceMonitor(const ResourceMonitorConfig& config = {});
    ~ResourceMonitor();
    
    // EN: Lifecycle management / FR: Gestion du cycle de vie
    bool start();
    void stop();
    void pause();
    void resume();
    bool isRunning() const;
    bool isPaused() const;
    
    // EN: Configuration management / FR: Gestion de la configuration
    void updateConfig(const ResourceMonitorConfig& config);
    ResourceMonitorConfig getConfig() const;
    void addThreshold(const ResourceThreshold& threshold);
    bool removeThreshold(ResourceType resource_type);
    std::vector<ResourceThreshold> getThresholds() const;
    
    // EN: Current resource monitoring / FR: Surveillance des ressources actuelles
    std::optional<ResourceUsage> getCurrentUsage(ResourceType resource_type) const;
    std::map<ResourceType, ResourceUsage> getAllCurrentUsage() const;
    SystemResourceInfo getSystemInfo() const;
    bool isResourceAvailable(ResourceType resource_type) const;
    
    // EN: Historical data and statistics / FR: Données historiques et statistiques
    std::vector<ResourceUsage> getResourceHistory(ResourceType resource_type, 
                                                  std::chrono::minutes lookback = std::chrono::minutes(10)) const;
    ResourceStatistics getResourceStatistics(ResourceType resource_type, 
                                            std::chrono::minutes period = std::chrono::minutes(60)) const;
    std::map<ResourceType, ResourceStatistics> getAllResourceStatistics(std::chrono::minutes period = std::chrono::minutes(60)) const;
    void clearHistory(ResourceType resource_type = ResourceType::SYSTEM);
    
    // EN: Alert management / FR: Gestion des alertes
    void setAlertCallback(ResourceAlertCallback callback);
    void setResourceUpdateCallback(ResourceUpdateCallback callback);
    void setThrottlingCallback(ThrottlingCallback callback);
    void setStatisticsCallback(ResourceStatisticsCallback callback);
    void removeAllCallbacks();
    
    std::vector<ResourceAlert> getActiveAlerts() const;
    std::vector<ResourceAlert> getAlertHistory(std::chrono::hours lookback = std::chrono::hours(24)) const;
    void acknowledgeAlert(ResourceType resource_type);
    void muteAlerts(ResourceType resource_type, std::chrono::minutes duration);
    
    // EN: Throttling management / FR: Gestion du throttling
    bool isThrottlingActive(ResourceType resource_type) const;
    double getCurrentThrottlingFactor(ResourceType resource_type) const;
    void manualThrottle(ResourceType resource_type, double factor, std::chrono::minutes duration);
    void disableThrottling(ResourceType resource_type);
    void enableThrottling(ResourceType resource_type);
    std::map<ResourceType, double> getAllThrottlingFactors() const;
    
    // EN: Predictive analysis / FR: Analyse prédictive
    std::optional<double> predictResourceUsage(ResourceType resource_type, 
                                             std::chrono::minutes forecast_period = std::chrono::minutes(5)) const;
    std::chrono::system_clock::time_point predictNextAlert(ResourceType resource_type) const;
    std::vector<ResourceAlert> getPredictedAlerts(std::chrono::minutes forecast_period = std::chrono::minutes(30)) const;
    double getResourceTrendSlope(ResourceType resource_type) const;
    
    // EN: Resource optimization recommendations / FR: Recommandations d'optimisation des ressources
    struct OptimizationRecommendation {
        ResourceType resource_type;
        std::string recommendation;
        std::string rationale;
        double potential_improvement; // EN: Estimated improvement percentage / FR: Pourcentage d'amélioration estimé
        std::string implementation_difficulty; // EN: "easy", "medium", "hard" / FR: "facile", "moyen", "difficile"
        std::vector<std::string> action_steps;
    };
    
    std::vector<OptimizationRecommendation> getOptimizationRecommendations() const;
    
    // EN: Integration with other systems / FR: Intégration avec d'autres systèmes
    void attachToPipeline(std::shared_ptr<PipelineEngine> pipeline);
    void detachFromPipeline();
    void attachToLogger(std::shared_ptr<Logger> logger);
    void attachToThreadPool(std::shared_ptr<ThreadPool> thread_pool);
    
    // EN: Custom metric management / FR: Gestion des métriques personnalisées
    bool addCustomMetric(const std::string& name, std::function<double()> collector, ResourceUnit unit = ResourceUnit::CUSTOM);
    bool removeCustomMetric(const std::string& name);
    std::vector<std::string> getCustomMetricNames() const;
    std::optional<double> getCustomMetricValue(const std::string& name) const;
    
    // EN: Export and import / FR: Export et import
    bool exportData(const std::string& filepath, std::chrono::hours period = std::chrono::hours(24)) const;
    bool importData(const std::string& filepath);
    std::string exportToJSON(std::chrono::hours period = std::chrono::hours(24)) const;
    bool importFromJSON(const std::string& json_data);
    
    // EN: Performance and diagnostics / FR: Performance et diagnostics
    struct MonitorPerformance {
        std::chrono::milliseconds avg_collection_time{0};
        std::chrono::milliseconds max_collection_time{0};
        size_t collections_per_second = 0;
        size_t failed_collections = 0;
        double cpu_overhead_percentage = 0.0;
        size_t memory_usage_bytes = 0;
    };
    
    MonitorPerformance getMonitorPerformance() const;
    void resetPerformanceCounters();
    bool runSelfDiagnostics() const;
    
    // EN: Static utility methods / FR: Méthodes utilitaires statiques
    static std::string resourceTypeToString(ResourceType type);
    static std::string alertSeverityToString(ResourceAlertSeverity severity);
    static std::string formatResourceValue(double value, ResourceUnit unit);
    static bool isResourceCritical(double value, const ResourceThreshold& threshold);

private:
    // EN: Internal implementation using PIMPL pattern / FR: Implémentation interne utilisant le motif PIMPL
    class ResourceMonitorImpl;
    std::unique_ptr<ResourceMonitorImpl> impl_;
};

// EN: Specialized resource monitors for specific use cases
// FR: Moniteurs de ressources spécialisés pour des cas d'usage spécifiques

// EN: Pipeline-aware resource monitor with automatic throttling
// FR: Moniteur de ressources conscient du pipeline avec throttling automatique
class PipelineResourceMonitor : public ResourceMonitor {
public:
    explicit PipelineResourceMonitor(const ResourceMonitorConfig& config = {});
    
    // EN: Pipeline-specific operations / FR: Opérations spécifiques au pipeline
    void setPipelineStageThresholds(const std::string& stage_name, const std::vector<ResourceThreshold>& thresholds);
    void notifyStageStart(const std::string& stage_name);
    void notifyStageEnd(const std::string& stage_name);
    std::map<std::string, ResourceStatistics> getStageResourceUsage() const;
    std::string getMostResourceIntensiveStage() const;
    
    // EN: Automatic optimization for pipeline execution / FR: Optimisation automatique pour l'exécution du pipeline
    bool shouldThrottlePipeline() const;
    double getRecommendedPipelineThrottleFactor() const;
    std::chrono::milliseconds getRecommendedStageDelay() const;
};

// EN: Network-focused resource monitor / FR: Moniteur de ressources axé sur le réseau
class NetworkResourceMonitor : public ResourceMonitor {
public:
    explicit NetworkResourceMonitor(const ResourceMonitorConfig& config = {});
    
    // EN: Network-specific monitoring / FR: Surveillance spécifique au réseau
    struct NetworkInterfaceStats {
        std::string interface_name;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t packets_sent = 0;
        uint64_t packets_received = 0;
        uint64_t errors = 0;
        uint64_t dropped = 0;
        double utilization_percentage = 0.0;
        std::chrono::system_clock::time_point last_updated;
    };
    
    std::vector<NetworkInterfaceStats> getNetworkInterfaceStats() const;
    double getTotalNetworkUtilization() const;
    std::string getBusietstNetworkInterface() const;
    bool isNetworkSaturated() const;
    
    // EN: Network throttling / FR: Throttling réseau
    void setNetworkThrottlingEnabled(bool enabled);
    void setMaxNetworkUtilization(double max_percentage);
    double getCurrentNetworkThrottleFactor() const;
};

// EN: Memory-focused resource monitor with advanced analysis
// FR: Moniteur de ressources axé sur la mémoire avec analyse avancée
class MemoryResourceMonitor : public ResourceMonitor {
public:
    explicit MemoryResourceMonitor(const ResourceMonitorConfig& config = {});
    
    // EN: Memory-specific analysis / FR: Analyse spécifique à la mémoire
    struct MemoryBreakdown {
        size_t heap_usage = 0;
        size_t stack_usage = 0;
        size_t shared_memory = 0;
        size_t mapped_files = 0;
        size_t cached_memory = 0;
        size_t buffer_memory = 0;
        size_t free_memory = 0;
        double fragmentation_percentage = 0.0;
    };
    
    MemoryBreakdown getDetailedMemoryBreakdown() const;
    bool isMemoryFragmented() const;
    size_t predictMemoryPressure(std::chrono::minutes forecast_period = std::chrono::minutes(5)) const;
    bool recommendGarbageCollection() const;
    
    // EN: Memory optimization / FR: Optimisation mémoire
    void triggerMemoryCleanup();
    void setMemoryGrowthThreshold(double percentage);
    bool isMemoryLeakDetected() const;
    std::vector<std::string> getMemoryOptimizationTips() const;
};

// EN: Resource monitor manager for coordinating multiple monitors
// FR: Gestionnaire de moniteur de ressources pour coordonner plusieurs moniteurs
class ResourceMonitorManager {
public:
    // EN: Singleton access / FR: Accès singleton
    static ResourceMonitorManager& getInstance();
    
    // EN: Monitor management / FR: Gestion des moniteurs
    std::string createMonitor(const std::string& name, const ResourceMonitorConfig& config = {});
    std::string createPipelineMonitor(const std::string& name, const ResourceMonitorConfig& config = {});
    std::string createNetworkMonitor(const std::string& name, const ResourceMonitorConfig& config = {});
    std::string createMemoryMonitor(const std::string& name, const ResourceMonitorConfig& config = {});
    
    bool removeMonitor(const std::string& monitor_id);
    std::shared_ptr<ResourceMonitor> getMonitor(const std::string& monitor_id);
    std::vector<std::string> getMonitorIds() const;
    
    // EN: Global operations / FR: Opérations globales
    void startAll();
    void stopAll();
    void pauseAll();
    void resumeAll();
    
    // EN: Aggregated statistics / FR: Statistiques agrégées
    struct GlobalResourceStatus {
        double overall_system_health = 100.0; // EN: 0-100% system health score / FR: Score de santé système 0-100%
        size_t active_monitors = 0;
        size_t total_alerts = 0;
        size_t critical_alerts = 0;
        bool is_any_resource_throttled = false;
        std::map<ResourceType, double> average_usage;
        std::map<ResourceType, ResourceAlertSeverity> worst_alert_severity;
        std::chrono::system_clock::time_point last_updated;
    };
    
    GlobalResourceStatus getGlobalStatus() const;
    std::string getGlobalStatusSummary() const;
    bool isSystemHealthy() const;
    
    // EN: Emergency procedures / FR: Procédures d'urgence
    void emergencyThrottleAll(double factor = 0.1);
    void emergencyStop();
    void resetAllThrottling();
    
private:
    // EN: Private constructor for singleton / FR: Constructeur privé pour singleton
    ResourceMonitorManager() = default;
    ~ResourceMonitorManager() = default;
    ResourceMonitorManager(const ResourceMonitorManager&) = delete;
    ResourceMonitorManager& operator=(const ResourceMonitorManager&) = delete;
    
    // EN: Internal state / FR: État interne
    mutable std::mutex monitors_mutex_;
    std::unordered_map<std::string, std::shared_ptr<ResourceMonitor>> monitors_;
    std::atomic<size_t> monitor_counter_{0};
};

// EN: RAII helper class for automatic resource monitoring
// FR: Classe d'aide RAII pour la surveillance automatique des ressources
class AutoResourceMonitor {
public:
    AutoResourceMonitor(const std::string& name, const ResourceMonitorConfig& config = {});
    ~AutoResourceMonitor();
    
    // EN: Monitor operations / FR: Opérations de surveillance
    std::shared_ptr<ResourceMonitor> getMonitor() const;
    std::string getMonitorId() const;
    bool isHealthy() const;
    void enableEmergencyMode();

private:
    std::string monitor_id_;
    std::shared_ptr<ResourceMonitor> monitor_;
    bool auto_cleanup_;
    bool emergency_mode_enabled_;
};

// EN: Utility functions for resource monitoring
// FR: Fonctions utilitaires pour la surveillance des ressources
namespace ResourceUtils {
    
    // EN: Configuration helpers / FR: Assistants de configuration
    ResourceMonitorConfig createDefaultConfig();
    ResourceMonitorConfig createLightweightConfig();
    ResourceMonitorConfig createHighPerformanceConfig();
    ResourceMonitorConfig createServerConfig();
    ResourceMonitorConfig createDevelopmentConfig();
    
    // EN: Threshold generation / FR: Génération de seuils
    std::vector<ResourceThreshold> createDefaultThresholds();
    std::vector<ResourceThreshold> createConservativeThresholds();
    std::vector<ResourceThreshold> createAggressiveThresholds();
    ResourceThreshold createCPUThreshold(double warning = 75.0, double critical = 90.0);
    ResourceThreshold createMemoryThreshold(double warning = 80.0, double critical = 95.0);
    ResourceThreshold createNetworkThreshold(double warning = 70.0, double critical = 85.0);
    
    // EN: Formatting utilities / FR: Utilitaires de formatage
    std::string formatBytes(size_t bytes);
    std::string formatBytesPerSecond(double bytes_per_second);
    std::string formatPercentage(double percentage);
    std::string formatDuration(std::chrono::milliseconds duration);
    
    // EN: Statistical utilities / FR: Utilitaires statistiques
    double calculateMean(const std::vector<double>& values);
    double calculateMedian(std::vector<double> values);
    double calculateStandardDeviation(const std::vector<double>& values);
    double calculatePercentile(std::vector<double> values, double percentile);
    double calculateTrendSlope(const std::vector<std::pair<double, double>>& time_value_pairs);
    
    // EN: System utilities / FR: Utilitaires système
    SystemResourceInfo getCurrentSystemInfo();
    bool isSystemResourceAvailable(ResourceType resource_type);
    std::vector<ResourceType> getAvailableResourceTypes();
    
    // EN: Alert utilities / FR: Utilitaires d'alerte
    std::string generateAlertMessage(const ResourceAlert& alert);
    std::string generateRecommendedAction(const ResourceAlert& alert);
    ResourceAlertSeverity calculateAlertSeverity(double current_value, const ResourceThreshold& threshold);
    
    // EN: Performance utilities / FR: Utilitaires de performance
    double calculateResourceEfficiency(const ResourceStatistics& stats);
    bool isResourceUsageOptimal(const ResourceStatistics& stats);
    std::vector<std::string> generateOptimizationTips(ResourceType resource_type, const ResourceStatistics& stats);
}

} // namespace Orchestrator
} // namespace BBP