// EN: Implementation of the ThreadPool class. Provides high-performance thread pool with priority queue and auto-scaling.
// FR: Implémentation de la classe ThreadPool. Fournit un pool de threads haute performance avec queue prioritaire et auto-scaling.

#include "../../../include/infrastructure/threading/thread_pool.hpp"
#include "../../../include/infrastructure/logging/logger.hpp"
#include <algorithm>
#include <stdexcept>

#define LOG_DEBUG(module, message) BBP::Logger::getInstance().debug(module, message)
#define LOG_INFO(module, message) BBP::Logger::getInstance().info(module, message)
#define LOG_WARN(module, message) BBP::Logger::getInstance().warn(module, message)
#define LOG_ERROR(module, message) BBP::Logger::getInstance().error(module, message)

namespace BBP {

// EN: Constructor with optional configuration.
// FR: Constructeur avec configuration optionnelle.
ThreadPool::ThreadPool(const ThreadPoolConfig& config) 
    : config_(config), start_time_(std::chrono::system_clock::now()) {
    
    // EN: Initialize statistics.
    // FR: Initialise les statistiques.
    stats_.created_at = start_time_;
    stats_.total_threads = 0;
    
    // EN: Validate configuration.
    // FR: Valide la configuration.
    if (config_.initial_threads > config_.max_threads) {
        throw std::invalid_argument("initial_threads cannot be greater than max_threads");
    }
    if (config_.min_threads > config_.max_threads) {
        throw std::invalid_argument("min_threads cannot be greater than max_threads");
    }
    if (config_.min_threads == 0) {
        throw std::invalid_argument("min_threads must be at least 1");
    }
    
    // EN: Create initial worker threads.
    // FR: Crée les threads workers initiaux.
    {
        std::lock_guard<std::mutex> lock(threads_mutex_);
        for (size_t i = 0; i < config_.initial_threads; ++i) {
            workers_.emplace_back(std::make_unique<std::thread>(&ThreadPool::workerLoop, this));
            stats_.total_threads++;
        }
    }
    
    // EN: Start auto-scaling thread if enabled.
    // FR: Démarre le thread d'auto-scaling si activé.
    if (config_.enable_auto_scaling) {
        scaling_thread_ = std::make_unique<std::thread>(&ThreadPool::scalingLoop, this);
    }
    
    LOG_INFO("threadpool", "Thread pool started with " + std::to_string(config_.initial_threads) + " threads");
}

// EN: Destructor - waits for all tasks to complete and stops all threads.
// FR: Destructeur - attend que toutes les tâches se terminent et arrête tous les threads.
ThreadPool::~ThreadPool() {
    shutdown();
}

// EN: Wait for all currently queued tasks to complete.
// FR: Attend que toutes les tâches actuellement en queue se terminent.
void ThreadPool::waitForAll() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_condition_.wait(lock, [this] { 
        return task_queue_.empty() && active_threads_.load() == 0; 
    });
}

// EN: Shutdown the thread pool gracefully.
// FR: Arrête le pool de threads de manière gracieuse.
void ThreadPool::shutdown() {
    if (shutdown_requested_.exchange(true)) {
        return; // EN: Already shutting down. FR: Déjà en cours d'arrêt.
    }
    
    LOG_INFO("threadpool", "Initiating graceful shutdown");
    
    // EN: Wait for all tasks to complete.
    // FR: Attend que toutes les tâches se terminent.
    waitForAll();
    
    // EN: Notify all workers to stop.
    // FR: Notifie tous les workers de s'arrêter.
    queue_condition_.notify_all();
    
    // EN: Stop scaling thread first.
    // FR: Arrête d'abord le thread de scaling.
    if (scaling_thread_ && scaling_thread_->joinable()) {
        scaling_thread_->join();
    }
    
    // EN: Join all worker threads.
    // FR: Joint tous les threads workers.
    {
        std::lock_guard<std::mutex> lock(threads_mutex_);
        for (auto& worker : workers_) {
            if (worker && worker->joinable()) {
                worker->join();
            }
        }
        workers_.clear();
    }
    
    LOG_INFO("threadpool", "Thread pool shutdown completed - Processed " + 
             std::to_string(completed_tasks_.load()) + " tasks");
}

// EN: Force shutdown immediately, cancelling pending tasks.
// FR: Force l'arrêt immédiatement, annulant les tâches en attente.
void ThreadPool::forceShutdown() {
    if (shutdown_requested_.exchange(true)) {
        return;
    }
    
    force_shutdown_.store(true);
    LOG_WARN("threadpool", "Forcing immediate shutdown - " + 
             std::to_string(task_queue_.size()) + " tasks will be cancelled");
    
    // EN: Clear pending tasks.
    // FR: Efface les tâches en attente.
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        // EN: Create empty queue to clear current one.
        // FR: Crée une queue vide pour effacer l'actuelle.
        std::priority_queue<detail::Task> empty;
        task_queue_.swap(empty);
    }
    
    queue_condition_.notify_all();
    
    // EN: Join threads immediately.
    // FR: Joint les threads immédiatement.
    if (scaling_thread_ && scaling_thread_->joinable()) {
        scaling_thread_->join();
    }
    
    {
        std::lock_guard<std::mutex> lock(threads_mutex_);
        for (auto& worker : workers_) {
            if (worker && worker->joinable()) {
                worker->join();
            }
        }
        workers_.clear();
    }
    
    LOG_INFO("threadpool", "Forced shutdown completed");
}

// EN: Pause task execution (threads remain alive).
// FR: Met en pause l'exécution des tâches (les threads restent vivants).
void ThreadPool::pause() {
    paused_.store(true);
    LOG_INFO("threadpool", "Thread pool paused");
}

// EN: Resume task execution.
// FR: Reprend l'exécution des tâches.
void ThreadPool::resume() {
    paused_.store(false);
    queue_condition_.notify_all();
    LOG_INFO("threadpool", "Thread pool resumed");
}

// EN: Check if the thread pool is currently paused.
// FR: Vérifie si le pool de threads est actuellement en pause.
bool ThreadPool::isPaused() const {
    return paused_.load();
}

// EN: Get current thread pool statistics.
// FR: Obtient les statistiques actuelles du pool de threads.
ThreadPoolStats ThreadPool::getStats() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    ThreadPoolStats current_stats = stats_;
    current_stats.total_threads = workers_.size();
    current_stats.active_threads = active_threads_.load();
    current_stats.idle_threads = current_stats.total_threads - current_stats.active_threads;
    current_stats.queued_tasks = task_queue_.size();
    current_stats.completed_tasks = completed_tasks_.load();
    current_stats.failed_tasks = failed_tasks_.load();
    current_stats.peak_queue_size = peak_queue_size_.load();
    
    auto now = std::chrono::system_clock::now();
    current_stats.total_runtime = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
    
    return current_stats;
}

// EN: Update configuration dynamically.
// FR: Met à jour la configuration dynamiquement.
void ThreadPool::updateConfig(const ThreadPoolConfig& config) {
    // EN: Validate new configuration.
    // FR: Valide la nouvelle configuration.
    if (config.initial_threads > config.max_threads || 
        config.min_threads > config.max_threads || 
        config.min_threads == 0) {
        throw std::invalid_argument("Invalid thread pool configuration");
    }
    
    {
        std::lock_guard<std::mutex> lock(threads_mutex_);
        config_ = config;
    }
    
    LOG_INFO("threadpool", "Thread pool configuration updated");
}

// EN: Set callback for task completion events.
// FR: Définit le callback pour les événements de completion des tâches.
void ThreadPool::setTaskCallback(std::function<void(const std::string&, bool, std::chrono::milliseconds)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    task_callback_ = std::move(callback);
}

// EN: Worker thread function.
// FR: Fonction du thread worker.
void ThreadPool::workerLoop() {
    LOG_DEBUG("threadpool", "Worker thread started");
    
    while (!shutdown_requested_.load()) {
        detail::Task task([]{}, TaskPriority::NORMAL);
        bool has_task = false;
        
        // EN: Wait for task or shutdown signal.
        // FR: Attend une tâche ou signal d'arrêt.
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_condition_.wait(lock, [this] {
                return !task_queue_.empty() || shutdown_requested_.load() || (!paused_.load() && !task_queue_.empty());
            });
            
            if (shutdown_requested_.load() && task_queue_.empty()) {
                break;
            }
            
            if (paused_.load()) {
                continue;
            }
            
            if (!task_queue_.empty()) {
                task = std::move(const_cast<detail::Task&>(task_queue_.top()));
                task_queue_.pop();
                has_task = true;
            }
        }
        
        if (!has_task) {
            continue;
        }
        
        // EN: Execute task and measure duration.
        // FR: Exécute la tâche et mesure la durée.
        active_threads_++;
        auto start = std::chrono::system_clock::now();
        bool success = true;
        
        try {
            // EN: Check if task has timeout.
            // FR: Vérifie si la tâche a un timeout.
            if (config_.enable_task_timeout && task.timeout.count() > 0) {
                // EN: For simplicity, execute without actual timeout enforcement.
                // FR: Pour simplifier, exécute sans vraie application de timeout.
                // TODO: Implement proper timeout mechanism with std::async or similar
                task.function();
            } else {
                task.function();
            }
        } catch (const std::exception& e) {
            success = false;
            failed_tasks_++;
            LOG_ERROR("threadpool", "Task execution failed: " + std::string(e.what()));
        } catch (...) {
            success = false;
            failed_tasks_++;
            LOG_ERROR("threadpool", "Task execution failed with unknown exception");
        }
        
        auto end = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (success) {
            completed_tasks_++;
        }
        active_threads_--;
        
        // EN: Update statistics and trigger callback.
        // FR: Met à jour les statistiques et déclenche le callback.
        updateStats(task.name, success, duration);
        
        // EN: Notify waiting threads that a task is complete.
        // FR: Notifie les threads en attente qu'une tâche est terminée.
        queue_condition_.notify_all();
    }
    
    LOG_DEBUG("threadpool", "Worker thread stopped");
}

// EN: Auto-scaling manager thread function.
// FR: Fonction du thread gestionnaire d'auto-scaling.
void ThreadPool::scalingLoop() {
    LOG_DEBUG("threadpool", "Auto-scaling thread started");
    
    while (!shutdown_requested_.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5)); // EN: Check every 5 seconds. FR: Vérifie toutes les 5 secondes.
        
        if (shutdown_requested_.load()) {
            break;
        }
        
        double load = calculateLoad();
        
        // EN: Scale up if high load and can add more threads.
        // FR: Scale up si charge élevée et peut ajouter plus de threads.
        if (load > 0.8 && workers_.size() < config_.max_threads) {
            scaleUp();
        }
        // EN: Scale down if low load and can remove threads.
        // FR: Scale down si charge faible et peut supprimer des threads.
        else if (load < 0.2 && workers_.size() > config_.min_threads) {
            scaleDown();
        }
        
        cleanupThreads();
    }
    
    LOG_DEBUG("threadpool", "Auto-scaling thread stopped");
}

// EN: Add new worker threads if needed.
// FR: Ajoute de nouveaux threads workers si nécessaire.
void ThreadPool::scaleUp() {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    
    if (workers_.size() >= config_.max_threads) {
        return;
    }
    
    size_t threads_to_add = std::min(size_t(2), config_.max_threads - workers_.size());
    
    for (size_t i = 0; i < threads_to_add; ++i) {
        workers_.emplace_back(std::make_unique<std::thread>(&ThreadPool::workerLoop, this));
        stats_.total_threads++;
    }
    
    LOG_INFO("threadpool", "Scaled up: added " + std::to_string(threads_to_add) + 
             " threads (total: " + std::to_string(workers_.size()) + ")");
}

// EN: Remove idle worker threads if possible.
// FR: Supprime les threads workers inactifs si possible.
void ThreadPool::scaleDown() {
    // EN: For simplicity, we don't implement actual thread termination here.
    // FR: Pour simplifier, on n'implémente pas la vraie terminaison de threads ici.
    // EN: In a production system, this would involve signaling specific threads to stop.
    // FR: Dans un système de production, cela impliquerait de signaler à des threads spécifiques de s'arrêter.
    
    std::lock_guard<std::mutex> lock(threads_mutex_);
    size_t target_size = std::max(config_.min_threads, workers_.size() - 1);
    
    if (workers_.size() > target_size) {
        LOG_DEBUG("threadpool", "Scale down requested but not implemented - would reduce to " + 
                  std::to_string(target_size) + " threads");
    }
}

// EN: Clean up finished threads.
// FR: Nettoie les threads terminés.
void ThreadPool::cleanupThreads() {
    std::lock_guard<std::mutex> lock(threads_mutex_);
    
    // EN: Remove any threads that have finished (shouldn't happen in normal operation).
    // FR: Supprime tous threads qui ont terminé (ne devrait pas arriver en fonctionnement normal).
    workers_.erase(
        std::remove_if(workers_.begin(), workers_.end(),
            [](const std::unique_ptr<std::thread>& t) {
                return t && !t->joinable();
            }),
        workers_.end()
    );
}

// EN: Calculate current load metrics.
// FR: Calcule les métriques de charge actuelles.
double ThreadPool::calculateLoad() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    size_t total_threads = workers_.size();
    size_t active = active_threads_.load();
    size_t queued = task_queue_.size();
    
    if (total_threads == 0) {
        return 0.0;
    }
    
    // EN: Load is combination of thread utilization and queue pressure.
    // FR: La charge est une combinaison de l'utilisation des threads et de la pression de la queue.
    double thread_utilization = static_cast<double>(active) / total_threads;
    double queue_pressure = std::min(1.0, static_cast<double>(queued) / (total_threads * 2));
    
    return std::max(thread_utilization, queue_pressure);
}

// EN: Update statistics.
// FR: Met à jour les statistiques.
void ThreadPool::updateStats(const std::string& task_name, bool success, std::chrono::milliseconds duration) {
    // EN: Update average task duration using simple moving average.
    // FR: Met à jour la durée moyenne des tâches en utilisant une moyenne mobile simple.
    static std::mutex stats_mutex;
    static size_t task_count = 0;
    static double total_duration = 0.0;
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex);
        task_count++;
        total_duration += duration.count();
        stats_.average_task_duration_ms = total_duration / task_count;
    }
    
    // EN: Trigger callback if set.
    // FR: Déclenche le callback s'il est défini.
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (task_callback_) {
            try {
                task_callback_(task_name, success, duration);
            } catch (const std::exception& e) {
                LOG_ERROR("threadpool", "Task callback failed: " + std::string(e.what()));
            }
        }
    }
}

} // namespace BBP