#pragma once

// EN: Delta compression for CSV files to optimize storage for change monitoring
// FR: Compression delta pour fichiers CSV afin d'optimiser le stockage pour surveillance des changements

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <chrono>
#include <optional>
#include <functional>
#include <fstream>
#include <atomic>
#include <mutex>

namespace BBP {
namespace CSV {

// EN: Forward declarations
// FR: Déclarations anticipées
class DeltaCompressor;
class DeltaDecompressor;
class ChangeDetector;

// EN: Delta operation types for tracking changes
// FR: Types d'opérations delta pour suivre les changements
enum class DeltaOperation {
    NONE = 0,           // EN: No change / FR: Aucun changement
    INSERT,             // EN: New row added / FR: Nouvelle ligne ajoutée
    DELETE,             // EN: Row removed / FR: Ligne supprimée
    UPDATE,             // EN: Row modified / FR: Ligne modifiée
    MOVE               // EN: Row position changed / FR: Position de ligne changée
};

// EN: Compression algorithms available
// FR: Algorithmes de compression disponibles
enum class CompressionAlgorithm {
    NONE = 0,           // EN: No compression / FR: Pas de compression
    RLE,               // EN: Run-Length Encoding / FR: Encodage par plages
    DELTA_ENCODING,    // EN: Delta encoding for numerical values / FR: Encodage delta pour valeurs numériques
    DICTIONARY,        // EN: Dictionary compression / FR: Compression par dictionnaire
    LZ77,             // EN: LZ77 compression / FR: Compression LZ77
    HYBRID            // EN: Hybrid approach combining multiple algorithms / FR: Approche hybride combinant plusieurs algorithmes
};

// EN: Change detection strategies
// FR: Stratégies de détection de changements
enum class ChangeDetectionMode {
    CONTENT_HASH = 0,   // EN: Compare content hashes / FR: Comparer les hash de contenu
    FIELD_BY_FIELD,     // EN: Compare each field individually / FR: Comparer chaque champ individuellement
    KEY_BASED,          // EN: Use specific key columns for identification / FR: Utiliser colonnes clés spécifiques pour identification
    SEMANTIC,           // EN: Semantic change detection / FR: Détection de changements sémantiques
    TIMESTAMP_BASED     // EN: Use timestamps for change detection / FR: Utiliser timestamps pour détection de changements
};

// EN: Delta compression error types
// FR: Types d'erreurs de compression delta
enum class DeltaError {
    SUCCESS = 0,        // EN: Operation successful / FR: Opération réussie
    FILE_NOT_FOUND,     // EN: Input file not found / FR: Fichier d'entrée introuvable
    INVALID_FORMAT,     // EN: Invalid CSV format / FR: Format CSV invalide
    COMPRESSION_FAILED, // EN: Compression algorithm failed / FR: Algorithme de compression échoué
    DECOMPRESSION_FAILED, // EN: Decompression failed / FR: Décompression échouée
    INVALID_CONFIG,     // EN: Invalid configuration / FR: Configuration invalide
    MEMORY_ERROR,       // EN: Memory allocation error / FR: Erreur d'allocation mémoire
    IO_ERROR,          // EN: Input/Output error / FR: Erreur d'entrée/sortie
    VERSION_MISMATCH   // EN: Delta format version mismatch / FR: Incompatibilité de version du format delta
};

// EN: Delta record representing a single change
// FR: Enregistrement delta représentant un seul changement
struct DeltaRecord {
    DeltaOperation operation{DeltaOperation::NONE};     // EN: Type of change / FR: Type de changement
    size_t row_index{0};                               // EN: Row index in original file / FR: Index de ligne dans fichier original
    std::vector<std::string> old_values;              // EN: Previous values (for UPDATE/DELETE) / FR: Valeurs précédentes (pour UPDATE/DELETE)
    std::vector<std::string> new_values;              // EN: New values (for INSERT/UPDATE) / FR: Nouvelles valeurs (pour INSERT/UPDATE)
    std::vector<size_t> changed_columns;              // EN: Which columns changed / FR: Quelles colonnes ont changé
    std::string timestamp;                             // EN: When change occurred / FR: Quand le changement s'est produit
    std::string change_hash;                           // EN: Hash of the change / FR: Hash du changement
    std::unordered_map<std::string, std::string> metadata; // EN: Additional metadata / FR: Métadonnées additionnelles
    
    // EN: Serialization methods
    // FR: Méthodes de sérialisation
    std::string serialize() const;
    static DeltaRecord deserialize(const std::string& data);
    
    // EN: Comparison operators
    // FR: Opérateurs de comparaison
    bool operator==(const DeltaRecord& other) const;
    bool operator!=(const DeltaRecord& other) const { return !(*this == other); }
};

// EN: Delta file header with metadata
// FR: En-tête de fichier delta avec métadonnées
struct DeltaHeader {
    std::string version{"1.0"};                        // EN: Delta format version / FR: Version du format delta
    std::string source_file;                           // EN: Original source file path / FR: Chemin du fichier source original
    std::string target_file;                           // EN: Target file path / FR: Chemin du fichier cible
    std::string creation_timestamp;                    // EN: When delta was created / FR: Quand le delta a été créé
    CompressionAlgorithm algorithm{CompressionAlgorithm::HYBRID}; // EN: Compression algorithm used / FR: Algorithme de compression utilisé
    ChangeDetectionMode detection_mode{ChangeDetectionMode::CONTENT_HASH}; // EN: Change detection mode / FR: Mode de détection des changements
    std::vector<std::string> key_columns;             // EN: Key columns for identification / FR: Colonnes clés pour identification
    std::unordered_map<std::string, std::string> metadata; // EN: Additional metadata / FR: Métadonnées additionnelles
    size_t total_changes{0};                          // EN: Total number of changes / FR: Nombre total de changements
    size_t compression_ratio{0};                       // EN: Compression ratio achieved / FR: Ratio de compression atteint
    
    // EN: Serialization methods
    // FR: Méthodes de sérialisation
    std::string serialize() const;
    static DeltaHeader deserialize(const std::string& data);
};

// EN: Configuration for delta compression operations
// FR: Configuration pour les opérations de compression delta
class DeltaConfig {
public:
    // EN: Default constructor with sensible defaults
    // FR: Constructeur par défaut avec valeurs par défaut sensées
    DeltaConfig();
    
    // EN: Configuration validation
    // FR: Validation de la configuration
    bool isValid() const;
    std::vector<std::string> getValidationErrors() const;
    
    // EN: Core compression settings
    // FR: Paramètres de compression principaux
    CompressionAlgorithm algorithm{CompressionAlgorithm::HYBRID};
    ChangeDetectionMode detection_mode{ChangeDetectionMode::CONTENT_HASH};
    
    // EN: Key configuration for change detection
    // FR: Configuration de clé pour détection de changements
    std::vector<std::string> key_columns;             // EN: Columns to use as unique identifier / FR: Colonnes à utiliser comme identifiant unique
    std::string timestamp_column;                     // EN: Column containing timestamp / FR: Colonne contenant le timestamp
    bool case_sensitive_keys{true};                   // EN: Case sensitivity for key matching / FR: Sensibilité à la casse pour correspondance de clé
    bool trim_key_whitespace{true};                   // EN: Trim whitespace from keys / FR: Supprimer espaces des clés
    
    // EN: Compression optimization settings
    // FR: Paramètres d'optimisation de compression
    double similarity_threshold{0.8};                 // EN: Threshold for considering fields similar / FR: Seuil pour considérer champs similaires
    size_t max_dictionary_size{1000};                // EN: Maximum dictionary entries / FR: Entrées maximales du dictionnaire
    bool enable_run_length_encoding{true};           // EN: Enable RLE compression / FR: Activer compression RLE
    bool enable_delta_encoding{true};                // EN: Enable delta encoding for numbers / FR: Activer encodage delta pour nombres
    bool enable_dictionary_compression{true};         // EN: Enable dictionary compression / FR: Activer compression par dictionnaire
    
    // EN: Performance and memory settings
    // FR: Paramètres de performance et mémoire
    size_t chunk_size{10000};                        // EN: Processing chunk size / FR: Taille de chunk de traitement
    size_t max_memory_usage{100 * 1024 * 1024};     // EN: Maximum memory usage in bytes / FR: Usage mémoire maximum en octets
    bool enable_parallel_processing{true};           // EN: Enable multi-threading / FR: Activer multi-threading
    size_t num_threads{0};                           // EN: Number of threads (0 = auto) / FR: Nombre de threads (0 = auto)
    
    // EN: Output format settings
    // FR: Paramètres de format de sortie
    bool binary_format{false};                       // EN: Use binary delta format / FR: Utiliser format delta binaire
    bool compress_output{true};                      // EN: Compress final delta file / FR: Compresser fichier delta final
    std::string output_encoding{"UTF-8"};           // EN: Output file encoding / FR: Encodage fichier de sortie
    
    // EN: Advanced options
    // FR: Options avancées
    bool preserve_order{false};                      // EN: Preserve row order in delta / FR: Préserver ordre des lignes dans delta
    bool include_metadata{true};                     // EN: Include metadata in delta / FR: Inclure métadonnées dans delta
    bool validate_integrity{true};                   // EN: Validate delta integrity / FR: Valider intégrité du delta
    double min_compression_ratio{1.1};              // EN: Minimum compression ratio to accept / FR: Ratio de compression minimum à accepter
};

// EN: Statistics for delta compression operations
// FR: Statistiques pour les opérations de compression delta
class DeltaStatistics {
public:
    DeltaStatistics() = default;
    ~DeltaStatistics() = default;
    
    // EN: Reset all statistics
    // FR: Réinitialiser toutes les statistiques
    void reset();
    
    // EN: Statistics accessors
    // FR: Accesseurs de statistiques
    size_t getTotalRecordsProcessed() const { return total_records_processed_.load(); }
    size_t getTotalChangesDetected() const { return total_changes_detected_.load(); }
    size_t getInsertsDetected() const { return inserts_detected_.load(); }
    size_t getUpdatesDetected() const { return updates_detected_.load(); }
    size_t getDeletesDetected() const { return deletes_detected_.load(); }
    size_t getMovesDetected() const { return moves_detected_.load(); }
    size_t getOriginalSize() const { return original_size_.load(); }
    size_t getCompressedSize() const { return compressed_size_.load(); }
    double getCompressionRatio() const;
    size_t getProcessingTimeMs() const { return processing_time_ms_.load(); }
    size_t getMemoryUsageBytes() const { return memory_usage_bytes_.load(); }
    
    // EN: Internal methods for updating statistics
    // FR: Méthodes internes pour mettre à jour les statistiques
    void incrementRecordsProcessed(size_t count = 1) { total_records_processed_ += count; }
    void incrementChangesDetected(size_t count = 1) { total_changes_detected_ += count; }
    void incrementInserts(size_t count = 1) { inserts_detected_ += count; }
    void incrementUpdates(size_t count = 1) { updates_detected_ += count; }
    void incrementDeletes(size_t count = 1) { deletes_detected_ += count; }
    void incrementMoves(size_t count = 1) { moves_detected_ += count; }
    void setOriginalSize(size_t size) { original_size_ = size; }
    void setCompressedSize(size_t size) { compressed_size_ = size; }
    void setProcessingTime(size_t ms) { processing_time_ms_ = ms; }
    void setMemoryUsage(size_t bytes) { memory_usage_bytes_ = bytes; }

private:
    // EN: Thread-safe counters
    // FR: Compteurs thread-safe
    std::atomic<size_t> total_records_processed_{0};
    std::atomic<size_t> total_changes_detected_{0};
    std::atomic<size_t> inserts_detected_{0};
    std::atomic<size_t> updates_detected_{0};
    std::atomic<size_t> deletes_detected_{0};
    std::atomic<size_t> moves_detected_{0};
    std::atomic<size_t> original_size_{0};
    std::atomic<size_t> compressed_size_{0};
    std::atomic<size_t> processing_time_ms_{0};
    std::atomic<size_t> memory_usage_bytes_{0};
};

// EN: Change detector for identifying differences between CSV datasets
// FR: Détecteur de changements pour identifier les différences entre jeux de données CSV
class ChangeDetector {
public:
    explicit ChangeDetector(const DeltaConfig& config);
    ~ChangeDetector() = default;
    
    // EN: Main change detection methods
    // FR: Méthodes principales de détection de changements
    std::vector<DeltaRecord> detectChanges(
        const std::vector<std::vector<std::string>>& old_data,
        const std::vector<std::vector<std::string>>& new_data,
        const std::vector<std::string>& headers
    );
    
    DeltaError detectChangesFromFiles(
        const std::string& old_file,
        const std::string& new_file,
        std::vector<DeltaRecord>& changes
    );
    
    // EN: Specialized detection methods
    // FR: Méthodes de détection spécialisées
    std::vector<DeltaRecord> detectContentHashChanges(
        const std::vector<std::vector<std::string>>& old_data,
        const std::vector<std::vector<std::string>>& new_data,
        const std::vector<std::string>& headers
    );
    
    std::vector<DeltaRecord> detectFieldByFieldChanges(
        const std::vector<std::vector<std::string>>& old_data,
        const std::vector<std::vector<std::string>>& new_data,
        const std::vector<std::string>& headers
    );
    
    std::vector<DeltaRecord> detectKeyBasedChanges(
        const std::vector<std::vector<std::string>>& old_data,
        const std::vector<std::vector<std::string>>& new_data,
        const std::vector<std::string>& headers
    );
    
    // EN: Utility methods
    // FR: Méthodes utilitaires
    std::string generateRowHash(const std::vector<std::string>& row) const;
    std::string generateKeyFromRow(const std::vector<std::string>& row, const std::vector<std::string>& headers) const;
    bool areRowsSimilar(const std::vector<std::string>& row1, const std::vector<std::string>& row2) const;
    std::vector<size_t> findChangedColumns(const std::vector<std::string>& old_row, const std::vector<std::string>& new_row) const;

private:
    DeltaConfig config_;
    std::unordered_map<std::string, size_t> key_column_indices_;
    
    // EN: Helper methods
    // FR: Méthodes d'aide
    void buildKeyColumnIndices(const std::vector<std::string>& headers);
    DeltaRecord createInsertRecord(size_t index, const std::vector<std::string>& row);
    DeltaRecord createDeleteRecord(size_t index, const std::vector<std::string>& row);
    DeltaRecord createUpdateRecord(size_t index, const std::vector<std::string>& old_row, const std::vector<std::string>& new_row);
    DeltaRecord createMoveRecord(size_t old_index, size_t new_index, const std::vector<std::string>& row);
};

// EN: Delta compressor for creating compressed change representations
// FR: Compresseur delta pour créer des représentations de changements compressées
class DeltaCompressor {
public:
    explicit DeltaCompressor(const DeltaConfig& config);
    ~DeltaCompressor() = default;
    
    // EN: Main compression methods
    // FR: Méthodes principales de compression
    DeltaError compress(
        const std::string& old_file,
        const std::string& new_file,
        const std::string& delta_file
    );
    
    DeltaError compressFromRecords(
        const std::vector<DeltaRecord>& changes,
        const std::string& delta_file,
        const DeltaHeader& header
    );
    
    // EN: Streaming compression for large datasets
    // FR: Compression streaming pour gros jeux de données
    DeltaError compressStreaming(
        const std::string& old_file,
        const std::string& new_file,
        const std::string& delta_file
    );
    
    // EN: Compression algorithm implementations
    // FR: Implémentations d'algorithmes de compression
    std::vector<uint8_t> applyRunLengthEncoding(const std::vector<uint8_t>& data);
    std::vector<uint8_t> applyDeltaEncoding(const std::vector<int64_t>& values);
    std::vector<uint8_t> applyDictionaryCompression(const std::vector<std::string>& strings);
    std::vector<uint8_t> applyLZ77Compression(const std::vector<uint8_t>& data);
    std::vector<uint8_t> applyHybridCompression(const std::vector<DeltaRecord>& records);
    
    // EN: Statistics and monitoring
    // FR: Statistiques et surveillance
    const DeltaStatistics& getStatistics() const { return stats_; }
    void resetStatistics() { stats_.reset(); }
    
    // EN: Configuration management
    // FR: Gestion de la configuration
    void setConfig(const DeltaConfig& config) { config_ = config; }
    const DeltaConfig& getConfig() const { return config_; }

private:
    DeltaConfig config_;
    DeltaStatistics stats_;
    std::unique_ptr<ChangeDetector> change_detector_;
    std::mutex compression_mutex_;
    
    // EN: Internal compression helpers
    // FR: Assistants de compression internes
    DeltaError writeHeader(std::ofstream& file, const DeltaHeader& header);
    DeltaError writeRecords(std::ofstream& file, const std::vector<DeltaRecord>& records);
    DeltaError optimizeRecords(std::vector<DeltaRecord>& records);
    size_t calculateMemoryUsage(const std::vector<DeltaRecord>& records);
    
    // EN: Compression utilities
    // FR: Utilitaires de compression
    std::unordered_map<std::string, size_t> buildDictionary(const std::vector<std::string>& strings);
    std::vector<uint8_t> serializeToBytes(const std::vector<DeltaRecord>& records);
    bool shouldUseCompression(size_t original_size, size_t compressed_size) const;
};

// EN: Delta decompressor for reconstructing files from compressed deltas
// FR: Décompresseur delta pour reconstruire des fichiers à partir de deltas compressés
class DeltaDecompressor {
public:
    explicit DeltaDecompressor(const DeltaConfig& config = DeltaConfig{});
    ~DeltaDecompressor() = default;
    
    // EN: Main decompression methods
    // FR: Méthodes principales de décompression
    DeltaError decompress(
        const std::string& delta_file,
        const std::string& base_file,
        const std::string& output_file
    );
    
    DeltaError decompressToRecords(
        const std::string& delta_file,
        std::vector<DeltaRecord>& records,
        DeltaHeader& header
    );
    
    // EN: Apply delta records to reconstruct target file
    // FR: Appliquer enregistrements delta pour reconstruire fichier cible
    DeltaError applyDelta(
        const std::vector<std::vector<std::string>>& base_data,
        const std::vector<DeltaRecord>& changes,
        std::vector<std::vector<std::string>>& result_data
    );
    
    // EN: Decompression algorithm implementations
    // FR: Implémentations d'algorithmes de décompression
    std::vector<uint8_t> decompressRunLengthEncoding(const std::vector<uint8_t>& data);
    std::vector<int64_t> decompressDeltaEncoding(const std::vector<uint8_t>& data);
    std::vector<std::string> decompressDictionaryCompression(const std::vector<uint8_t>& data);
    std::vector<uint8_t> decompressLZ77(const std::vector<uint8_t>& data);
    std::vector<DeltaRecord> decompressHybridFormat(const std::vector<uint8_t>& data);
    
    // EN: Validation and verification
    // FR: Validation et vérification
    bool validateDelta(const std::string& delta_file);
    bool verifyIntegrity(const std::vector<DeltaRecord>& records, const DeltaHeader& header);
    
    // EN: Statistics access
    // FR: Accès aux statistiques
    const DeltaStatistics& getStatistics() const { return stats_; }

private:
    DeltaConfig config_;
    DeltaStatistics stats_;
    std::mutex decompression_mutex_;
    
    // EN: Internal decompression helpers
    // FR: Assistants de décompression internes
    DeltaError readHeader(std::ifstream& file, DeltaHeader& header);
    DeltaError readRecords(std::ifstream& file, std::vector<DeltaRecord>& records, const DeltaHeader& header);
    DeltaError applyInsert(std::vector<std::vector<std::string>>& data, const DeltaRecord& record);
    DeltaError applyDelete(std::vector<std::vector<std::string>>& data, const DeltaRecord& record);
    DeltaError applyUpdate(std::vector<std::vector<std::string>>& data, const DeltaRecord& record);
    DeltaError applyMove(std::vector<std::vector<std::string>>& data, const DeltaRecord& record);
    
    // EN: Utility methods
    // FR: Méthodes utilitaires
    std::vector<DeltaRecord> deserializeFromBytes(const std::vector<uint8_t>& data);
    size_t findRowByKey(const std::vector<std::vector<std::string>>& data, const std::string& key, const std::vector<std::string>& headers);
};

// EN: Utility namespace for delta compression operations
// FR: Namespace utilitaire pour les opérations de compression delta
namespace DeltaUtils {
    // EN: File operations
    // FR: Opérations sur les fichiers
    std::vector<std::vector<std::string>> loadCsvFile(const std::string& filepath);
    DeltaError saveCsvFile(const std::string& filepath, const std::vector<std::vector<std::string>>& data);
    bool fileExists(const std::string& filepath);
    size_t getFileSize(const std::string& filepath);
    std::string getFileHash(const std::string& filepath);
    
    // EN: String utilities
    // FR: Utilitaires de chaînes
    std::string trim(const std::string& str);
    std::string toLower(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string join(const std::vector<std::string>& parts, const std::string& delimiter);
    
    // EN: Timestamp utilities
    // FR: Utilitaires de timestamp
    std::string getCurrentTimestamp();
    std::string formatTimestamp(const std::chrono::system_clock::time_point& time);
    std::chrono::system_clock::time_point parseTimestamp(const std::string& timestamp);
    
    // EN: Hash utilities
    // FR: Utilitaires de hash
    std::string computeSHA256(const std::string& data);
    std::string computeMD5(const std::string& data);
    std::string computeContentHash(const std::vector<std::string>& row);
    
    // EN: Compression utilities
    // FR: Utilitaires de compression
    double calculateCompressionRatio(size_t original_size, size_t compressed_size);
    bool isCompressible(const std::vector<DeltaRecord>& records, double min_ratio);
    size_t estimateCompressionSize(const std::vector<DeltaRecord>& records, CompressionAlgorithm algorithm);
    
    // EN: Performance utilities
    // FR: Utilitaires de performance
    size_t getOptimalChunkSize(size_t total_records, size_t available_memory);
    size_t getOptimalThreadCount();
    void logPerformanceMetrics(const DeltaStatistics& stats);
}

} // namespace CSV
} // namespace BBP