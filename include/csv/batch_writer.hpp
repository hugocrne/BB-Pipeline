// EN: High-performance batch CSV writer with periodic flush and compression support
// FR: Writer CSV batch haute performance avec flush périodique et support de compression

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <chrono>
#include <optional>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <sstream>

namespace BBP {
namespace CSV {

// EN: Forward declarations
// FR: Déclarations anticipées
class BatchWriter;
class WriterBuffer;
class WriterStatistics;

// EN: Compression types supported by the writer
// FR: Types de compression supportés par le writer
enum class CompressionType {
    NONE,           // EN: No compression / FR: Pas de compression
    GZIP,           // EN: GZIP compression / FR: Compression GZIP
    ZLIB,           // EN: ZLIB compression / FR: Compression ZLIB
    AUTO            // EN: Automatic selection based on file extension / FR: Sélection automatique basée sur l'extension de fichier
};

// EN: Writer error types
// FR: Types d'erreur du writer
enum class WriterError {
    SUCCESS,                    // EN: No error / FR: Aucune erreur
    FILE_WRITE_ERROR,          // EN: Error writing to file / FR: Erreur d'écriture dans le fichier
    FILE_OPEN_ERROR,           // EN: Error opening file / FR: Erreur d'ouverture du fichier
    COMPRESSION_ERROR,         // EN: Compression operation failed / FR: Échec de l'opération de compression
    BUFFER_OVERFLOW,           // EN: Internal buffer overflow / FR: Débordement de buffer interne
    MEMORY_ALLOCATION_ERROR,   // EN: Memory allocation failure / FR: Échec d'allocation mémoire
    INVALID_CONFIGURATION,     // EN: Invalid writer configuration / FR: Configuration de writer invalide
    THREAD_ERROR,              // EN: Threading/concurrency error / FR: Erreur de threading/concurrence
    DISK_SPACE_ERROR          // EN: Insufficient disk space / FR: Espace disque insuffisant
};

// EN: Flush trigger types
// FR: Types de déclenchement de flush
enum class FlushTrigger {
    MANUAL,         // EN: Manual flush only / FR: Flush manuel uniquement
    ROW_COUNT,      // EN: Flush after N rows / FR: Flush après N lignes
    BUFFER_SIZE,    // EN: Flush when buffer reaches size limit / FR: Flush quand le buffer atteint la limite de taille
    TIME_INTERVAL,  // EN: Flush after time interval / FR: Flush après intervalle de temps
    MIXED           // EN: Combination of triggers / FR: Combinaison de déclencheurs
};

// EN: Writer configuration options
// FR: Options de configuration du writer
struct WriterConfig {
    // EN: File format configuration
    // FR: Configuration du format de fichier
    char delimiter{','};                    // EN: Field delimiter character / FR: Caractère délimiteur de champ
    char quote_char{'"'};                   // EN: Quote character for fields / FR: Caractère de quote pour les champs
    char escape_char{'"'};                  // EN: Escape character / FR: Caractère d'échappement
    bool always_quote{false};               // EN: Always quote all fields / FR: Toujours quoter tous les champs
    bool quote_empty_fields{false};         // EN: Quote empty fields / FR: Quoter les champs vides
    std::string line_ending{"\n"};         // EN: Line ending sequence / FR: Séquence de fin de ligne
    std::string encoding{"UTF-8"};         // EN: File encoding / FR: Encodage du fichier
    bool write_header{true};                // EN: Write header row / FR: Écrire la ligne d'en-tête
    bool write_bom{false};                  // EN: Write BOM for UTF-8/16 / FR: Écrire BOM pour UTF-8/16
    
    // EN: Buffer and performance configuration
    // FR: Configuration buffer et performance
    size_t buffer_size{65536};              // EN: Buffer size in bytes (64KB default) / FR: Taille du buffer en octets (64KB par défaut)
    size_t max_rows_in_buffer{10000};       // EN: Maximum rows to buffer before flush / FR: Maximum de lignes à buffer avant flush
    size_t max_field_size{1048576};         // EN: Maximum field size (1MB default) / FR: Taille maximum de champ (1MB par défaut)
    bool enable_background_flush{true};     // EN: Enable background flush thread / FR: Activer le thread de flush en arrière-plan
    
    // EN: Flush configuration
    // FR: Configuration de flush
    FlushTrigger flush_trigger{FlushTrigger::MIXED};  // EN: When to trigger flush / FR: Quand déclencher le flush
    std::chrono::milliseconds flush_interval{5000};  // EN: Auto-flush interval (5s default) / FR: Intervalle de flush automatique (5s par défaut)
    size_t flush_row_threshold{1000};                // EN: Flush after N rows / FR: Flush après N lignes
    size_t flush_size_threshold{32768};              // EN: Flush when buffer reaches size / FR: Flush quand le buffer atteint la taille
    
    // EN: Compression configuration
    // FR: Configuration de compression
    CompressionType compression{CompressionType::NONE};  // EN: Compression type / FR: Type de compression
    int compression_level{6};                            // EN: Compression level (1-9) / FR: Niveau de compression (1-9)
    bool compress_in_background{true};                   // EN: Compress in background thread / FR: Comprimer dans un thread en arrière-plan
    
    // EN: Error handling and recovery
    // FR: Gestion d'erreur et récupération
    bool create_backup{false};              // EN: Create backup before overwriting / FR: Créer une sauvegarde avant écrasement
    size_t max_retry_attempts{3};           // EN: Maximum retry attempts on error / FR: Nombre maximum de tentatives en cas d'erreur
    std::chrono::milliseconds retry_delay{1000}; // EN: Delay between retries / FR: Délai entre les tentatives
    bool continue_on_error{false};          // EN: Continue writing on non-fatal errors / FR: Continuer l'écriture sur erreurs non-fatales
    std::string temp_file_suffix{".tmp"};   // EN: Suffix for temporary files / FR: Suffixe pour fichiers temporaires
    
    // EN: Threading configuration
    // FR: Configuration de threading
    bool enable_concurrent_access{false};   // EN: Enable thread-safe concurrent access / FR: Activer l'accès concurrent thread-safe
    size_t writer_thread_count{1};         // EN: Number of writer threads / FR: Nombre de threads de writer
    
    // EN: Default constructor with sensible defaults
    // FR: Constructeur par défaut avec valeurs par défaut sensées
    WriterConfig() = default;
    
    // EN: Validate configuration
    // FR: Valider la configuration
    bool isValid() const;
    
    // EN: Auto-detect compression from filename
    // FR: Détection automatique de compression depuis le nom de fichier
    CompressionType detectCompressionFromFilename(const std::string& filename) const;
};

// EN: Represents a CSV row to be written
// FR: Représente une ligne CSV à écrire
class CsvRow {
public:
    // EN: Constructors
    // FR: Constructeurs
    CsvRow() = default;
    explicit CsvRow(const std::vector<std::string>& fields);
    explicit CsvRow(std::vector<std::string>&& fields);
    CsvRow(std::initializer_list<std::string> fields);
    
    // EN: Copy and move semantics
    // FR: Sémantiques de copie et déplacement
    CsvRow(const CsvRow& other) = default;
    CsvRow& operator=(const CsvRow& other) = default;
    CsvRow(CsvRow&& other) noexcept = default;
    CsvRow& operator=(CsvRow&& other) noexcept = default;
    
    // EN: Field access and manipulation
    // FR: Accès et manipulation des champs
    void addField(const std::string& field);
    void addField(std::string&& field);
    void setField(size_t index, const std::string& field);
    void setField(size_t index, std::string&& field);
    const std::string& getField(size_t index) const;
    std::string& getField(size_t index);
    
    // EN: Operators for convenience
    // FR: Opérateurs pour commodité
    CsvRow& operator<<(const std::string& field);
    CsvRow& operator<<(std::string&& field);
    const std::string& operator[](size_t index) const;
    std::string& operator[](size_t index);
    
    // EN: Template methods for type conversion
    // FR: Méthodes template pour conversion de type
    template<typename T>
    void addField(const T& value);
    
    template<typename T>
    void setField(size_t index, const T& value);
    
    // EN: Row information
    // FR: Informations de ligne
    size_t getFieldCount() const { return fields_.size(); }
    bool isEmpty() const { return fields_.empty(); }
    void clear() { fields_.clear(); }
    void reserve(size_t capacity) { fields_.reserve(capacity); }
    
    // EN: Iterators for range-based for loops
    // FR: Itérateurs pour boucles for basées sur plage
    std::vector<std::string>::iterator begin() { return fields_.begin(); }
    std::vector<std::string>::iterator end() { return fields_.end(); }
    std::vector<std::string>::const_iterator begin() const { return fields_.begin(); }
    std::vector<std::string>::const_iterator end() const { return fields_.end(); }
    std::vector<std::string>::const_iterator cbegin() const { return fields_.cbegin(); }
    std::vector<std::string>::const_iterator cend() const { return fields_.cend(); }
    
    // EN: Conversion to string
    // FR: Conversion en chaîne
    std::string toString(const WriterConfig& config = WriterConfig{}) const;
    
private:
    std::vector<std::string> fields_;       // EN: Field values / FR: Valeurs des champs
};

// EN: Writer statistics and performance metrics
// FR: Statistiques du writer et métriques de performance
class WriterStatistics {
public:
    // EN: Default constructor
    // FR: Constructeur par défaut
    WriterStatistics();
    
    // EN: Copy constructor for safe copying of atomic values
    // FR: Constructeur de copie pour copie sûre des valeurs atomiques
    WriterStatistics(const WriterStatistics& other);
    WriterStatistics& operator=(const WriterStatistics& other);
    
    // EN: Reset all statistics
    // FR: Remet à zéro toutes les statistiques
    void reset();
    
    // EN: Timing operations
    // FR: Opérations de chronométrage
    void startTiming();
    void stopTiming();
    void recordFlushTime(std::chrono::duration<double> duration);
    void recordCompressionTime(std::chrono::duration<double> duration);
    
    // EN: Statistics updates
    // FR: Mises à jour des statistiques
    void incrementRowsWritten() { rows_written_++; }
    void incrementRowsSkipped() { rows_skipped_++; }
    void incrementRowsWithErrors() { rows_with_errors_++; }
    void incrementFlushCount() { flush_count_++; }
    void addBytesWritten(size_t bytes) { bytes_written_ += bytes; }
    void addBytesCompressed(size_t original, size_t compressed);
    void recordBufferUtilization(double utilization);
    void recordError(WriterError error);
    
    // EN: Getters
    // FR: Accesseurs
    size_t getRowsWritten() const { return rows_written_.load(); }
    size_t getRowsSkipped() const { return rows_skipped_.load(); }
    size_t getRowsWithErrors() const { return rows_with_errors_.load(); }
    size_t getFlushCount() const { return flush_count_.load(); }
    size_t getBytesWritten() const { return bytes_written_.load(); }
    size_t getBytesOriginal() const { return bytes_original_.load(); }
    size_t getBytesCompressed() const { return bytes_compressed_.load(); }
    std::chrono::duration<double> getWritingDuration() const { return writing_duration_; }
    std::chrono::duration<double> getTotalFlushTime() const { return total_flush_time_; }
    std::chrono::duration<double> getTotalCompressionTime() const { return total_compression_time_; }
    
    // EN: Calculated metrics
    // FR: Métriques calculées
    double getRowsPerSecond() const;
    double getBytesPerSecond() const;
    double getCompressionRatio() const;
    double getAverageBufferUtilization() const;
    double getAverageFlushTime() const;
    double getAverageCompressionTime() const;
    std::unordered_map<WriterError, size_t> getErrorCounts() const;
    
    // EN: Generate comprehensive report
    // FR: Génère un rapport complet
    std::string generateReport() const;
    
private:
    // EN: Core statistics (atomic for thread safety)
    // FR: Statistiques principales (atomiques pour sécurité thread)
    std::atomic<size_t> rows_written_{0};           // EN: Number of rows successfully written / FR: Nombre de lignes écrites avec succès
    std::atomic<size_t> rows_skipped_{0};           // EN: Number of rows skipped / FR: Nombre de lignes ignorées
    std::atomic<size_t> rows_with_errors_{0};       // EN: Number of rows with errors / FR: Nombre de lignes avec erreurs
    std::atomic<size_t> flush_count_{0};            // EN: Number of flush operations / FR: Nombre d'opérations de flush
    std::atomic<size_t> bytes_written_{0};          // EN: Total bytes written / FR: Total d'octets écrits
    std::atomic<size_t> bytes_original_{0};         // EN: Original bytes before compression / FR: Octets originaux avant compression
    std::atomic<size_t> bytes_compressed_{0};       // EN: Compressed bytes / FR: Octets compressés
    
    // EN: Timing information
    // FR: Informations de chronométrage
    std::chrono::high_resolution_clock::time_point start_time_;  // EN: Writing start time / FR: Heure de début d'écriture
    std::chrono::duration<double> writing_duration_{0};         // EN: Total writing duration / FR: Durée totale d'écriture
    std::chrono::duration<double> total_flush_time_{0};         // EN: Total flush time / FR: Temps total de flush
    std::chrono::duration<double> total_compression_time_{0};   // EN: Total compression time / FR: Temps total de compression
    
    // EN: Buffer and performance metrics
    // FR: Métriques de buffer et performance
    std::atomic<double> total_buffer_utilization_{0.0}; // EN: Sum of buffer utilization samples / FR: Somme des échantillons d'utilisation buffer
    std::atomic<size_t> buffer_utilization_samples_{0}; // EN: Number of buffer utilization samples / FR: Nombre d'échantillons d'utilisation buffer
    
    // EN: Error tracking
    // FR: Suivi des erreurs
    std::unordered_map<WriterError, std::atomic<size_t>> error_counts_; // EN: Count of each error type / FR: Compte de chaque type d'erreur
    mutable std::mutex stats_mutex_;                    // EN: Mutex for thread-safe access / FR: Mutex pour accès thread-safe
};

// EN: Callback function types
// FR: Types de fonction callback
using FlushCallback = std::function<void(size_t rows_written, size_t bytes_written)>;
using ErrorCallback = std::function<void(WriterError error, const std::string& message)>;
using ProgressCallback = std::function<void(size_t rows_written, double progress_percent)>;

// EN: High-performance batch CSV writer with compression and periodic flush
// FR: Writer CSV batch haute performance avec compression et flush périodique
class BatchWriter {
public:
    // EN: Constructor with default configuration
    // FR: Constructeur avec configuration par défaut
    BatchWriter();
    
    // EN: Constructor with custom configuration
    // FR: Constructeur avec configuration personnalisée
    explicit BatchWriter(const WriterConfig& config);
    
    // EN: Destructor - ensures proper cleanup and flush
    // FR: Destructeur - garantit nettoyage et flush appropriés
    ~BatchWriter();
    
    // EN: Copy constructor and assignment (deleted for performance and safety)
    // FR: Constructeur de copie et assignation (supprimés pour performance et sécurité)
    BatchWriter(const BatchWriter&) = delete;
    BatchWriter& operator=(const BatchWriter&) = delete;
    
    // EN: Move constructor and assignment
    // FR: Constructeur de déplacement et assignation
    BatchWriter(BatchWriter&& other) noexcept;
    BatchWriter& operator=(BatchWriter&& other) noexcept;
    
    // EN: Configuration management
    // FR: Gestion de la configuration
    void setConfig(const WriterConfig& config);
    const WriterConfig& getConfig() const { return config_; }
    
    // EN: Callback registration
    // FR: Enregistrement des callbacks
    void setFlushCallback(FlushCallback callback) { flush_callback_ = std::move(callback); }
    void setErrorCallback(ErrorCallback callback) { error_callback_ = std::move(callback); }
    void setProgressCallback(ProgressCallback callback) { progress_callback_ = std::move(callback); }
    
    // EN: File operations
    // FR: Opérations de fichier
    WriterError openFile(const std::string& filename);
    WriterError openStream(std::ostream& stream);
    WriterError closeFile();
    bool isOpen() const { return file_open_; }
    std::string getCurrentFilename() const { return current_filename_; }
    
    // EN: Header management
    // FR: Gestion des en-têtes
    WriterError writeHeader(const std::vector<std::string>& headers);
    WriterError writeHeader(const CsvRow& header_row);
    bool hasHeaderWritten() const { return header_written_; }
    
    // EN: Row writing methods
    // FR: Méthodes d'écriture de ligne
    WriterError writeRow(const CsvRow& row);
    WriterError writeRow(CsvRow&& row);
    WriterError writeRow(const std::vector<std::string>& fields);
    WriterError writeRow(std::vector<std::string>&& fields);
    WriterError writeRow(std::initializer_list<std::string> fields);
    
    // EN: Batch writing methods
    // FR: Méthodes d'écriture en lot
    WriterError writeRows(const std::vector<CsvRow>& rows);
    WriterError writeRows(std::vector<CsvRow>&& rows);
    template<typename Iterator>
    WriterError writeRows(Iterator begin, Iterator end);
    
    // EN: Template method for writing any type
    // FR: Méthode template pour écrire n'importe quel type
    template<typename T>
    WriterError writeValue(const T& value);
    
    // EN: Flush operations
    // FR: Opérations de flush
    WriterError flush();
    WriterError flushIfNeeded();
    void enableAutoFlush(bool enable = true);
    void disableAutoFlush() { enableAutoFlush(false); }
    
    // EN: Buffer management
    // FR: Gestion du buffer
    size_t getBufferedRowCount();
    size_t getBufferSize() const;
    double getBufferUtilization() const;
    void clearBuffer();
    
    // EN: Statistics and monitoring
    // FR: Statistiques et surveillance
    WriterStatistics getStatistics() const;
    void resetStatistics() { stats_.reset(); }
    
    // EN: Utility methods
    // FR: Méthodes utilitaires
    static std::string escapeField(const std::string& field, const WriterConfig& config = WriterConfig{});
    static bool needsQuoting(const std::string& field, const WriterConfig& config = WriterConfig{});
    static WriterError createBackupFile(const std::string& filename, const std::string& backup_suffix = ".bak");
    static bool isValidFilename(const std::string& filename);
    static size_t estimateCompressedSize(size_t original_size, CompressionType compression);
    
    // EN: Advanced operations
    // FR: Opérations avancées
    WriterError setCompressionLevel(int level);
    WriterError enableCompression(CompressionType type, int level = 6);
    WriterError disableCompression();
    bool isCompressionEnabled() const;
    
    // EN: Thread control for background operations
    // FR: Contrôle de thread pour opérations en arrière-plan
    void startBackgroundFlush();
    void stopBackgroundFlush();
    bool isBackgroundFlushRunning() const { return background_flush_running_; }
    
    // EN: Error recovery
    // FR: Récupération d'erreur
    WriterError recover();
    void setRetryPolicy(size_t max_attempts, std::chrono::milliseconds delay);
    
private:
    // EN: Configuration and state
    // FR: Configuration et état
    WriterConfig config_;                           // EN: Writer configuration / FR: Configuration du writer
    std::string current_filename_;                  // EN: Current output filename / FR: Nom de fichier de sortie actuel
    bool file_open_{false};                        // EN: File open status / FR: État d'ouverture du fichier
    bool header_written_{false};                   // EN: Header written status / FR: État d'écriture de l'en-tête
    
    // EN: File handling
    // FR: Gestion de fichier
    std::unique_ptr<std::ostream, std::function<void(std::ostream*)>> output_stream_;   // EN: Output stream / FR: Stream de sortie
    std::unique_ptr<std::ofstream> file_stream_;    // EN: File stream / FR: Stream de fichier
    bool owns_stream_{false};                      // EN: Whether we own the stream / FR: Si on possède le stream
    
    // EN: Buffer management
    // FR: Gestion du buffer
    std::vector<CsvRow> row_buffer_;               // EN: Buffered rows / FR: Lignes mises en buffer
    std::ostringstream string_buffer_;             // EN: String buffer for formatting / FR: Buffer de chaîne pour formatage
    size_t current_buffer_size_{0};               // EN: Current buffer size in bytes / FR: Taille actuelle du buffer en octets
    std::chrono::steady_clock::time_point last_flush_time_; // EN: Last flush timestamp / FR: Timestamp du dernier flush
    
    // EN: Threading support
    // FR: Support de threading
    std::mutex writer_mutex_;                      // EN: Main writer mutex / FR: Mutex principal du writer
    std::mutex buffer_mutex_;                      // EN: Buffer access mutex / FR: Mutex d'accès au buffer
    std::condition_variable flush_condition_;      // EN: Flush condition variable / FR: Variable de condition de flush
    std::unique_ptr<std::thread> background_thread_; // EN: Background flush thread / FR: Thread de flush en arrière-plan
    std::atomic<bool> background_flush_running_{false}; // EN: Background flush status / FR: État du flush en arrière-plan
    std::atomic<bool> should_stop_background_{false}; // EN: Stop background thread flag / FR: Flag d'arrêt du thread arrière-plan
    
    // EN: Callbacks
    // FR: Callbacks
    FlushCallback flush_callback_;                 // EN: Flush completion callback / FR: Callback de fin de flush
    ErrorCallback error_callback_;                 // EN: Error handling callback / FR: Callback de gestion d'erreur
    ProgressCallback progress_callback_;           // EN: Progress reporting callback / FR: Callback de rapport de progression
    
    // EN: Statistics and monitoring
    // FR: Statistiques et surveillance
    WriterStatistics stats_;                       // EN: Performance statistics / FR: Statistiques de performance
    
    // EN: Internal methods
    // FR: Méthodes internes
    WriterError openFileInternal(const std::string& filename);
    WriterError flushInternal();
    WriterError writeRowInternal(const CsvRow& row);
    WriterError compressAndWrite(const std::string& data);
    std::string formatRow(const CsvRow& row) const;
    bool shouldFlush() const;
    void backgroundFlushWorker();
    void reportError(WriterError error, const std::string& message);
    void reportProgress();
    void updateBufferSize();
    WriterError retryOperation(std::function<WriterError()> operation);
    
    // EN: Compression helpers
    // FR: Assistants de compression
    WriterError initializeCompression();
    WriterError finalizeCompression();
    std::string compressString(const std::string& input, CompressionType type, int level);
    
    // EN: File system helpers
    // FR: Assistants du système de fichiers
    bool hasEnoughDiskSpace(size_t required_bytes) const;
    std::string generateTempFilename() const;
    WriterError atomicFileWrite(const std::string& filename, const std::string& data);
};

// EN: Template implementations
// FR: Implémentations de template

template<typename T>
void CsvRow::addField(const T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        fields_.push_back(value);
    } else if constexpr (std::is_arithmetic_v<T>) {
        fields_.push_back(std::to_string(value));
    } else if constexpr (std::is_same_v<T, bool>) {
        fields_.push_back(value ? "true" : "false");
    } else {
        // EN: Try to use stream operator
        // FR: Essaie d'utiliser l'opérateur de stream
        std::ostringstream oss;
        oss << value;
        fields_.push_back(oss.str());
    }
}

template<typename T>
void CsvRow::setField(size_t index, const T& value) {
    if (index >= fields_.size()) {
        fields_.resize(index + 1);
    }
    
    if constexpr (std::is_same_v<T, std::string>) {
        fields_[index] = value;
    } else if constexpr (std::is_arithmetic_v<T>) {
        fields_[index] = std::to_string(value);
    } else if constexpr (std::is_same_v<T, bool>) {
        fields_[index] = value ? "true" : "false";
    } else {
        // EN: Try to use stream operator
        // FR: Essaie d'utiliser l'opérateur de stream
        std::ostringstream oss;
        oss << value;
        fields_[index] = oss.str();
    }
}

template<typename Iterator>
WriterError BatchWriter::writeRows(Iterator begin, Iterator end) {
    WriterError last_error = WriterError::SUCCESS;
    
    for (auto it = begin; it != end; ++it) {
        WriterError error = writeRow(*it);
        if (error != WriterError::SUCCESS) {
            last_error = error;
            if (!config_.continue_on_error) {
                return error;
            }
        }
    }
    
    return last_error;
}

template<typename T>
WriterError BatchWriter::writeValue(const T& value) {
    CsvRow row;
    row.addField(value);
    return writeRow(std::move(row));
}

} // namespace CSV
} // namespace BBP