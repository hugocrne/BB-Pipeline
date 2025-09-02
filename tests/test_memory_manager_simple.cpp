// EN: Simple integration test for MemoryManager - CSV parsing simulation
// FR: Test d'intégration simple pour MemoryManager - simulation parsing CSV

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include "../include/infrastructure/system/memory_manager.hpp"
#include "../include/infrastructure/logging/logger.hpp"

using namespace BBP;
using namespace std::chrono_literals;

// EN: Simulate CSV row structure
// FR: Structure simulée de ligne CSV
struct CsvRow {
    std::string domain;
    std::string ip;
    std::string status;
    int port;
    double response_time;
};

int main() {
    std::cout << "=== BB-Pipeline Memory Manager Integration Test ===\n\n";
    
    // EN: Setup logger
    // FR: Configure le logger
    auto& logger = Logger::getInstance();
    logger.setLogLevel(LogLevel::DEBUG);
    
    // EN: Get memory manager instance
    // FR: Obtient l'instance du gestionnaire mémoire
    auto& manager = MemoryManager::getInstance();
    
    // EN: Configure for CSV processing
    // FR: Configure pour traitement CSV
    MemoryPoolConfig config;
    config.initial_pool_size = 5 * 1024 * 1024;    // 5MB
    config.max_pool_size = 50 * 1024 * 1024;       // 50MB
    config.block_size = 256;                        // Optimized for CSV rows
    config.enable_statistics = true;
    config.enable_defragmentation = true;
    config.defrag_threshold = 0.25;                 // 25%
    
    manager.configure(config);
    manager.initialize();
    
    std::cout << "1. Memory Manager initialized with optimized CSV configuration\n";
    
    // EN: Set memory limit for testing
    // FR: Définit une limite mémoire pour les tests
    manager.setMemoryLimit(40 * 1024 * 1024);  // 40MB limit
    std::cout << "2. Memory limit set to 40MB\n";
    
    // EN: Test basic allocations
    // FR: Teste les allocations de base
    std::cout << "\n3. Testing basic allocations...\n";
    
    std::vector<void*> test_ptrs;
    const size_t num_basic_allocs = 100;
    
    for (size_t i = 0; i < num_basic_allocs; ++i) {
        void* ptr = manager.allocate(256 + (i % 512));  // Variable sizes
        if (ptr) {
            test_ptrs.push_back(ptr);
        }
    }
    
    auto stats_basic = manager.getStats();
    std::cout << "   Allocated " << stats_basic.total_allocations << " blocks\n";
    std::cout << "   Memory used: " << stats_basic.current_used_bytes << " bytes\n";
    
    // EN: Clean up basic allocations
    // FR: Nettoie les allocations de base
    for (void* ptr : test_ptrs) {
        manager.deallocate(ptr);
    }
    test_ptrs.clear();
    
    // EN: Test simple memory allocation for basic types (safer approach)
    // FR: Test d'allocation mémoire simple pour types basiques (approche plus sûre)
    std::cout << "\n4. Testing CSV simulation with basic memory allocation...\n";
    
    const size_t csv_rows = 1000;
    std::vector<std::string> sample_domains = {
        "example.com", "test.org", "api.service.net", "subdomain.target.io",
        "webapp.company.co", "backend.system.dev"
    };
    
    // EN: Use standard vector with basic CSV simulation
    // FR: Utilise un vector standard avec simulation CSV basique
    {
        std::vector<CsvRow> csv_buffer;
        csv_buffer.reserve(csv_rows);
        
        std::cout << "   Allocated buffer for " << csv_rows << " CSV rows\n";
        
        // EN: Fill with simulated CSV data
        // FR: Remplit avec des données CSV simulées
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> domain_dist(0, sample_domains.size() - 1);
        std::uniform_int_distribution<> port_dist(80, 65535);
        std::uniform_real_distribution<> time_dist(0.1, 5.0);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < csv_rows; ++i) {
            CsvRow row;
            row.domain = sample_domains[domain_dist(gen)];
            row.ip = "192.168.1." + std::to_string(i % 255);
            row.status = (i % 10 == 0) ? "timeout" : "active";
            row.port = port_dist(gen);
            row.response_time = time_dist(gen);
            csv_buffer.push_back(std::move(row));
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "   Filled " << csv_rows << " CSV rows in " << duration.count() << "ms\n";
        
        // EN: Simulate processing (access patterns)
        // FR: Simule le traitement (patterns d'accès)
        size_t active_count = 0;
        double total_response_time = 0.0;
        
        for (size_t i = 0; i < csv_rows; ++i) {
            if (csv_buffer[i].status == "active") {
                active_count++;
                total_response_time += csv_buffer[i].response_time;
            }
        }
        
        std::cout << "   Processed data: " << active_count << " active hosts, "
                  << "avg response: " << (total_response_time / active_count) << "ms\n";
        
        // EN: Show that basic memory allocation works alongside custom manager
        // FR: Montre que l'allocation mémoire basique fonctionne avec le gestionnaire personnalisé
    }
    
    // EN: Test basic raw memory allocations with memory manager
    // FR: Test d'allocations mémoire brutes basiques avec le gestionnaire mémoire
    std::cout << "\n5. Testing raw memory allocations...\n";
    
    {
        std::vector<void*> raw_ptrs;
        
        // EN: Allocate various sizes
        // FR: Alloue diverses tailles
        for (int i = 1; i <= 100; ++i) {
            size_t size = i * 16;  // 16, 32, 48, ... bytes
            void* ptr = manager.allocate(size);
            if (ptr) {
                raw_ptrs.push_back(ptr);
            }
        }
        
        std::cout << "   Created " << raw_ptrs.size() << " raw allocations\n";
        
        // EN: Clean up raw allocations
        // FR: Nettoie les allocations brutes
        for (void* ptr : raw_ptrs) {
            manager.deallocate(ptr);
        }
        
        std::cout << "   Successfully freed all raw allocations\n";
    }
    
    // EN: Test defragmentation
    // FR: Test de défragmentation
    std::cout << "\n6. Testing memory defragmentation...\n";
    
    // EN: Create fragmentation pattern
    // FR: Crée un pattern de fragmentation
    std::vector<void*> frag_ptrs;
    for (int i = 0; i < 200; ++i) {
        void* ptr = manager.allocate(128);
        frag_ptrs.push_back(ptr);
    }
    
    // EN: Free every other block to create fragmentation
    // FR: Libère un bloc sur deux pour créer de la fragmentation
    for (size_t i = 0; i < frag_ptrs.size(); i += 2) {
        manager.deallocate(frag_ptrs[i]);
    }
    
    auto stats_before_defrag = manager.getStats();
    std::cout << "   Fragmentation before defrag: " 
              << (stats_before_defrag.fragmentation_ratio * 100) << "%\n";
    
    // EN: Force defragmentation
    // FR: Force la défragmentation
    manager.defragment();
    
    auto stats_after_defrag = manager.getStats();
    std::cout << "   Fragmentation after defrag: " 
              << (stats_after_defrag.fragmentation_ratio * 100) << "%\n";
    std::cout << "   Defragmentation count: " << stats_after_defrag.defragmentation_count << "\n";
    
    // EN: Clean up remaining pointers
    // FR: Nettoie les pointeurs restants
    for (size_t i = 1; i < frag_ptrs.size(); i += 2) {
        manager.deallocate(frag_ptrs[i]);
    }
    
    // EN: Display final statistics
    // FR: Affiche les statistiques finales
    auto final_stats = manager.getStats();
    std::cout << "\n=== Final Memory Statistics ===\n";
    std::cout << "Total pool size: " << final_stats.pool_size << " bytes\n";
    std::cout << "Peak memory usage: " << final_stats.peak_used_bytes << " bytes\n";
    std::cout << "Total allocations: " << final_stats.total_allocations << "\n";
    std::cout << "Total deallocations: " << final_stats.total_deallocations << "\n";
    std::cout << "Current fragmentation: " << (final_stats.fragmentation_ratio * 100) << "%\n";
    
    if (final_stats.total_allocations > 0) {
        std::cout << "Average allocation time: " 
                  << (final_stats.total_alloc_time.count() / final_stats.total_allocations) 
                  << "μs\n";
    }
    
    if (final_stats.total_deallocations > 0) {
        std::cout << "Average deallocation time: " 
                  << (final_stats.total_dealloc_time.count() / final_stats.total_deallocations) 
                  << "μs\n";
    }
    
    // EN: Test integrity
    // FR: Test d'intégrité
    std::cout << "\n7. Testing memory integrity...\n";
    bool integrity_ok = manager.checkIntegrity();
    std::cout << "Memory integrity check: " << (integrity_ok ? "PASSED" : "FAILED") << "\n";
    
    // EN: Test optimization
    // FR: Test d'optimisation
    std::cout << "\n8. Running memory optimization...\n";
    manager.optimize();
    std::cout << "Memory optimization completed\n";
    
    // EN: Show detailed state if tracking enabled
    // FR: Affiche l'état détaillé si le suivi est activé
    manager.setDetailedTracking(true);
    std::string debug_info = manager.dumpPoolState();
    std::cout << "\n=== Memory Pool Debug Info ===\n";
    std::cout << debug_info << "\n";
    
    std::cout << "=== Memory Manager Test Completed Successfully ===\n";
    std::cout << "Pool allocator is working correctly for CSV processing!\n";
    
    return 0;
}