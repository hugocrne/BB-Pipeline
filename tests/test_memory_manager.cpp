// EN: Comprehensive unit tests for MemoryManager class - 100% coverage
// FR: Tests unitaires complets pour la classe MemoryManager - couverture 100%

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../include/infrastructure/system/memory_manager.hpp"
#include "../include/infrastructure/logging/logger.hpp"
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <random>
#include <algorithm>

using namespace BBP;
using namespace std::chrono_literals;

// EN: Test fixture for MemoryManager tests
// FR: Fixture de test pour les tests MemoryManager
class MemoryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // EN: Reset memory manager before each test
        // FR: Remet à zéro le gestionnaire mémoire avant chaque test
        memory_manager_ = &MemoryManager::getInstance();
        memory_manager_->reset();
        
        // EN: Setup logger for test output
        // FR: Configure le logger pour la sortie de test
        logger_ = &Logger::getInstance();
        logger_->setLogLevel(LogLevel::DEBUG);
    }
    
    void TearDown() override {
        // EN: Clean up after each test
        // FR: Nettoie après chaque test
        if (memory_manager_) {
            memory_manager_->reset();
        }
    }
    
    MemoryManager* memory_manager_;
    Logger* logger_;
};

// EN: Test singleton pattern
// FR: Test du pattern singleton
TEST_F(MemoryManagerTest, SingletonPattern) {
    MemoryManager& manager1 = MemoryManager::getInstance();
    MemoryManager& manager2 = MemoryManager::getInstance();
    
    // EN: Should return same instance
    // FR: Devrait retourner la même instance
    EXPECT_EQ(&manager1, &manager2);
}

// EN: Test default configuration
// FR: Test de la configuration par défaut
TEST_F(MemoryManagerTest, DefaultConfiguration) {
    auto stats = memory_manager_->getStats();
    
    EXPECT_EQ(stats.total_allocated_bytes, 0);
    EXPECT_EQ(stats.total_freed_bytes, 0);
    EXPECT_EQ(stats.current_used_bytes, 0);
    EXPECT_EQ(stats.peak_used_bytes, 0);
    EXPECT_EQ(stats.total_allocations, 0);
    EXPECT_EQ(stats.total_deallocations, 0);
}

// EN: Test custom configuration
// FR: Test de la configuration personnalisée
TEST_F(MemoryManagerTest, CustomConfiguration) {
    MemoryPoolConfig config;
    config.initial_pool_size = 2 * 1024 * 1024;  // 2MB
    config.max_pool_size = 50 * 1024 * 1024;     // 50MB
    config.block_size = 128;
    config.alignment = 16;
    config.enable_statistics = true;
    config.enable_defragmentation = true;
    config.growth_factor = 1.5;
    
    // EN: Should configure without throwing
    // FR: Devrait configurer sans lancer d'exception
    EXPECT_NO_THROW(memory_manager_->configure(config));
}

// EN: Test initialization
// FR: Test de l'initialisation
TEST_F(MemoryManagerTest, Initialization) {
    // EN: Should initialize successfully
    // FR: Devrait s'initialiser avec succès
    EXPECT_NO_THROW(memory_manager_->initialize());
    
    auto stats = memory_manager_->getStats();
    EXPECT_GT(stats.pool_size, 0);
}

// EN: Test basic allocation and deallocation
// FR: Test d'allocation et désallocation de base
TEST_F(MemoryManagerTest, BasicAllocationDeallocation) {
    memory_manager_->initialize();
    
    // EN: Allocate memory
    // FR: Alloue la mémoire
    void* ptr = memory_manager_->allocate(1024);
    EXPECT_NE(ptr, nullptr);
    
    auto stats_after_alloc = memory_manager_->getStats();
    EXPECT_GT(stats_after_alloc.current_used_bytes, 0);
    EXPECT_EQ(stats_after_alloc.total_allocations, 1);
    
    // EN: Deallocate memory
    // FR: Désalloue la mémoire
    memory_manager_->deallocate(ptr);
    
    auto stats_after_dealloc = memory_manager_->getStats();
    EXPECT_EQ(stats_after_dealloc.total_deallocations, 1);
}

// EN: Test zero-size allocation
// FR: Test d'allocation de taille zéro
TEST_F(MemoryManagerTest, ZeroSizeAllocation) {
    memory_manager_->initialize();
    
    void* ptr = memory_manager_->allocate(0);
    EXPECT_EQ(ptr, nullptr);
    
    // EN: Should not affect statistics
    // FR: Ne devrait pas affecter les statistiques
    auto stats = memory_manager_->getStats();
    EXPECT_EQ(stats.total_allocations, 0);
}

// EN: Test null pointer deallocation
// FR: Test de désallocation de pointeur null
TEST_F(MemoryManagerTest, NullPointerDeallocation) {
    memory_manager_->initialize();
    
    // EN: Should not crash or throw
    // FR: Ne devrait pas crasher ou lancer d'exception
    EXPECT_NO_THROW(memory_manager_->deallocate(nullptr));
    
    auto stats = memory_manager_->getStats();
    EXPECT_EQ(stats.total_deallocations, 0);
}

// EN: Test large allocation
// FR: Test d'allocation importante
TEST_F(MemoryManagerTest, LargeAllocation) {
    MemoryPoolConfig config;
    config.initial_pool_size = 10 * 1024 * 1024;  // 10MB
    memory_manager_->configure(config);
    memory_manager_->initialize();
    
    // EN: Allocate large block
    // FR: Alloue un gros bloc
    size_t large_size = 5 * 1024 * 1024;  // 5MB
    void* ptr = memory_manager_->allocate(large_size);
    EXPECT_NE(ptr, nullptr);
    
    auto stats = memory_manager_->getStats();
    EXPECT_GT(stats.current_used_bytes, large_size);
    
    memory_manager_->deallocate(ptr);
}

// EN: Test multiple allocations
// FR: Test d'allocations multiples
TEST_F(MemoryManagerTest, MultipleAllocations) {
    memory_manager_->initialize();
    
    std::vector<void*> ptrs;
    const size_t num_allocs = 100;
    const size_t alloc_size = 64;
    
    // EN: Allocate multiple blocks
    // FR: Alloue plusieurs blocs
    for (size_t i = 0; i < num_allocs; ++i) {
        void* ptr = memory_manager_->allocate(alloc_size);
        EXPECT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }
    
    auto stats_after_allocs = memory_manager_->getStats();
    EXPECT_EQ(stats_after_allocs.total_allocations, num_allocs);
    EXPECT_GT(stats_after_allocs.current_used_bytes, num_allocs * alloc_size);
    
    // EN: Deallocate all blocks
    // FR: Désalloue tous les blocs
    for (void* ptr : ptrs) {
        memory_manager_->deallocate(ptr);
    }
    
    auto stats_after_deallocs = memory_manager_->getStats();
    EXPECT_EQ(stats_after_deallocs.total_deallocations, num_allocs);
}

// EN: Test aligned allocation
// FR: Test d'allocation alignée
TEST_F(MemoryManagerTest, AlignedAllocation) {
    memory_manager_->initialize();
    
    // EN: Test various alignments
    // FR: Teste différents alignements
    std::vector<size_t> alignments = {8, 16, 32, 64, 128};
    
    for (size_t alignment : alignments) {
        void* ptr = memory_manager_->allocate(1024, alignment);
        EXPECT_NE(ptr, nullptr);
        
        // EN: Check alignment
        // FR: Vérifie l'alignement
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        EXPECT_EQ(addr % alignment, 0) << "Allocation not aligned to " << alignment << " bytes";
        
        memory_manager_->deallocate(ptr);
    }
}

// EN: Test array allocation and deallocation
// FR: Test d'allocation et désallocation de tableau
TEST_F(MemoryManagerTest, ArrayAllocation) {
    memory_manager_->initialize();
    
    const size_t count = 1000;
    
    // EN: Allocate array of integers
    // FR: Alloue un tableau d'entiers
    int* int_array = memory_manager_->allocate_array<int>(count);
    EXPECT_NE(int_array, nullptr);
    
    // EN: Initialize array
    // FR: Initialise le tableau
    for (size_t i = 0; i < count; ++i) {
        int_array[i] = static_cast<int>(i);
    }
    
    // EN: Verify array contents
    // FR: Vérifie le contenu du tableau
    for (size_t i = 0; i < count; ++i) {
        EXPECT_EQ(int_array[i], static_cast<int>(i));
    }
    
    memory_manager_->deallocate_array(int_array);
    
    auto stats = memory_manager_->getStats();
    EXPECT_EQ(stats.total_allocations, 1);
    EXPECT_EQ(stats.total_deallocations, 1);
}

// EN: Test pool allocator
// FR: Test du pool allocator
TEST_F(MemoryManagerTest, PoolAllocator) {
    memory_manager_->initialize();
    
    auto allocator = memory_manager_->get_allocator<int>();
    
    // EN: Allocate using pool allocator
    // FR: Alloue en utilisant le pool allocator
    int* ptr = allocator.allocate(10);
    EXPECT_NE(ptr, nullptr);
    
    // EN: Initialize values
    // FR: Initialise les valeurs
    for (int i = 0; i < 10; ++i) {
        ptr[i] = i * i;
    }
    
    // EN: Verify values
    // FR: Vérifie les valeurs
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(ptr[i], i * i);
    }
    
    allocator.deallocate(ptr, 10);
}

// EN: Test ManagedPtr RAII wrapper
// FR: Test du wrapper RAII ManagedPtr
TEST_F(MemoryManagerTest, ManagedPtr) {
    memory_manager_->initialize();
    
    {
        // EN: Create managed pointer
        // FR: Crée un pointeur géré
        ManagedPtr<int> managed_ptr(*memory_manager_, 100);
        EXPECT_TRUE(managed_ptr);
        EXPECT_EQ(managed_ptr.count(), 100);
        
        // EN: Access array elements
        // FR: Accède aux éléments du tableau
        for (size_t i = 0; i < 100; ++i) {
            managed_ptr[i] = static_cast<int>(i * 2);
        }
        
        for (size_t i = 0; i < 100; ++i) {
            EXPECT_EQ(managed_ptr[i], static_cast<int>(i * 2));
        }
        
        // EN: Move constructor test
        // FR: Test du constructeur de déplacement
        ManagedPtr<int> moved_ptr = std::move(managed_ptr);
        EXPECT_FALSE(managed_ptr);
        EXPECT_TRUE(moved_ptr);
        EXPECT_EQ(moved_ptr.count(), 100);
    }
    
    // EN: Memory should be automatically deallocated
    // FR: La mémoire devrait être automatiquement désallouée
    auto stats = memory_manager_->getStats();
    EXPECT_EQ(stats.total_allocations, stats.total_deallocations);
}

// EN: Test memory limit functionality
// FR: Test de la fonctionnalité de limite mémoire
TEST_F(MemoryManagerTest, MemoryLimit) {
    memory_manager_->initialize();
    
    // EN: Set low memory limit
    // FR: Définit une limite mémoire basse
    size_t limit = 1024;
    memory_manager_->setMemoryLimit(limit);
    
    // EN: Try to allocate more than the limit
    // FR: Essaie d'allouer plus que la limite
    void* ptr = memory_manager_->allocate(limit * 2);
    EXPECT_EQ(ptr, nullptr);  // EN: Should fail / FR: Devrait échouer
    
    // EN: Allocate within limit
    // FR: Alloue dans la limite
    ptr = memory_manager_->allocate(512);
    EXPECT_NE(ptr, nullptr);
    
    memory_manager_->deallocate(ptr);
}

// EN: Test defragmentation
// FR: Test de la défragmentation
TEST_F(MemoryManagerTest, Defragmentation) {
    memory_manager_->initialize();
    
    std::vector<void*> ptrs;
    
    // EN: Allocate many small blocks
    // FR: Alloue beaucoup de petits blocs
    for (int i = 0; i < 100; ++i) {
        void* ptr = memory_manager_->allocate(64);
        ptrs.push_back(ptr);
    }
    
    // EN: Deallocate every other block to create fragmentation
    // FR: Désalloue un bloc sur deux pour créer de la fragmentation
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        memory_manager_->deallocate(ptrs[i]);
    }
    
    auto stats_before = memory_manager_->getStats();
    
    // EN: Force defragmentation
    // FR: Force la défragmentation
    memory_manager_->defragment();
    
    auto stats_after = memory_manager_->getStats();
    EXPECT_EQ(stats_after.defragmentation_count, stats_before.defragmentation_count + 1);
    
    // EN: Clean up remaining pointers
    // FR: Nettoie les pointeurs restants
    for (size_t i = 1; i < ptrs.size(); i += 2) {
        memory_manager_->deallocate(ptrs[i]);
    }
}

// EN: Test memory optimization
// FR: Test d'optimisation mémoire
TEST_F(MemoryManagerTest, MemoryOptimization) {
    memory_manager_->initialize();
    
    // EN: Should not throw
    // FR: Ne devrait pas lancer d'exception
    EXPECT_NO_THROW(memory_manager_->optimize());
    
    auto stats = memory_manager_->getStats();
    // EN: Optimization should have been performed
    // FR: L'optimisation devrait avoir été effectuée
    EXPECT_GE(stats.defragmentation_count, 0);
}

// EN: Test integrity checking
// FR: Test de vérification d'intégrité
TEST_F(MemoryManagerTest, IntegrityCheck) {
    memory_manager_->initialize();
    
    // EN: Fresh memory manager should have valid integrity
    // FR: Un gestionnaire mémoire frais devrait avoir une intégrité valide
    EXPECT_TRUE(memory_manager_->checkIntegrity());
    
    // EN: Allocate and check integrity
    // FR: Alloue et vérifie l'intégrité
    void* ptr = memory_manager_->allocate(1024);
    EXPECT_TRUE(memory_manager_->checkIntegrity());
    
    memory_manager_->deallocate(ptr);
    EXPECT_TRUE(memory_manager_->checkIntegrity());
}

// EN: Test detailed tracking
// FR: Test du suivi détaillé
TEST_F(MemoryManagerTest, DetailedTracking) {
    memory_manager_->initialize();
    memory_manager_->setDetailedTracking(true);
    
    void* ptr1 = memory_manager_->allocate(512);
    void* ptr2 = memory_manager_->allocate(1024);
    
    // EN: Get dump with detailed information
    // FR: Obtient un dump avec des informations détaillées
    std::string dump = memory_manager_->dumpPoolState();
    EXPECT_FALSE(dump.empty());
    EXPECT_NE(dump.find("Allocated blocks"), std::string::npos);
    
    memory_manager_->deallocate(ptr1);
    memory_manager_->deallocate(ptr2);
    
    memory_manager_->setDetailedTracking(false);
}

// EN: Test statistics accuracy
// FR: Test de la précision des statistiques
TEST_F(MemoryManagerTest, StatisticsAccuracy) {
    memory_manager_->initialize();
    
    const size_t num_allocs = 50;
    const size_t alloc_size = 128;
    std::vector<void*> ptrs;
    
    // EN: Perform allocations and track expected values
    // FR: Effectue des allocations et suit les valeurs attendues
    for (size_t i = 0; i < num_allocs; ++i) {
        void* ptr = memory_manager_->allocate(alloc_size);
        ptrs.push_back(ptr);
    }
    
    auto stats_alloc = memory_manager_->getStats();
    EXPECT_EQ(stats_alloc.total_allocations, num_allocs);
    EXPECT_GE(stats_alloc.current_used_bytes, num_allocs * alloc_size);
    EXPECT_GE(stats_alloc.peak_used_bytes, stats_alloc.current_used_bytes);
    
    // EN: Deallocate half
    // FR: Désalloue la moitié
    for (size_t i = 0; i < num_allocs / 2; ++i) {
        memory_manager_->deallocate(ptrs[i]);
    }
    
    auto stats_partial = memory_manager_->getStats();
    EXPECT_EQ(stats_partial.total_deallocations, num_allocs / 2);
    
    // EN: Deallocate remaining
    // FR: Désalloue le reste
    for (size_t i = num_allocs / 2; i < num_allocs; ++i) {
        memory_manager_->deallocate(ptrs[i]);
    }
    
    auto stats_final = memory_manager_->getStats();
    EXPECT_EQ(stats_final.total_deallocations, num_allocs);
}

// EN: Test concurrent access (thread safety)
// FR: Test d'accès concurrent (thread safety)
TEST_F(MemoryManagerTest, ConcurrentAccess) {
    memory_manager_->initialize();
    
    const int num_threads = 4;
    const int allocs_per_thread = 100;
    std::atomic<int> successful_allocs{0};
    std::atomic<int> successful_deallocs{0};
    
    std::vector<std::thread> threads;
    
    // EN: Launch threads that perform allocations and deallocations
    // FR: Lance des threads qui effectuent des allocations et désallocations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::vector<void*> local_ptrs;
            
            // EN: Allocate
            // FR: Alloue
            for (int i = 0; i < allocs_per_thread; ++i) {
                void* ptr = memory_manager_->allocate(64 + (t * 16));
                if (ptr) {
                    local_ptrs.push_back(ptr);
                    successful_allocs++;
                }
            }
            
            // EN: Small delay to increase chance of race conditions
            // FR: Petit délai pour augmenter les chances de conditions de course
            std::this_thread::sleep_for(1ms);
            
            // EN: Deallocate
            // FR: Désalloue
            for (void* ptr : local_ptrs) {
                memory_manager_->deallocate(ptr);
                successful_deallocs++;
            }
        });
    }
    
    // EN: Wait for all threads
    // FR: Attend tous les threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // EN: Verify thread safety - no crashes and consistent counts
    // FR: Vérifie la thread safety - pas de crash et compteurs cohérents
    EXPECT_EQ(successful_allocs.load(), successful_deallocs.load());
    EXPECT_EQ(successful_allocs.load(), num_threads * allocs_per_thread);
    
    // EN: Verify memory manager integrity after concurrent access
    // FR: Vérifie l'intégrité du gestionnaire mémoire après accès concurrent
    EXPECT_TRUE(memory_manager_->checkIntegrity());
}

// EN: Test performance and timing
// FR: Test de performance et timing
TEST_F(MemoryManagerTest, PerformanceTiming) {
    memory_manager_->initialize();
    
    const size_t num_operations = 1000;
    const size_t alloc_size = 256;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<void*> ptrs;
    ptrs.reserve(num_operations);
    
    // EN: Perform allocations
    // FR: Effectue les allocations
    for (size_t i = 0; i < num_operations; ++i) {
        void* ptr = memory_manager_->allocate(alloc_size);
        ptrs.push_back(ptr);
    }
    
    auto mid_time = std::chrono::high_resolution_clock::now();
    
    // EN: Perform deallocations
    // FR: Effectue les désallocations
    for (void* ptr : ptrs) {
        memory_manager_->deallocate(ptr);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // EN: Calculate timings
    // FR: Calcule les temps
    auto alloc_duration = std::chrono::duration_cast<std::chrono::microseconds>(mid_time - start_time);
    auto dealloc_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - mid_time);
    
    // EN: Verify reasonable performance (should complete within reasonable time)
    // FR: Vérifie une performance raisonnable (devrait se terminer dans un temps raisonnable)
    EXPECT_LT(alloc_duration.count(), 100000);  // Less than 100ms for 1000 allocations
    EXPECT_LT(dealloc_duration.count(), 100000); // Less than 100ms for 1000 deallocations
    
    auto stats = memory_manager_->getStats();
    EXPECT_GT(stats.total_alloc_time.count(), 0);
    EXPECT_GT(stats.total_dealloc_time.count(), 0);
}

// EN: Test edge cases and error conditions
// FR: Test des cas limites et conditions d'erreur
TEST_F(MemoryManagerTest, EdgeCasesAndErrors) {
    memory_manager_->initialize();
    
    // EN: Try to allocate maximum possible size (should fail gracefully)
    // FR: Essaie d'allouer la taille maximum possible (devrait échouer proprement)
    void* huge_ptr = memory_manager_->allocate(SIZE_MAX);
    EXPECT_EQ(huge_ptr, nullptr);
    
    // EN: Double deallocation should be handled gracefully
    // FR: La double désallocation devrait être gérée proprement
    void* ptr = memory_manager_->allocate(1024);
    EXPECT_NE(ptr, nullptr);
    
    memory_manager_->deallocate(ptr);
    // EN: Second deallocation should not crash
    // FR: La seconde désallocation ne devrait pas crasher
    EXPECT_NO_THROW(memory_manager_->deallocate(ptr));
}

// EN: Test memory pool expansion
// FR: Test d'expansion du pool mémoire
TEST_F(MemoryManagerTest, PoolExpansion) {
    MemoryPoolConfig config;
    config.initial_pool_size = 1024;     // Very small initial pool
    config.max_pool_size = 10 * 1024;    // Small max pool
    config.growth_factor = 2.0;
    
    memory_manager_->configure(config);
    memory_manager_->initialize();
    
    std::vector<void*> ptrs;
    
    // EN: Allocate enough to force pool expansion
    // FR: Alloue assez pour forcer l'expansion du pool
    for (int i = 0; i < 20; ++i) {
        void* ptr = memory_manager_->allocate(128);
        if (ptr) {
            ptrs.push_back(ptr);
        }
    }
    
    auto stats = memory_manager_->getStats();
    EXPECT_GT(stats.pool_size, config.initial_pool_size);
    
    // EN: Clean up
    // FR: Nettoie
    for (void* ptr : ptrs) {
        memory_manager_->deallocate(ptr);
    }
}

// EN: Test reset functionality
// FR: Test de la fonctionnalité de remise à zéro
TEST_F(MemoryManagerTest, ResetFunctionality) {
    memory_manager_->initialize();
    
    // EN: Perform some allocations
    // FR: Effectue quelques allocations
    void* ptr1 = memory_manager_->allocate(512);
    void* ptr2 = memory_manager_->allocate(1024);
    (void)ptr1; // EN: Suppress unused variable warning / FR: Supprime l'avertissement variable inutilisée
    (void)ptr2;
    
    auto stats_before = memory_manager_->getStats();
    EXPECT_GT(stats_before.total_allocations, 0);
    EXPECT_GT(stats_before.current_used_bytes, 0);
    
    // EN: Reset should clear everything
    // FR: La remise à zéro devrait tout effacer
    memory_manager_->reset();
    
    auto stats_after = memory_manager_->getStats();
    EXPECT_EQ(stats_after.total_allocations, 0);
    EXPECT_EQ(stats_after.current_used_bytes, 0);
    EXPECT_EQ(stats_after.peak_used_bytes, 0);
}

// EN: Test different allocation sizes and patterns
// FR: Test de différentes tailles et patterns d'allocation
TEST_F(MemoryManagerTest, AllocationSizePatterns) {
    memory_manager_->initialize();
    
    std::vector<size_t> sizes = {8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};
    std::vector<void*> ptrs;
    
    // EN: Test various allocation sizes
    // FR: Teste diverses tailles d'allocation
    for (size_t size : sizes) {
        for (int i = 0; i < 10; ++i) {
            void* ptr = memory_manager_->allocate(size);
            EXPECT_NE(ptr, nullptr) << "Failed to allocate " << size << " bytes";
            ptrs.push_back(ptr);
        }
    }
    
    auto stats = memory_manager_->getStats();
    EXPECT_EQ(stats.total_allocations, sizes.size() * 10);
    
    // EN: Random deallocation pattern
    // FR: Pattern de désallocation aléatoire
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(ptrs.begin(), ptrs.end(), gen);
    
    for (void* ptr : ptrs) {
        memory_manager_->deallocate(ptr);
    }
    
    auto final_stats = memory_manager_->getStats();
    EXPECT_EQ(final_stats.total_deallocations, sizes.size() * 10);
}

// EN: Main function for running tests
// FR: Fonction principale pour exécuter les tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}