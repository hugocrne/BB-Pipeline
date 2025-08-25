#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>

namespace BBP {

// EN: Task priority levels for the thread pool queue.
// FR: Niveaux de priorité des tâches pour la queue du pool de threads.
enum class TaskPriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    URGENT = 3
};

// EN: Thread pool statistics for monitoring and optimization.
// FR: Statistiques du pool de threads pour le monitoring et l'optimisation.
struct ThreadPoolStats {
    size_t total_threads = 0;
    size_t active_threads = 0;
    size_t idle_threads = 0;
    size_t queued_tasks = 0;
    size_t completed_tasks = 0;
    size_t failed_tasks = 0;
    double average_task_duration_ms = 0.0;
    size_t peak_queue_size = 0;
    std::chrono::system_clock::time_point created_at;
    std::chrono::milliseconds total_runtime{0};
};

// EN: Configuration for thread pool behavior and limits.
// FR: Configuration pour le comportement et les limites du pool de threads.
struct ThreadPoolConfig {
    // EN: Initial number of worker threads.
    // FR: Nombre initial de threads workers.
    size_t initial_threads = std::thread::hardware_concurrency();
    
    // EN: Maximum number of threads that can be created.
    // FR: Nombre maximum de threads qui peuvent être créés.
    size_t max_threads = std::thread::hardware_concurrency() * 2;
    
    // EN: Minimum number of threads to keep alive.
    // FR: Nombre minimum de threads à maintenir vivants.
    size_t min_threads = 1;
    
    // EN: Maximum number of tasks in the queue before blocking.
    // FR: Nombre maximum de tâches dans la queue avant blocage.
    size_t max_queue_size = 1000;
    
    // EN: Time to wait before terminating idle threads.
    // FR: Temps d'attente avant de terminer les threads inactifs.
    std::chrono::seconds idle_timeout{60};
    
    // EN: Enable automatic scaling based on load.
    // FR: Active la mise à l'échelle automatique basée sur la charge.
    bool enable_auto_scaling = true;
    
    // EN: Enable task timeout and cancellation.
    // FR: Active le timeout et l'annulation des tâches.
    bool enable_task_timeout = false;
    
    // EN: Default timeout for tasks if enabled.
    // FR: Timeout par défaut pour les tâches si activé.
    std::chrono::seconds default_task_timeout{30};
};

namespace detail {
    // EN: Internal task wrapper with priority and metadata.
    // FR: Wrapper interne de tâche avec priorité et métadonnées.
    struct Task {
        std::function<void()> function;
        TaskPriority priority;
        std::chrono::system_clock::time_point created_at;
        std::chrono::seconds timeout{0};
        std::string name;
        
        Task(std::function<void()> f, TaskPriority p, const std::string& n = "")
            : function(std::move(f)), priority(p), created_at(std::chrono::system_clock::now()), name(n) {}
            
        bool operator<(const Task& other) const {
            // EN: Higher priority values have higher precedence (reversed for priority_queue).
            // FR: Les valeurs de priorité plus élevées ont une précédence plus élevée (inversé pour priority_queue).
            return static_cast<int>(priority) < static_cast<int>(other.priority);
        }
    };
}

// EN: High-performance thread pool with priority queue and automatic scaling.
// FR: Pool de threads haute performance avec queue prioritaire et mise à l'échelle automatique.
class ThreadPool {
public:
    // EN: Constructor with optional configuration.
    // FR: Constructeur avec configuration optionnelle.
    explicit ThreadPool(const ThreadPoolConfig& config = ThreadPoolConfig{});
    
    // EN: Destructor - waits for all tasks to complete and stops all threads.
    // FR: Destructeur - attend que toutes les tâches se terminent et arrête tous les threads.
    ~ThreadPool();
    
    // EN: Non-copyable and non-movable.
    // FR: Non-copiable et non-déplaçable.
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
    
    // EN: Submit a task with specified priority and return a future.
    // FR: Soumet une tâche avec priorité spécifiée et retourne un future.
    template<typename F, typename... Args>
    auto submit(TaskPriority priority, F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>;
    
    // EN: Submit a task with normal priority (convenience method).
    // FR: Soumet une tâche avec priorité normale (méthode de convenance).
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>;
    
    // EN: Submit a named task with priority for better debugging.
    // FR: Soumet une tâche nommée avec priorité pour un meilleur débogage.
    template<typename F, typename... Args>
    auto submitNamed(const std::string& name, TaskPriority priority, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>;
    
    // EN: Submit a task with timeout.
    // FR: Soumet une tâche avec timeout.
    template<typename F, typename... Args>
    auto submitWithTimeout(TaskPriority priority, std::chrono::seconds timeout, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type>;
    
    // EN: Wait for all currently queued tasks to complete.
    // FR: Attend que toutes les tâches actuellement en queue se terminent.
    void waitForAll();
    
    // EN: Shutdown the thread pool gracefully.
    // FR: Arrête le pool de threads de manière gracieuse.
    void shutdown();
    
    // EN: Force shutdown immediately, cancelling pending tasks.
    // FR: Force l'arrêt immédiatement, annulant les tâches en attente.
    void forceShutdown();
    
    // EN: Pause task execution (threads remain alive).
    // FR: Met en pause l'exécution des tâches (les threads restent vivants).
    void pause();
    
    // EN: Resume task execution.
    // FR: Reprend l'exécution des tâches.
    void resume();
    
    // EN: Check if the thread pool is currently paused.
    // FR: Vérifie si le pool de threads est actuellement en pause.
    bool isPaused() const;
    
    // EN: Get current thread pool statistics.
    // FR: Obtient les statistiques actuelles du pool de threads.
    ThreadPoolStats getStats() const;
    
    // EN: Get current configuration.
    // FR: Obtient la configuration actuelle.
    const ThreadPoolConfig& getConfig() const { return config_; }
    
    // EN: Update configuration dynamically (some changes require restart).
    // FR: Met à jour la configuration dynamiquement (certains changements nécessitent un redémarrage).
    void updateConfig(const ThreadPoolConfig& config);
    
    // EN: Set callback for task completion events.
    // FR: Définit le callback pour les événements de completion des tâches.
    void setTaskCallback(std::function<void(const std::string&, bool, std::chrono::milliseconds)> callback);

private:
    // EN: Worker thread function.
    // FR: Fonction du thread worker.
    void workerLoop();
    
    // EN: Auto-scaling manager thread function.
    // FR: Fonction du thread gestionnaire d'auto-scaling.
    void scalingLoop();
    
    // EN: Add new worker threads if needed.
    // FR: Ajoute de nouveaux threads workers si nécessaire.
    void scaleUp();
    
    // EN: Remove idle worker threads if possible.
    // FR: Supprime les threads workers inactifs si possible.
    void scaleDown();
    
    // EN: Clean up finished threads.
    // FR: Nettoie les threads terminés.
    void cleanupThreads();
    
    // EN: Calculate current load metrics.
    // FR: Calcule les métriques de charge actuelles.
    double calculateLoad() const;
    
    // EN: Update statistics.
    // FR: Met à jour les statistiques.
    void updateStats(const std::string& task_name, bool success, std::chrono::milliseconds duration);
    
    ThreadPoolConfig config_;
    
    // EN: Thread management.
    // FR: Gestion des threads.
    std::vector<std::unique_ptr<std::thread>> workers_;
    std::unique_ptr<std::thread> scaling_thread_;
    mutable std::mutex threads_mutex_;
    
    // EN: Task queue with priority.
    // FR: Queue de tâches avec priorité.
    std::priority_queue<detail::Task> task_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    
    // EN: State management.
    // FR: Gestion d'état.
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<bool> force_shutdown_{false};
    std::atomic<bool> paused_{false};
    std::atomic<size_t> active_threads_{0};
    std::atomic<size_t> completed_tasks_{0};
    std::atomic<size_t> failed_tasks_{0};
    
    // EN: Statistics and monitoring.
    // FR: Statistiques et monitoring.
    mutable ThreadPoolStats stats_;
    std::chrono::system_clock::time_point start_time_;
    std::atomic<size_t> peak_queue_size_{0};
    
    // EN: Task completion callback.
    // FR: Callback de completion des tâches.
    std::function<void(const std::string&, bool, std::chrono::milliseconds)> task_callback_;
    mutable std::mutex callback_mutex_;
};

// EN: Template method implementations.
// FR: Implémentations des méthodes templates.
template<typename F, typename... Args>
auto ThreadPool::submit(TaskPriority priority, F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    return submitNamed("", priority, std::forward<F>(f), std::forward<Args>(args)...);
}

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    return submit(TaskPriority::NORMAL, std::forward<F>(f), std::forward<Args>(args)...);
}

template<typename F, typename... Args>
auto ThreadPool::submitNamed(const std::string& name, TaskPriority priority, F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    using return_type = typename std::invoke_result<F, Args...>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        if (shutdown_requested_) {
            throw std::runtime_error("ThreadPool is shutting down, cannot accept new tasks");
        }
        
        if (config_.max_queue_size > 0 && task_queue_.size() >= config_.max_queue_size) {
            throw std::runtime_error("Task queue is full, cannot accept new tasks");
        }
        
        task_queue_.emplace([task, name](){ 
            (*task)(); 
        }, priority, name);
        
        // EN: Update peak queue size.
        // FR: Met à jour la taille maximale de la queue.
        size_t current_size = task_queue_.size();
        size_t current_peak = peak_queue_size_.load();
        while (current_size > current_peak && 
               !peak_queue_size_.compare_exchange_weak(current_peak, current_size)) {
            current_peak = peak_queue_size_.load();
        }
    }
    
    queue_condition_.notify_one();
    return result;
}

template<typename F, typename... Args>
auto ThreadPool::submitWithTimeout(TaskPriority priority, std::chrono::seconds timeout, F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type> {
    using return_type = typename std::invoke_result<F, Args...>::type;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        if (shutdown_requested_) {
            throw std::runtime_error("ThreadPool is shutting down, cannot accept new tasks");
        }
        
        if (config_.max_queue_size > 0 && task_queue_.size() >= config_.max_queue_size) {
            throw std::runtime_error("Task queue is full, cannot accept new tasks");
        }
        
        detail::Task wrapper_task([task](){ 
            (*task)(); 
        }, priority);
        wrapper_task.timeout = timeout;
        
        task_queue_.push(std::move(wrapper_task));
        
        // EN: Update peak queue size.
        // FR: Met à jour la taille maximale de la queue.
        size_t current_size = task_queue_.size();
        size_t current_peak = peak_queue_size_.load();
        while (current_size > current_peak && 
               !peak_queue_size_.compare_exchange_weak(current_peak, current_size)) {
            current_peak = peak_queue_size_.load();
        }
    }
    
    queue_condition_.notify_one();
    return result;
}

} // namespace BBP