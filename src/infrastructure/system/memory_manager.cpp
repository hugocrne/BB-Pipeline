// EN: Implementation of the MemoryManager class. Provides pool allocator optimized for massive CSV parsing.
// FR: Implémentation de la classe MemoryManager. Fournit un pool allocator optimisé pour parsing CSV massif.

#include "../../../include/infrastructure/system/memory_manager.hpp"
#include "../../../include/infrastructure/logging/logger.hpp"
#include <algorithm>
#include <cstring>
#include <cassert>
#include <sstream>
#include <iomanip>

#define LOG_DEBUG(module, message) BBP::Logger::getInstance().debug(module, message)
#define LOG_INFO(module, message) BBP::Logger::getInstance().info(module, message)
#define LOG_WARN(module, message) BBP::Logger::getInstance().warn(module, message)
#define LOG_ERROR(module, message) BBP::Logger::getInstance().error(module, message)

namespace BBP {

// EN: Static instance pointer for singleton
// FR: Pointeur d'instance statique pour le singleton
MemoryManager* MemoryManager::instance_ = nullptr;

// EN: Get the singleton memory manager instance.
// FR: Obtient l'instance singleton du gestionnaire mémoire.
MemoryManager& MemoryManager::getInstance() {
    static MemoryManager instance;
    instance_ = &instance;
    return instance;
}

// EN: Private constructor - initializes default configuration and statistics.
// FR: Constructeur privé - initialise la configuration et statistiques par défaut.
MemoryManager::MemoryManager() 
    : free_list_head_(nullptr), start_time_(std::chrono::system_clock::now()) {
    
    // EN: Initialize default configuration
    // FR: Initialise la configuration par défaut
    config_ = MemoryPoolConfig{};
    
    // EN: Initialize statistics
    // FR: Initialise les statistiques
    stats_.created_at = start_time_;
    
    LOG_DEBUG("memory_manager", "MemoryManager instance created");
}

// EN: Destructor - ensures proper cleanup of all memory pools.
// FR: Destructeur - assure un nettoyage approprié de tous les pools mémoire.
MemoryManager::~MemoryManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // EN: Log final statistics before cleanup
    // FR: Log des statistiques finales avant nettoyage
    LOG_INFO("memory_manager", "MemoryManager shutting down - Peak usage: " + 
             std::to_string(stats_.peak_used_bytes) + " bytes, " +
             "Total allocations: " + std::to_string(stats_.total_allocations));
    
    // EN: Check for memory leaks
    // FR: Vérifie les fuites mémoire
    if (stats_.current_used_bytes > 0) {
        LOG_WARN("memory_manager", "Memory leak detected: " + 
                 std::to_string(stats_.current_used_bytes) + " bytes not freed");
    }
    
    // EN: Pools are automatically freed by unique_ptr destructors
    // FR: Les pools sont automatiquement libérés par les destructeurs unique_ptr
    pools_.clear();
    free_list_head_ = nullptr;
    allocated_blocks_.clear();
}

// EN: Configure the memory manager with custom settings.
// FR: Configure le gestionnaire mémoire avec des paramètres personnalisés.
void MemoryManager::configure(const MemoryPoolConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_.load()) {
        LOG_WARN("memory_manager", "Cannot configure after initialization");
        return;
    }
    
    config_ = config;
    
    LOG_INFO("memory_manager", "MemoryManager configured - Initial pool size: " + 
             std::to_string(config_.initial_pool_size) + " bytes, Max pool size: " +
             std::to_string(config_.max_pool_size) + " bytes, Block size: " +
             std::to_string(config_.block_size) + " bytes");
}

// EN: Initialize memory pools by creating the initial pool.
// FR: Initialise les pools mémoire en créant le pool initial.
void MemoryManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_.exchange(true)) {
        LOG_WARN("memory_manager", "MemoryManager already initialized");
        return;
    }
    
    // EN: Create initial memory pool
    // FR: Crée le pool mémoire initial
    if (!expand_pool(config_.initial_pool_size)) {
        initialized_ = false;
        throw std::runtime_error("Failed to create initial memory pool");
    }
    
    LOG_INFO("memory_manager", "MemoryManager initialized successfully with " +
             std::to_string(config_.initial_pool_size) + " bytes initial pool");
}

// EN: Allocate memory with specified size and alignment.
// FR: Alloue la mémoire avec taille et alignement spécifiés.
void* MemoryManager::allocate(size_t size, size_t alignment) {
    if (size == 0) {
        return nullptr;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_.load()) {
        initialize();
    }
    
    void* ptr = allocate_internal(size, alignment);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    update_stats(size, true, duration);
    
    return ptr;
}

// EN: Deallocate previously allocated memory.
// FR: Désalloue la mémoire précédemment allouée.
void MemoryManager::deallocate(void* ptr) {
    if (!ptr) {
        return;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    deallocate_internal(ptr);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    update_stats(0, false, duration);
}

// EN: Force defragmentation of memory pools.
// FR: Force la défragmentation des pools mémoire.
void MemoryManager::defragment() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LOG_DEBUG("memory_manager", "Starting memory defragmentation");
    
    merge_free_blocks();
    stats_.defragmentation_count++;
    
    // EN: Recalculate fragmentation ratio
    // FR: Recalcule le ratio de fragmentation
    stats_.fragmentation_ratio = calculate_fragmentation();
    
    LOG_DEBUG("memory_manager", "Defragmentation completed - New fragmentation ratio: " +
              std::to_string(stats_.fragmentation_ratio));
}

// EN: Reset all memory pools (for testing).
// FR: Remet à zéro tous les pools mémoire (pour les tests).
void MemoryManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // EN: Clear all pools and data structures
    // FR: Efface tous les pools et structures de données
    pools_.clear();
    free_list_head_ = nullptr;
    allocated_blocks_.clear();
    
    // EN: Reset flags and statistics
    // FR: Remet à zéro les flags et statistiques
    initialized_ = false;
    stats_ = MemoryPoolStats{};
    stats_.created_at = std::chrono::system_clock::now();
    
    LOG_DEBUG("memory_manager", "MemoryManager reset completed");
}

// EN: Get current memory statistics.
// FR: Obtient les statistiques mémoire actuelles.
MemoryPoolStats MemoryManager::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    MemoryPoolStats current_stats = stats_;
    
    // EN: Update real-time values
    // FR: Met à jour les valeurs temps réel
    current_stats.pool_size = 0;
    for (const auto& pool : pools_) {
        current_stats.pool_size += config_.initial_pool_size * std::pow(config_.growth_factor, &pool - &pools_[0]);
    }
    
    current_stats.available_bytes = current_stats.pool_size - current_stats.current_used_bytes;
    current_stats.fragmentation_ratio = calculate_fragmentation();
    
    return current_stats;
}

// EN: Check memory pool integrity.
// FR: Vérifie l'intégrité du pool mémoire.
bool MemoryManager::checkIntegrity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // EN: Validate all allocated blocks
    // FR: Valide tous les blocs alloués
    for (const auto& [ptr, header] : allocated_blocks_) {
        if (!validate_block_header(header)) {
            LOG_ERROR("memory_manager", "Integrity check failed: Invalid block header");
            return false;
        }
    }
    
    // EN: Validate free list
    // FR: Valide la liste libre
    BlockHeader* current = free_list_head_;
    while (current) {
        if (!validate_block_header(current) || !current->is_free) {
            LOG_ERROR("memory_manager", "Integrity check failed: Invalid free block");
            return false;
        }
        current = current->next_free;
    }
    
    return true;
}

// EN: Optimize memory pools based on usage patterns.
// FR: Optimise les pools mémoire basés sur les patterns d'usage.
void MemoryManager::optimize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LOG_DEBUG("memory_manager", "Starting memory optimization");
    
    // EN: Check if defragmentation is needed
    // FR: Vérifie si la défragmentation est nécessaire
    double frag_ratio = calculate_fragmentation();
    if (config_.enable_defragmentation && frag_ratio > config_.defrag_threshold) {
        merge_free_blocks();
        stats_.defragmentation_count++;
    }
    
    // EN: Log optimization results
    // FR: Log les résultats d'optimisation
    LOG_DEBUG("memory_manager", "Optimization completed - Fragmentation: " +
              std::to_string(frag_ratio * 100) + "%");
}

// EN: Set memory usage limits.
// FR: Définit les limites d'utilisation mémoire.
void MemoryManager::setMemoryLimit(size_t max_memory_bytes) {
    memory_limit_.store(max_memory_bytes);
    LOG_INFO("memory_manager", "Memory limit set to " + std::to_string(max_memory_bytes) + " bytes");
}

// EN: Get current memory usage.
// FR: Obtient l'utilisation mémoire actuelle.
size_t MemoryManager::getCurrentUsage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_.current_used_bytes;
}

// EN: Enable/disable detailed tracking (for debugging).
// FR: Active/désactive le suivi détaillé (pour debugging).
void MemoryManager::setDetailedTracking(bool enabled) {
    detailed_tracking_.store(enabled);
    LOG_DEBUG("memory_manager", "Detailed tracking " + std::string(enabled ? "enabled" : "disabled"));
}

// EN: Dump memory pool state (for debugging).
// FR: Dump de l'état du pool mémoire (pour debugging).
std::string MemoryManager::dumpPoolState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "=== Memory Pool State Dump ===\n";
    oss << "Pools count: " << pools_.size() << "\n";
    oss << "Current used bytes: " << stats_.current_used_bytes << "\n";
    oss << "Peak used bytes: " << stats_.peak_used_bytes << "\n";
    oss << "Total allocations: " << stats_.total_allocations << "\n";
    oss << "Total deallocations: " << stats_.total_deallocations << "\n";
    oss << "Fragmentation ratio: " << std::fixed << std::setprecision(2) << stats_.fragmentation_ratio * 100 << "%\n";
    
    // EN: Dump allocated blocks if detailed tracking is enabled
    // FR: Dump des blocs alloués si le suivi détaillé est activé
    if (detailed_tracking_.load()) {
        oss << "\nAllocated blocks (" << allocated_blocks_.size() << "):\n";
        for (const auto& [ptr, header] : allocated_blocks_) {
            oss << "  Block at " << ptr << ": size=" << header->size << " bytes\n";
        }
    }
    
    return oss.str();
}

// EN: Internal allocation with tracking.
// FR: Allocation interne avec suivi.
void* MemoryManager::allocate_internal(size_t size, size_t alignment) {
    // EN: The actual size needed is just the user request size, alignment is handled by find_best_fit_block
    // FR: La taille réelle nécessaire est juste la taille demandée par l'utilisateur, l'alignement est géré par find_best_fit_block
    size_t total_size = size + sizeof(BlockHeader);
    
    // EN: Check memory limit
    // FR: Vérifie la limite mémoire
    if (stats_.current_used_bytes + total_size > memory_limit_.load()) {
        LOG_WARN("memory_manager", "Memory limit exceeded, allocation failed");
        return nullptr;
    }
    
    // EN: Find suitable free block (this will account for alignment padding)
    // FR: Trouve un bloc libre approprié (ceci tiendra compte du padding d'alignement)
    BlockHeader* block = find_best_fit_block(size, alignment);
    
    if (!block) {
        // EN: Expand pool if needed - account for worst-case alignment padding
        // FR: Étend le pool si nécessaire - tenir compte du padding d'alignement du pire cas
        size_t required_size = total_size + alignment;
        if (!expand_pool(required_size)) {
            LOG_ERROR("memory_manager", "Failed to expand pool for allocation of " + 
                      std::to_string(required_size) + " bytes");
            return nullptr;
        }
        
        block = find_best_fit_block(size, alignment);
        if (!block) {
            LOG_ERROR("memory_manager", "No suitable block found after pool expansion");
            return nullptr;
        }
    }
    
    // EN: Remove block from free list
    // FR: Retire le bloc de la liste libre
    if (block == free_list_head_) {
        free_list_head_ = block->next_free;
    } else {
        // EN: Find previous block in free list
        // FR: Trouve le bloc précédent dans la liste libre
        BlockHeader* prev = free_list_head_;
        while (prev && prev->next_free != block) {
            prev = prev->next_free;
        }
        if (prev) {
            prev->next_free = block->next_free;
        }
    }
    
    // EN: Split block if it's significantly larger than needed
    // FR: Divise le bloc s'il est significativement plus grand que nécessaire
    if (block->size > size + sizeof(BlockHeader) + 64) {
        split_block(block, size);
    }
    
    // EN: Mark block as allocated
    // FR: Marque le bloc comme alloué
    block->is_free = false;
    block->next_free = nullptr;
    block->magic = BlockHeader::MAGIC_ALLOCATED;
    block->allocated_at = std::chrono::system_clock::now();
    
    // EN: Calculate properly aligned user pointer
    // FR: Calcule le pointeur utilisateur correctement aligné
    uintptr_t user_addr = reinterpret_cast<uintptr_t>(block) + sizeof(BlockHeader);
    uintptr_t misalignment = user_addr % alignment;
    if (misalignment) {
        user_addr += (alignment - misalignment);
    }
    void* user_ptr = reinterpret_cast<void*>(user_addr);
    
    // EN: Update tracking
    // FR: Met à jour le suivi
    if (detailed_tracking_.load()) {
        allocated_blocks_[user_ptr] = block;
    }
    
    return user_ptr;
}

// EN: Internal deallocation with tracking.
// FR: Désallocation interne avec suivi.
void MemoryManager::deallocate_internal(void* ptr) {
    BlockHeader* block = nullptr;
    
    // EN: If detailed tracking is enabled, use it to find the block
    // FR: Si le suivi détaillé est activé, l'utiliser pour trouver le bloc
    if (detailed_tracking_.load()) {
        auto it = allocated_blocks_.find(ptr);
        if (it != allocated_blocks_.end()) {
            block = it->second;
        }
    } else {
        // EN: Try to get block header from user pointer (works only for unaligned allocations)
        // FR: Essaie d'obtenir l'en-tête du bloc depuis le pointeur utilisateur (fonctionne seulement pour les allocations non-alignées)
        block = reinterpret_cast<BlockHeader*>(
            reinterpret_cast<char*>(ptr) - sizeof(BlockHeader));
    }
    
    // EN: Validate block
    // FR: Valide le bloc
    if (!block || !validate_block_header(block) || block->is_free) {
        LOG_ERROR("memory_manager", "Invalid deallocation attempt");
        return;
    }
    
    // EN: Mark block as free
    // FR: Marque le bloc comme libre
    block->is_free = true;
    block->magic = BlockHeader::MAGIC_FREE;
    block->next_free = free_list_head_;
    free_list_head_ = block;
    
    // EN: Remove from allocated blocks tracking
    // FR: Retire du suivi des blocs alloués
    if (detailed_tracking_.load()) {
        allocated_blocks_.erase(ptr);
    }
    
    // EN: Merge with adjacent free blocks if defragmentation is enabled
    // FR: Fusionne avec les blocs libres adjacents si la défragmentation est activée
    if (config_.enable_defragmentation) {
        merge_free_blocks();
    }
}

// EN: Expand memory pool when needed.
// FR: Étend le pool mémoire quand nécessaire.
bool MemoryManager::expand_pool(size_t required_size) {
    // EN: Calculate new pool size with growth factor
    // FR: Calcule la nouvelle taille de pool avec le facteur de croissance
    size_t new_pool_size = std::max(required_size, 
        static_cast<size_t>(config_.initial_pool_size * std::pow(config_.growth_factor, pools_.size())));
    
    // EN: Check against maximum pool size
    // FR: Vérifie contre la taille maximum de pool
    size_t total_size = 0;
    for (size_t i = 0; i <= pools_.size(); ++i) {
        total_size += config_.initial_pool_size * std::pow(config_.growth_factor, i);
    }
    
    if (total_size > config_.max_pool_size) {
        LOG_WARN("memory_manager", "Cannot expand pool: would exceed maximum size");
        return false;
    }
    
    // EN: Allocate new pool
    // FR: Alloue un nouveau pool
    try {
        auto new_pool = std::make_unique<char[]>(new_pool_size);
        
        // EN: Initialize pool with a single large free block
        // FR: Initialise le pool avec un seul grand bloc libre
        BlockHeader* block = reinterpret_cast<BlockHeader*>(new_pool.get());
        block->size = new_pool_size - sizeof(BlockHeader);
        block->is_free = true;
        block->next_free = free_list_head_;
        block->magic = BlockHeader::MAGIC_FREE;
        
        free_list_head_ = block;
        pools_.push_back(std::move(new_pool));
        
        LOG_DEBUG("memory_manager", "Pool expanded by " + std::to_string(new_pool_size) + " bytes");
        return true;
        
    } catch (const std::bad_alloc&) {
        LOG_ERROR("memory_manager", "Failed to allocate new pool of " + std::to_string(new_pool_size) + " bytes");
        return false;
    }
}

// EN: Find free block with best fit algorithm.
// FR: Trouve un bloc libre avec algorithme best fit.
BlockHeader* MemoryManager::find_best_fit_block(size_t size, size_t alignment) {
    BlockHeader* best_block = nullptr;
    size_t best_size = SIZE_MAX;
    
    BlockHeader* current = free_list_head_;
    while (current) {
        if (current->is_free) {
            // EN: Calculate user pointer address after header
            // FR: Calcule l'adresse du pointeur utilisateur après l'en-tête
            uintptr_t user_addr = reinterpret_cast<uintptr_t>(current) + sizeof(BlockHeader);
            
            // EN: Calculate padding needed for alignment
            // FR: Calcule le padding nécessaire pour l'alignement
            uintptr_t misalignment = user_addr % alignment;
            size_t padding = misalignment ? (alignment - misalignment) : 0;
            
            // EN: Total space needed includes original size plus any alignment padding
            // FR: L'espace total nécessaire inclut la taille originale plus le padding d'alignement
            size_t total_needed = size + padding;
            
            if (current->size >= total_needed) {
                if (current->size < best_size) {
                    best_block = current;
                    best_size = current->size;
                }
            }
        }
        current = current->next_free;
    }
    
    return best_block;
}

// EN: Split block if too large.
// FR: Divise le bloc s'il est trop grand.
void MemoryManager::split_block(BlockHeader* block, size_t size) {
    if (block->size <= size + sizeof(BlockHeader)) {
        return; // EN: Block is too small to split / FR: Bloc trop petit pour être divisé
    }
    
    // EN: Create new block from the remaining space
    // FR: Crée un nouveau bloc avec l'espace restant
    BlockHeader* new_block = reinterpret_cast<BlockHeader*>(
        reinterpret_cast<char*>(block) + sizeof(BlockHeader) + size);
    
    new_block->size = block->size - size - sizeof(BlockHeader);
    new_block->is_free = true;
    new_block->next_free = free_list_head_;
    new_block->magic = BlockHeader::MAGIC_FREE;
    
    // EN: Update original block size
    // FR: Met à jour la taille du bloc original
    block->size = size;
    
    // EN: Add new block to free list
    // FR: Ajoute le nouveau bloc à la liste libre
    free_list_head_ = new_block;
}

// EN: Merge adjacent free blocks.
// FR: Fusionne les blocs libres adjacents.
void MemoryManager::merge_free_blocks() {
    // EN: Sort free blocks by address for efficient merging
    // FR: Trie les blocs libres par adresse pour fusion efficace
    std::vector<BlockHeader*> free_blocks;
    
    BlockHeader* current = free_list_head_;
    while (current) {
        if (current->is_free) {
            free_blocks.push_back(current);
        }
        current = current->next_free;
    }
    
    if (free_blocks.size() < 2) {
        return; // EN: Nothing to merge / FR: Rien à fusionner
    }
    
    std::sort(free_blocks.begin(), free_blocks.end());
    
    // EN: Merge adjacent blocks
    // FR: Fusionne les blocs adjacents
    auto it = free_blocks.begin();
    while (it != free_blocks.end() - 1) {
        BlockHeader* current_block = *it;
        BlockHeader* next_block = *(it + 1);
        
        // EN: Check if blocks are adjacent
        // FR: Vérifie si les blocs sont adjacents
        char* current_end = reinterpret_cast<char*>(current_block) + sizeof(BlockHeader) + current_block->size;
        if (current_end == reinterpret_cast<char*>(next_block)) {
            // EN: Merge blocks
            // FR: Fusionne les blocs
            current_block->size += sizeof(BlockHeader) + next_block->size;
            next_block->magic = 0; // EN: Invalidate merged block / FR: Invalide le bloc fusionné
            
            // EN: Remove merged block from vector
            // FR: Retire le bloc fusionné du vecteur
            it = free_blocks.erase(it + 1) - 1;
        } else {
            ++it;
        }
    }
    
    // EN: Rebuild free list
    // FR: Reconstruit la liste libre
    free_list_head_ = nullptr;
    for (auto block : free_blocks) {
        if (block->magic == BlockHeader::MAGIC_FREE) {
            block->next_free = free_list_head_;
            free_list_head_ = block;
        }
    }
}

// EN: Calculate fragmentation ratio.
// FR: Calcule le ratio de fragmentation.
double MemoryManager::calculate_fragmentation() const {
    if (stats_.current_used_bytes == 0) {
        return 0.0;
    }
    
    // EN: Count free blocks and calculate average size
    // FR: Compte les blocs libres et calcule la taille moyenne
    size_t free_block_count = 0;
    
    BlockHeader* current = free_list_head_;
    while (current) {
        if (current->is_free) {
            free_block_count++;
        }
        current = current->next_free;
    }
    
    if (free_block_count <= 1) {
        return 0.0; // EN: No fragmentation / FR: Pas de fragmentation
    }
    
    // EN: Fragmentation ratio based on number of free blocks
    // FR: Ratio de fragmentation basé sur le nombre de blocs libres
    return static_cast<double>(free_block_count - 1) / free_block_count;
}

// EN: Update statistics.
// FR: Met à jour les statistiques.
void MemoryManager::update_stats(size_t size, bool is_allocation, std::chrono::microseconds duration) {
    if (is_allocation) {
        stats_.total_allocations++;
        stats_.total_allocated_bytes += size;
        stats_.current_used_bytes += size;
        stats_.total_alloc_time += std::chrono::duration_cast<std::chrono::milliseconds>(duration);
        
        if (stats_.current_used_bytes > stats_.peak_used_bytes) {
            stats_.peak_used_bytes = stats_.current_used_bytes;
        }
        
        // EN: Update size histogram
        // FR: Met à jour l'histogramme de taille
        if (config_.enable_statistics) {
            stats_.size_histogram[size]++;
        }
    } else {
        stats_.total_deallocations++;
        stats_.total_dealloc_time += std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    }
    
    // EN: Update fragmentation ratio periodically
    // FR: Met à jour le ratio de fragmentation périodiquement
    if ((stats_.total_allocations + stats_.total_deallocations) % 100 == 0) {
        stats_.fragmentation_ratio = calculate_fragmentation();
    }
}

// EN: Align size to specified boundary.
// FR: Aligne la taille sur la frontière spécifiée.
size_t MemoryManager::align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// EN: Validate block header.
// FR: Valide l'en-tête du bloc.
bool MemoryManager::validate_block_header(const BlockHeader* header) const {
    if (!header) {
        return false;
    }
    
    // EN: Check magic number
    // FR: Vérifie le nombre magique
    if (header->is_free) {
        return header->magic == BlockHeader::MAGIC_FREE;
    } else {
        return header->magic == BlockHeader::MAGIC_ALLOCATED;
    }
}

// EN: PoolAllocator implementation
// FR: Implémentation PoolAllocator

template<typename T>
typename PoolAllocator<T>::pointer PoolAllocator<T>::allocate(size_type n) {
    if (n == 0) {
        return nullptr;
    }
    
    size_type size = n * sizeof(T);
    void* ptr = manager_->allocate(size, alignof(T));
    
    if (!ptr) {
        throw std::bad_alloc();
    }
    
    return static_cast<pointer>(ptr);
}

template<typename T>
void PoolAllocator<T>::deallocate(pointer p, size_type /* n */) {
    if (p) {
        manager_->deallocate(p);
    }
}

// EN: Explicit template instantiations for common types
// FR: Instanciations explicites de templates pour types communs
template class PoolAllocator<char>;
template class PoolAllocator<int>;
template class PoolAllocator<double>;
template class PoolAllocator<std::string>;

} // namespace BBP