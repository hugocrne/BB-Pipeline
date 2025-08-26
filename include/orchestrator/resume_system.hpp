// EN: Resume System for BB-Pipeline - Intelligent recovery after crash with checkpoint mechanism
// FR: Système de Reprise pour BB-Pipeline - Récupération intelligente après crash avec mécanisme de checkpoint

#pragma once

#include <chrono>
#include <fstream>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <nlohmann/json.hpp>

namespace BBP {

// EN: Forward declarations for dependency injection
// FR: Déclarations forward pour l'injection de dépendances
class Logger;
class ThreadPool;
class PipelineEngine;

// EN: Resume system state enumeration
// FR: Énumération des états du système de reprise
enum class ResumeState {
    IDLE,           // EN: System is idle, no active operation / FR: Système inactif, aucune opération active
    RUNNING,        // EN: Operation is running normally / FR: Opération en cours d'exécution normale
    CHECKPOINTING,  // EN: Creating checkpoint / FR: Création de checkpoint en cours
    PAUSED,         // EN: Operation paused for checkpoint / FR: Opération en pause pour checkpoint
    RECOVERING,     // EN: Recovering from crash / FR: Récupération après crash en cours
    FAILED,         // EN: Recovery failed / FR: Récupération échouée
    COMPLETED       // EN: Operation completed successfully / FR: Opération terminée avec succès
};

// EN: Checkpoint granularity levels
// FR: Niveaux de granularité des checkpoints
enum class CheckpointGranularity {
    COARSE = 0,     // EN: Module-level checkpoints / FR: Checkpoints au niveau module
    MEDIUM = 1,     // EN: Task-level checkpoints / FR: Checkpoints au niveau tâche
    FINE = 2        // EN: Sub-task level checkpoints / FR: Checkpoints au niveau sous-tâche
};

// EN: Checkpoint strategy types
// FR: Types de stratégies de checkpoint
enum class CheckpointStrategy {
    TIME_BASED,     // EN: Based on time intervals / FR: Basé sur les intervalles de temps
    PROGRESS_BASED, // EN: Based on progress percentage / FR: Basé sur le pourcentage de progression
    HYBRID,         // EN: Combination of time and progress / FR: Combinaison de temps et progression
    MANUAL,         // EN: Manual checkpoint creation / FR: Création manuelle de checkpoint
    ADAPTIVE        // EN: Adaptive based on system load / FR: Adaptatif basé sur la charge système
};

// EN: Resume mode configuration
// FR: Configuration du mode de reprise
enum class ResumeMode {
    FULL_RESTART,   // EN: Restart from beginning / FR: Redémarre depuis le début
    LAST_CHECKPOINT, // EN: Resume from last checkpoint / FR: Reprend depuis le dernier checkpoint
    BEST_CHECKPOINT, // EN: Resume from best checkpoint / FR: Reprend depuis le meilleur checkpoint
    INTERACTIVE     // EN: Ask user for resume point / FR: Demande à l'utilisateur le point de reprise
};

// EN: Checkpoint metadata structure
// FR: Structure des métadonnées de checkpoint
struct CheckpointMetadata {
    std::string checkpoint_id;                           // EN: Unique checkpoint identifier / FR: Identifiant unique du checkpoint
    std::chrono::system_clock::time_point timestamp;     // EN: Creation timestamp / FR: Timestamp de création
    std::string pipeline_id;                             // EN: Associated pipeline ID / FR: ID du pipeline associé
    std::string stage_name;                              // EN: Current pipeline stage / FR: Étape actuelle du pipeline
    CheckpointGranularity granularity;                   // EN: Checkpoint granularity level / FR: Niveau de granularité
    double progress_percentage;                          // EN: Overall progress percentage / FR: Pourcentage de progression global
    size_t memory_footprint;                             // EN: Memory usage at checkpoint / FR: Utilisation mémoire au checkpoint
    std::chrono::milliseconds elapsed_time;              // EN: Elapsed execution time / FR: Temps d'exécution écoulé
    std::map<std::string, std::string> custom_metadata;  // EN: Custom metadata fields / FR: Champs de métadonnées personnalisés
    bool is_verified;                                    // EN: Checkpoint verification status / FR: Statut de vérification du checkpoint
    std::string verification_hash;                       // EN: Verification hash / FR: Hash de vérification
};

// EN: Resume context information
// FR: Informations de contexte de reprise
struct ResumeContext {
    std::string operation_id;                            // EN: Operation identifier / FR: Identifiant de l'opération
    std::string pipeline_config_path;                    // EN: Pipeline configuration path / FR: Chemin de configuration du pipeline
    std::vector<std::string> completed_stages;           // EN: List of completed stages / FR: Liste des étapes terminées
    std::vector<std::string> pending_stages;             // EN: List of pending stages / FR: Liste des étapes en attente
    std::map<std::string, nlohmann::json> stage_results; // EN: Results from completed stages / FR: Résultats des étapes terminées
    std::chrono::system_clock::time_point start_time;    // EN: Original start time / FR: Heure de démarrage originale
    std::chrono::system_clock::time_point resume_time;   // EN: Resume timestamp / FR: Timestamp de reprise
    ResumeMode resume_mode;                              // EN: Selected resume mode / FR: Mode de reprise sélectionné
    std::string resume_reason;                           // EN: Reason for resume / FR: Raison de la reprise
};

// EN: Checkpoint configuration
// FR: Configuration des checkpoints
struct CheckpointConfig {
    std::string checkpoint_dir;                          // EN: Checkpoint storage directory / FR: Répertoire de stockage des checkpoints
    CheckpointStrategy strategy;                         // EN: Checkpoint strategy / FR: Stratégie de checkpoint
    CheckpointGranularity granularity;                   // EN: Checkpoint granularity / FR: Granularité des checkpoints
    std::chrono::seconds time_interval;                  // EN: Time-based checkpoint interval / FR: Intervalle de checkpoint basé sur le temps
    double progress_threshold;                           // EN: Progress-based checkpoint threshold / FR: Seuil de checkpoint basé sur la progression
    size_t max_checkpoints;                              // EN: Maximum checkpoints to keep / FR: Nombre maximum de checkpoints à conserver
    bool enable_compression;                             // EN: Enable checkpoint compression / FR: Active la compression des checkpoints
    bool enable_encryption;                              // EN: Enable checkpoint encryption / FR: Active le chiffrement des checkpoints
    std::string encryption_key;                          // EN: Encryption key / FR: Clé de chiffrement
    bool enable_verification;                            // EN: Enable checkpoint verification / FR: Active la vérification des checkpoints
    size_t max_memory_threshold_mb;                      // EN: Max memory before checkpoint / FR: Mémoire max avant checkpoint
    bool auto_cleanup;                                   // EN: Auto-cleanup old checkpoints / FR: Nettoyage automatique des anciens checkpoints
    std::chrono::hours cleanup_age;                      // EN: Age threshold for cleanup / FR: Seuil d'âge pour le nettoyage
};

// EN: Resume statistics for monitoring
// FR: Statistiques de reprise pour monitoring
struct ResumeStatistics {
    std::chrono::system_clock::time_point created_at;    // EN: Statistics creation time / FR: Heure de création des statistiques
    size_t total_resumes;                                // EN: Total resume operations / FR: Total des opérations de reprise
    size_t successful_resumes;                           // EN: Successful resumes / FR: Reprises réussies
    size_t failed_resumes;                               // EN: Failed resumes / FR: Reprises échouées
    std::chrono::milliseconds total_recovery_time;       // EN: Total time spent in recovery / FR: Temps total passé en récupération
    std::chrono::milliseconds average_recovery_time;     // EN: Average recovery time / FR: Temps moyen de récupération
    std::map<std::string, size_t> stage_resume_counts;   // EN: Resume counts by stage / FR: Comptes de reprise par étape
    std::vector<std::string> recent_failures;           // EN: Recent failure reasons / FR: Raisons d'échec récentes
    size_t total_checkpoints_created;                    // EN: Total checkpoints created / FR: Total des checkpoints créés
    size_t checkpoints_used_for_recovery;                // EN: Checkpoints used for recovery / FR: Checkpoints utilisés pour la récupération
    double checkpoint_overhead_percentage;               // EN: Checkpoint overhead / FR: Surcharge des checkpoints
};

namespace detail {
    // EN: Internal checkpoint data structure
    // FR: Structure interne des données de checkpoint
    struct CheckpointData {
        CheckpointMetadata metadata;                     // EN: Checkpoint metadata / FR: Métadonnées du checkpoint
        nlohmann::json pipeline_state;                   // EN: Serialized pipeline state / FR: État sérialisé du pipeline
        std::vector<uint8_t> binary_data;                // EN: Binary data if needed / FR: Données binaires si nécessaire
        
        // EN: Serialize checkpoint to JSON
        // FR: Sérialise le checkpoint en JSON
        nlohmann::json toJson() const;
        
        // EN: Deserialize checkpoint from JSON
        // FR: Désérialise le checkpoint depuis JSON
        static CheckpointData fromJson(const nlohmann::json& json);
        
        // EN: Verify checkpoint integrity
        // FR: Vérifie l'intégrité du checkpoint
        bool verify() const;
    };
    
    // EN: Checkpoint storage interface
    // FR: Interface de stockage des checkpoints
    class ICheckpointStorage {
    public:
        virtual ~ICheckpointStorage() = default;
        
        // EN: Save checkpoint data
        // FR: Sauvegarde les données du checkpoint
        virtual bool saveCheckpoint(const std::string& checkpoint_id, const CheckpointData& data) = 0;
        
        // EN: Load checkpoint data
        // FR: Charge les données du checkpoint
        virtual std::optional<CheckpointData> loadCheckpoint(const std::string& checkpoint_id) = 0;
        
        // EN: List available checkpoints
        // FR: Liste les checkpoints disponibles
        virtual std::vector<std::string> listCheckpoints(const std::string& pipeline_id = "") = 0;
        
        // EN: Delete checkpoint
        // FR: Supprime un checkpoint
        virtual bool deleteCheckpoint(const std::string& checkpoint_id) = 0;
        
        // EN: Get checkpoint metadata
        // FR: Obtient les métadonnées du checkpoint
        virtual std::optional<CheckpointMetadata> getCheckpointMetadata(const std::string& checkpoint_id) = 0;
    };
    
    // EN: File-based checkpoint storage implementation
    // FR: Implémentation de stockage de checkpoint basée sur fichiers
    class FileCheckpointStorage : public ICheckpointStorage {
    public:
        explicit FileCheckpointStorage(const std::string& storage_dir);
        
        bool saveCheckpoint(const std::string& checkpoint_id, const CheckpointData& data) override;
        std::optional<CheckpointData> loadCheckpoint(const std::string& checkpoint_id) override;
        std::vector<std::string> listCheckpoints(const std::string& pipeline_id = "") override;
        bool deleteCheckpoint(const std::string& checkpoint_id) override;
        std::optional<CheckpointMetadata> getCheckpointMetadata(const std::string& checkpoint_id) override;
        
    private:
        std::string storage_dir_;
        mutable std::mutex storage_mutex_;
        
        std::string getCheckpointPath(const std::string& checkpoint_id) const;
        bool ensureStorageDirectory() const;
    };
}

// EN: Main Resume System class - Handles checkpoint creation and recovery
// FR: Classe principale du système de reprise - Gère la création de checkpoints et la récupération
class ResumeSystem {
public:
    // EN: Constructor with configuration
    // FR: Constructeur avec configuration
    explicit ResumeSystem(const CheckpointConfig& config = CheckpointConfig{});
    
    // EN: Destructor
    // FR: Destructeur
    ~ResumeSystem();
    
    // EN: Non-copyable and non-movable
    // FR: Non-copiable et non-déplaçable
    ResumeSystem(const ResumeSystem&) = delete;
    ResumeSystem& operator=(const ResumeSystem&) = delete;
    ResumeSystem(ResumeSystem&&) = delete;
    ResumeSystem& operator=(ResumeSystem&&) = delete;
    
    // EN: Initialize resume system
    // FR: Initialise le système de reprise
    bool initialize();
    
    // EN: Shutdown resume system
    // FR: Arrête le système de reprise
    void shutdown();
    
    // EN: Start monitoring operation for checkpointing
    // FR: Commence le monitoring d'opération pour checkpointing
    bool startMonitoring(const std::string& operation_id, const std::string& pipeline_config_path);
    
    // EN: Stop monitoring current operation
    // FR: Arrête le monitoring de l'opération actuelle
    void stopMonitoring();
    
    // EN: Create manual checkpoint
    // FR: Crée un checkpoint manuel
    std::string createCheckpoint(const std::string& stage_name, 
                                const nlohmann::json& pipeline_state,
                                const std::map<std::string, std::string>& custom_metadata = {});
    
    // EN: Create automatic checkpoint (called by pipeline)
    // FR: Crée un checkpoint automatique (appelé par le pipeline)
    std::string createAutomaticCheckpoint(const std::string& stage_name, 
                                         const nlohmann::json& pipeline_state,
                                         double progress_percentage = 0.0);
    
    // EN: Check if resume is possible for an operation
    // FR: Vérifie si la reprise est possible pour une opération
    bool canResume(const std::string& operation_id) const;
    
    // EN: Get available resume points for an operation
    // FR: Obtient les points de reprise disponibles pour une opération
    std::vector<CheckpointMetadata> getAvailableResumePoints(const std::string& operation_id) const;
    
    // EN: Resume operation from checkpoint
    // FR: Reprend l'opération depuis un checkpoint
    std::optional<ResumeContext> resumeFromCheckpoint(const std::string& checkpoint_id, 
                                                      ResumeMode mode = ResumeMode::LAST_CHECKPOINT);
    
    // EN: Resume operation automatically (finds best checkpoint)
    // FR: Reprend l'opération automatiquement (trouve le meilleur checkpoint)
    std::optional<ResumeContext> resumeAutomatically(const std::string& operation_id);
    
    // EN: Verify checkpoint integrity
    // FR: Vérifie l'intégrité du checkpoint
    bool verifyCheckpoint(const std::string& checkpoint_id) const;
    
    // EN: List all checkpoints for an operation
    // FR: Liste tous les checkpoints pour une opération
    std::vector<std::string> listCheckpoints(const std::string& operation_id = "") const;
    
    // EN: Delete checkpoint
    // FR: Supprime un checkpoint
    bool deleteCheckpoint(const std::string& checkpoint_id);
    
    // EN: Clean up old checkpoints
    // FR: Nettoie les anciens checkpoints
    size_t cleanupOldCheckpoints();
    
    // EN: Get current resume state
    // FR: Obtient l'état actuel de reprise
    ResumeState getCurrentState() const;
    
    // EN: Get resume statistics
    // FR: Obtient les statistiques de reprise
    ResumeStatistics getStatistics() const;
    
    // EN: Reset statistics
    // FR: Remet à zéro les statistiques
    void resetStatistics();
    
    // EN: Update configuration
    // FR: Met à jour la configuration
    void updateConfig(const CheckpointConfig& config);
    
    // EN: Get current configuration
    // FR: Obtient la configuration actuelle
    const CheckpointConfig& getConfig() const { return config_; }
    
    // EN: Set progress callback for monitoring
    // FR: Définit le callback de progression pour monitoring
    void setProgressCallback(std::function<void(const std::string&, double)> callback);
    
    // EN: Set checkpoint callback
    // FR: Définit le callback de checkpoint
    void setCheckpointCallback(std::function<void(const std::string&, const CheckpointMetadata&)> callback);
    
    // EN: Set recovery callback
    // FR: Définit le callback de récupération
    void setRecoveryCallback(std::function<void(const std::string&, bool)> callback);
    
    // EN: Force checkpoint creation (emergency)
    // FR: Force la création de checkpoint (urgence)
    std::string forceCheckpoint(const std::string& reason);
    
    // EN: Register custom serializer for specific data types
    // FR: Enregistre un sérialiseur personnalisé pour des types de données spécifiques
    void registerSerializer(const std::string& type_name, 
                           std::function<nlohmann::json(const void*)> serializer,
                           std::function<void(const nlohmann::json&, void*)> deserializer);
    
    // EN: Enable/disable detailed logging
    // FR: Active/désactive le logging détaillé
    void setDetailedLogging(bool enabled);
    
    // EN: Get checkpoint metadata
    // FR: Obtient les métadonnées du checkpoint
    std::optional<CheckpointMetadata> getCheckpointMetadata(const std::string& checkpoint_id) const;

private:
    // EN: PIMPL to hide implementation details
    // FR: PIMPL pour cacher les détails d'implémentation
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    
    // EN: Configuration
    // FR: Configuration
    CheckpointConfig config_;
    
    // EN: Current state
    // FR: État actuel
    std::atomic<ResumeState> current_state_;
    
    // EN: Statistics
    // FR: Statistiques
    mutable std::mutex stats_mutex_;
    ResumeStatistics statistics_;
    
    // EN: Thread safety
    // FR: Thread safety
    mutable std::mutex mutex_;
};

// EN: Resume System Manager - Singleton for global resume management
// FR: Gestionnaire du système de reprise - Singleton pour la gestion globale de reprise
class ResumeSystemManager {
public:
    // EN: Get singleton instance
    // FR: Obtient l'instance singleton
    static ResumeSystemManager& getInstance();
    
    // EN: Initialize with configuration
    // FR: Initialise avec la configuration
    bool initialize(const CheckpointConfig& config = CheckpointConfig{});
    
    // EN: Shutdown manager
    // FR: Arrête le gestionnaire
    void shutdown();
    
    // EN: Get resume system
    // FR: Obtient le système de reprise
    ResumeSystem& getResumeSystem();
    
    // EN: Register pipeline for auto-checkpointing
    // FR: Enregistre le pipeline pour checkpointing automatique
    bool registerPipeline(const std::string& pipeline_id, PipelineEngine* pipeline);
    
    // EN: Unregister pipeline
    // FR: Désenregistre le pipeline
    void unregisterPipeline(const std::string& pipeline_id);
    
    // EN: Handle crash recovery
    // FR: Gère la récupération après crash
    std::vector<std::string> detectCrashedOperations();
    
    // EN: Automatic recovery attempt
    // FR: Tentative de récupération automatique
    bool attemptAutomaticRecovery(const std::string& operation_id);
    
    // EN: Get global statistics
    // FR: Obtient les statistiques globales
    ResumeStatistics getGlobalStatistics() const;

private:
    // EN: Private constructor for singleton
    // FR: Constructeur privé pour singleton
    ResumeSystemManager() = default;
    
    // EN: Non-copyable and non-movable
    // FR: Non-copiable et non-déplaçable
    ResumeSystemManager(const ResumeSystemManager&) = delete;
    ResumeSystemManager& operator=(const ResumeSystemManager&) = delete;
    ResumeSystemManager(ResumeSystemManager&&) = delete;
    ResumeSystemManager& operator=(ResumeSystemManager&&) = delete;
    
    std::unique_ptr<ResumeSystem> resume_system_;
    std::unordered_map<std::string, PipelineEngine*> registered_pipelines_;
    mutable std::mutex manager_mutex_;
    bool initialized_ = false;
};

// EN: RAII helper for automatic checkpoint management
// FR: Helper RAII pour la gestion automatique des checkpoints
class AutoCheckpointGuard {
public:
    // EN: Constructor with operation details
    // FR: Constructeur avec détails de l'opération
    AutoCheckpointGuard(const std::string& operation_id, 
                       const std::string& stage_name,
                       ResumeSystem& resume_system);
    
    // EN: Destructor - creates final checkpoint
    // FR: Destructeur - crée le checkpoint final
    ~AutoCheckpointGuard();
    
    // EN: Update progress
    // FR: Met à jour la progression
    void updateProgress(double percentage);
    
    // EN: Set pipeline state
    // FR: Définit l'état du pipeline
    void setPipelineState(const nlohmann::json& state);
    
    // EN: Add custom metadata
    // FR: Ajoute des métadonnées personnalisées
    void addMetadata(const std::string& key, const std::string& value);
    
    // EN: Force checkpoint creation
    // FR: Force la création de checkpoint
    std::string forceCheckpoint();

private:
    std::string operation_id_;
    std::string stage_name_;
    ResumeSystem& resume_system_;
    nlohmann::json current_state_;
    std::map<std::string, std::string> metadata_;
    double current_progress_;
    std::chrono::system_clock::time_point start_time_;
    std::string last_checkpoint_id_;
};

// EN: Utility functions for resume system operations
// FR: Fonctions utilitaires pour les opérations du système de reprise
namespace ResumeSystemUtils {
    
    // EN: Create default checkpoint configuration
    // FR: Crée la configuration de checkpoint par défaut
    CheckpointConfig createDefaultConfig();
    
    // EN: Create high-frequency checkpoint configuration
    // FR: Crée la configuration de checkpoint haute fréquence
    CheckpointConfig createHighFrequencyConfig();
    
    // EN: Create low-overhead checkpoint configuration
    // FR: Crée la configuration de checkpoint faible surcharge
    CheckpointConfig createLowOverheadConfig();
    
    // EN: Estimate checkpoint size for given state
    // FR: Estime la taille du checkpoint pour l'état donné
    size_t estimateCheckpointSize(const nlohmann::json& pipeline_state);
    
    // EN: Validate checkpoint configuration
    // FR: Valide la configuration de checkpoint
    bool validateConfig(const CheckpointConfig& config);
    
    // EN: Generate unique operation ID
    // FR: Génère un ID d'opération unique
    std::string generateOperationId();
    
    // EN: Parse resume context from command line arguments
    // FR: Parse le contexte de reprise depuis les arguments de ligne de commande
    std::optional<ResumeContext> parseResumeContext(const std::vector<std::string>& args);
    
    // EN: Compress checkpoint data
    // FR: Compresse les données du checkpoint
    std::vector<uint8_t> compressCheckpointData(const std::vector<uint8_t>& data);
    
    // EN: Decompress checkpoint data
    // FR: Décompresse les données du checkpoint
    std::vector<uint8_t> decompressCheckpointData(const std::vector<uint8_t>& compressed_data);
    
    // EN: Encrypt checkpoint data
    // FR: Chiffre les données du checkpoint
    std::vector<uint8_t> encryptCheckpointData(const std::vector<uint8_t>& data, const std::string& key);
    
    // EN: Decrypt checkpoint data
    // FR: Déchiffre les données du checkpoint
    std::vector<uint8_t> decryptCheckpointData(const std::vector<uint8_t>& encrypted_data, const std::string& key);
    
} // namespace ResumeSystemUtils

// EN: Convenience macros for checkpoint management
// FR: Macros de convenance pour la gestion des checkpoints

#define RESUME_SYSTEM_CHECKPOINT(operation_id, stage_name, state) \
    ResumeSystemManager::getInstance().getResumeSystem().createAutomaticCheckpoint(stage_name, state)

#define RESUME_SYSTEM_AUTO_GUARD(operation_id, stage_name) \
    AutoCheckpointGuard _auto_checkpoint_guard(operation_id, stage_name, ResumeSystemManager::getInstance().getResumeSystem())

#define RESUME_SYSTEM_UPDATE_PROGRESS(percentage) \
    if (auto* guard = dynamic_cast<AutoCheckpointGuard*>(&_auto_checkpoint_guard)) { \
        guard->updateProgress(percentage); \
    }

} // namespace BBP