// EN: Resume System implementation for BB-Pipeline - Intelligent recovery after crash with checkpoint mechanism
// FR: Implémentation du système de reprise pour BB-Pipeline - Récupération intelligente après crash avec mécanisme de checkpoint

#include "orchestrator/resume_system.hpp"
#include "infrastructure/logging/logger.hpp"
#include "infrastructure/threading/thread_pool.hpp"
#include "orchestrator/pipeline_engine.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <random>
#include <cstring>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zlib.h>

namespace BBP {

namespace detail {

// EN: Convert timestamp to ISO8601 string
// FR: Convertit le timestamp en chaîne ISO8601
std::string timestampToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return ss.str();
}

// EN: Parse timestamp from ISO8601 string
// FR: Parse le timestamp depuis une chaîne ISO8601
std::chrono::system_clock::time_point timestampFromString(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    
    // EN: Handle milliseconds if present
    // FR: Gère les millisecondes si présentes
    if (str.find('.') != std::string::npos) {
        auto dot_pos = str.find('.');
        auto ms_str = str.substr(dot_pos + 1, 3);
        if (!ms_str.empty() && std::all_of(ms_str.begin(), ms_str.end(), ::isdigit)) {
            auto ms = std::chrono::milliseconds(std::stoi(ms_str));
            tp += ms;
        }
    }
    
    return tp;
}

// EN: Generate SHA-256 hash for verification
// FR: Génère un hash SHA-256 pour vérification
std::string generateHash(const std::string& data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return "";
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data.c_str(), data.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    EVP_MD_CTX_free(ctx);
    
    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

// EN: CheckpointData implementation
// FR: Implémentation de CheckpointData
nlohmann::json CheckpointData::toJson() const {
    nlohmann::json json;
    
    // EN: Serialize metadata
    // FR: Sérialise les métadonnées
    json["metadata"]["checkpoint_id"] = metadata.checkpoint_id;
    json["metadata"]["timestamp"] = timestampToString(metadata.timestamp);
    json["metadata"]["pipeline_id"] = metadata.pipeline_id;
    json["metadata"]["stage_name"] = metadata.stage_name;
    json["metadata"]["granularity"] = static_cast<int>(metadata.granularity);
    json["metadata"]["progress_percentage"] = metadata.progress_percentage;
    json["metadata"]["memory_footprint"] = metadata.memory_footprint;
    json["metadata"]["elapsed_time"] = metadata.elapsed_time.count();
    json["metadata"]["custom_metadata"] = metadata.custom_metadata;
    json["metadata"]["is_verified"] = metadata.is_verified;
    json["metadata"]["verification_hash"] = metadata.verification_hash;
    
    // EN: Serialize pipeline state
    // FR: Sérialise l'état du pipeline
    json["pipeline_state"] = pipeline_state;
    
    // EN: Serialize binary data as base64 if present
    // FR: Sérialise les données binaires en base64 si présentes
    if (!binary_data.empty()) {
        json["binary_data"] = nlohmann::json::binary(binary_data);
    }
    
    return json;
}

CheckpointData CheckpointData::fromJson(const nlohmann::json& json) {
    CheckpointData data;
    
    // EN: Deserialize metadata
    // FR: Désérialise les métadonnées
    if (json.contains("metadata")) {
        const auto& meta = json["metadata"];
        data.metadata.checkpoint_id = meta.value("checkpoint_id", "");
        data.metadata.timestamp = timestampFromString(meta.value("timestamp", ""));
        data.metadata.pipeline_id = meta.value("pipeline_id", "");
        data.metadata.stage_name = meta.value("stage_name", "");
        data.metadata.granularity = static_cast<CheckpointGranularity>(meta.value("granularity", 0));
        data.metadata.progress_percentage = meta.value("progress_percentage", 0.0);
        data.metadata.memory_footprint = meta.value("memory_footprint", 0UL);
        data.metadata.elapsed_time = std::chrono::milliseconds(meta.value("elapsed_time", 0));
        data.metadata.custom_metadata = meta.value("custom_metadata", std::map<std::string, std::string>{});
        data.metadata.is_verified = meta.value("is_verified", false);
        data.metadata.verification_hash = meta.value("verification_hash", "");
    }
    
    // EN: Deserialize pipeline state
    // FR: Désérialise l'état du pipeline
    if (json.contains("pipeline_state")) {
        data.pipeline_state = json["pipeline_state"];
    }
    
    // EN: Deserialize binary data if present
    // FR: Désérialise les données binaires si présentes
    if (json.contains("binary_data")) {
        data.binary_data = json["binary_data"].get_binary();
    }
    
    return data;
}

bool CheckpointData::verify() const {
    if (!metadata.is_verified) {
        return false;
    }
    
    // EN: Generate hash of current data and compare
    // FR: Génère le hash des données actuelles et compare
    auto json_str = toJson().dump();
    auto current_hash = generateHash(json_str);
    
    return current_hash == metadata.verification_hash;
}

// EN: FileCheckpointStorage implementation
// FR: Implémentation de FileCheckpointStorage
FileCheckpointStorage::FileCheckpointStorage(const std::string& storage_dir) 
    : storage_dir_(storage_dir) {
    ensureStorageDirectory();
}

bool FileCheckpointStorage::saveCheckpoint(const std::string& checkpoint_id, const CheckpointData& data) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        auto filepath = getCheckpointPath(checkpoint_id);
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        auto json_data = data.toJson();
        file << json_data.dump(2);
        file.close();
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("ResumeSystem", "Failed to save checkpoint " + checkpoint_id + ": " + e.what());
        return false;
    }
}

std::optional<CheckpointData> FileCheckpointStorage::loadCheckpoint(const std::string& checkpoint_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        auto filepath = getCheckpointPath(checkpoint_id);
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return std::nullopt;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        
        auto json_data = nlohmann::json::parse(content);
        return CheckpointData::fromJson(json_data);
    } catch (const std::exception& e) {
        LOG_ERROR("ResumeSystem", "Failed to load checkpoint " + checkpoint_id + ": " + e.what());
        return std::nullopt;
    }
}

std::vector<std::string> FileCheckpointStorage::listCheckpoints(const std::string& pipeline_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<std::string> checkpoints;
    
    try {
        if (!std::filesystem::exists(storage_dir_)) {
            return checkpoints;
        }
        
        for (const auto& entry : std::filesystem::directory_iterator(storage_dir_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".checkpoint") {
                auto filename = entry.path().stem().string();
                
                if (pipeline_id.empty()) {
                    checkpoints.push_back(filename);
                } else {
                    // EN: Filter by pipeline ID if specified
                    // FR: Filtre par ID de pipeline si spécifié
                    auto metadata = getCheckpointMetadata(filename);
                    if (metadata && metadata->pipeline_id == pipeline_id) {
                        checkpoints.push_back(filename);
                    }
                }
            }
        }
        
        // EN: Sort by timestamp (newest first)
        // FR: Trie par timestamp (plus récent d'abord)
        std::sort(checkpoints.begin(), checkpoints.end(), [this](const std::string& a, const std::string& b) {
            auto meta_a = getCheckpointMetadata(a);
            auto meta_b = getCheckpointMetadata(b);
            if (meta_a && meta_b) {
                return meta_a->timestamp > meta_b->timestamp;
            }
            return a > b;
        });
        
    } catch (const std::exception& e) {
        LOG_ERROR("ResumeSystem", "Failed to list checkpoints: " + std::string(e.what()));
    }
    
    return checkpoints;
}

bool FileCheckpointStorage::deleteCheckpoint(const std::string& checkpoint_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    try {
        auto filepath = getCheckpointPath(checkpoint_id);
        if (std::filesystem::exists(filepath)) {
            return std::filesystem::remove(filepath);
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("ResumeSystem", "Failed to delete checkpoint " + checkpoint_id + ": " + e.what());
        return false;
    }
}

std::optional<CheckpointMetadata> FileCheckpointStorage::getCheckpointMetadata(const std::string& checkpoint_id) {
    try {
        auto data = loadCheckpoint(checkpoint_id);
        if (data) {
            return data->metadata;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("ResumeSystem", "Failed to get checkpoint metadata " + checkpoint_id + ": " + e.what());
    }
    
    return std::nullopt;
}

std::string FileCheckpointStorage::getCheckpointPath(const std::string& checkpoint_id) const {
    return (std::filesystem::path(storage_dir_) / (checkpoint_id + ".checkpoint")).string();
}

bool FileCheckpointStorage::ensureStorageDirectory() const {
    try {
        if (!std::filesystem::exists(storage_dir_)) {
            std::filesystem::create_directories(storage_dir_);
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("ResumeSystem", "Failed to create storage directory: " + std::string(e.what()));
        return false;
    }
}

} // namespace detail

// EN: ResumeSystem PIMPL implementation
// FR: Implémentation PIMPL de ResumeSystem
class ResumeSystem::Impl {
public:
    // EN: Constructor
    // FR: Constructeur
    explicit Impl(const CheckpointConfig& config) 
        : config_(config)
        , current_state_(ResumeState::IDLE)
        , monitoring_thread_(nullptr)
        , shutdown_requested_(false)
        , last_checkpoint_time_(std::chrono::system_clock::now())
        , last_progress_(0.0)
        , checkpoint_counter_(0)
        , detailed_logging_(false) {
        
        // EN: Initialize storage
        // FR: Initialise le stockage
        storage_ = std::make_unique<detail::FileCheckpointStorage>(config_.checkpoint_dir);
        
        // EN: Initialize statistics
        // FR: Initialise les statistiques
        statistics_.created_at = std::chrono::system_clock::now();
    }
    
    // EN: Destructor
    // FR: Destructeur
    ~Impl() {
        shutdown();
    }
    
    // EN: Initialize implementation
    // FR: Initialise l'implémentation
    bool initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (current_state_ != ResumeState::IDLE) {
            return false;
        }
        
        try {
            // EN: Initialize monitoring thread if needed
            // FR: Initialise le thread de monitoring si nécessaire
            if (config_.strategy == CheckpointStrategy::TIME_BASED || 
                config_.strategy == CheckpointStrategy::HYBRID ||
                config_.strategy == CheckpointStrategy::ADAPTIVE) {
                startMonitoringThread();
            }
            
            LOG_INFO("ResumeSystem", "Resume system initialized successfully");
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("ResumeSystem", "Failed to initialize resume system: " + std::string(e.what()));
            return false;
        }
    }
    
    // EN: Shutdown implementation
    // FR: Arrête l'implémentation
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_requested_ = true;
            current_state_ = ResumeState::IDLE;
        }
        
        monitoring_condition_.notify_all();
        
        if (monitoring_thread_ && monitoring_thread_->joinable()) {
            monitoring_thread_->join();
            monitoring_thread_.reset();
        }
        
        LOG_INFO("ResumeSystem", "Resume system shutdown completed");
    }
    
    // EN: Start monitoring operation
    // FR: Commence le monitoring d'opération
    bool startMonitoring(const std::string& operation_id, const std::string& pipeline_config_path) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (current_state_ != ResumeState::IDLE) {
            return false;
        }
        
        current_operation_id_ = operation_id;
        current_pipeline_config_ = pipeline_config_path;
        current_state_ = ResumeState::RUNNING;
        operation_start_time_ = std::chrono::system_clock::now();
        last_checkpoint_time_ = operation_start_time_;
        last_progress_ = 0.0;
        
        LOG_INFO("ResumeSystem", "Started monitoring operation: " + operation_id);
        return true;
    }
    
    // EN: Stop monitoring
    // FR: Arrête le monitoring
    void stopMonitoring() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (current_state_ == ResumeState::RUNNING) {
            current_state_ = ResumeState::IDLE;
            current_operation_id_.clear();
            current_pipeline_config_.clear();
            
            LOG_INFO("ResumeSystem", "Stopped monitoring operation");
        }
    }
    
    // EN: Create checkpoint
    // FR: Crée un checkpoint
    std::string createCheckpoint(const std::string& stage_name, 
                                const nlohmann::json& pipeline_state,
                                const std::map<std::string, std::string>& custom_metadata,
                                double progress_percentage,
                                bool is_automatic) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            // EN: Generate unique checkpoint ID
            // FR: Génère un ID unique de checkpoint
            auto checkpoint_id = generateCheckpointId();
            
            // EN: Create checkpoint metadata
            // FR: Crée les métadonnées du checkpoint
            detail::CheckpointMetadata metadata;
            metadata.checkpoint_id = checkpoint_id;
            metadata.timestamp = std::chrono::system_clock::now();
            metadata.pipeline_id = current_operation_id_;
            metadata.stage_name = stage_name;
            metadata.granularity = config_.granularity;
            metadata.progress_percentage = progress_percentage;
            metadata.memory_footprint = estimateMemoryUsage();
            metadata.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                metadata.timestamp - operation_start_time_);
            metadata.custom_metadata = custom_metadata;
            metadata.is_verified = config_.enable_verification;
            
            // EN: Create checkpoint data
            // FR: Crée les données du checkpoint
            detail::CheckpointData data;
            data.metadata = metadata;
            data.pipeline_state = pipeline_state;
            
            // EN: Generate verification hash if enabled
            // FR: Génère le hash de vérification si activé
            if (config_.enable_verification) {
                auto json_str = data.toJson().dump();
                data.metadata.verification_hash = detail::generateHash(json_str);
            }
            
            // EN: Compress data if enabled
            // FR: Compresse les données si activé
            if (config_.enable_compression && !pipeline_state.empty()) {
                auto json_str = pipeline_state.dump();
                data.binary_data = ResumeSystemUtils::compressCheckpointData(
                    std::vector<uint8_t>(json_str.begin(), json_str.end()));
            }
            
            // EN: Encrypt data if enabled
            // FR: Chiffre les données si activé
            if (config_.enable_encryption && !config_.encryption_key.empty()) {
                if (!data.binary_data.empty()) {
                    data.binary_data = ResumeSystemUtils::encryptCheckpointData(data.binary_data, config_.encryption_key);
                }
            }
            
            // EN: Save checkpoint
            // FR: Sauvegarde le checkpoint
            if (!storage_->saveCheckpoint(checkpoint_id, data)) {
                throw std::runtime_error("Failed to save checkpoint to storage");
            }
            
            // EN: Update statistics
            // FR: Met à jour les statistiques
            updateStatistics(true, is_automatic);
            
            // EN: Call checkpoint callback
            // FR: Appelle le callback de checkpoint
            if (checkpoint_callback_) {
                checkpoint_callback_(checkpoint_id, metadata);
            }
            
            // EN: Clean up old checkpoints if needed
            // FR: Nettoie les anciens checkpoints si nécessaire
            if (config_.auto_cleanup) {
                cleanupOldCheckpoints();
            }
            
            last_checkpoint_time_ = metadata.timestamp;
            last_progress_ = progress_percentage;
            
            if (detailed_logging_) {
                LOG_INFO("ResumeSystem", "Created checkpoint " + checkpoint_id + " for stage " + stage_name);
            }
            
            return checkpoint_id;
            
        } catch (const std::exception& e) {
            LOG_ERROR("ResumeSystem", "Failed to create checkpoint: " + std::string(e.what()));
            updateStatistics(false, is_automatic);
            return "";
        }
    }
    
    // EN: Check if resume is possible
    // FR: Vérifie si la reprise est possible
    bool canResume(const std::string& operation_id) const {
        auto checkpoints = storage_->listCheckpoints(operation_id);
        return !checkpoints.empty();
    }
    
    // EN: Get available resume points
    // FR: Obtient les points de reprise disponibles
    std::vector<CheckpointMetadata> getAvailableResumePoints(const std::string& operation_id) const {
        std::vector<CheckpointMetadata> resume_points;
        
        auto checkpoint_ids = storage_->listCheckpoints(operation_id);
        for (const auto& checkpoint_id : checkpoint_ids) {
            auto metadata = storage_->getCheckpointMetadata(checkpoint_id);
            if (metadata) {
                resume_points.push_back(*metadata);
            }
        }
        
        return resume_points;
    }
    
    // EN: Resume from checkpoint
    // FR: Reprend depuis un checkpoint
    std::optional<ResumeContext> resumeFromCheckpoint(const std::string& checkpoint_id, ResumeMode mode) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            current_state_ = ResumeState::RECOVERING;
            
            // EN: Load checkpoint data
            // FR: Charge les données du checkpoint
            auto data = storage_->loadCheckpoint(checkpoint_id);
            if (!data) {
                current_state_ = ResumeState::FAILED;
                return std::nullopt;
            }
            
            // EN: Verify checkpoint if enabled
            // FR: Vérifie le checkpoint si activé
            if (config_.enable_verification && !data->verify()) {
                LOG_ERROR("ResumeSystem", "Checkpoint verification failed: " + checkpoint_id);
                current_state_ = ResumeState::FAILED;
                return std::nullopt;
            }
            
            // EN: Decrypt data if needed
            // FR: Déchiffre les données si nécessaire
            if (config_.enable_encryption && !config_.encryption_key.empty() && !data->binary_data.empty()) {
                data->binary_data = ResumeSystemUtils::decryptCheckpointData(data->binary_data, config_.encryption_key);
            }
            
            // EN: Decompress data if needed
            // FR: Décompresse les données si nécessaire
            if (config_.enable_compression && !data->binary_data.empty()) {
                data->binary_data = ResumeSystemUtils::decompressCheckpointData(data->binary_data);
                
                // EN: Reconstruct pipeline state from binary data
                // FR: Reconstruit l'état du pipeline depuis les données binaires
                if (!data->binary_data.empty()) {
                    std::string json_str(data->binary_data.begin(), data->binary_data.end());
                    data->pipeline_state = nlohmann::json::parse(json_str);
                }
            }
            
            // EN: Create resume context
            // FR: Crée le contexte de reprise
            ResumeContext context;
            context.operation_id = data->metadata.pipeline_id;
            context.pipeline_config_path = current_pipeline_config_;
            context.start_time = data->metadata.timestamp - data->metadata.elapsed_time;
            context.resume_time = std::chrono::system_clock::now();
            context.resume_mode = mode;
            context.resume_reason = "Resume from checkpoint " + checkpoint_id;
            
            // EN: Extract completed and pending stages from pipeline state
            // FR: Extrait les étapes terminées et en attente depuis l'état du pipeline
            if (data->pipeline_state.contains("completed_stages")) {
                context.completed_stages = data->pipeline_state["completed_stages"].get<std::vector<std::string>>();
            }
            if (data->pipeline_state.contains("pending_stages")) {
                context.pending_stages = data->pipeline_state["pending_stages"].get<std::vector<std::string>>();
            }
            if (data->pipeline_state.contains("stage_results")) {
                context.stage_results = data->pipeline_state["stage_results"].get<std::map<std::string, nlohmann::json>>();
            }
            
            // EN: Update statistics
            // FR: Met à jour les statistiques
            updateStatistics(true, false);
            statistics_.successful_resumes++;
            statistics_.total_recovery_time += std::chrono::duration_cast<std::chrono::milliseconds>(
                context.resume_time - context.start_time);
            
            // EN: Call recovery callback
            // FR: Appelle le callback de récupération
            if (recovery_callback_) {
                recovery_callback_(checkpoint_id, true);
            }
            
            current_state_ = ResumeState::RUNNING;
            current_operation_id_ = context.operation_id;
            operation_start_time_ = context.start_time;
            
            LOG_INFO("ResumeSystem", "Successfully resumed from checkpoint " + checkpoint_id);
            
            return context;
            
        } catch (const std::exception& e) {
            LOG_ERROR("ResumeSystem", "Failed to resume from checkpoint: " + std::string(e.what()));
            
            current_state_ = ResumeState::FAILED;
            statistics_.failed_resumes++;
            
            if (recovery_callback_) {
                recovery_callback_(checkpoint_id, false);
            }
            
            return std::nullopt;
        }
    }
    
    // EN: Resume automatically
    // FR: Reprend automatiquement
    std::optional<ResumeContext> resumeAutomatically(const std::string& operation_id) {
        auto checkpoints = getAvailableResumePoints(operation_id);
        if (checkpoints.empty()) {
            return std::nullopt;
        }
        
        // EN: Find the best checkpoint (most recent with highest progress)
        // FR: Trouve le meilleur checkpoint (plus récent avec progression la plus élevée)
        auto best_checkpoint = std::max_element(checkpoints.begin(), checkpoints.end(),
            [](const CheckpointMetadata& a, const CheckpointMetadata& b) {
                if (std::abs(a.progress_percentage - b.progress_percentage) > 0.01) {
                    return a.progress_percentage < b.progress_percentage;
                }
                return a.timestamp < b.timestamp;
            });
        
        return resumeFromCheckpoint(best_checkpoint->checkpoint_id, ResumeMode::BEST_CHECKPOINT);
    }
    
    // EN: Verify checkpoint
    // FR: Vérifie le checkpoint
    bool verifyCheckpoint(const std::string& checkpoint_id) const {
        auto data = storage_->loadCheckpoint(checkpoint_id);
        return data && data->verify();
    }
    
    // EN: List checkpoints
    // FR: Liste les checkpoints
    std::vector<std::string> listCheckpoints(const std::string& operation_id) const {
        return storage_->listCheckpoints(operation_id);
    }
    
    // EN: Delete checkpoint
    // FR: Supprime un checkpoint
    bool deleteCheckpoint(const std::string& checkpoint_id) {
        return storage_->deleteCheckpoint(checkpoint_id);
    }
    
    // EN: Clean up old checkpoints
    // FR: Nettoie les anciens checkpoints
    size_t cleanupOldCheckpoints() {
        if (!config_.auto_cleanup) {
            return 0;
        }
        
        auto all_checkpoints = storage_->listCheckpoints();
        size_t deleted_count = 0;
        auto now = std::chrono::system_clock::now();
        
        for (const auto& checkpoint_id : all_checkpoints) {
            auto metadata = storage_->getCheckpointMetadata(checkpoint_id);
            if (!metadata) {
                continue;
            }
            
            // EN: Delete if older than cleanup age
            // FR: Supprime si plus ancien que l'âge de nettoyage
            auto age = std::chrono::duration_cast<std::chrono::hours>(now - metadata->timestamp);
            if (age > config_.cleanup_age) {
                if (storage_->deleteCheckpoint(checkpoint_id)) {
                    deleted_count++;
                }
            }
        }
        
        // EN: Keep only max_checkpoints for each operation
        // FR: Garde seulement max_checkpoints pour chaque opération
        std::unordered_map<std::string, std::vector<std::string>> operation_checkpoints;
        for (const auto& checkpoint_id : all_checkpoints) {
            auto metadata = storage_->getCheckpointMetadata(checkpoint_id);
            if (metadata) {
                operation_checkpoints[metadata->pipeline_id].push_back(checkpoint_id);
            }
        }
        
        for (auto& [operation_id, checkpoints] : operation_checkpoints) {
            if (checkpoints.size() > config_.max_checkpoints) {
                // EN: Sort by timestamp and keep only the newest
                // FR: Trie par timestamp et garde seulement les plus récents
                std::sort(checkpoints.begin(), checkpoints.end(), 
                    [this](const std::string& a, const std::string& b) {
                        auto meta_a = storage_->getCheckpointMetadata(a);
                        auto meta_b = storage_->getCheckpointMetadata(b);
                        if (meta_a && meta_b) {
                            return meta_a->timestamp > meta_b->timestamp;
                        }
                        return false;
                    });
                
                for (size_t i = config_.max_checkpoints; i < checkpoints.size(); ++i) {
                    if (storage_->deleteCheckpoint(checkpoints[i])) {
                        deleted_count++;
                    }
                }
            }
        }
        
        if (deleted_count > 0 && detailed_logging_) {
            LOG_INFO("ResumeSystem", "Cleaned up " + std::to_string(deleted_count) + " old checkpoints");
        }
        
        return deleted_count;
    }
    
    // EN: Get checkpoint metadata
    // FR: Obtient les métadonnées du checkpoint
    std::optional<CheckpointMetadata> getCheckpointMetadata(const std::string& checkpoint_id) const {
        return storage_->getCheckpointMetadata(checkpoint_id);
    }
    
    // EN: Update configuration
    // FR: Met à jour la configuration
    void updateConfig(const CheckpointConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        
        // EN: Reinitialize storage if directory changed
        // FR: Réinitialise le stockage si le répertoire a changé
        storage_ = std::make_unique<detail::FileCheckpointStorage>(config_.checkpoint_dir);
    }
    
    // EN: Get current state
    // FR: Obtient l'état actuel
    ResumeState getCurrentState() const {
        return current_state_.load();
    }
    
    // EN: Get statistics
    // FR: Obtient les statistiques
    ResumeStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        auto stats = statistics_;
        
        // EN: Calculate average recovery time
        // FR: Calcule le temps moyen de récupération
        if (stats.successful_resumes > 0) {
            stats.average_recovery_time = std::chrono::milliseconds(
                stats.total_recovery_time.count() / stats.successful_resumes);
        }
        
        // EN: Calculate checkpoint overhead
        // FR: Calcule la surcharge des checkpoints
        if (checkpoint_counter_ > 0 && operation_start_time_ != std::chrono::system_clock::time_point{}) {
            auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - operation_start_time_);
            if (total_time.count() > 0) {
                stats.checkpoint_overhead_percentage = (stats.total_recovery_time.count() * 100.0) / total_time.count();
            }
        }
        
        return stats;
    }
    
    // EN: Reset statistics
    // FR: Remet à zéro les statistiques
    void resetStatistics() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        statistics_ = ResumeStatistics{};
        statistics_.created_at = std::chrono::system_clock::now();
        checkpoint_counter_ = 0;
    }
    
    // EN: Set callbacks
    // FR: Définit les callbacks
    void setProgressCallback(std::function<void(const std::string&, double)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        progress_callback_ = std::move(callback);
    }
    
    void setCheckpointCallback(std::function<void(const std::string&, const CheckpointMetadata&)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        checkpoint_callback_ = std::move(callback);
    }
    
    void setRecoveryCallback(std::function<void(const std::string&, bool)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        recovery_callback_ = std::move(callback);
    }
    
    // EN: Force checkpoint
    // FR: Force un checkpoint
    std::string forceCheckpoint(const std::string& reason) {
        if (current_state_ != ResumeState::RUNNING) {
            return "";
        }
        
        std::map<std::string, std::string> metadata;
        metadata["force_reason"] = reason;
        metadata["force_timestamp"] = detail::timestampToString(std::chrono::system_clock::now());
        
        return createCheckpoint("force_checkpoint", nlohmann::json{}, metadata, last_progress_, false);
    }
    
    // EN: Set detailed logging
    // FR: Définit le logging détaillé
    void setDetailedLogging(bool enabled) {
        detailed_logging_ = enabled;
    }

private:
    // EN: Start monitoring thread
    // FR: Démarre le thread de monitoring
    void startMonitoringThread() {
        monitoring_thread_ = std::make_unique<std::thread>([this]() {
            monitoringLoop();
        });
    }
    
    // EN: Monitoring thread loop
    // FR: Boucle du thread de monitoring
    void monitoringLoop() {
        while (!shutdown_requested_) {
            std::unique_lock<std::mutex> lock(mutex_);
            
            if (monitoring_condition_.wait_for(lock, std::chrono::seconds(1), [this] { return shutdown_requested_; })) {
                break;
            }
            
            if (current_state_ == ResumeState::RUNNING && !current_operation_id_.empty()) {
                checkAutomaticCheckpoint(lock);
            }
        }
    }
    
    // EN: Check if automatic checkpoint should be created
    // FR: Vérifie si un checkpoint automatique doit être créé
    void checkAutomaticCheckpoint(std::unique_lock<std::mutex>& lock) {
        auto now = std::chrono::system_clock::now();
        auto time_since_last = std::chrono::duration_cast<std::chrono::seconds>(now - last_checkpoint_time_);
        
        bool should_checkpoint = false;
        
        switch (config_.strategy) {
            case CheckpointStrategy::TIME_BASED:
                should_checkpoint = time_since_last >= config_.time_interval;
                break;
                
            case CheckpointStrategy::PROGRESS_BASED:
                // EN: Progress-based checkpointing is handled externally
                // FR: Le checkpointing basé sur la progression est géré externellement
                break;
                
            case CheckpointStrategy::HYBRID:
                should_checkpoint = time_since_last >= config_.time_interval ||
                                  (last_progress_ > 0 && (last_progress_ - statistics_.checkpoint_overhead_percentage) >= config_.progress_threshold);
                break;
                
            case CheckpointStrategy::ADAPTIVE:
                // EN: Adaptive strategy based on memory usage and system load
                // FR: Stratégie adaptative basée sur l'utilisation mémoire et charge système
                should_checkpoint = time_since_last >= config_.time_interval ||
                                  estimateMemoryUsage() >= config_.max_memory_threshold_mb * 1024 * 1024;
                break;
                
            default:
                break;
        }
        
        if (should_checkpoint) {
            // EN: Create automatic checkpoint
            // FR: Crée un checkpoint automatique
            nlohmann::json state;
            state["auto_checkpoint"] = true;
            state["timestamp"] = detail::timestampToString(now);
            
            lock.unlock();
            createCheckpoint("auto_checkpoint", state, {}, last_progress_, true);
            lock.lock();
        }
    }
    
    // EN: Generate unique checkpoint ID
    // FR: Génère un ID unique de checkpoint
    std::string generateCheckpointId() {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(10000, 99999);
        
        return current_operation_id_ + "_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
    }
    
    // EN: Estimate current memory usage
    // FR: Estime l'utilisation mémoire actuelle
    size_t estimateMemoryUsage() const {
        // EN: Simple estimation based on process memory
        // FR: Estimation simple basée sur la mémoire du processus
        try {
            std::ifstream status("/proc/self/status");
            std::string line;
            while (std::getline(status, line)) {
                if (line.starts_with("VmRSS:")) {
                    std::istringstream iss(line);
                    std::string key, value, unit;
                    iss >> key >> value >> unit;
                    return std::stoull(value) * 1024; // Convert kB to bytes
                }
            }
        } catch (...) {
            // EN: Fallback estimation
            // FR: Estimation de secours
        }
        
        return 64 * 1024 * 1024; // 64MB default estimate
    }
    
    // EN: Update statistics
    // FR: Met à jour les statistiques
    void updateStatistics(bool success, bool is_automatic) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        if (success) {
            statistics_.total_checkpoints_created++;
            if (!is_automatic) {
                checkpoint_counter_++;
            }
        }
    }

private:
    CheckpointConfig config_;
    std::atomic<ResumeState> current_state_;
    std::unique_ptr<detail::ICheckpointStorage> storage_;
    
    // EN: Monitoring state
    // FR: État de monitoring
    std::string current_operation_id_;
    std::string current_pipeline_config_;
    std::chrono::system_clock::time_point operation_start_time_;
    std::chrono::system_clock::time_point last_checkpoint_time_;
    double last_progress_;
    
    // EN: Threading
    // FR: Threading
    std::unique_ptr<std::thread> monitoring_thread_;
    std::condition_variable monitoring_condition_;
    std::atomic<bool> shutdown_requested_;
    
    // EN: Statistics
    // FR: Statistiques
    mutable std::mutex stats_mutex_;
    ResumeStatistics statistics_;
    std::atomic<size_t> checkpoint_counter_;
    
    // EN: Callbacks
    // FR: Callbacks
    std::function<void(const std::string&, double)> progress_callback_;
    std::function<void(const std::string&, const CheckpointMetadata&)> checkpoint_callback_;
    std::function<void(const std::string&, bool)> recovery_callback_;
    
    // EN: Configuration
    // FR: Configuration
    std::atomic<bool> detailed_logging_;
    
    // EN: Thread safety
    // FR: Thread safety
    mutable std::mutex mutex_;
};

// EN: ResumeSystem implementation
// FR: Implémentation de ResumeSystem
ResumeSystem::ResumeSystem(const CheckpointConfig& config) 
    : pimpl_(std::make_unique<Impl>(config))
    , config_(config)
    , current_state_(ResumeState::IDLE) {
    
    statistics_.created_at = std::chrono::system_clock::now();
}

ResumeSystem::~ResumeSystem() = default;

bool ResumeSystem::initialize() {
    return pimpl_->initialize();
}

void ResumeSystem::shutdown() {
    pimpl_->shutdown();
}

bool ResumeSystem::startMonitoring(const std::string& operation_id, const std::string& pipeline_config_path) {
    return pimpl_->startMonitoring(operation_id, pipeline_config_path);
}

void ResumeSystem::stopMonitoring() {
    pimpl_->stopMonitoring();
}

std::string ResumeSystem::createCheckpoint(const std::string& stage_name, 
                                         const nlohmann::json& pipeline_state,
                                         const std::map<std::string, std::string>& custom_metadata) {
    return pimpl_->createCheckpoint(stage_name, pipeline_state, custom_metadata, 0.0, false);
}

std::string ResumeSystem::createAutomaticCheckpoint(const std::string& stage_name, 
                                                   const nlohmann::json& pipeline_state,
                                                   double progress_percentage) {
    return pimpl_->createCheckpoint(stage_name, pipeline_state, {}, progress_percentage, true);
}

bool ResumeSystem::canResume(const std::string& operation_id) const {
    return pimpl_->canResume(operation_id);
}

std::vector<CheckpointMetadata> ResumeSystem::getAvailableResumePoints(const std::string& operation_id) const {
    return pimpl_->getAvailableResumePoints(operation_id);
}

std::optional<ResumeContext> ResumeSystem::resumeFromCheckpoint(const std::string& checkpoint_id, ResumeMode mode) {
    return pimpl_->resumeFromCheckpoint(checkpoint_id, mode);
}

std::optional<ResumeContext> ResumeSystem::resumeAutomatically(const std::string& operation_id) {
    return pimpl_->resumeAutomatically(operation_id);
}

bool ResumeSystem::verifyCheckpoint(const std::string& checkpoint_id) const {
    return pimpl_->verifyCheckpoint(checkpoint_id);
}

std::vector<std::string> ResumeSystem::listCheckpoints(const std::string& operation_id) const {
    return pimpl_->listCheckpoints(operation_id);
}

bool ResumeSystem::deleteCheckpoint(const std::string& checkpoint_id) {
    return pimpl_->deleteCheckpoint(checkpoint_id);
}

size_t ResumeSystem::cleanupOldCheckpoints() {
    return pimpl_->cleanupOldCheckpoints();
}

ResumeState ResumeSystem::getCurrentState() const {
    return pimpl_->getCurrentState();
}

ResumeStatistics ResumeSystem::getStatistics() const {
    return pimpl_->getStatistics();
}

void ResumeSystem::resetStatistics() {
    pimpl_->resetStatistics();
}

void ResumeSystem::updateConfig(const CheckpointConfig& config) {
    config_ = config;
    pimpl_->updateConfig(config);
}

void ResumeSystem::setProgressCallback(std::function<void(const std::string&, double)> callback) {
    pimpl_->setProgressCallback(std::move(callback));
}

void ResumeSystem::setCheckpointCallback(std::function<void(const std::string&, const CheckpointMetadata&)> callback) {
    pimpl_->setCheckpointCallback(std::move(callback));
}

void ResumeSystem::setRecoveryCallback(std::function<void(const std::string&, bool)> callback) {
    pimpl_->setRecoveryCallback(std::move(callback));
}

std::string ResumeSystem::forceCheckpoint(const std::string& reason) {
    return pimpl_->forceCheckpoint(reason);
}

void ResumeSystem::registerSerializer(const std::string& type_name, 
                                     std::function<nlohmann::json(const void*)> serializer,
                                     std::function<void(const nlohmann::json&, void*)> deserializer) {
    // EN: Custom serializers implementation would go here
    // FR: L'implémentation des sérialiseurs personnalisés irait ici
    LOG_INFO("ResumeSystem", "Registered custom serializer for type: " + type_name);
}

void ResumeSystem::setDetailedLogging(bool enabled) {
    pimpl_->setDetailedLogging(enabled);
}

std::optional<CheckpointMetadata> ResumeSystem::getCheckpointMetadata(const std::string& checkpoint_id) const {
    return pimpl_->getCheckpointMetadata(checkpoint_id);
}

// EN: ResumeSystemManager implementation
// FR: Implémentation de ResumeSystemManager
ResumeSystemManager& ResumeSystemManager::getInstance() {
    static ResumeSystemManager instance;
    return instance;
}

bool ResumeSystemManager::initialize(const CheckpointConfig& config) {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    if (initialized_) {
        return true;
    }
    
    try {
        resume_system_ = std::make_unique<ResumeSystem>(config);
        if (resume_system_->initialize()) {
            initialized_ = true;
            LOG_INFO("ResumeSystemManager", "Resume system manager initialized successfully");
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("ResumeSystemManager", "Failed to initialize manager: " + std::string(e.what()));
    }
    
    return false;
}

void ResumeSystemManager::shutdown() {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    if (resume_system_) {
        resume_system_->shutdown();
        resume_system_.reset();
    }
    
    registered_pipelines_.clear();
    initialized_ = false;
    
    LOG_INFO("ResumeSystemManager", "Resume system manager shutdown completed");
}

ResumeSystem& ResumeSystemManager::getResumeSystem() {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    if (!resume_system_) {
        throw std::runtime_error("Resume system manager not initialized");
    }
    
    return *resume_system_;
}

bool ResumeSystemManager::registerPipeline(const std::string& pipeline_id, PipelineEngine* pipeline) {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    if (registered_pipelines_.find(pipeline_id) != registered_pipelines_.end()) {
        return false; // Already registered
    }
    
    registered_pipelines_[pipeline_id] = pipeline;
    LOG_INFO("ResumeSystemManager", "Registered pipeline: " + pipeline_id);
    return true;
}

void ResumeSystemManager::unregisterPipeline(const std::string& pipeline_id) {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    auto it = registered_pipelines_.find(pipeline_id);
    if (it != registered_pipelines_.end()) {
        registered_pipelines_.erase(it);
        LOG_INFO("ResumeSystemManager", "Unregistered pipeline: " + pipeline_id);
    }
}

std::vector<std::string> ResumeSystemManager::detectCrashedOperations() {
    std::vector<std::string> crashed_operations;
    
    if (!resume_system_) {
        return crashed_operations;
    }
    
    // EN: Get all available checkpoints
    // FR: Obtient tous les checkpoints disponibles
    auto all_checkpoints = resume_system_->listCheckpoints();
    std::unordered_set<std::string> operation_ids;
    
    for (const auto& checkpoint_id : all_checkpoints) {
        auto metadata = resume_system_->getCheckpointMetadata(checkpoint_id);
        if (metadata) {
            operation_ids.insert(metadata->pipeline_id);
        }
    }
    
    // EN: Check which operations are not currently running
    // FR: Vérifie quelles opérations ne sont pas actuellement en cours
    for (const auto& operation_id : operation_ids) {
        auto it = registered_pipelines_.find(operation_id);
        if (it == registered_pipelines_.end() || it->second == nullptr) {
            crashed_operations.push_back(operation_id);
        }
    }
    
    return crashed_operations;
}

bool ResumeSystemManager::attemptAutomaticRecovery(const std::string& operation_id) {
    if (!resume_system_) {
        return false;
    }
    
    try {
        auto context = resume_system_->resumeAutomatically(operation_id);
        if (context) {
            LOG_INFO("ResumeSystemManager", "Successfully recovered operation: " + operation_id);
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("ResumeSystemManager", "Failed to recover operation " + operation_id + ": " + e.what());
    }
    
    return false;
}

ResumeStatistics ResumeSystemManager::getGlobalStatistics() const {
    if (!resume_system_) {
        return ResumeStatistics{};
    }
    
    return resume_system_->getStatistics();
}

// EN: AutoCheckpointGuard implementation
// FR: Implémentation de AutoCheckpointGuard
AutoCheckpointGuard::AutoCheckpointGuard(const std::string& operation_id, 
                                       const std::string& stage_name,
                                       ResumeSystem& resume_system)
    : operation_id_(operation_id)
    , stage_name_(stage_name)
    , resume_system_(resume_system)
    , current_progress_(0.0)
    , start_time_(std::chrono::system_clock::now()) {
}

AutoCheckpointGuard::~AutoCheckpointGuard() {
    // EN: Create final checkpoint on destruction
    // FR: Crée le checkpoint final à la destruction
    if (!current_state_.empty()) {
        try {
            metadata_["completion_time"] = detail::timestampToString(std::chrono::system_clock::now());
            metadata_["final_progress"] = std::to_string(current_progress_);
            
            last_checkpoint_id_ = resume_system_.createCheckpoint(stage_name_ + "_final", current_state_, metadata_);
        } catch (const std::exception& e) {
            LOG_ERROR("AutoCheckpointGuard", "Failed to create final checkpoint: " + std::string(e.what()));
        }
    }
}

void AutoCheckpointGuard::updateProgress(double percentage) {
    current_progress_ = percentage;
    
    // EN: Create progress checkpoint if significant change
    // FR: Crée un checkpoint de progression si changement significatif
    static double last_checkpoint_progress = 0.0;
    if (std::abs(percentage - last_checkpoint_progress) >= 10.0) { // 10% threshold
        metadata_["progress_checkpoint"] = "true";
        metadata_["progress_percentage"] = std::to_string(percentage);
        
        forceCheckpoint();
        last_checkpoint_progress = percentage;
    }
}

void AutoCheckpointGuard::setPipelineState(const nlohmann::json& state) {
    current_state_ = state;
}

void AutoCheckpointGuard::addMetadata(const std::string& key, const std::string& value) {
    metadata_[key] = value;
}

std::string AutoCheckpointGuard::forceCheckpoint() {
    if (current_state_.empty()) {
        current_state_["forced"] = true;
        current_state_["timestamp"] = detail::timestampToString(std::chrono::system_clock::now());
    }
    
    metadata_["forced_checkpoint"] = "true";
    last_checkpoint_id_ = resume_system_.createCheckpoint(stage_name_, current_state_, metadata_);
    return last_checkpoint_id_;
}

// EN: Utility functions implementation
// FR: Implémentation des fonctions utilitaires
namespace ResumeSystemUtils {

CheckpointConfig createDefaultConfig() {
    CheckpointConfig config;
    config.checkpoint_dir = "./checkpoints";
    config.strategy = CheckpointStrategy::HYBRID;
    config.granularity = CheckpointGranularity::MEDIUM;
    config.time_interval = std::chrono::seconds(300); // 5 minutes
    config.progress_threshold = 10.0; // 10%
    config.max_checkpoints = 10;
    config.enable_compression = true;
    config.enable_encryption = false;
    config.enable_verification = true;
    config.max_memory_threshold_mb = 512;
    config.auto_cleanup = true;
    config.cleanup_age = std::chrono::hours(72); // 3 days
    return config;
}

CheckpointConfig createHighFrequencyConfig() {
    auto config = createDefaultConfig();
    config.strategy = CheckpointStrategy::TIME_BASED;
    config.time_interval = std::chrono::seconds(60); // 1 minute
    config.progress_threshold = 5.0; // 5%
    config.max_checkpoints = 20;
    config.granularity = CheckpointGranularity::FINE;
    return config;
}

CheckpointConfig createLowOverheadConfig() {
    auto config = createDefaultConfig();
    config.strategy = CheckpointStrategy::PROGRESS_BASED;
    config.progress_threshold = 25.0; // 25%
    config.max_checkpoints = 5;
    config.enable_compression = false;
    config.enable_verification = false;
    config.granularity = CheckpointGranularity::COARSE;
    return config;
}

size_t estimateCheckpointSize(const nlohmann::json& pipeline_state) {
    try {
        std::string serialized = pipeline_state.dump();
        return serialized.size();
    } catch (...) {
        return 1024; // 1KB default estimate
    }
}

bool validateConfig(const CheckpointConfig& config) {
    if (config.checkpoint_dir.empty()) {
        return false;
    }
    
    if (config.max_checkpoints == 0) {
        return false;
    }
    
    if (config.time_interval.count() <= 0) {
        return false;
    }
    
    if (config.progress_threshold < 0.0 || config.progress_threshold > 100.0) {
        return false;
    }
    
    return true;
}

std::string generateOperationId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    
    return "op_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

std::optional<ResumeContext> parseResumeContext(const std::vector<std::string>& args) {
    // EN: Simple command line parsing for resume context
    // FR: Parsing simple de ligne de commande pour contexte de reprise
    ResumeContext context;
    
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--resume-operation" && i + 1 < args.size()) {
            context.operation_id = args[i + 1];
        } else if (args[i] == "--resume-config" && i + 1 < args.size()) {
            context.pipeline_config_path = args[i + 1];
        } else if (args[i] == "--resume-mode" && i + 1 < args.size()) {
            if (args[i + 1] == "full") {
                context.resume_mode = ResumeMode::FULL_RESTART;
            } else if (args[i + 1] == "last") {
                context.resume_mode = ResumeMode::LAST_CHECKPOINT;
            } else if (args[i + 1] == "best") {
                context.resume_mode = ResumeMode::BEST_CHECKPOINT;
            } else if (args[i + 1] == "interactive") {
                context.resume_mode = ResumeMode::INTERACTIVE;
            }
        }
    }
    
    if (!context.operation_id.empty()) {
        context.resume_time = std::chrono::system_clock::now();
        return context;
    }
    
    return std::nullopt;
}

std::vector<uint8_t> compressCheckpointData(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return {};
    }
    
    // EN: Use zlib for compression
    // FR: Utilise zlib pour la compression
    uLongf dest_len = compressBound(data.size());
    std::vector<uint8_t> compressed(dest_len);
    
    int result = compress(compressed.data(), &dest_len, data.data(), data.size());
    if (result != Z_OK) {
        LOG_ERROR("ResumeSystemUtils", "Compression failed with error: " + std::to_string(result));
        return data; // Return original data if compression fails
    }
    
    compressed.resize(dest_len);
    return compressed;
}

std::vector<uint8_t> decompressCheckpointData(const std::vector<uint8_t>& compressed_data) {
    if (compressed_data.empty()) {
        return {};
    }
    
    // EN: Estimate uncompressed size (start with 4x compressed size)
    // FR: Estime la taille décompressée (commence avec 4x la taille compressée)
    uLongf dest_len = compressed_data.size() * 4;
    std::vector<uint8_t> decompressed;
    
    int result;
    do {
        decompressed.resize(dest_len);
        result = uncompress(decompressed.data(), &dest_len, compressed_data.data(), compressed_data.size());
        
        if (result == Z_BUF_ERROR) {
            dest_len *= 2; // Double buffer size and try again
        }
    } while (result == Z_BUF_ERROR && dest_len < compressed_data.size() * 100);
    
    if (result != Z_OK) {
        LOG_ERROR("ResumeSystemUtils", "Decompression failed with error: " + std::to_string(result));
        return compressed_data; // Return compressed data if decompression fails
    }
    
    decompressed.resize(dest_len);
    return decompressed;
}

std::vector<uint8_t> encryptCheckpointData(const std::vector<uint8_t>& data, const std::string& key) {
    if (data.empty() || key.empty()) {
        return data;
    }
    
    // EN: Simple XOR encryption for demonstration (use proper encryption in production)
    // FR: Chiffrement XOR simple pour démonstration (utiliser un chiffrement approprié en production)
    std::vector<uint8_t> encrypted = data;
    
    for (size_t i = 0; i < encrypted.size(); ++i) {
        encrypted[i] ^= key[i % key.size()];
    }
    
    return encrypted;
}

std::vector<uint8_t> decryptCheckpointData(const std::vector<uint8_t>& encrypted_data, const std::string& key) {
    // EN: XOR decryption is the same as encryption
    // FR: Le déchiffrement XOR est identique au chiffrement
    return encryptCheckpointData(encrypted_data, key);
}

} // namespace ResumeSystemUtils

} // namespace BBP