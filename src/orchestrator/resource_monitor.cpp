// EN: Resource Monitor implementation for BB-Pipeline - CPU/RAM/Network monitoring with throttling
// FR: Implémentation du Moniteur de Ressources pour BB-Pipeline - Surveillance CPU/RAM/Réseau avec throttling

#include "orchestrator/resource_monitor.hpp"
#include "infrastructure/logging/logger.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <thread>
#include <chrono>
#include <random>
#include <regex>

// EN: Platform-specific includes for system monitoring
// FR: Inclusions spécifiques à la plateforme pour la surveillance système
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")
#else
#include <sys/resource.h>
#include <sys/statvfs.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#include <proc/readproc.h>
#endif
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif
#include <ifaddrs.h>
#include <net/if.h>
#endif

namespace BBP {
namespace Orchestrator {

// EN: Internal implementation class for ResourceMonitor using PIMPL pattern
// FR: Classe d'implémentation interne pour ResourceMonitor utilisant le motif PIMPL
class ResourceMonitor::ResourceMonitorImpl {
public:
    // EN: Constructor with configuration
    // FR: Constructeur avec configuration
    explicit ResourceMonitorImpl(const ResourceMonitorConfig& config)
        : config_(config)
        , is_running_(false)
        , is_paused_(false)
        , monitoring_thread_running_(false)
        , collection_counter_(0)
        , failed_collections_(0) {
        
        // EN: Initialize system information
        // FR: Initialiser les informations système
        system_info_ = collectSystemInfo();
        
        // EN: Initialize resource collectors
        // FR: Initialiser les collecteurs de ressources
        initializeResourceCollectors();
        
        // EN: Setup default thresholds if none provided
        // FR: Configurer les seuils par défaut si aucun n'est fourni
        if (config_.thresholds.empty()) {
            config_.thresholds = ResourceUtils::createDefaultThresholds();
        }
        
        // EN: Initialize history storage
        // FR: Initialiser le stockage d'historique
        for (int i = 0; i <= static_cast<int>(ResourceType::CUSTOM); ++i) {
            ResourceType type = static_cast<ResourceType>(i);
            resource_history_[type].reserve(config_.history_size);
            alert_history_[type].reserve(1000); // Keep last 1000 alerts per resource
        }
        
        // EN: Initialize performance counters
        // FR: Initialiser les compteurs de performance
        performance_stats_.avg_collection_time = std::chrono::milliseconds(0);
        performance_stats_.max_collection_time = std::chrono::milliseconds(0);
        performance_stats_.collections_per_second = 0;
        performance_stats_.failed_collections = 0;
        performance_stats_.cpu_overhead_percentage = 0.0;
        performance_stats_.memory_usage_bytes = 0;
        
        start_time_ = std::chrono::steady_clock::now();
    }
    
    // EN: Destructor - cleanup resources
    // FR: Destructeur - nettoyage des ressources
    ~ResourceMonitorImpl() {
        stop();
    }
    
    // EN: Start resource monitoring
    // FR: Démarrer la surveillance des ressources
    bool start() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        
        if (is_running_) {
            return false;
        }
        
        is_running_ = true;
        is_paused_ = false;
        
        // EN: Start monitoring thread
        // FR: Démarrer le thread de surveillance
        startMonitoringThread();
        
        // EN: Log monitoring start
        // FR: Journaliser le démarrage de la surveillance
        logEvent("Resource monitoring started", ResourceAlertSeverity::INFO);
        
        return true;
    }
    
    // EN: Stop resource monitoring
    // FR: Arrêter la surveillance des ressources
    void stop() {
        std::unique_lock<std::mutex> lock(state_mutex_);
        
        if (!is_running_) {
            return;
        }
        
        is_running_ = false;
        is_paused_ = false;
        
        // EN: Stop monitoring thread
        // FR: Arrêter le thread de surveillance
        stopMonitoringThread();
        
        // EN: Log monitoring stop
        // FR: Journaliser l'arrêt de la surveillance
        logEvent("Resource monitoring stopped", ResourceAlertSeverity::INFO);
    }
    
    // EN: Pause resource monitoring
    // FR: Mettre en pause la surveillance des ressources
    void pause() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (is_running_) {
            is_paused_ = true;
            logEvent("Resource monitoring paused", ResourceAlertSeverity::INFO);
        }
    }
    
    // EN: Resume resource monitoring
    // FR: Reprendre la surveillance des ressources
    void resume() {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (is_running_ && is_paused_) {
            is_paused_ = false;
            monitoring_cv_.notify_all();
            logEvent("Resource monitoring resumed", ResourceAlertSeverity::INFO);
        }
    }
    
    // EN: Check if monitoring is running
    // FR: Vérifier si la surveillance est en cours
    bool isRunning() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return is_running_;
    }
    
    // EN: Check if monitoring is paused
    // FR: Vérifier si la surveillance est en pause
    bool isPaused() const {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return is_paused_;
    }
    
    // EN: Update configuration
    // FR: Mettre à jour la configuration
    void updateConfig(const ResourceMonitorConfig& config) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_ = config;
        
        // EN: Reinitialize collectors if needed
        // FR: Réinitialiser les collecteurs si nécessaire
        initializeResourceCollectors();
    }
    
    // EN: Get current configuration
    // FR: Obtenir la configuration actuelle
    ResourceMonitorConfig getConfig() const {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return config_;
    }
    
    // EN: Get current resource usage
    // FR: Obtenir l'usage actuel des ressources
    std::optional<ResourceUsage> getCurrentUsage(ResourceType resource_type) const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        auto it = current_usage_.find(resource_type);
        if (it != current_usage_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    // EN: Get all current resource usage
    // FR: Obtenir l'usage actuel de toutes les ressources
    std::map<ResourceType, ResourceUsage> getAllCurrentUsage() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return current_usage_;
    }
    
    // EN: Get system information
    // FR: Obtenir les informations système
    SystemResourceInfo getSystemInfo() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return system_info_;
    }
    
    // EN: Add resource threshold
    // FR: Ajouter un seuil de ressource
    void addThreshold(const ResourceThreshold& threshold) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        
        // EN: Remove existing threshold for the same resource type
        // FR: Supprimer le seuil existant pour le même type de ressource
        auto it = std::find_if(config_.thresholds.begin(), config_.thresholds.end(),
                              [&threshold](const ResourceThreshold& t) {
                                  return t.resource_type == threshold.resource_type;
                              });
        
        if (it != config_.thresholds.end()) {
            *it = threshold;
        } else {
            config_.thresholds.push_back(threshold);
        }
    }
    
    // EN: Remove resource threshold
    // FR: Supprimer un seuil de ressource
    bool removeThreshold(ResourceType resource_type) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        
        auto it = std::find_if(config_.thresholds.begin(), config_.thresholds.end(),
                              [resource_type](const ResourceThreshold& t) {
                                  return t.resource_type == resource_type;
                              });
        
        if (it != config_.thresholds.end()) {
            config_.thresholds.erase(it);
            return true;
        }
        return false;
    }
    
    // EN: Get resource statistics
    // FR: Obtenir les statistiques de ressources
    ResourceStatistics getResourceStatistics(ResourceType resource_type, std::chrono::minutes period) const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        ResourceStatistics stats;
        stats.resource_type = resource_type;
        stats.period_end = std::chrono::system_clock::now();
        stats.period_start = stats.period_end - period;
        
        auto history_it = resource_history_.find(resource_type);
        if (history_it == resource_history_.end()) {
            return stats;
        }
        
        const auto& history = history_it->second;
        
        // EN: Filter samples within the period
        // FR: Filtrer les échantillons dans la période
        std::vector<double> values;
        for (const auto& usage : history) {
            if (usage.timestamp >= stats.period_start && usage.timestamp <= stats.period_end) {
                values.push_back(usage.current_value);
                stats.raw_samples.push_back(usage.current_value);
            }
        }
        
        if (values.empty()) {
            return stats;
        }
        
        // EN: Calculate statistical measures
        // FR: Calculer les mesures statistiques
        stats.sample_count = values.size();
        stats.mean_value = ResourceUtils::calculateMean(values);
        stats.median_value = ResourceUtils::calculateMedian(values);
        stats.standard_deviation = ResourceUtils::calculateStandardDeviation(values);
        stats.variance = stats.standard_deviation * stats.standard_deviation;
        stats.minimum_value = *std::min_element(values.begin(), values.end());
        stats.maximum_value = *std::max_element(values.begin(), values.end());
        stats.percentile_95 = ResourceUtils::calculatePercentile(values, 95.0);
        stats.percentile_99 = ResourceUtils::calculatePercentile(values, 99.0);
        
        // EN: Calculate trend analysis
        // FR: Calculer l'analyse de tendance
        calculateTrendAnalysis(stats, history);
        
        // EN: Calculate threshold exceedance times
        // FR: Calculer les temps de dépassement de seuils
        calculateThresholdTimes(stats, resource_type, history);
        
        stats.total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            stats.period_end - stats.period_start);
        
        return stats;
    }
    
    // EN: Get active alerts
    // FR: Obtenir les alertes actives
    std::vector<ResourceAlert> getActiveAlerts() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return active_alerts_;
    }
    
    // EN: Check if throttling is active
    // FR: Vérifier si le throttling est actif
    bool isThrottlingActive(ResourceType resource_type) const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        auto it = throttling_factors_.find(resource_type);
        return it != throttling_factors_.end() && it->second < 1.0;
    }
    
    // EN: Get current throttling factor
    // FR: Obtenir le facteur de throttling actuel
    double getCurrentThrottlingFactor(ResourceType resource_type) const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        auto it = throttling_factors_.find(resource_type);
        if (it != throttling_factors_.end()) {
            return it->second;
        }
        return 1.0; // No throttling
    }
    
    // EN: Set event callbacks
    // FR: Définir les callbacks d'événement
    void setAlertCallback(ResourceAlertCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        alert_callback_ = callback;
    }
    
    void setResourceUpdateCallback(ResourceUpdateCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        resource_update_callback_ = callback;
    }
    
    void setThrottlingCallback(ThrottlingCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        throttling_callback_ = callback;
    }
    
    // EN: Get monitor performance statistics
    // FR: Obtenir les statistiques de performance du moniteur
    ResourceMonitor::MonitorPerformance getMonitorPerformance() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return performance_stats_;
    }

private:
    // EN: Configuration and state
    // FR: Configuration et état
    ResourceMonitorConfig config_;
    mutable std::mutex state_mutex_;
    mutable std::mutex config_mutex_;
    mutable std::mutex data_mutex_;
    mutable std::mutex callback_mutex_;
    
    std::atomic<bool> is_running_;
    std::atomic<bool> is_paused_;
    std::atomic<bool> monitoring_thread_running_;
    std::atomic<size_t> collection_counter_;
    std::atomic<size_t> failed_collections_;
    
    std::chrono::steady_clock::time_point start_time_;
    
    // EN: Monitoring thread management
    // FR: Gestion du thread de surveillance
    std::thread monitoring_thread_;
    std::condition_variable monitoring_cv_;
    
    // EN: Resource data storage
    // FR: Stockage des données de ressources
    std::map<ResourceType, ResourceUsage> current_usage_;
    std::map<ResourceType, std::deque<ResourceUsage>> resource_history_;
    std::map<ResourceType, std::deque<ResourceAlert>> alert_history_;
    std::vector<ResourceAlert> active_alerts_;
    std::map<ResourceType, double> throttling_factors_;
    SystemResourceInfo system_info_;
    
    // EN: Performance monitoring
    // FR: Surveillance des performances
    ResourceMonitor::MonitorPerformance performance_stats_;
    
    // EN: Event callbacks
    // FR: Callbacks d'événement
    ResourceAlertCallback alert_callback_;
    ResourceUpdateCallback resource_update_callback_;
    ThrottlingCallback throttling_callback_;
    
    // EN: Custom metrics
    // FR: Métriques personnalisées
    std::map<std::string, std::function<double()>> custom_metrics_;
    std::map<std::string, ResourceUnit> custom_metric_units_;
    
    // EN: Platform-specific data
    // FR: Données spécifiques à la plateforme
    #ifdef _WIN32
    HANDLE process_handle_;
    PDH_HQUERY pdh_query_;
    PDH_HCOUNTER cpu_counter_;
    PDH_HCOUNTER memory_counter_;
    #else
    proc_t* proc_info_;
    #endif
    
    // EN: Initialize resource collectors
    // FR: Initialiser les collecteurs de ressources
    void initializeResourceCollectors() {
        #ifdef _WIN32
        process_handle_ = GetCurrentProcess();
        
        // EN: Initialize PDH for performance counters
        // FR: Initialiser PDH pour les compteurs de performance
        PdhOpenQueryA(nullptr, 0, &pdh_query_);
        PdhAddEnglishCounterA(pdh_query_, "\\Processor(_Total)\\% Processor Time", 0, &cpu_counter_);
        PdhAddEnglishCounterA(pdh_query_, "\\Memory\\Available Bytes", 0, &memory_counter_);
        #else
        // EN: Initialize Linux proc filesystem access
        // FR: Initialiser l'accès au système de fichiers proc Linux
        proc_info_ = nullptr;
        #endif
    }
    
    // EN: Start monitoring thread
    // FR: Démarrer le thread de surveillance
    void startMonitoringThread() {
        monitoring_thread_running_ = true;
        monitoring_thread_ = std::thread([this]() {
            monitoringLoop();
        });
    }
    
    // EN: Stop monitoring thread
    // FR: Arrêter le thread de surveillance
    void stopMonitoringThread() {
        if (monitoring_thread_running_) {
            monitoring_thread_running_ = false;
            monitoring_cv_.notify_all();
            
            if (monitoring_thread_.joinable()) {
                monitoring_thread_.join();
            }
        }
    }
    
    // EN: Main monitoring loop
    // FR: Boucle de surveillance principale
    void monitoringLoop() {
        while (monitoring_thread_running_) {
            auto loop_start = std::chrono::steady_clock::now();
            
            // EN: Wait if paused
            // FR: Attendre si en pause
            std::unique_lock<std::mutex> lock(state_mutex_);
            monitoring_cv_.wait(lock, [this]() {
                return !is_paused_ || !monitoring_thread_running_;
            });
            
            if (!monitoring_thread_running_) {
                break;
            }
            
            lock.unlock();
            
            try {
                // EN: Collect resource data
                // FR: Collecter les données de ressources
                collectAllResources();
                
                // EN: Process alerts and throttling
                // FR: Traiter les alertes et le throttling
                processAlerts();
                updateThrottling();
                
                // EN: Update performance statistics
                // FR: Mettre à jour les statistiques de performance
                updatePerformanceStats(loop_start);
                
                collection_counter_.fetch_add(1);
                
            } catch (const std::exception& e) {
                failed_collections_.fetch_add(1);
                logEvent("Collection failed: " + std::string(e.what()), ResourceAlertSeverity::WARNING);
            }
            
            // EN: Calculate next collection time
            // FR: Calculer le prochain temps de collecte
            auto next_collection = calculateNextCollectionTime();
            std::this_thread::sleep_until(next_collection);
        }
    }
    
    // EN: Collect system information
    // FR: Collecter les informations système
    SystemResourceInfo collectSystemInfo() const {
        SystemResourceInfo info;
        
        #ifdef _WIN32
        // EN: Windows system information collection
        // FR: Collection d'informations système Windows
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        info.cpu_core_count = sys_info.dwNumberOfProcessors;
        info.cpu_logical_count = sys_info.dwNumberOfProcessors;
        
        MEMORYSTATUSEX mem_status;
        mem_status.dwLength = sizeof(mem_status);
        GlobalMemoryStatusEx(&mem_status);
        info.total_physical_memory = mem_status.ullTotalPhys;
        info.available_physical_memory = mem_status.ullAvailPhys;
        info.total_virtual_memory = mem_status.ullTotalVirtual;
        info.available_virtual_memory = mem_status.ullAvailVirtual;
        info.page_size = sys_info.dwPageSize;
        
        info.operating_system = "Windows";
        
        #else
        // EN: Linux system information collection
        // FR: Collection d'informations système Linux
        info.cpu_core_count = std::thread::hardware_concurrency();
        info.cpu_logical_count = sysconf(_SC_NPROCESSORS_ONLN);
        
        struct sysinfo sys_info;
        if (sysinfo(&sys_info) == 0) {
            info.total_physical_memory = sys_info.totalram * sys_info.mem_unit;
            info.available_physical_memory = sys_info.freeram * sys_info.mem_unit;
            info.total_virtual_memory = (sys_info.totalram + sys_info.totalswap) * sys_info.mem_unit;
            info.available_virtual_memory = (sys_info.freeram + sys_info.freeswap) * sys_info.mem_unit;
            info.process_count = sys_info.procs;
            info.system_load_1min = sys_info.loads[0] / 65536.0;
            info.system_load_5min = sys_info.loads[1] / 65536.0;
            info.system_load_15min = sys_info.loads[2] / 65536.0;
        }
        
        info.page_size = sysconf(_SC_PAGESIZE);
        info.operating_system = "Linux";
        
        // EN: Get CPU information from /proc/cpuinfo
        // FR: Obtenir les informations CPU depuis /proc/cpuinfo
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    info.cpu_model = line.substr(pos + 2);
                    break;
                }
            }
        }
        
        // EN: Get network interfaces
        // FR: Obtenir les interfaces réseau
        collectNetworkInterfaces(info);
        #endif
        
        info.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time_);
        
        return info;
    }
    
    // EN: Collect all resource data
    // FR: Collecter toutes les données de ressources
    void collectAllResources() {
        auto collection_start = std::chrono::steady_clock::now();
        
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        // EN: Collect CPU usage
        // FR: Collecter l'usage CPU
        if (config_.enable_system_monitoring) {
            collectCPUUsage();
            collectMemoryUsage();
            collectSystemMetrics();
        }
        
        // EN: Collect network usage
        // FR: Collecter l'usage réseau
        if (config_.enable_network_monitoring) {
            collectNetworkUsage();
        }
        
        // EN: Collect disk usage
        // FR: Collecter l'usage disque
        if (config_.enable_disk_monitoring) {
            collectDiskUsage();
        }
        
        // EN: Collect process-specific metrics
        // FR: Collecter les métriques spécifiques au processus
        if (config_.enable_process_monitoring) {
            collectProcessMetrics();
        }
        
        // EN: Collect custom metrics
        // FR: Collecter les métriques personnalisées
        collectCustomMetrics();
        
        // EN: Update history for all resources
        // FR: Mettre à jour l'historique pour toutes les ressources
        updateResourceHistory();
        
        // EN: Notify callbacks
        // FR: Notifier les callbacks
        notifyResourceUpdates();
        
        auto collection_end = std::chrono::steady_clock::now();
        auto collection_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            collection_end - collection_start);
        
        // EN: Update performance statistics
        // FR: Mettre à jour les statistiques de performance
        performance_stats_.avg_collection_time = 
            (performance_stats_.avg_collection_time + collection_time) / 2;
        performance_stats_.max_collection_time = 
            std::max(performance_stats_.max_collection_time, collection_time);
    }
    
    // EN: Collect CPU usage data
    // FR: Collecter les données d'usage CPU
    void collectCPUUsage() {
        ResourceUsage usage;
        usage.type = ResourceType::CPU;
        usage.unit = ResourceUnit::PERCENTAGE;
        usage.timestamp = std::chrono::system_clock::now();
        
        #ifdef _WIN32
        // EN: Windows CPU usage collection using PDH
        // FR: Collection d'usage CPU Windows utilisant PDH
        PdhCollectQueryData(pdh_query_);
        
        PDH_FMT_COUNTERVALUE counter_value;
        PdhGetFormattedCounterValue(cpu_counter_, PDH_FMT_DOUBLE, nullptr, &counter_value);
        
        if (counter_value.CStatus == PDH_CSTATUS_VALID_DATA) {
            usage.current_value = counter_value.dblValue;
            usage.is_valid = true;
        } else {
            usage.is_valid = false;
            usage.error_message = "Failed to collect CPU data from PDH";
        }
        
        #else
        // EN: Linux CPU usage collection from /proc/stat
        // FR: Collection d'usage CPU Linux depuis /proc/stat
        static unsigned long long prev_idle = 0, prev_total = 0;
        
        std::ifstream stat_file("/proc/stat");
        std::string line;
        if (std::getline(stat_file, line)) {
            std::istringstream iss(line);
            std::string cpu_label;
            unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
            
            iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            
            unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
            unsigned long long idle_time = idle + iowait;
            
            if (prev_total > 0) {
                unsigned long long total_diff = total - prev_total;
                unsigned long long idle_diff = idle_time - prev_idle;
                
                if (total_diff > 0) {
                    usage.current_value = ((double)(total_diff - idle_diff) / total_diff) * 100.0;
                    usage.is_valid = true;
                }
            }
            
            prev_total = total;
            prev_idle = idle_time;
        } else {
            usage.is_valid = false;
            usage.error_message = "Failed to read /proc/stat";
        }
        #endif
        
        current_usage_[ResourceType::CPU] = usage;
    }
    
    // EN: Collect memory usage data
    // FR: Collecter les données d'usage mémoire
    void collectMemoryUsage() {
        ResourceUsage usage;
        usage.type = ResourceType::MEMORY;
        usage.unit = ResourceUnit::PERCENTAGE;
        usage.timestamp = std::chrono::system_clock::now();
        
        #ifdef _WIN32
        // EN: Windows memory usage collection
        // FR: Collection d'usage mémoire Windows
        MEMORYSTATUSEX mem_status;
        mem_status.dwLength = sizeof(mem_status);
        
        if (GlobalMemoryStatusEx(&mem_status)) {
            usage.current_value = (double)(mem_status.ullTotalPhys - mem_status.ullAvailPhys) / 
                                 mem_status.ullTotalPhys * 100.0;
            usage.metadata["total_bytes"] = static_cast<double>(mem_status.ullTotalPhys);
            usage.metadata["used_bytes"] = static_cast<double>(mem_status.ullTotalPhys - mem_status.ullAvailPhys);
            usage.metadata["available_bytes"] = static_cast<double>(mem_status.ullAvailPhys);
            usage.is_valid = true;
        } else {
            usage.is_valid = false;
            usage.error_message = "Failed to get memory status";
        }
        
        #else
        // EN: Linux memory usage collection from /proc/meminfo
        // FR: Collection d'usage mémoire Linux depuis /proc/meminfo
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        std::map<std::string, size_t> memory_info;
        
        while (std::getline(meminfo, line)) {
            std::istringstream iss(line);
            std::string key;
            size_t value;
            std::string unit;
            
            if (iss >> key >> value >> unit) {
                // EN: Remove colon from key / FR: Supprimer les deux-points de la clé
                if (!key.empty() && key.back() == ':') {
                    key.pop_back();
                }
                memory_info[key] = value * 1024; // Convert KB to bytes
            }
        }
        
        if (memory_info.find("MemTotal") != memory_info.end() && 
            memory_info.find("MemAvailable") != memory_info.end()) {
            
            size_t total = memory_info["MemTotal"];
            size_t available = memory_info["MemAvailable"];
            size_t used = total - available;
            
            usage.current_value = (double)used / total * 100.0;
            usage.metadata["total_bytes"] = static_cast<double>(total);
            usage.metadata["used_bytes"] = static_cast<double>(used);
            usage.metadata["available_bytes"] = static_cast<double>(available);
            
            if (memory_info.find("Cached") != memory_info.end()) {
                usage.metadata["cached_bytes"] = static_cast<double>(memory_info["Cached"]);
            }
            if (memory_info.find("Buffers") != memory_info.end()) {
                usage.metadata["buffer_bytes"] = static_cast<double>(memory_info["Buffers"]);
            }
            
            usage.is_valid = true;
        } else {
            usage.is_valid = false;
            usage.error_message = "Failed to parse /proc/meminfo";
        }
        #endif
        
        current_usage_[ResourceType::MEMORY] = usage;
    }
    
    // EN: Collect network usage data
    // FR: Collecter les données d'usage réseau
    void collectNetworkUsage() {
        ResourceUsage usage;
        usage.type = ResourceType::NETWORK;
        usage.unit = ResourceUnit::BYTES_PER_SECOND;
        usage.timestamp = std::chrono::system_clock::now();
        
        static std::map<std::string, uint64_t> prev_rx_bytes, prev_tx_bytes;
        static auto prev_time = std::chrono::steady_clock::now();
        
        #ifdef _WIN32
        // EN: Windows network statistics collection
        // FR: Collection de statistiques réseau Windows
        // TODO: Implement Windows network collection using GetIfTable2
        usage.current_value = 0.0;
        usage.is_valid = true;
        
        #else
        // EN: Linux network statistics from /proc/net/dev
        // FR: Statistiques réseau Linux depuis /proc/net/dev
        std::ifstream net_dev("/proc/net/dev");
        std::string line;
        
        // EN: Skip header lines
        // FR: Ignorer les lignes d'en-tête
        std::getline(net_dev, line);
        std::getline(net_dev, line);
        
        uint64_t total_rx = 0, total_tx = 0;
        
        while (std::getline(net_dev, line)) {
            std::istringstream iss(line);
            std::string interface;
            uint64_t rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_compressed, rx_multicast;
            uint64_t tx_bytes, tx_packets, tx_errs, tx_drop, tx_fifo, tx_colls, tx_carrier, tx_compressed;
            
            if (iss >> interface >> rx_bytes >> rx_packets >> rx_errs >> rx_drop >> rx_fifo 
                    >> rx_frame >> rx_compressed >> rx_multicast >> tx_bytes >> tx_packets 
                    >> tx_errs >> tx_drop >> tx_fifo >> tx_colls >> tx_carrier >> tx_compressed) {
                
                // EN: Remove colon from interface name
                // FR: Supprimer les deux-points du nom d'interface
                if (!interface.empty() && interface.back() == ':') {
                    interface.pop_back();
                }
                
                // EN: Skip loopback interface
                // FR: Ignorer l'interface de bouclage
                if (interface == "lo") {
                    continue;
                }
                
                total_rx += rx_bytes;
                total_tx += tx_bytes;
                
                usage.metadata[interface + "_rx_bytes"] = static_cast<double>(rx_bytes);
                usage.metadata[interface + "_tx_bytes"] = static_cast<double>(tx_bytes);
                usage.metadata[interface + "_rx_packets"] = static_cast<double>(rx_packets);
                usage.metadata[interface + "_tx_packets"] = static_cast<double>(tx_packets);
                usage.metadata[interface + "_errors"] = static_cast<double>(rx_errs + tx_errs);
            }
        }
        
        auto current_time = std::chrono::steady_clock::now();
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - prev_time).count() / 1000.0;
        
        if (time_diff > 0 && !prev_rx_bytes.empty()) {
            double rx_rate = (total_rx - prev_rx_bytes["total"]) / time_diff;
            double tx_rate = (total_tx - prev_tx_bytes["total"]) / time_diff;
            usage.current_value = rx_rate + tx_rate;
            usage.metadata["rx_bytes_per_second"] = rx_rate;
            usage.metadata["tx_bytes_per_second"] = tx_rate;
        }
        
        prev_rx_bytes["total"] = total_rx;
        prev_tx_bytes["total"] = total_tx;
        prev_time = current_time;
        
        usage.is_valid = true;
        #endif
        
        current_usage_[ResourceType::NETWORK] = usage;
    }
    
    // EN: Collect disk usage data
    // FR: Collecter les données d'usage disque
    void collectDiskUsage() {
        ResourceUsage usage;
        usage.type = ResourceType::DISK;
        usage.unit = ResourceUnit::BYTES_PER_SECOND;
        usage.timestamp = std::chrono::system_clock::now();
        
        #ifdef _WIN32
        // EN: Windows disk usage collection
        // FR: Collection d'usage disque Windows
        // TODO: Implement Windows disk I/O monitoring
        usage.current_value = 0.0;
        usage.is_valid = true;
        
        #else
        // EN: Linux disk usage from /proc/diskstats
        // FR: Usage disque Linux depuis /proc/diskstats
        static std::map<std::string, uint64_t> prev_read_sectors, prev_write_sectors;
        static auto prev_time = std::chrono::steady_clock::now();
        
        std::ifstream diskstats("/proc/diskstats");
        std::string line;
        
        uint64_t total_read_sectors = 0, total_write_sectors = 0;
        
        while (std::getline(diskstats, line)) {
            std::istringstream iss(line);
            int major, minor;
            std::string device;
            uint64_t reads_completed, reads_merged, sectors_read, time_reading;
            uint64_t writes_completed, writes_merged, sectors_written, time_writing;
            uint64_t ios_in_progress, time_ios, weighted_time_ios;
            
            if (iss >> major >> minor >> device >> reads_completed >> reads_merged 
                    >> sectors_read >> time_reading >> writes_completed >> writes_merged 
                    >> sectors_written >> time_writing >> ios_in_progress >> time_ios 
                    >> weighted_time_ios) {
                
                // EN: Only consider physical drives (exclude partitions and special devices)
                // FR: Ne considérer que les lecteurs physiques (exclure partitions et périphériques spéciaux)
                if (device.find("loop") == std::string::npos && 
                    device.find("ram") == std::string::npos &&
                    std::isalpha(device.back())) {
                    
                    total_read_sectors += sectors_read;
                    total_write_sectors += sectors_written;
                    
                    usage.metadata[device + "_read_sectors"] = static_cast<double>(sectors_read);
                    usage.metadata[device + "_write_sectors"] = static_cast<double>(sectors_written);
                    usage.metadata[device + "_reads"] = static_cast<double>(reads_completed);
                    usage.metadata[device + "_writes"] = static_cast<double>(writes_completed);
                }
            }
        }
        
        auto current_time = std::chrono::steady_clock::now();
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - prev_time).count() / 1000.0;
        
        if (time_diff > 0 && !prev_read_sectors.empty()) {
            // EN: Assume 512 bytes per sector
            // FR: Supposer 512 octets par secteur
            double read_rate = (total_read_sectors - prev_read_sectors["total"]) * 512 / time_diff;
            double write_rate = (total_write_sectors - prev_write_sectors["total"]) * 512 / time_diff;
            usage.current_value = read_rate + write_rate;
            usage.metadata["read_bytes_per_second"] = read_rate;
            usage.metadata["write_bytes_per_second"] = write_rate;
        }
        
        prev_read_sectors["total"] = total_read_sectors;
        prev_write_sectors["total"] = total_write_sectors;
        prev_time = current_time;
        
        usage.is_valid = true;
        #endif
        
        current_usage_[ResourceType::DISK] = usage;
    }
    
    // EN: Collect process-specific metrics
    // FR: Collecter les métriques spécifiques au processus
    void collectProcessMetrics() {
        ResourceUsage usage;
        usage.type = ResourceType::PROCESS;
        usage.unit = ResourceUnit::PERCENTAGE;
        usage.timestamp = std::chrono::system_clock::now();
        
        #ifdef _WIN32
        // EN: Windows process metrics
        // FR: Métriques de processus Windows
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(process_handle_, &pmc, sizeof(pmc))) {
            usage.metadata["working_set_size"] = static_cast<double>(pmc.WorkingSetSize);
            usage.metadata["pagefile_usage"] = static_cast<double>(pmc.PagefileUsage);
            usage.metadata["page_faults"] = static_cast<double>(pmc.PageFaultCount);
        }
        
        FILETIME creation_time, exit_time, kernel_time, user_time;
        if (GetProcessTimes(process_handle_, &creation_time, &exit_time, &kernel_time, &user_time)) {
            ULARGE_INTEGER kernel, user;
            kernel.LowPart = kernel_time.dwLowDateTime;
            kernel.HighPart = kernel_time.dwHighDateTime;
            user.LowPart = user_time.dwLowDateTime;
            user.HighPart = user_time.dwHighDateTime;
            
            static ULARGE_INTEGER prev_kernel = {0}, prev_user = {0};
            static auto prev_time = std::chrono::steady_clock::now();
            
            auto current_time = std::chrono::steady_clock::now();
            auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                current_time - prev_time).count();
            
            if (time_diff > 0 && prev_kernel.QuadPart > 0) {
                double kernel_diff = (kernel.QuadPart - prev_kernel.QuadPart) / 10000.0; // Convert to ms
                double user_diff = (user.QuadPart - prev_user.QuadPart) / 10000.0;
                usage.current_value = (kernel_diff + user_diff) / time_diff * 100.0;
            }
            
            prev_kernel = kernel;
            prev_user = user;
            prev_time = current_time;
        }
        
        usage.is_valid = true;
        
        #else
        // EN: Linux process metrics from /proc/self/stat and /proc/self/status
        // FR: Métriques de processus Linux depuis /proc/self/stat et /proc/self/status
        std::ifstream stat_file("/proc/self/stat");
        std::string stat_line;
        
        if (std::getline(stat_file, stat_line)) {
            std::istringstream iss(stat_line);
            std::vector<std::string> fields;
            std::string field;
            
            while (iss >> field) {
                fields.push_back(field);
            }
            
            if (fields.size() >= 24) {
                // EN: CPU times are in fields 13 (utime) and 14 (stime)
                // FR: Les temps CPU sont dans les champs 13 (utime) et 14 (stime)
                static unsigned long prev_utime = 0, prev_stime = 0;
                static auto prev_time = std::chrono::steady_clock::now();
                
                unsigned long utime = std::stoul(fields[13]);
                unsigned long stime = std::stoul(fields[14]);
                
                auto current_time = std::chrono::steady_clock::now();
                auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - prev_time).count() / 1000.0;
                
                if (time_diff > 0 && prev_utime > 0) {
                    long clock_ticks_per_sec = sysconf(_SC_CLK_TCK);
                    double cpu_time_diff = ((utime + stime) - (prev_utime + prev_stime)) / 
                                          (double)clock_ticks_per_sec;
                    usage.current_value = (cpu_time_diff / time_diff) * 100.0;
                }
                
                prev_utime = utime;
                prev_stime = stime;
                prev_time = current_time;
                
                // EN: Memory info (RSS in field 23, VSZ in field 22)
                // FR: Infos mémoire (RSS dans le champ 23, VSZ dans le champ 22)
                usage.metadata["rss_pages"] = static_cast<double>(std::stoul(fields[23]));
                usage.metadata["vsize_bytes"] = static_cast<double>(std::stoul(fields[22]));
                usage.metadata["threads"] = static_cast<double>(std::stoul(fields[19]));
            }
        }
        
        // EN: Additional memory information from /proc/self/status
        // FR: Informations mémoire supplémentaires depuis /proc/self/status
        std::ifstream status_file("/proc/self/status");
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.find("VmRSS:") == 0) {
                std::istringstream iss(line);
                std::string label;
                size_t value;
                std::string unit;
                if (iss >> label >> value >> unit) {
                    usage.metadata["rss_kb"] = static_cast<double>(value);
                }
            } else if (line.find("VmSize:") == 0) {
                std::istringstream iss(line);
                std::string label;
                size_t value;
                std::string unit;
                if (iss >> label >> value >> unit) {
                    usage.metadata["vsize_kb"] = static_cast<double>(value);
                }
            }
        }
        
        usage.is_valid = true;
        #endif
        
        current_usage_[ResourceType::PROCESS] = usage;
    }
    
    // EN: Collect system-wide metrics
    // FR: Collecter les métriques à l'échelle du système
    void collectSystemMetrics() {
        ResourceUsage usage;
        usage.type = ResourceType::SYSTEM;
        usage.unit = ResourceUnit::COUNT;
        usage.timestamp = std::chrono::system_clock::now();
        
        #ifdef _WIN32
        // EN: Windows system metrics
        // FR: Métriques système Windows
        // TODO: Implement Windows system metrics collection
        usage.current_value = 0.0;
        usage.is_valid = true;
        
        #else
        // EN: Linux system metrics
        // FR: Métriques système Linux
        struct sysinfo sys_info;
        if (sysinfo(&sys_info) == 0) {
            usage.current_value = sys_info.loads[0] / 65536.0; // 1-minute load average
            usage.metadata["load_1min"] = sys_info.loads[0] / 65536.0;
            usage.metadata["load_5min"] = sys_info.loads[1] / 65536.0;
            usage.metadata["load_15min"] = sys_info.loads[2] / 65536.0;
            usage.metadata["processes"] = static_cast<double>(sys_info.procs);
            usage.metadata["uptime_seconds"] = static_cast<double>(sys_info.uptime);
        }
        
        // EN: Get number of file descriptors
        // FR: Obtenir le nombre de descripteurs de fichiers
        std::ifstream fd_count("/proc/sys/fs/file-nr");
        if (fd_count.is_open()) {
            size_t allocated, unused, max_fds;
            if (fd_count >> allocated >> unused >> max_fds) {
                usage.metadata["open_file_descriptors"] = static_cast<double>(allocated);
                usage.metadata["max_file_descriptors"] = static_cast<double>(max_fds);
            }
        }
        
        usage.is_valid = true;
        #endif
        
        current_usage_[ResourceType::SYSTEM] = usage;
    }
    
    // EN: Collect custom metrics
    // FR: Collecter les métriques personnalisées
    void collectCustomMetrics() {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        
        for (const auto& [name, collector] : custom_metrics_) {
            try {
                ResourceUsage usage;
                usage.type = ResourceType::CUSTOM;
                usage.unit = custom_metric_units_[name];
                usage.timestamp = std::chrono::system_clock::now();
                usage.current_value = collector();
                usage.metadata["metric_name"] = name;
                usage.is_valid = true;
                
                // EN: Store with custom key
                // FR: Stocker avec une clé personnalisée
                current_usage_[static_cast<ResourceType>(
                    static_cast<int>(ResourceType::CUSTOM) + std::hash<std::string>{}(name) % 1000)] = usage;
                
            } catch (const std::exception& e) {
                // EN: Log but don't fail the entire collection
                // FR: Journaliser mais ne pas faire échouer toute la collecte
                logEvent("Custom metric '" + name + "' collection failed: " + e.what(), 
                        ResourceAlertSeverity::WARNING);
            }
        }
    }
    
    // EN: Process alerts based on current resource usage
    // FR: Traiter les alertes basées sur l'usage actuel des ressources
    void processAlerts() {
        std::lock_guard<std::mutex> config_lock(config_mutex_);
        std::lock_guard<std::mutex> data_lock(data_mutex_);
        
        // EN: Clear expired active alerts
        // FR: Effacer les alertes actives expirées
        auto now = std::chrono::system_clock::now();
        active_alerts_.erase(
            std::remove_if(active_alerts_.begin(), active_alerts_.end(),
                          [now](const ResourceAlert& alert) {
                              return (now - alert.timestamp) > std::chrono::minutes(5);
                          }),
            active_alerts_.end());
        
        for (const auto& threshold : config_.thresholds) {
            auto usage_it = current_usage_.find(threshold.resource_type);
            if (usage_it == current_usage_.end() || !usage_it->second.is_valid) {
                continue;
            }
            
            const ResourceUsage& usage = usage_it->second;
            
            // EN: Check threshold violations
            // FR: Vérifier les violations de seuils
            ResourceAlertSeverity severity = ResourceAlertSeverity::INFO;
            bool threshold_exceeded = false;
            double threshold_value = 0.0;
            
            if (usage.current_value >= threshold.emergency_threshold) {
                severity = ResourceAlertSeverity::EMERGENCY;
                threshold_exceeded = true;
                threshold_value = threshold.emergency_threshold;
            } else if (usage.current_value >= threshold.critical_threshold) {
                severity = ResourceAlertSeverity::CRITICAL;
                threshold_exceeded = true;
                threshold_value = threshold.critical_threshold;
            } else if (usage.current_value >= threshold.warning_threshold) {
                severity = ResourceAlertSeverity::WARNING;
                threshold_exceeded = true;
                threshold_value = threshold.warning_threshold;
            }
            
            if (threshold_exceeded) {
                // EN: Check if we already have an active alert for this resource
                // FR: Vérifier si nous avons déjà une alerte active pour cette ressource
                auto existing_alert = std::find_if(active_alerts_.begin(), active_alerts_.end(),
                                                  [&threshold](const ResourceAlert& alert) {
                                                      return alert.resource_type == threshold.resource_type;
                                                  });
                
                if (existing_alert == active_alerts_.end()) {
                    // EN: Create new alert
                    // FR: Créer une nouvelle alerte
                    ResourceAlert alert;
                    alert.resource_type = threshold.resource_type;
                    alert.severity = severity;
                    alert.timestamp = now;
                    alert.current_value = usage.current_value;
                    alert.threshold_value = threshold_value;
                    alert.unit = usage.unit;
                    alert.message = generateAlertMessage(alert, threshold);
                    alert.recommended_action = generateRecommendedAction(alert, threshold);
                    
                    active_alerts_.push_back(alert);
                    
                    // EN: Add to history
                    // FR: Ajouter à l'historique
                    alert_history_[threshold.resource_type].push_back(alert);
                    if (alert_history_[threshold.resource_type].size() > 1000) {
                        alert_history_[threshold.resource_type].pop_front();
                    }
                    
                    // EN: Trigger callback
                    // FR: Déclencher le callback
                    if (alert_callback_) {
                        try {
                            alert_callback_(alert);
                        } catch (const std::exception& e) {
                            logEvent("Alert callback failed: " + std::string(e.what()),
                                   ResourceAlertSeverity::WARNING);
                        }
                    }
                    
                    // EN: Apply throttling if enabled
                    // FR: Appliquer le throttling si activé
                    if (threshold.enable_throttling && config_.enable_throttling) {
                        applyThrottling(threshold.resource_type, threshold.throttling_factor, 
                                      threshold.throttling_strategy);
                    }
                }
            }
        }
    }
    
    // EN: Update throttling based on current conditions
    // FR: Mettre à jour le throttling selon les conditions actuelles
    void updateThrottling() {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        // EN: Decay throttling factors over time
        // FR: Faire décroître les facteurs de throttling avec le temps
        for (auto& [resource_type, factor] : throttling_factors_) {
            if (factor < 1.0) {
                // EN: Gradually increase throttling factor (reduce throttling)
                // FR: Augmenter graduellement le facteur de throttling (réduire le throttling)
                factor = std::min(1.0, factor + 0.01);
                
                if (throttling_callback_) {
                    try {
                        throttling_callback_(resource_type, factor, factor < 1.0);
                    } catch (const std::exception& e) {
                        logEvent("Throttling callback failed: " + std::string(e.what()),
                               ResourceAlertSeverity::WARNING);
                    }
                }
            }
        }
        
        // EN: Remove throttling factors that have returned to normal
        // FR: Supprimer les facteurs de throttling qui sont revenus à la normale
        auto it = throttling_factors_.begin();
        while (it != throttling_factors_.end()) {
            if (it->second >= 1.0) {
                it = throttling_factors_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // EN: Apply throttling to a resource
    // FR: Appliquer le throttling à une ressource
    void applyThrottling(ResourceType resource_type, double factor, ThrottlingStrategy strategy) {
        double final_factor = factor;
        
        // EN: Adjust factor based on strategy
        // FR: Ajuster le facteur selon la stratégie
        switch (strategy) {
            case ThrottlingStrategy::LINEAR:
                // EN: Use factor as-is / FR: Utiliser le facteur tel quel
                break;
                
            case ThrottlingStrategy::EXPONENTIAL:
                // EN: Apply exponential reduction / FR: Appliquer une réduction exponentielle
                final_factor = std::pow(factor, 2.0);
                break;
                
            case ThrottlingStrategy::ADAPTIVE: {
                // EN: Adapt based on current resource usage
                // FR: Adapter selon l'usage actuel des ressources
                auto usage_it = current_usage_.find(resource_type);
                if (usage_it != current_usage_.end()) {
                    double usage_ratio = usage_it->second.current_value / 100.0;
                    final_factor = factor * (2.0 - usage_ratio); // More aggressive as usage increases
                }
                break;
            }
            
            case ThrottlingStrategy::AGGRESSIVE:
                // EN: Apply aggressive throttling
                // FR: Appliquer un throttling agressif
                final_factor = factor * 0.5;
                break;
                
            default:
                break;
        }
        
        // EN: Ensure factor is within valid range
        // FR: S'assurer que le facteur est dans la plage valide
        final_factor = std::max(0.01, std::min(1.0, final_factor));
        
        throttling_factors_[resource_type] = final_factor;
        
        logEvent("Throttling applied to " + resourceTypeToString(resource_type) + 
                " with factor " + std::to_string(final_factor), ResourceAlertSeverity::INFO);
    }
    
    // EN: Generate alert message
    // FR: Générer un message d'alerte
    std::string generateAlertMessage(const ResourceAlert& alert, const ResourceThreshold& threshold) const {
        std::ostringstream oss;
        oss << resourceTypeToString(alert.resource_type) << " usage is " 
            << std::fixed << std::setprecision(1) << alert.current_value;
        
        if (alert.unit == ResourceUnit::PERCENTAGE) {
            oss << "%";
        } else {
            oss << " " << resourceUnitToString(alert.unit);
        }
        
        oss << " (threshold: " << alert.threshold_value;
        if (alert.unit == ResourceUnit::PERCENTAGE) {
            oss << "%";
        }
        oss << ")";
        
        return oss.str();
    }
    
    // EN: Generate recommended action
    // FR: Générer une action recommandée
    std::string generateRecommendedAction(const ResourceAlert& alert, const ResourceThreshold& threshold) const {
        switch (alert.resource_type) {
            case ResourceType::CPU:
                if (alert.severity >= ResourceAlertSeverity::CRITICAL) {
                    return "Consider reducing thread count or enabling aggressive throttling";
                } else {
                    return "Monitor CPU usage and consider load balancing";
                }
                
            case ResourceType::MEMORY:
                if (alert.severity >= ResourceAlertSeverity::CRITICAL) {
                    return "Free memory immediately or restart process";
                } else {
                    return "Monitor memory usage and consider garbage collection";
                }
                
            case ResourceType::NETWORK:
                return "Reduce network operations or increase timeout values";
                
            case ResourceType::DISK:
                return "Reduce disk I/O operations or optimize data access patterns";
                
            default:
                return "Monitor resource usage and consider optimization";
        }
    }
    
    // EN: Update resource history
    // FR: Mettre à jour l'historique des ressources
    void updateResourceHistory() {
        for (auto& [resource_type, usage] : current_usage_) {
            auto& history = resource_history_[resource_type];
            
            // EN: Add current usage to history
            // FR: Ajouter l'usage actuel à l'historique
            history.push_back(usage);
            
            // EN: Maintain history size limit
            // FR: Maintenir la limite de taille d'historique
            if (history.size() > config_.history_size) {
                history.pop_front();
            }
            
            // EN: Update average and peak values
            // FR: Mettre à jour les valeurs moyennes et de pic
            if (history.size() > 1) {
                std::vector<double> values;
                values.reserve(history.size());
                for (const auto& h : history) {
                    values.push_back(h.current_value);
                }
                
                usage.average_value = ResourceUtils::calculateMean(values);
                usage.peak_value = *std::max_element(values.begin(), values.end());
                usage.minimum_value = *std::min_element(values.begin(), values.end());
            }
        }
    }
    
    // EN: Notify resource update callbacks
    // FR: Notifier les callbacks de mise à jour de ressources
    void notifyResourceUpdates() {
        if (!resource_update_callback_) {
            return;
        }
        
        std::lock_guard<std::mutex> lock(callback_mutex_);
        
        for (const auto& [resource_type, usage] : current_usage_) {
            try {
                resource_update_callback_(usage);
            } catch (const std::exception& e) {
                logEvent("Resource update callback failed: " + std::string(e.what()),
                       ResourceAlertSeverity::WARNING);
            }
        }
    }
    
    // EN: Calculate next collection time based on frequency mode
    // FR: Calculer le prochain temps de collecte selon le mode de fréquence
    std::chrono::steady_clock::time_point calculateNextCollectionTime() const {
        auto now = std::chrono::steady_clock::now();
        std::chrono::milliseconds interval = config_.collection_interval;
        
        if (config_.frequency == MonitoringFrequency::ADAPTIVE) {
            // EN: Adaptive frequency based on system load
            // FR: Fréquence adaptative basée sur la charge système
            auto cpu_usage_it = current_usage_.find(ResourceType::CPU);
            if (cpu_usage_it != current_usage_.end() && cpu_usage_it->second.is_valid) {
                double cpu_usage = cpu_usage_it->second.current_value;
                
                if (cpu_usage > 80.0) {
                    interval = config_.adaptive_min_interval; // High load, monitor more frequently
                } else if (cpu_usage < 20.0) {
                    interval = config_.adaptive_max_interval; // Low load, monitor less frequently
                } else {
                    // EN: Linear interpolation between min and max
                    // FR: Interpolation linéaire entre min et max
                    double factor = (cpu_usage - 20.0) / 60.0; // Normalize to 0-1
                    interval = std::chrono::milliseconds(static_cast<long>(
                        config_.adaptive_max_interval.count() - 
                        factor * (config_.adaptive_max_interval.count() - config_.adaptive_min_interval.count())
                    ));
                }
            }
        }
        
        return now + interval;
    }
    
    // EN: Update performance statistics
    // FR: Mettre à jour les statistiques de performance
    void updatePerformanceStats(std::chrono::steady_clock::time_point loop_start) {
        auto now = std::chrono::steady_clock::now();
        auto runtime = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
        
        if (runtime.count() > 1000) { // After 1 second of runtime
            performance_stats_.collections_per_second = 
                (collection_counter_.load() * 1000) / runtime.count();
        }
        
        performance_stats_.failed_collections = failed_collections_.load();
        
        // EN: Estimate CPU overhead (rough approximation)
        // FR: Estimer la surcharge CPU (approximation grossière)
        auto collection_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - loop_start);
        
        if (collection_time < config_.collection_interval) {
            performance_stats_.cpu_overhead_percentage = 
                (double)collection_time.count() / config_.collection_interval.count() * 100.0;
        }
        
        // EN: Estimate memory usage (basic approximation)
        // FR: Estimer l'usage mémoire (approximation basique)
        size_t estimated_memory = 
            sizeof(ResourceMonitorImpl) +
            (current_usage_.size() * sizeof(ResourceUsage)) +
            (resource_history_.size() * config_.history_size * sizeof(ResourceUsage)) +
            (active_alerts_.size() * sizeof(ResourceAlert));
        
        performance_stats_.memory_usage_bytes = estimated_memory;
    }
    
    // EN: Calculate trend analysis for resource statistics
    // FR: Calculer l'analyse de tendance pour les statistiques de ressources
    void calculateTrendAnalysis(ResourceStatistics& stats, const std::deque<ResourceUsage>& history) const {
        if (history.size() < 10) {
            return; // Need at least 10 samples for trend analysis
        }
        
        // EN: Prepare time-value pairs for trend calculation
        // FR: Préparer les paires temps-valeur pour le calcul de tendance
        std::vector<std::pair<double, double>> time_value_pairs;
        time_value_pairs.reserve(history.size());
        
        auto reference_time = history.front().timestamp;
        for (const auto& usage : history) {
            if (usage.timestamp >= stats.period_start && usage.timestamp <= stats.period_end) {
                double time_offset = std::chrono::duration_cast<std::chrono::milliseconds>(
                    usage.timestamp - reference_time).count();
                time_value_pairs.emplace_back(time_offset, usage.current_value);
            }
        }
        
        if (time_value_pairs.size() >= 10) {
            stats.trend_slope = ResourceUtils::calculateTrendSlope(time_value_pairs);
            
            // EN: Calculate correlation coefficient for trend strength
            // FR: Calculer le coefficient de corrélation pour la force de tendance
            double mean_time = 0.0, mean_value = 0.0;
            for (const auto& [time, value] : time_value_pairs) {
                mean_time += time;
                mean_value += value;
            }
            mean_time /= time_value_pairs.size();
            mean_value /= time_value_pairs.size();
            
            double numerator = 0.0, sum_sq_time = 0.0, sum_sq_value = 0.0;
            for (const auto& [time, value] : time_value_pairs) {
                double time_diff = time - mean_time;
                double value_diff = value - mean_value;
                numerator += time_diff * value_diff;
                sum_sq_time += time_diff * time_diff;
                sum_sq_value += value_diff * value_diff;
            }
            
            if (sum_sq_time > 0 && sum_sq_value > 0) {
                stats.trend_correlation = numerator / std::sqrt(sum_sq_time * sum_sq_value);
            }
            
            stats.is_increasing_trend = stats.trend_slope > 0.1; // Positive slope indicates increasing
            stats.is_stable = std::abs(stats.trend_correlation) < 0.3; // Low correlation indicates stability
        }
    }
    
    // EN: Calculate time spent above thresholds
    // FR: Calculer le temps passé au-dessus des seuils
    void calculateThresholdTimes(ResourceStatistics& stats, ResourceType resource_type, 
                                const std::deque<ResourceUsage>& history) const {
        
        // EN: Find threshold for this resource type
        // FR: Trouver le seuil pour ce type de ressource
        ResourceThreshold threshold;
        bool threshold_found = false;
        
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            auto it = std::find_if(config_.thresholds.begin(), config_.thresholds.end(),
                                  [resource_type](const ResourceThreshold& t) {
                                      return t.resource_type == resource_type;
                                  });
            if (it != config_.thresholds.end()) {
                threshold = *it;
                threshold_found = true;
            }
        }
        
        if (!threshold_found) {
            return;
        }
        
        // EN: Calculate time above each threshold level
        // FR: Calculer le temps au-dessus de chaque niveau de seuil
        auto prev_time = stats.period_start;
        for (const auto& usage : history) {
            if (usage.timestamp >= stats.period_start && usage.timestamp <= stats.period_end) {
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    usage.timestamp - prev_time);
                
                if (usage.current_value >= threshold.emergency_threshold) {
                    stats.time_above_emergency += duration;
                    stats.time_above_critical += duration;
                    stats.time_above_warning += duration;
                } else if (usage.current_value >= threshold.critical_threshold) {
                    stats.time_above_critical += duration;
                    stats.time_above_warning += duration;
                } else if (usage.current_value >= threshold.warning_threshold) {
                    stats.time_above_warning += duration;
                }
                
                prev_time = usage.timestamp;
            }
        }
        
        // EN: Count alerts for this resource
        // FR: Compter les alertes pour cette ressource
        auto alert_it = alert_history_.find(resource_type);
        if (alert_it != alert_history_.end()) {
            for (const auto& alert : alert_it->second) {
                if (alert.timestamp >= stats.period_start && alert.timestamp <= stats.period_end) {
                    stats.alert_count++;
                }
            }
        }
    }
    
    // EN: Collect network interfaces information
    // FR: Collecter les informations des interfaces réseau
    void collectNetworkInterfaces(SystemResourceInfo& info) const {
        #ifdef _WIN32
        // EN: Windows network interface collection
        // FR: Collection d'interfaces réseau Windows
        // TODO: Implement Windows network interface enumeration
        #else
        // EN: Linux network interface collection using getifaddrs
        // FR: Collection d'interfaces réseau Linux utilisant getifaddrs
        struct ifaddrs* interfaces = nullptr;
        if (getifaddrs(&interfaces) == 0) {
            for (struct ifaddrs* ifa = interfaces; ifa != nullptr; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_PACKET) {
                    std::string if_name(ifa->ifa_name);
                    
                    // EN: Skip loopback interface
                    // FR: Ignorer l'interface de bouclage
                    if (if_name != "lo") {
                        info.network_interfaces.push_back(if_name);
                        info.interface_status[if_name] = (ifa->ifa_flags & IFF_UP) != 0;
                        
                        // EN: Try to get speed information from ethtool
                        // FR: Essayer d'obtenir les informations de vitesse depuis ethtool
                        int sock = socket(AF_INET, SOCK_DGRAM, 0);
                        if (sock >= 0) {
                            struct ifreq ifr;
                            struct ethtool_cmd edata;
                            
                            std::strncpy(ifr.ifr_name, if_name.c_str(), IFNAMSIZ - 1);
                            ifr.ifr_data = reinterpret_cast<char*>(&edata);
                            edata.cmd = ETHTOOL_GSET;
                            
                            if (ioctl(sock, SIOCETHTOOL, &ifr) >= 0) {
                                info.interface_speeds[if_name] = ethtool_cmd_speed(&edata) * 1000000ULL; // Convert Mbps to bps
                            }
                            
                            close(sock);
                        }
                    }
                }
            }
            freeifaddrs(interfaces);
        }
        #endif
    }
    
    // EN: Log monitoring event
    // FR: Journaliser un événement de surveillance
    void logEvent(const std::string& message, ResourceAlertSeverity severity) const {
        if (!config_.enable_logging) {
            return;
        }
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::ostringstream log_entry;
        log_entry << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] "
                  << "[" << alertSeverityToString(severity) << "] "
                  << "ResourceMonitor: " << message;
        
        if (!config_.log_file_path.empty()) {
            std::ofstream log_file(config_.log_file_path, std::ios::app);
            if (log_file.is_open()) {
                log_file << log_entry.str() << std::endl;
            }
        }
        
        // EN: Also log to console for debug builds
        // FR: Également journaliser vers la console pour les builds de debug
        #ifdef DEBUG
        std::cout << log_entry.str() << std::endl;
        #endif
    }
    
    // EN: Convert resource type to string
    // FR: Convertir le type de ressource en chaîne
    std::string resourceTypeToString(ResourceType type) const {
        switch (type) {
            case ResourceType::CPU: return "CPU";
            case ResourceType::MEMORY: return "Memory";
            case ResourceType::NETWORK: return "Network";
            case ResourceType::DISK: return "Disk";
            case ResourceType::PROCESS: return "Process";
            case ResourceType::SYSTEM: return "System";
            case ResourceType::CUSTOM: return "Custom";
            default: return "Unknown";
        }
    }
    
    // EN: Convert resource unit to string
    // FR: Convertir l'unité de ressource en chaîne
    std::string resourceUnitToString(ResourceUnit unit) const {
        switch (unit) {
            case ResourceUnit::PERCENTAGE: return "%";
            case ResourceUnit::BYTES: return "bytes";
            case ResourceUnit::BYTES_PER_SECOND: return "bytes/s";
            case ResourceUnit::COUNT: return "count";
            case ResourceUnit::MILLISECONDS: return "ms";
            case ResourceUnit::HERTZ: return "Hz";
            case ResourceUnit::CUSTOM: return "custom";
            default: return "unknown";
        }
    }
    
    // EN: Convert alert severity to string
    // FR: Convertir la sévérité d'alerte en chaîne
    std::string alertSeverityToString(ResourceAlertSeverity severity) const {
        switch (severity) {
            case ResourceAlertSeverity::DEBUG: return "DEBUG";
            case ResourceAlertSeverity::INFO: return "INFO";
            case ResourceAlertSeverity::WARNING: return "WARNING";
            case ResourceAlertSeverity::CRITICAL: return "CRITICAL";
            case ResourceAlertSeverity::EMERGENCY: return "EMERGENCY";
            default: return "UNKNOWN";
        }
    }
};

// EN: ResourceMonitor public interface implementation
// FR: Implémentation de l'interface publique ResourceMonitor

ResourceMonitor::ResourceMonitor(const ResourceMonitorConfig& config)
    : impl_(std::make_unique<ResourceMonitorImpl>(config)) {
}

ResourceMonitor::~ResourceMonitor() = default;

bool ResourceMonitor::start() {
    return impl_->start();
}

void ResourceMonitor::stop() {
    impl_->stop();
}

void ResourceMonitor::pause() {
    impl_->pause();
}

void ResourceMonitor::resume() {
    impl_->resume();
}

bool ResourceMonitor::isRunning() const {
    return impl_->isRunning();
}

bool ResourceMonitor::isPaused() const {
    return impl_->isPaused();
}

void ResourceMonitor::updateConfig(const ResourceMonitorConfig& config) {
    impl_->updateConfig(config);
}

ResourceMonitorConfig ResourceMonitor::getConfig() const {
    return impl_->getConfig();
}

void ResourceMonitor::addThreshold(const ResourceThreshold& threshold) {
    impl_->addThreshold(threshold);
}

bool ResourceMonitor::removeThreshold(ResourceType resource_type) {
    return impl_->removeThreshold(resource_type);
}

std::optional<ResourceUsage> ResourceMonitor::getCurrentUsage(ResourceType resource_type) const {
    return impl_->getCurrentUsage(resource_type);
}

std::map<ResourceType, ResourceUsage> ResourceMonitor::getAllCurrentUsage() const {
    return impl_->getAllCurrentUsage();
}

SystemResourceInfo ResourceMonitor::getSystemInfo() const {
    return impl_->getSystemInfo();
}

ResourceStatistics ResourceMonitor::getResourceStatistics(ResourceType resource_type, std::chrono::minutes period) const {
    return impl_->getResourceStatistics(resource_type, period);
}

std::vector<ResourceAlert> ResourceMonitor::getActiveAlerts() const {
    return impl_->getActiveAlerts();
}

bool ResourceMonitor::isThrottlingActive(ResourceType resource_type) const {
    return impl_->isThrottlingActive(resource_type);
}

double ResourceMonitor::getCurrentThrottlingFactor(ResourceType resource_type) const {
    return impl_->getCurrentThrottlingFactor(resource_type);
}

void ResourceMonitor::setAlertCallback(ResourceAlertCallback callback) {
    impl_->setAlertCallback(callback);
}

void ResourceMonitor::setResourceUpdateCallback(ResourceUpdateCallback callback) {
    impl_->setResourceUpdateCallback(callback);
}

void ResourceMonitor::setThrottlingCallback(ThrottlingCallback callback) {
    impl_->setThrottlingCallback(callback);
}

ResourceMonitor::MonitorPerformance ResourceMonitor::getMonitorPerformance() const {
    return impl_->getMonitorPerformance();
}

// EN: Static utility method implementations
// FR: Implémentations des méthodes utilitaires statiques

std::string ResourceMonitor::resourceTypeToString(ResourceType type) {
    switch (type) {
        case ResourceType::CPU: return "CPU";
        case ResourceType::MEMORY: return "Memory";
        case ResourceType::NETWORK: return "Network";
        case ResourceType::DISK: return "Disk";
        case ResourceType::PROCESS: return "Process";
        case ResourceType::SYSTEM: return "System";
        case ResourceType::CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

std::string ResourceMonitor::alertSeverityToString(ResourceAlertSeverity severity) {
    switch (severity) {
        case ResourceAlertSeverity::DEBUG: return "DEBUG";
        case ResourceAlertSeverity::INFO: return "INFO";
        case ResourceAlertSeverity::WARNING: return "WARNING";
        case ResourceAlertSeverity::CRITICAL: return "CRITICAL";
        case ResourceAlertSeverity::EMERGENCY: return "EMERGENCY";
        default: return "UNKNOWN";
    }
}

std::string ResourceMonitor::formatResourceValue(double value, ResourceUnit unit) {
    std::ostringstream oss;
    
    switch (unit) {
        case ResourceUnit::PERCENTAGE:
            oss << std::fixed << std::setprecision(1) << value << "%";
            break;
        case ResourceUnit::BYTES:
            oss << ResourceUtils::formatBytes(static_cast<size_t>(value));
            break;
        case ResourceUnit::BYTES_PER_SECOND:
            oss << ResourceUtils::formatBytesPerSecond(value);
            break;
        case ResourceUnit::MILLISECONDS:
            oss << std::fixed << std::setprecision(1) << value << "ms";
            break;
        default:
            oss << std::fixed << std::setprecision(2) << value;
            break;
    }
    
    return oss.str();
}

bool ResourceMonitor::isResourceCritical(double value, const ResourceThreshold& threshold) {
    return value >= threshold.critical_threshold;
}

// EN: Utility functions implementation
// FR: Implémentation des fonctions utilitaires

namespace ResourceUtils {

ResourceMonitorConfig createDefaultConfig() {
    ResourceMonitorConfig config;
    config.frequency = MonitoringFrequency::NORMAL;
    config.collection_interval = std::chrono::milliseconds(1000);
    config.history_size = 300;
    config.enable_system_monitoring = true;
    config.enable_process_monitoring = true;
    config.enable_network_monitoring = true;
    config.enable_disk_monitoring = true;
    config.enable_alerts = true;
    config.enable_throttling = true;
    config.enable_logging = true;
    config.thresholds = createDefaultThresholds();
    return config;
}

std::vector<ResourceThreshold> createDefaultThresholds() {
    std::vector<ResourceThreshold> thresholds;
    
    // EN: CPU threshold
    // FR: Seuil CPU
    thresholds.push_back(createCPUThreshold());
    
    // EN: Memory threshold
    // FR: Seuil mémoire
    thresholds.push_back(createMemoryThreshold());
    
    // EN: Network threshold (in bytes per second)
    // FR: Seuil réseau (en octets par seconde)
    thresholds.push_back(createNetworkThreshold());
    
    return thresholds;
}

ResourceThreshold createCPUThreshold(double warning, double critical) {
    ResourceThreshold threshold;
    threshold.resource_type = ResourceType::CPU;
    threshold.warning_threshold = warning;
    threshold.critical_threshold = critical;
    threshold.emergency_threshold = 98.0;
    threshold.duration_before_alert = std::chrono::seconds(30);
    threshold.enable_throttling = true;
    threshold.throttling_strategy = ThrottlingStrategy::ADAPTIVE;
    threshold.throttling_factor = 0.7;
    return threshold;
}

ResourceThreshold createMemoryThreshold(double warning, double critical) {
    ResourceThreshold threshold;
    threshold.resource_type = ResourceType::MEMORY;
    threshold.warning_threshold = warning;
    threshold.critical_threshold = critical;
    threshold.emergency_threshold = 98.0;
    threshold.duration_before_alert = std::chrono::seconds(20);
    threshold.enable_throttling = true;
    threshold.throttling_strategy = ThrottlingStrategy::AGGRESSIVE;
    threshold.throttling_factor = 0.5;
    return threshold;
}

ResourceThreshold createNetworkThreshold(double warning, double critical) {
    ResourceThreshold threshold;
    threshold.resource_type = ResourceType::NETWORK;
    threshold.warning_threshold = warning * 1024 * 1024; // Convert MB/s to bytes/s
    threshold.critical_threshold = critical * 1024 * 1024;
    threshold.emergency_threshold = 90.0 * 1024 * 1024;
    threshold.duration_before_alert = std::chrono::seconds(10);
    threshold.enable_throttling = true;
    threshold.throttling_strategy = ThrottlingStrategy::LINEAR;
    threshold.throttling_factor = 0.8;
    return threshold;
}

std::string formatBytes(size_t bytes) {
    const std::vector<std::string> units = {"B", "KB", "MB", "GB", "TB"};
    size_t unit_index = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_index < units.size() - 1) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit_index];
    return oss.str();
}

std::string formatBytesPerSecond(double bytes_per_second) {
    return formatBytes(static_cast<size_t>(bytes_per_second)) + "/s";
}

std::string formatPercentage(double percentage) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << percentage << "%";
    return oss.str();
}

double calculateMean(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    return sum / values.size();
}

double calculateMedian(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    
    std::sort(values.begin(), values.end());
    size_t size = values.size();
    
    if (size % 2 == 0) {
        return (values[size / 2 - 1] + values[size / 2]) / 2.0;
    } else {
        return values[size / 2];
    }
}

double calculateStandardDeviation(const std::vector<double>& values) {
    if (values.size() < 2) {
        return 0.0;
    }
    
    double mean = calculateMean(values);
    double sum_squared_diffs = 0.0;
    
    for (double value : values) {
        double diff = value - mean;
        sum_squared_diffs += diff * diff;
    }
    
    return std::sqrt(sum_squared_diffs / (values.size() - 1));
}

double calculatePercentile(std::vector<double> values, double percentile) {
    if (values.empty()) {
        return 0.0;
    }
    
    std::sort(values.begin(), values.end());
    size_t index = static_cast<size_t>((percentile / 100.0) * (values.size() - 1));
    return values[std::min(index, values.size() - 1)];
}

double calculateTrendSlope(const std::vector<std::pair<double, double>>& time_value_pairs) {
    if (time_value_pairs.size() < 2) {
        return 0.0;
    }
    
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    size_t n = time_value_pairs.size();
    
    for (const auto& [x, y] : time_value_pairs) {
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    double denominator = n * sum_x2 - sum_x * sum_x;
    if (std::abs(denominator) < 1e-9) {
        return 0.0;
    }
    
    return (n * sum_xy - sum_x * sum_y) / denominator;
}

} // namespace ResourceUtils

} // namespace Orchestrator
} // namespace BBP