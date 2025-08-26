// EN: Intelligent CSV merger with deduplication and advanced merge strategies
// FR: Moteur de fusion CSV intelligent avec déduplication et stratégies de fusion avancées

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <chrono>
#include <optional>
#include <regex>
#include <atomic>
#include <mutex>
#include <fstream>

namespace BBP {
namespace CSV {

// EN: Forward declarations
// FR: Déclarations anticipées
class MergerEngine;
class MergeConfig;
class MergeStatistics;
class DuplicateResolver;

// EN: Merge strategies for handling different CSV sources
// FR: Stratégies de fusion pour gérer différentes sources CSV
enum class MergeStrategy {
    APPEND,              // EN: Simple append all rows / FR: Ajout simple de toutes les lignes
    SMART_MERGE,         // EN: Intelligent merge with conflict resolution / FR: Fusion intelligente avec résolution de conflits
    PRIORITY_MERGE,      // EN: Priority-based merge with source weighting / FR: Fusion basée sur priorité avec pondération des sources
    TIME_BASED,          // EN: Time-based merge using timestamps / FR: Fusion basée sur le temps avec timestamps
    SCHEMA_AWARE         // EN: Schema-aware merge with type validation / FR: Fusion consciente du schéma avec validation de type
};

// EN: Deduplication strategies for handling duplicate records
// FR: Stratégies de déduplication pour gérer les enregistrements dupliqués
enum class DeduplicationStrategy {
    NONE,                // EN: No deduplication / FR: Pas de déduplication
    EXACT_MATCH,         // EN: Exact field matching / FR: Correspondance exacte des champs
    FUZZY_MATCH,         // EN: Fuzzy string matching / FR: Correspondance floue de chaînes
    KEY_BASED,           // EN: Key-based deduplication / FR: Déduplication basée sur clé
    CONTENT_HASH,        // EN: Content hash-based / FR: Basé sur le hash du contenu
    CUSTOM_FUNCTION      // EN: Custom deduplication function / FR: Fonction de déduplication personnalisée
};

// EN: Conflict resolution strategies when merging overlapping data
// FR: Stratégies de résolution de conflits lors de fusion de données qui se chevauchent
enum class ConflictResolution {
    KEEP_FIRST,          // EN: Keep first occurrence / FR: Garder la première occurrence
    KEEP_LAST,           // EN: Keep last occurrence / FR: Garder la dernière occurrence
    KEEP_NEWEST,         // EN: Keep newest by timestamp / FR: Garder le plus récent par timestamp
    KEEP_OLDEST,         // EN: Keep oldest by timestamp / FR: Garder le plus ancien par timestamp
    MERGE_VALUES,        // EN: Merge conflicting values / FR: Fusionner les valeurs conflictuelles
    PRIORITY_SOURCE,     // EN: Use source priority / FR: Utiliser la priorité de source
    CUSTOM_RESOLVER      // EN: Custom resolver function / FR: Fonction de résolution personnalisée
};

// EN: Error types that can occur during merging operations
// FR: Types d'erreur qui peuvent survenir pendant les opérations de fusion
enum class MergeError {
    SUCCESS = 0,
    FILE_NOT_FOUND,
    SCHEMA_MISMATCH,
    INVALID_CONFIG,
    MEMORY_ERROR,
    IO_ERROR,
    PARSE_ERROR,
    DUPLICATE_RESOLUTION_FAILED,
    MERGE_CONFLICT_UNRESOLVED,
    OUTPUT_ERROR
};

// EN: Input source configuration for CSV files to merge
// FR: Configuration de source d'entrée pour les fichiers CSV à fusionner
struct InputSource {
    std::string filepath;                        // EN: Path to CSV file / FR: Chemin vers le fichier CSV
    std::string name;                           // EN: Source name identifier / FR: Nom identifiant de la source
    int priority{0};                            // EN: Source priority (higher = more important) / FR: Priorité de source (plus élevé = plus important)
    std::string encoding{"UTF-8"};             // EN: File encoding / FR: Encodage du fichier
    char delimiter{','};                        // EN: CSV delimiter / FR: Délimiteur CSV
    bool has_header{true};                      // EN: Whether source has header row / FR: Si la source a une ligne d'en-tête
    std::optional<std::string> timestamp_column; // EN: Column name for timestamp-based operations / FR: Nom de colonne pour opérations basées timestamp
    std::unordered_map<std::string, std::string> metadata; // EN: Additional source metadata / FR: Métadonnées additionnelles de source
};

// EN: Merge configuration with all merge parameters
// FR: Configuration de fusion avec tous les paramètres de fusion
class MergeConfig {
public:
    // EN: Default constructor with sensible defaults
    // FR: Constructeur par défaut avec valeurs par défaut sensées
    MergeConfig();
    
    // EN: Configuration validation
    // FR: Validation de la configuration
    bool isValid() const;
    std::vector<std::string> getValidationErrors() const;
    
    // EN: Core merge settings
    // FR: Paramètres de fusion principaux
    MergeStrategy merge_strategy{MergeStrategy::SMART_MERGE};
    DeduplicationStrategy dedup_strategy{DeduplicationStrategy::KEY_BASED};
    ConflictResolution conflict_resolution{ConflictResolution::KEEP_NEWEST};
    
    // EN: Key configuration for deduplication
    // FR: Configuration de clé pour déduplication
    std::vector<std::string> key_columns;      // EN: Columns to use as unique key / FR: Colonnes à utiliser comme clé unique
    bool case_sensitive_keys{true};           // EN: Case sensitivity for key matching / FR: Sensibilité à la casse pour correspondance de clé
    bool trim_key_whitespace{true};           // EN: Trim whitespace from keys / FR: Supprimer les espaces des clés
    
    // EN: Fuzzy matching configuration
    // FR: Configuration de correspondance floue
    double fuzzy_threshold{0.85};             // EN: Similarity threshold (0.0-1.0) / FR: Seuil de similarité (0.0-1.0)
    bool enable_phonetic_matching{false};     // EN: Enable phonetic matching algorithms / FR: Activer algorithmes de correspondance phonétique
    
    // EN: Output configuration
    // FR: Configuration de sortie
    std::string output_filepath;              // EN: Path for merged output file / FR: Chemin pour fichier de sortie fusionné
    char output_delimiter{','};               // EN: Output CSV delimiter / FR: Délimiteur CSV de sortie
    std::string output_encoding{"UTF-8"};     // EN: Output file encoding / FR: Encodage du fichier de sortie
    bool write_source_info{false};            // EN: Add source tracking columns / FR: Ajouter colonnes de suivi de source
    bool preserve_order{true};                // EN: Preserve original row ordering / FR: Préserver l'ordre original des lignes
    
    // EN: Memory and performance settings
    // FR: Paramètres mémoire et performance
    size_t memory_limit{512 * 1024 * 1024};   // EN: Memory limit in bytes (512MB) / FR: Limite mémoire en octets (512MB)
    size_t chunk_size{10000};                 // EN: Processing chunk size / FR: Taille de chunk de traitement
    bool enable_streaming{true};              // EN: Enable streaming for large files / FR: Activer streaming pour gros fichiers
    bool parallel_processing{true};           // EN: Enable parallel processing / FR: Activer traitement parallèle
    size_t max_threads{4};                    // EN: Maximum number of threads / FR: Nombre maximum de threads
    
    // EN: Advanced options
    // FR: Options avancées
    bool strict_schema_validation{true};      // EN: Enforce strict schema validation / FR: Forcer validation stricte du schéma
    bool auto_detect_types{true};             // EN: Automatically detect column types / FR: Détecter automatiquement les types de colonnes
    std::vector<std::regex> exclude_patterns; // EN: Patterns for excluding rows / FR: Patterns pour exclure des lignes
    std::unordered_map<std::string, std::string> column_mappings; // EN: Column name mappings / FR: Mappings de noms de colonnes
    
    // EN: Custom functions
    // FR: Fonctions personnalisées
    std::function<bool(const std::vector<std::string>&, const std::vector<std::string>&)> custom_dedup_function;
    std::function<std::vector<std::string>(const std::vector<std::vector<std::string>>&)> custom_conflict_resolver;
    std::function<bool(const std::vector<std::string>&)> custom_row_filter;
};

// EN: Statistics collector for merge operations
// FR: Collecteur de statistiques pour opérations de fusion
class MergeStatistics {
public:
    // EN: Constructor
    // FR: Constructeur
    MergeStatistics();
    
    // EN: Reset all statistics
    // FR: Remet à zéro toutes les statistiques
    void reset();
    
    // EN: Timing operations
    // FR: Opérations de chronométrage
    void startTiming();
    void stopTiming();
    void recordPhaseTime(const std::string& phase, std::chrono::duration<double> duration);
    
    // EN: Statistics accessors
    // FR: Accesseurs de statistiques
    size_t getTotalRowsProcessed() const { return total_rows_processed_.load(); }
    size_t getTotalRowsOutput() const { return total_rows_output_.load(); }
    size_t getDuplicatesRemoved() const { return duplicates_removed_.load(); }
    size_t getConflictsResolved() const { return conflicts_resolved_.load(); }
    size_t getFilesProcessed() const { return files_processed_.load(); }
    size_t getTotalBytesProcessed() const { return total_bytes_processed_.load(); }
    
    // EN: Performance metrics
    // FR: Métriques de performance
    double getRowsPerSecond() const;
    double getBytesPerSecond() const;
    double getDeduplicationRatio() const;
    std::chrono::duration<double> getTotalDuration() const { return total_duration_; }
    std::unordered_map<std::string, std::chrono::duration<double>> getPhaseTimings() const;
    
    // EN: Statistics incrementers (thread-safe)
    // FR: Incrémenteurs de statistiques (thread-safe)
    void incrementRowsProcessed(size_t count = 1) { total_rows_processed_ += count; }
    void incrementRowsOutput(size_t count = 1) { total_rows_output_ += count; }
    void incrementDuplicatesRemoved(size_t count = 1) { duplicates_removed_ += count; }
    void incrementConflictsResolved(size_t count = 1) { conflicts_resolved_ += count; }
    void incrementFilesProcessed(size_t count = 1) { files_processed_ += count; }
    void addBytesProcessed(size_t bytes) { total_bytes_processed_ += bytes; }
    
    // EN: Error tracking
    // FR: Suivi d'erreurs
    void recordError(MergeError error, const std::string& message);
    std::unordered_map<MergeError, size_t> getErrorCounts() const;
    std::vector<std::string> getErrorMessages() const;
    
    // EN: Generate comprehensive report
    // FR: Génère un rapport complet
    std::string generateReport() const;

private:
    // EN: Core statistics (atomic for thread safety)
    // FR: Statistiques principales (atomiques pour sécurité thread)
    std::atomic<size_t> total_rows_processed_{0};
    std::atomic<size_t> total_rows_output_{0};
    std::atomic<size_t> duplicates_removed_{0};
    std::atomic<size_t> conflicts_resolved_{0};
    std::atomic<size_t> files_processed_{0};
    std::atomic<size_t> total_bytes_processed_{0};
    
    // EN: Timing information
    // FR: Informations de chronométrage
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::duration<double> total_duration_{0};
    std::unordered_map<std::string, std::chrono::duration<double>> phase_timings_;
    mutable std::mutex timing_mutex_;
    
    // EN: Error tracking
    // FR: Suivi d'erreurs
    std::unordered_map<MergeError, size_t> error_counts_;
    std::vector<std::string> error_messages_;
    mutable std::mutex error_mutex_;
};

// EN: Duplicate resolver for handling duplicate detection and resolution
// FR: Résolveur de doublons pour gérer la détection et résolution de doublons
class DuplicateResolver {
public:
    // EN: Constructor with configuration
    // FR: Constructeur avec configuration
    explicit DuplicateResolver(const MergeConfig& config);
    
    // EN: Check if two rows are duplicates based on strategy
    // FR: Vérifie si deux lignes sont des doublons selon la stratégie
    bool areDuplicates(const std::vector<std::string>& row1, 
                      const std::vector<std::string>& row2,
                      const std::vector<std::string>& headers) const;
    
    // EN: Resolve conflict between duplicate rows
    // FR: Résout le conflit entre lignes dupliquées
    std::vector<std::string> resolveConflict(
        const std::vector<std::vector<std::string>>& conflicting_rows,
        const std::vector<std::string>& headers,
        const std::vector<InputSource>& sources) const;
    
    // EN: Generate unique key for row based on key columns
    // FR: Génère une clé unique pour la ligne basée sur les colonnes clés
    std::string generateKey(const std::vector<std::string>& row,
                           const std::vector<std::string>& headers) const;
    
    // EN: Calculate similarity score between two strings (0.0-1.0)
    // FR: Calcule le score de similarité entre deux chaînes (0.0-1.0)
    double calculateSimilarity(const std::string& str1, const std::string& str2) const;

private:
    const MergeConfig& config_;
    
    // EN: Helper methods for different matching strategies
    // FR: Méthodes auxiliaires pour différentes stratégies de correspondance
    bool exactMatch(const std::vector<std::string>& row1, const std::vector<std::string>& row2) const;
    bool keyBasedMatch(const std::vector<std::string>& row1, 
                      const std::vector<std::string>& row2,
                      const std::vector<std::string>& headers) const;
    bool fuzzyMatch(const std::vector<std::string>& row1, const std::vector<std::string>& row2) const;
    bool contentHashMatch(const std::vector<std::string>& row1, const std::vector<std::string>& row2) const;
    
    // EN: String similarity algorithms
    // FR: Algorithmes de similarité de chaînes
    double levenshteinSimilarity(const std::string& str1, const std::string& str2) const;
    double jaccardSimilarity(const std::string& str1, const std::string& str2) const;
    std::string calculateHash(const std::vector<std::string>& row) const;
    
    // EN: Conflict resolution helpers
    // FR: Assistants de résolution de conflits
    std::vector<std::string> mergeValues(const std::vector<std::vector<std::string>>& rows,
                                        const std::vector<std::string>& headers) const;
    std::vector<std::string> selectByTimestamp(const std::vector<std::vector<std::string>>& rows,
                                              const std::vector<std::string>& headers,
                                              bool keep_newest) const;
};

// EN: Main merger engine class for intelligent CSV merging
// FR: Classe principale du moteur de fusion pour fusion intelligente de CSV
class MergerEngine {
public:
    // EN: Constructors
    // FR: Constructeurs
    MergerEngine();
    explicit MergerEngine(const MergeConfig& config);
    
    // EN: Destructor
    // FR: Destructeur
    ~MergerEngine() = default;
    
    // EN: Move semantics
    // FR: Sémantiques de déplacement
    // Move operations deleted due to atomic members
    MergerEngine(MergerEngine&& other) = delete;
    MergerEngine& operator=(MergerEngine&& other) = delete;
    
    // EN: Copy operations (deleted for performance)
    // FR: Opérations de copie (supprimées pour performance)
    MergerEngine(const MergerEngine&) = delete;
    MergerEngine& operator=(const MergerEngine&) = delete;
    
    // EN: Configuration management
    // FR: Gestion de configuration
    void setConfig(const MergeConfig& config);
    const MergeConfig& getConfig() const { return config_; }
    
    // EN: Input source management
    // FR: Gestion des sources d'entrée
    void addInputSource(const InputSource& source);
    void addInputSources(const std::vector<InputSource>& sources);
    void clearInputSources();
    size_t getInputSourceCount() const { return input_sources_.size(); }
    
    // EN: Main merge operations
    // FR: Opérations de fusion principales
    MergeError merge();
    MergeError mergeToFile(const std::string& output_path);
    MergeError mergeToStream(std::ostream& output_stream);
    
    // EN: Advanced merge operations
    // FR: Opérations de fusion avancées
    MergeError mergeWithCallback(std::function<bool(const std::vector<std::string>&, size_t)> row_callback);
    MergeError previewMerge(size_t max_rows = 100); // EN: Preview merge without writing / FR: Aperçu fusion sans écriture
    
    // EN: Statistics and monitoring
    // FR: Statistiques et surveillance
    const MergeStatistics& getStatistics() const { return stats_; }
    void resetStatistics() { stats_.reset(); }
    
    // EN: Schema operations
    // FR: Opérations de schéma
    std::vector<std::string> inferMergedSchema() const;
    bool validateSchemaCompatibility() const;
    std::unordered_map<std::string, std::vector<std::string>> getSchemaConflicts() const;
    
    // EN: Utility methods
    // FR: Méthodes utilitaires
    std::vector<std::string> getSupportedEncodings() const;
    std::vector<char> getSupportedDelimiters() const;
    size_t estimateOutputSize() const;
    size_t estimateMemoryUsage() const;
    
    // EN: Callback setters for monitoring
    // FR: Setters de callbacks pour surveillance
    void setProgressCallback(std::function<void(double, const std::string&)> callback) { progress_callback_ = callback; }
    void setErrorCallback(std::function<void(MergeError, const std::string&)> callback) { error_callback_ = callback; }

private:
    // EN: Configuration and state
    // FR: Configuration et état
    MergeConfig config_;
    std::vector<InputSource> input_sources_;
    MergeStatistics stats_;
    std::unique_ptr<DuplicateResolver> duplicate_resolver_;
    
    // EN: Callbacks
    // FR: Callbacks
    std::function<void(double, const std::string&)> progress_callback_;
    std::function<void(MergeError, const std::string&)> error_callback_;
    
    // EN: Thread safety
    // FR: Sécurité thread
    mutable std::mutex engine_mutex_;
    
    // EN: Internal processing methods
    // FR: Méthodes de traitement internes
    MergeError loadAndValidateSources();
    MergeError performMerge(std::ostream& output_stream);
    MergeError mergeWithStrategy();
    
    // EN: Merge strategy implementations
    // FR: Implémentations des stratégies de fusion
    MergeError appendMerge(std::ostream& output_stream);
    MergeError smartMerge(std::ostream& output_stream);
    MergeError priorityMerge(std::ostream& output_stream);
    MergeError timeBasedMerge(std::ostream& output_stream);
    MergeError schemaAwareMerge(std::ostream& output_stream);
    
    // EN: Helper methods
    // FR: Méthodes auxiliaires
    std::vector<std::string> readCsvHeaders(const std::string& filepath, char delimiter) const;
    std::vector<std::vector<std::string>> readCsvFile(const InputSource& source) const;
    bool writeRow(std::ostream& output_stream, const std::vector<std::string>& row) const;
    void reportProgress(double progress, const std::string& message) const;
    void reportError(MergeError error, const std::string& message);
    
    // EN: Schema validation and alignment
    // FR: Validation et alignement de schéma
    bool alignSchemas();
    std::vector<std::string> harmonizeHeaders() const;
    std::unordered_map<std::string, std::string> buildColumnMapping(
        const std::vector<std::string>& from_headers,
        const std::vector<std::string>& to_headers) const;
    
    // EN: Memory management for large datasets
    // FR: Gestion mémoire pour gros datasets
    bool shouldUseStreaming() const;
    MergeError streamingMerge(std::ostream& output_stream);
    void optimizeMemoryUsage();
};

// EN: Utility functions for merge operations
// FR: Fonctions utilitaires pour opérations de fusion
namespace MergeUtils {
    // EN: String utility functions
    // FR: Fonctions utilitaires de chaînes
    std::string trim(const std::string& str);
    std::string toLower(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
    std::string join(const std::vector<std::string>& parts, char delimiter);
    
    // EN: CSV parsing utilities
    // FR: Utilitaires de parsing CSV
    std::vector<std::string> parseCsvRow(const std::string& line, char delimiter);
    std::string escapeCsvField(const std::string& field, char delimiter);
    bool needsQuoting(const std::string& field, char delimiter);
    
    // EN: File and encoding utilities
    // FR: Utilitaires de fichier et encodage
    bool fileExists(const std::string& filepath);
    size_t getFileSize(const std::string& filepath);
    std::string detectEncoding(const std::string& filepath);
    char detectDelimiter(const std::string& filepath);
    
    // EN: Type detection and conversion
    // FR: Détection et conversion de type
    enum class DataType { STRING, INTEGER, FLOAT, BOOLEAN, DATE, TIMESTAMP };
    DataType detectType(const std::string& value);
    bool isNumeric(const std::string& value);
    bool isDate(const std::string& value);
    bool isTimestamp(const std::string& value);
    
    // EN: Performance optimization utilities
    // FR: Utilitaires d'optimisation de performance
    size_t estimateRowSize(const std::vector<std::string>& row);
    size_t estimateFileRows(const std::string& filepath);
    bool shouldUseParallelProcessing(size_t total_rows, size_t available_memory);
}

} // namespace CSV
} // namespace BBP