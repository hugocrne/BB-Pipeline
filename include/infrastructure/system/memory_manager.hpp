// EN: Memory Manager for BB-Pipeline - Pool allocator optimized for massive CSV parsing
// FR: Gestionnaire mémoire pour BB-Pipeline - Pool allocator optimisé pour parsing CSV massif

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <type_traits>
#include <string>

namespace BBP {

// EN: Memory pool configuration structure
// FR: Structure de configuration du pool mémoire
struct MemoryPoolConfig {
    size_t initial_pool_size{1024 * 1024};      // EN: Initial pool size in bytes (1MB) / FR: Taille initiale du pool en octets (1MB)
    size_t max_pool_size{100 * 1024 * 1024};    // EN: Maximum pool size (100MB) / FR: Taille maximum du pool (100MB)
    size_t block_size{64};                       // EN: Default block size / FR: Taille de bloc par défaut
    size_t alignment{sizeof(void*)};             // EN: Memory alignment / FR: Alignement mémoire
    bool enable_statistics{true};                // EN: Enable detailed statistics / FR: Active les statistiques détaillées
    bool enable_defragmentation{true};           // EN: Enable automatic defragmentation / FR: Active la défragmentation automatique
    double growth_factor{2.0};                  // EN: Pool growth factor / FR: Facteur de croissance du pool
    double defrag_threshold{0.3};               // EN: Fragmentation threshold (30%) / FR: Seuil de fragmentation (30%)
};

// EN: Memory pool statistics for monitoring and optimization
// FR: Statistiques du pool mémoire pour monitoring et optimisation
struct MemoryPoolStats {
    std::chrono::system_clock::time_point created_at;
    size_t total_allocated_bytes{0};       // EN: Total bytes allocated / FR: Total d'octets alloués
    size_t total_freed_bytes{0};           // EN: Total bytes freed / FR: Total d'octets libérés
    size_t current_used_bytes{0};          // EN: Currently used bytes / FR: Octets actuellement utilisés
    size_t peak_used_bytes{0};             // EN: Peak memory usage / FR: Pic d'utilisation mémoire
    size_t total_allocations{0};           // EN: Total allocation count / FR: Nombre total d'allocations
    size_t total_deallocations{0};         // EN: Total deallocation count / FR: Nombre total de désallocations
    size_t pool_size{0};                   // EN: Current pool size / FR: Taille actuelle du pool
    size_t available_bytes{0};             // EN: Available bytes in pool / FR: Octets disponibles dans le pool
    double fragmentation_ratio{0.0};       // EN: Fragmentation ratio (0-1) / FR: Ratio de fragmentation (0-1)
    size_t defragmentation_count{0};       // EN: Number of defragmentations / FR: Nombre de défragmentations
    std::chrono::milliseconds total_alloc_time{0}; // EN: Total allocation time / FR: Temps total d'allocation
    std::chrono::milliseconds total_dealloc_time{0}; // EN: Total deallocation time / FR: Temps total de désallocation
    std::unordered_map<size_t, size_t> size_histogram; // EN: Allocation size distribution / FR: Distribution des tailles d'allocation
};

// EN: Memory block header for tracking allocations
// FR: En-tête de bloc mémoire pour le suivi des allocations
struct alignas(sizeof(void*)) BlockHeader {
    size_t size;                    // EN: Block size / FR: Taille du bloc
    bool is_free;                   // EN: Free flag / FR: Flag de liberté
    BlockHeader* next_free;         // EN: Next free block / FR: Prochain bloc libre
    uint32_t magic;                 // EN: Magic number for validation / FR: Nombre magique pour validation
    std::chrono::system_clock::time_point allocated_at; // EN: Allocation timestamp / FR: Timestamp d'allocation
    
    static constexpr uint32_t MAGIC_ALLOCATED = 0xDEADBEEF;
    static constexpr uint32_t MAGIC_FREE = 0xFEEDFACE;
};

// EN: Custom allocator for CSV parsing optimization
// FR: Allocateur personnalisé pour l'optimisation du parsing CSV
template<typename T>
class PoolAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template<typename U>
    struct rebind {
        using other = PoolAllocator<U>;
    };

    // EN: Constructor with memory manager reference
    // FR: Constructeur avec référence au gestionnaire mémoire
    explicit PoolAllocator(class MemoryManager& manager) noexcept : manager_(&manager) {}
    
    // EN: Copy constructor
    // FR: Constructeur de copie
    template<typename U>
    PoolAllocator(const PoolAllocator<U>& other) noexcept : manager_(other.manager_) {}
    
    // EN: Allocate memory
    // FR: Alloue la mémoire
    pointer allocate(size_type n);
    
    // EN: Deallocate memory
    // FR: Désalloue la mémoire
    void deallocate(pointer p, size_type n);
    
    // EN: Get maximum allocation size
    // FR: Obtient la taille maximum d'allocation
    size_type max_size() const noexcept {
        return std::numeric_limits<size_type>::max() / sizeof(T);
    }
    
    // EN: Comparison operators
    // FR: Opérateurs de comparaison
    template<typename U>
    bool operator==(const PoolAllocator<U>& other) const noexcept {
        return manager_ == other.manager_;
    }
    
    template<typename U>
    bool operator!=(const PoolAllocator<U>& other) const noexcept {
        return !(*this == other);
    }

private:
    class MemoryManager* manager_;
    
    template<typename U> friend class PoolAllocator;
};

// EN: High-performance memory manager with pool allocator for CSV processing
// FR: Gestionnaire mémoire haute performance avec pool allocator pour traitement CSV
class MemoryManager {
public:
    // EN: Get the singleton instance
    // FR: Obtient l'instance singleton
    static MemoryManager& getInstance();
    
    // EN: Configure the memory manager
    // FR: Configure le gestionnaire mémoire
    void configure(const MemoryPoolConfig& config);
    
    // EN: Initialize memory pools
    // FR: Initialise les pools mémoire
    void initialize();
    
    // EN: Allocate memory with specified size and alignment
    // FR: Alloue la mémoire avec taille et alignement spécifiés
    void* allocate(size_t size, size_t alignment = sizeof(void*));
    
    // EN: Deallocate previously allocated memory
    // FR: Désalloue la mémoire précédemment allouée
    void deallocate(void* ptr);
    
    // EN: Allocate memory for specific type with count
    // FR: Alloue la mémoire pour un type spécifique avec compteur
    template<typename T>
    T* allocate_array(size_t count) {
        size_t size = count * sizeof(T);
        void* ptr = allocate(size, alignof(T));
        return static_cast<T*>(ptr);
    }
    
    // EN: Deallocate array memory
    // FR: Désalloue la mémoire de tableau
    template<typename T>
    void deallocate_array(T* ptr) {
        deallocate(static_cast<void*>(ptr));
    }
    
    // EN: Get allocator for specific type
    // FR: Obtient l'allocateur pour un type spécifique
    template<typename T>
    PoolAllocator<T> get_allocator() {
        return PoolAllocator<T>(*this);
    }
    
    // EN: Force defragmentation of memory pools
    // FR: Force la défragmentation des pools mémoire
    void defragment();
    
    // EN: Reset all memory pools (for testing)
    // FR: Remet à zéro tous les pools mémoire (pour les tests)
    void reset();
    
    // EN: Get current memory statistics
    // FR: Obtient les statistiques mémoire actuelles
    MemoryPoolStats getStats() const;
    
    // EN: Check memory pool integrity
    // FR: Vérifie l'intégrité du pool mémoire
    bool checkIntegrity() const;
    
    // EN: Optimize memory pools based on usage patterns
    // FR: Optimise les pools mémoire basés sur les patterns d'usage
    void optimize();
    
    // EN: Set memory usage limits
    // FR: Définit les limites d'utilisation mémoire
    void setMemoryLimit(size_t max_memory_bytes);
    
    // EN: Get current memory usage
    // FR: Obtient l'utilisation mémoire actuelle
    size_t getCurrentUsage() const;
    
    // EN: Enable/disable detailed tracking (for debugging)
    // FR: Active/désactive le suivi détaillé (pour debugging)
    void setDetailedTracking(bool enabled);
    
    // EN: Dump memory pool state (for debugging)
    // FR: Dump de l'état du pool mémoire (pour debugging)
    std::string dumpPoolState() const;
    
    // EN: Destructor
    // FR: Destructeur
    ~MemoryManager();

private:
    // EN: Private constructor for singleton
    // FR: Constructeur privé pour singleton
    MemoryManager();
    
    // EN: Non-copyable and non-movable
    // FR: Non-copiable et non-déplaçable
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;
    MemoryManager(MemoryManager&&) = delete;
    MemoryManager& operator=(MemoryManager&&) = delete;
    
    // EN: Internal allocation with tracking
    // FR: Allocation interne avec suivi
    void* allocate_internal(size_t size, size_t alignment);
    
    // EN: Internal deallocation with tracking
    // FR: Désallocation interne avec suivi
    void deallocate_internal(void* ptr);
    
    // EN: Expand memory pool when needed
    // FR: Étend le pool mémoire quand nécessaire
    bool expand_pool(size_t required_size);
    
    // EN: Find free block with best fit algorithm
    // FR: Trouve un bloc libre avec algorithme best fit
    BlockHeader* find_best_fit_block(size_t size, size_t alignment);
    
    // EN: Split block if too large
    // FR: Divise le bloc s'il est trop grand
    void split_block(BlockHeader* block, size_t size);
    
    // EN: Merge adjacent free blocks
    // FR: Fusionne les blocs libres adjacents
    void merge_free_blocks();
    
    // EN: Calculate fragmentation ratio
    // FR: Calcule le ratio de fragmentation
    double calculate_fragmentation() const;
    
    // EN: Update statistics
    // FR: Met à jour les statistiques
    void update_stats(size_t size, bool is_allocation, std::chrono::microseconds duration);
    
    // EN: Align size to specified boundary
    // FR: Aligne la taille sur la frontière spécifiée
    static size_t align_size(size_t size, size_t alignment);
    
    // EN: Validate block header
    // FR: Valide l'en-tête du bloc
    bool validate_block_header(const BlockHeader* header) const;

private:
    mutable std::mutex mutex_;                    // EN: Thread-safety mutex / FR: Mutex pour thread-safety
    MemoryPoolConfig config_;                     // EN: Configuration / FR: Configuration
    MemoryPoolStats stats_;                       // EN: Statistics / FR: Statistiques
    
    std::vector<std::unique_ptr<char[]>> pools_;  // EN: Memory pools / FR: Pools mémoire
    BlockHeader* free_list_head_;                 // EN: Free blocks list / FR: Liste des blocs libres
    
    std::atomic<bool> initialized_{false};        // EN: Initialization flag / FR: Flag d'initialisation
    std::atomic<size_t> memory_limit_{SIZE_MAX};  // EN: Memory usage limit / FR: Limite d'utilisation mémoire
    std::atomic<bool> detailed_tracking_{false};  // EN: Detailed tracking flag / FR: Flag de suivi détaillé
    
    std::chrono::system_clock::time_point start_time_; // EN: Manager creation time / FR: Temps de création du gestionnaire
    
    // EN: Block tracking for detailed analysis
    // FR: Suivi des blocs pour analyse détaillée
    std::unordered_map<void*, BlockHeader*> allocated_blocks_;
    
    static MemoryManager* instance_;              // EN: Singleton instance / FR: Instance singleton
};

// EN: RAII wrapper for memory management
// FR: Wrapper RAII pour la gestion mémoire
template<typename T>
class ManagedPtr {
public:
    // EN: Constructor with memory manager
    // FR: Constructeur avec gestionnaire mémoire
    explicit ManagedPtr(MemoryManager& manager, size_t count = 1) 
        : manager_(&manager), ptr_(nullptr), count_(count) {
        if (count > 0) {
            ptr_ = manager_->allocate_array<T>(count);
        }
    }
    
    // EN: Move constructor
    // FR: Constructeur de déplacement
    ManagedPtr(ManagedPtr&& other) noexcept 
        : manager_(other.manager_), ptr_(other.ptr_), count_(other.count_) {
        other.ptr_ = nullptr;
        other.count_ = 0;
    }
    
    // EN: Move assignment
    // FR: Affectation de déplacement
    ManagedPtr& operator=(ManagedPtr&& other) noexcept {
        if (this != &other) {
            reset();
            manager_ = other.manager_;
            ptr_ = other.ptr_;
            count_ = other.count_;
            other.ptr_ = nullptr;
            other.count_ = 0;
        }
        return *this;
    }
    
    // EN: Destructor
    // FR: Destructeur
    ~ManagedPtr() {
        reset();
    }
    
    // EN: Get raw pointer
    // FR: Obtient le pointeur brut
    T* get() const noexcept { return ptr_; }
    
    // EN: Get count
    // FR: Obtient le compteur
    size_t count() const noexcept { return count_; }
    
    // EN: Array access operator
    // FR: Opérateur d'accès tableau
    T& operator[](size_t index) const {
        return ptr_[index];
    }
    
    // EN: Boolean conversion
    // FR: Conversion booléenne
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    // EN: Reset managed pointer
    // FR: Remet à zéro le pointeur géré
    void reset() {
        if (ptr_) {
            manager_->deallocate_array(ptr_);
            ptr_ = nullptr;
            count_ = 0;
        }
    }

private:
    // EN: Non-copyable
    // FR: Non-copiable
    ManagedPtr(const ManagedPtr&) = delete;
    ManagedPtr& operator=(const ManagedPtr&) = delete;
    
    MemoryManager* manager_;
    T* ptr_;
    size_t count_;
};

} // namespace BBP