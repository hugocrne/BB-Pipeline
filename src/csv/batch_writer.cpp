// EN: High-performance batch CSV writer implementation with compression and periodic flush
// FR: Implémentation du writer CSV batch haute performance avec compression et flush périodique

#include "csv/batch_writer.hpp"
#include "infrastructure/logging/logger.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <zlib.h>
#include <cstring>

namespace BBP {
namespace CSV {

// EN: WriterConfig implementation
// FR: Implémentation de WriterConfig

bool WriterConfig::isValid() const {
    // EN: Basic validation of configuration parameters
    // FR: Validation basique des paramètres de configuration
    if (buffer_size == 0 || max_rows_in_buffer == 0 || max_field_size == 0) {
        return false;
    }
    
    if (flush_row_threshold == 0 && flush_trigger == FlushTrigger::ROW_COUNT) {
        return false;
    }
    
    if (flush_size_threshold == 0 && flush_trigger == FlushTrigger::BUFFER_SIZE) {
        return false;
    }
    
    if (compression_level < 1 || compression_level > 9) {
        return false;
    }
    
    if (max_retry_attempts == 0) {
        return false;
    }
    
    return true;
}

CompressionType WriterConfig::detectCompressionFromFilename(const std::string& filename) const {
    // EN: Auto-detect compression type from file extension
    // FR: Détection automatique du type de compression depuis l'extension de fichier
    std::string lower_filename = filename;
    std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), ::tolower);
    
    if (lower_filename.ends_with(".gz") || lower_filename.ends_with(".gzip")) {
        return CompressionType::GZIP;
    } else if (lower_filename.ends_with(".z") || lower_filename.ends_with(".zlib")) {
        return CompressionType::ZLIB;
    } else {
        return CompressionType::NONE;
    }
}

// EN: CsvRow implementation
// FR: Implémentation de CsvRow

CsvRow::CsvRow(const std::vector<std::string>& fields) : fields_(fields) {}

CsvRow::CsvRow(std::vector<std::string>&& fields) : fields_(std::move(fields)) {}

CsvRow::CsvRow(std::initializer_list<std::string> fields) : fields_(fields) {}

void CsvRow::addField(const std::string& field) {
    fields_.push_back(field);
}

void CsvRow::addField(std::string&& field) {
    fields_.push_back(std::move(field));
}

void CsvRow::setField(size_t index, const std::string& field) {
    if (index >= fields_.size()) {
        fields_.resize(index + 1);
    }
    fields_[index] = field;
}

void CsvRow::setField(size_t index, std::string&& field) {
    if (index >= fields_.size()) {
        fields_.resize(index + 1);
    }
    fields_[index] = std::move(field);
}

const std::string& CsvRow::getField(size_t index) const {
    static const std::string empty_string;
    if (index >= fields_.size()) {
        return empty_string;
    }
    return fields_[index];
}

std::string& CsvRow::getField(size_t index) {
    if (index >= fields_.size()) {
        fields_.resize(index + 1);
    }
    return fields_[index];
}

CsvRow& CsvRow::operator<<(const std::string& field) {
    addField(field);
    return *this;
}

CsvRow& CsvRow::operator<<(std::string&& field) {
    addField(std::move(field));
    return *this;
}

const std::string& CsvRow::operator[](size_t index) const {
    return getField(index);
}

std::string& CsvRow::operator[](size_t index) {
    return getField(index);
}

std::string CsvRow::toString(const WriterConfig& config) const {
    // EN: Convert row to CSV string format
    // FR: Convertit la ligne au format chaîne CSV
    if (fields_.empty()) {
        return std::string();
    }
    
    std::ostringstream oss;
    for (size_t i = 0; i < fields_.size(); ++i) {
        if (i > 0) {
            oss << config.delimiter;
        }
        
        const std::string& field = fields_[i];
        
        if (config.always_quote || BatchWriter::needsQuoting(field, config)) {
            oss << config.quote_char;
            // EN: Escape quotes within the field
            // FR: Échappe les quotes dans le champ
            for (char c : field) {
                if (c == config.quote_char) {
                    oss << config.escape_char << config.quote_char;
                } else {
                    oss << c;
                }
            }
            oss << config.quote_char;
        } else {
            oss << field;
        }
    }
    
    return oss.str();
}

// EN: WriterStatistics implementation
// FR: Implémentation de WriterStatistics

WriterStatistics::WriterStatistics() {
    reset();
}

WriterStatistics::WriterStatistics(const WriterStatistics& other) {
    // EN: Copy atomic values safely
    // FR: Copie les valeurs atomiques en sécurité
    rows_written_ = other.rows_written_.load();
    rows_skipped_ = other.rows_skipped_.load();
    rows_with_errors_ = other.rows_with_errors_.load();
    flush_count_ = other.flush_count_.load();
    bytes_written_ = other.bytes_written_.load();
    bytes_original_ = other.bytes_original_.load();
    bytes_compressed_ = other.bytes_compressed_.load();
    writing_duration_ = other.writing_duration_;
    total_flush_time_ = other.total_flush_time_;
    total_compression_time_ = other.total_compression_time_;
    total_buffer_utilization_ = other.total_buffer_utilization_.load();
    buffer_utilization_samples_ = other.buffer_utilization_samples_.load();
    start_time_ = other.start_time_;
    
    // EN: Skip copying error counts (complex with atomics)
    // FR: Ignore la copie des compteurs d'erreur (complexe avec les atomiques)
}

WriterStatistics& WriterStatistics::operator=(const WriterStatistics& other) {
    if (this != &other) {
        // EN: Copy atomic values safely
        // FR: Copie les valeurs atomiques en sécurité
        rows_written_ = other.rows_written_.load();
        rows_skipped_ = other.rows_skipped_.load();
        rows_with_errors_ = other.rows_with_errors_.load();
        flush_count_ = other.flush_count_.load();
        bytes_written_ = other.bytes_written_.load();
        bytes_original_ = other.bytes_original_.load();
        bytes_compressed_ = other.bytes_compressed_.load();
        writing_duration_ = other.writing_duration_;
        total_flush_time_ = other.total_flush_time_;
        total_compression_time_ = other.total_compression_time_;
        total_buffer_utilization_ = other.total_buffer_utilization_.load();
        buffer_utilization_samples_ = other.buffer_utilization_samples_.load();
        start_time_ = other.start_time_;
        
        // EN: Skip copying error counts (complex with atomics)
        // FR: Ignore la copie des compteurs d'erreur (complexe avec les atomiques)
    }
    return *this;
}

void WriterStatistics::reset() {
    // EN: Reset all statistics to initial state
    // FR: Remet toutes les statistiques à l'état initial
    rows_written_ = 0;
    rows_skipped_ = 0;
    rows_with_errors_ = 0;
    flush_count_ = 0;
    bytes_written_ = 0;
    bytes_original_ = 0;
    bytes_compressed_ = 0;
    writing_duration_ = std::chrono::duration<double>(0);
    total_flush_time_ = std::chrono::duration<double>(0);
    total_compression_time_ = std::chrono::duration<double>(0);
    total_buffer_utilization_ = 0.0;
    buffer_utilization_samples_ = 0;
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    error_counts_.clear();
}

void WriterStatistics::startTiming() {
    // EN: Start measuring writing time
    // FR: Commence à mesurer le temps d'écriture
    start_time_ = std::chrono::high_resolution_clock::now();
}

void WriterStatistics::stopTiming() {
    // EN: Stop measuring writing time and update duration
    // FR: Arrête de mesurer le temps d'écriture et met à jour la durée
    auto end_time = std::chrono::high_resolution_clock::now();
    writing_duration_ = end_time - start_time_;
}

void WriterStatistics::recordFlushTime(std::chrono::duration<double> duration) {
    // EN: Record flush operation time
    // FR: Enregistre le temps d'opération de flush
    total_flush_time_ += duration;
}

void WriterStatistics::recordCompressionTime(std::chrono::duration<double> duration) {
    // EN: Record compression operation time
    // FR: Enregistre le temps d'opération de compression
    total_compression_time_ += duration;
}

void WriterStatistics::addBytesCompressed(size_t original, size_t compressed) {
    // EN: Record compression statistics
    // FR: Enregistre les statistiques de compression
    bytes_original_ += original;
    bytes_compressed_ += compressed;
}

void WriterStatistics::recordBufferUtilization(double utilization) {
    // EN: Record buffer utilization sample
    // FR: Enregistre un échantillon d'utilisation du buffer
    total_buffer_utilization_ += utilization;
    buffer_utilization_samples_++;
}

void WriterStatistics::recordError(WriterError error) {
    // EN: Record error occurrence
    // FR: Enregistre l'occurrence d'une erreur
    std::lock_guard<std::mutex> lock(stats_mutex_);
    error_counts_[error]++;
}

double WriterStatistics::getRowsPerSecond() const {
    // EN: Calculate writing throughput in rows per second
    // FR: Calcule le débit d'écriture en lignes par seconde
    double duration_seconds = writing_duration_.count();
    if (duration_seconds <= 0.0) return 0.0;
    return static_cast<double>(rows_written_.load()) / duration_seconds;
}

double WriterStatistics::getBytesPerSecond() const {
    // EN: Calculate writing throughput in bytes per second
    // FR: Calcule le débit d'écriture en octets par seconde
    double duration_seconds = writing_duration_.count();
    if (duration_seconds <= 0.0) return 0.0;
    return static_cast<double>(bytes_written_.load()) / duration_seconds;
}

double WriterStatistics::getCompressionRatio() const {
    // EN: Calculate compression ratio
    // FR: Calcule le ratio de compression
    size_t original = bytes_original_.load();
    size_t compressed = bytes_compressed_.load();
    if (original == 0) return 0.0;
    return static_cast<double>(compressed) / static_cast<double>(original);
}

double WriterStatistics::getAverageBufferUtilization() const {
    // EN: Calculate average buffer utilization
    // FR: Calcule l'utilisation moyenne du buffer
    size_t samples = buffer_utilization_samples_.load();
    if (samples == 0) return 0.0;
    return total_buffer_utilization_.load() / static_cast<double>(samples);
}

double WriterStatistics::getAverageFlushTime() const {
    // EN: Calculate average flush time
    // FR: Calcule le temps moyen de flush
    size_t flushes = flush_count_.load();
    if (flushes == 0) return 0.0;
    return total_flush_time_.count() / static_cast<double>(flushes);
}

double WriterStatistics::getAverageCompressionTime() const {
    // EN: Calculate average compression time
    // FR: Calcule le temps moyen de compression
    size_t compressed_bytes = bytes_compressed_.load();
    if (compressed_bytes == 0) return 0.0;
    return total_compression_time_.count();
}

std::unordered_map<WriterError, size_t> WriterStatistics::getErrorCounts() const {
    // EN: Get copy of error counts
    // FR: Obtient une copie des comptes d'erreurs
    std::lock_guard<std::mutex> lock(stats_mutex_);
    std::unordered_map<WriterError, size_t> result;
    for (const auto& [error, count] : error_counts_) {
        result[error] = count.load();
    }
    return result;
}

std::string WriterStatistics::generateReport() const {
    // EN: Generate comprehensive statistics report
    // FR: Génère un rapport de statistiques complet
    std::ostringstream report;
    report << std::fixed << std::setprecision(2);
    
    report << "=== Batch Writer Statistics ===\n";
    report << "Writing Duration: " << writing_duration_.count() << " seconds\n";
    
    report << "Rows:\n";
    report << "  - Written: " << rows_written_.load() << "\n";
    report << "  - Skipped: " << rows_skipped_.load() << "\n";
    report << "  - With errors: " << rows_with_errors_.load() << "\n";
    report << "  - Total processed: " << (rows_written_.load() + rows_skipped_.load() + rows_with_errors_.load()) << "\n";
    
    report << "Performance:\n";
    report << "  - Bytes written: " << bytes_written_.load() << " bytes\n";
    report << "  - Rows/second: " << getRowsPerSecond() << "\n";
    report << "  - Bytes/second: " << getBytesPerSecond() << " (" 
           << (getBytesPerSecond() / 1024.0 / 1024.0) << " MB/s)\n";
    
    report << "Buffer:\n";
    report << "  - Flush operations: " << flush_count_.load() << "\n";
    report << "  - Average flush time: " << getAverageFlushTime() << " seconds\n";
    report << "  - Average buffer utilization: " << (getAverageBufferUtilization() * 100.0) << "%\n";
    
    if (bytes_compressed_.load() > 0) {
        report << "Compression:\n";
        report << "  - Original bytes: " << bytes_original_.load() << "\n";
        report << "  - Compressed bytes: " << bytes_compressed_.load() << "\n";
        report << "  - Compression ratio: " << (getCompressionRatio() * 100.0) << "%\n";
        report << "  - Space saved: " << (bytes_original_.load() - bytes_compressed_.load()) << " bytes\n";
        report << "  - Average compression time: " << getAverageCompressionTime() << " seconds\n";
    }
    
    auto error_counts = getErrorCounts();
    if (!error_counts.empty()) {
        report << "Errors:\n";
        for (const auto& [error, count] : error_counts) {
            report << "  - Error " << static_cast<int>(error) << ": " << count << " occurrences\n";
        }
    }
    
    return report.str();
}

// EN: BatchWriter implementation
// FR: Implémentation de BatchWriter

BatchWriter::BatchWriter() : config_() {
    // EN: Initialize with default configuration
    // FR: Initialise avec configuration par défaut
    last_flush_time_ = std::chrono::steady_clock::now();
    row_buffer_.reserve(config_.max_rows_in_buffer);
}

BatchWriter::BatchWriter(const WriterConfig& config) : config_(config) {
    // EN: Initialize with custom configuration
    // FR: Initialise avec configuration personnalisée
    if (!config_.isValid()) {
        reportError(WriterError::INVALID_CONFIGURATION, "Invalid writer configuration provided");
        return;
    }
    
    last_flush_time_ = std::chrono::steady_clock::now();
    row_buffer_.reserve(config_.max_rows_in_buffer);
}

BatchWriter::~BatchWriter() {
    // EN: Ensure proper cleanup and final flush
    // FR: S'assure du nettoyage approprié et du flush final
    if (background_flush_running_) {
        stopBackgroundFlush();
    }
    
    if (file_open_) {
        flush();
        closeFile();
    }
}

BatchWriter::BatchWriter(BatchWriter&& other) noexcept
    : config_(std::move(other.config_))
    , current_filename_(std::move(other.current_filename_))
    , file_open_(other.file_open_)
    , header_written_(other.header_written_)
    , output_stream_(std::move(other.output_stream_))
    , file_stream_(std::move(other.file_stream_))
    , owns_stream_(other.owns_stream_)
    , row_buffer_(std::move(other.row_buffer_))
    , string_buffer_(std::move(other.string_buffer_))
    , current_buffer_size_(other.current_buffer_size_)
    , last_flush_time_(other.last_flush_time_)
    , background_thread_(std::move(other.background_thread_))
    , flush_callback_(std::move(other.flush_callback_))
    , error_callback_(std::move(other.error_callback_))
    , progress_callback_(std::move(other.progress_callback_)) {
    
    // EN: Transfer atomic state
    // FR: Transfère l'état atomique
    background_flush_running_.store(other.background_flush_running_.load());
    should_stop_background_.store(other.should_stop_background_.load());
    
    // EN: Reset other object
    // FR: Remet l'autre objet à zéro
    other.file_open_ = false;
    other.header_written_ = false;
    other.owns_stream_ = false;
    other.current_buffer_size_ = 0;
    other.background_flush_running_.store(false);
    other.should_stop_background_.store(false);
}

BatchWriter& BatchWriter::operator=(BatchWriter&& other) noexcept {
    if (this != &other) {
        // EN: Clean up current state
        // FR: Nettoie l'état actuel
        if (background_flush_running_) {
            stopBackgroundFlush();
        }
        if (file_open_) {
            flush();
            closeFile();
        }
        
        // EN: Move all members
        // FR: Déplace tous les membres
        config_ = std::move(other.config_);
        current_filename_ = std::move(other.current_filename_);
        file_open_ = other.file_open_;
        header_written_ = other.header_written_;
        output_stream_ = std::move(other.output_stream_);
        file_stream_ = std::move(other.file_stream_);
        owns_stream_ = other.owns_stream_;
        row_buffer_ = std::move(other.row_buffer_);
        string_buffer_ = std::move(other.string_buffer_);
        current_buffer_size_ = other.current_buffer_size_;
        last_flush_time_ = other.last_flush_time_;
        background_thread_ = std::move(other.background_thread_);
        flush_callback_ = std::move(other.flush_callback_);
        error_callback_ = std::move(other.error_callback_);
        progress_callback_ = std::move(other.progress_callback_);
        
        // EN: Transfer atomic state
        // FR: Transfère l'état atomique
        background_flush_running_.store(other.background_flush_running_.load());
        should_stop_background_.store(other.should_stop_background_.load());
        
        // EN: Reset other object
        // FR: Remet l'autre objet à zéro
        other.file_open_ = false;
        other.header_written_ = false;
        other.owns_stream_ = false;
        other.current_buffer_size_ = 0;
        other.background_flush_running_.store(false);
        other.should_stop_background_.store(false);
    }
    return *this;
}

void BatchWriter::setConfig(const WriterConfig& config) {
    // EN: Update configuration (only when not actively writing)
    // FR: Met à jour la configuration (seulement quand pas en train d'écrire)
    std::lock_guard<std::mutex> lock(writer_mutex_);
    
    if (!config.isValid()) {
        reportError(WriterError::INVALID_CONFIGURATION, "Invalid writer configuration provided");
        return;
    }
    
    config_ = config;
    row_buffer_.reserve(config_.max_rows_in_buffer);
}

WriterError BatchWriter::openFile(const std::string& filename) {
    // EN: Open file for writing
    // FR: Ouvre le fichier pour écriture
    std::lock_guard<std::mutex> lock(writer_mutex_);
    
    if (file_open_) {
        reportError(WriterError::FILE_OPEN_ERROR, "File already open: " + current_filename_);
        return WriterError::FILE_OPEN_ERROR;
    }
    
    if (!isValidFilename(filename)) {
        reportError(WriterError::FILE_OPEN_ERROR, "Invalid filename: " + filename);
        return WriterError::FILE_OPEN_ERROR;
    }
    
    return openFileInternal(filename);
}

WriterError BatchWriter::openStream(std::ostream& stream) {
    // EN: Use external stream for writing
    // FR: Utilise un stream externe pour l'écriture
    std::lock_guard<std::mutex> lock(writer_mutex_);
    
    if (file_open_) {
        reportError(WriterError::FILE_OPEN_ERROR, "File already open: " + current_filename_);
        return WriterError::FILE_OPEN_ERROR;
    }
    
    output_stream_ = std::unique_ptr<std::ostream, std::function<void(std::ostream*)>>(&stream, [](std::ostream*) {
        // EN: Custom deleter that doesn't delete external stream
        // FR: Déléteur personnalisé qui ne supprime pas le stream externe
    });
    owns_stream_ = false;
    file_open_ = true;
    header_written_ = false;
    current_filename_ = "<external_stream>";
    
    stats_.startTiming();
    
    auto& logger = BBP::Logger::getInstance();
    logger.info("batch_writer", "Opened external stream for writing");
    
    return WriterError::SUCCESS;
}

WriterError BatchWriter::closeFile() {
    // EN: Close file and flush any remaining data
    // FR: Ferme le fichier et flush les données restantes
    std::lock_guard<std::mutex> lock(writer_mutex_);
    
    if (!file_open_) {
        return WriterError::SUCCESS;
    }
    
    // EN: Final flush before closing
    // FR: Flush final avant fermeture
    WriterError flush_result = flushInternal();
    
    // EN: Stop background flush if running
    // FR: Arrête le flush en arrière-plan s'il tourne
    if (background_flush_running_) {
        should_stop_background_ = true;
        flush_condition_.notify_all();
        if (background_thread_ && background_thread_->joinable()) {
            background_thread_->join();
        }
        background_flush_running_ = false;
    }
    
    // EN: Close streams
    // FR: Ferme les streams
    if (owns_stream_ && file_stream_) {
        file_stream_->close();
        file_stream_.reset();
    }
    
    output_stream_.reset();
    file_open_ = false;
    header_written_ = false;
    
    stats_.stopTiming();
    
    auto& logger = BBP::Logger::getInstance();
    logger.info("batch_writer", "Closed file: " + current_filename_ + 
                " (wrote " + std::to_string(stats_.getRowsWritten()) + " rows)");
    
    current_filename_.clear();
    return flush_result;
}

WriterError BatchWriter::writeHeader(const std::vector<std::string>& headers) {
    // EN: Write CSV header row
    // FR: Écrit la ligne d'en-tête CSV
    if (!config_.write_header) {
        return WriterError::SUCCESS;
    }
    
    if (header_written_) {
        reportError(WriterError::INVALID_CONFIGURATION, "Header already written");
        return WriterError::INVALID_CONFIGURATION;
    }
    
    CsvRow header_row(headers);
    WriterError result = writeRowInternal(header_row);
    if (result == WriterError::SUCCESS) {
        header_written_ = true;
        // EN: Force flush of header
        // FR: Force le flush de l'en-tête
        flushInternal();
    }
    
    return result;
}

WriterError BatchWriter::writeHeader(const CsvRow& header_row) {
    // EN: Write CSV header row from CsvRow
    // FR: Écrit la ligne d'en-tête CSV depuis CsvRow
    std::vector<std::string> headers;
    headers.reserve(header_row.getFieldCount());
    
    for (const auto& field : header_row) {
        headers.push_back(field);
    }
    
    return writeHeader(headers);
}

WriterError BatchWriter::writeRow(const CsvRow& row) {
    // EN: Write a single CSV row
    // FR: Écrit une seule ligne CSV
    if (!file_open_) {
        reportError(WriterError::FILE_WRITE_ERROR, "No file open for writing");
        return WriterError::FILE_WRITE_ERROR;
    }
    
    WriterError result = writeRowInternal(row);
    
    // EN: Check if we need to flush
    // FR: Vérifie si on a besoin de flusher
    if (result == WriterError::SUCCESS) {
        result = flushIfNeeded();
    }
    
    return result;
}

WriterError BatchWriter::writeRow(CsvRow&& row) {
    // EN: Write a single CSV row (move semantics)
    // FR: Écrit une seule ligne CSV (sémantiques de déplacement)
    return writeRow(row);
}

WriterError BatchWriter::writeRow(const std::vector<std::string>& fields) {
    // EN: Write row from vector of strings
    // FR: Écrit une ligne depuis un vecteur de chaînes
    CsvRow row(fields);
    return writeRow(row);
}

WriterError BatchWriter::writeRow(std::vector<std::string>&& fields) {
    // EN: Write row from vector of strings (move semantics)
    // FR: Écrit une ligne depuis un vecteur de chaînes (sémantiques de déplacement)
    CsvRow row(std::move(fields));
    return writeRow(row);
}

WriterError BatchWriter::writeRow(std::initializer_list<std::string> fields) {
    // EN: Write row from initializer list
    // FR: Écrit une ligne depuis une liste d'initialisation
    CsvRow row(fields);
    return writeRow(row);
}

WriterError BatchWriter::writeRows(const std::vector<CsvRow>& rows) {
    // EN: Write multiple rows in batch
    // FR: Écrit plusieurs lignes en lot
    WriterError last_error = WriterError::SUCCESS;
    
    for (const auto& row : rows) {
        WriterError error = writeRowInternal(row);
        if (error != WriterError::SUCCESS) {
            last_error = error;
            if (!config_.continue_on_error) {
                return error;
            }
        }
    }
    
    // EN: Flush after batch if needed
    // FR: Flush après le lot si nécessaire
    if (last_error == WriterError::SUCCESS) {
        last_error = flushIfNeeded();
    }
    
    return last_error;
}

WriterError BatchWriter::writeRows(std::vector<CsvRow>&& rows) {
    // EN: Write multiple rows in batch (move semantics)
    // FR: Écrit plusieurs lignes en lot (sémantiques de déplacement)
    return writeRows(rows);
}

WriterError BatchWriter::flush() {
    // EN: Force flush of buffered data
    // FR: Force le flush des données mises en buffer
    std::lock_guard<std::mutex> lock(writer_mutex_);
    return flushInternal();
}

WriterError BatchWriter::flushIfNeeded() {
    // EN: Flush if any trigger condition is met
    // FR: Flush si n'importe quelle condition de déclenchement est remplie
    if (shouldFlush()) {
        return flush();
    }
    return WriterError::SUCCESS;
}

void BatchWriter::enableAutoFlush(bool enable) {
    // EN: Enable or disable automatic flush
    // FR: Active ou désactive le flush automatique
    if (enable && !background_flush_running_) {
        startBackgroundFlush();
    } else if (!enable && background_flush_running_) {
        stopBackgroundFlush();
    }
}

size_t BatchWriter::getBufferedRowCount() {
    // EN: Get number of rows currently in buffer
    // FR: Obtient le nombre de lignes actuellement dans le buffer
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return row_buffer_.size();
}

size_t BatchWriter::getBufferSize() const {
    // EN: Get current buffer size in bytes
    // FR: Obtient la taille actuelle du buffer en octets
    return current_buffer_size_;
}

double BatchWriter::getBufferUtilization() const {
    // EN: Get buffer utilization percentage
    // FR: Obtient le pourcentage d'utilisation du buffer
    return static_cast<double>(current_buffer_size_) / static_cast<double>(config_.buffer_size);
}

void BatchWriter::clearBuffer() {
    // EN: Clear buffer without writing
    // FR: Vide le buffer sans écrire
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    row_buffer_.clear();
    string_buffer_.str("");
    string_buffer_.clear();
    current_buffer_size_ = 0;
}

WriterStatistics BatchWriter::getStatistics() const {
    // EN: Return a copy of statistics (safe for non-atomic access)
    // FR: Retourne une copie des statistiques (sûr pour accès non atomique)
    return stats_;
}

// EN: Static utility methods
// FR: Méthodes utilitaires statiques

std::string BatchWriter::escapeField(const std::string& field, const WriterConfig& config) {
    // EN: Escape field for CSV output
    // FR: Échappe le champ pour sortie CSV
    if (!needsQuoting(field, config)) {
        return field;
    }
    
    std::ostringstream oss;
    oss << config.quote_char;
    
    for (char c : field) {
        if (c == config.quote_char) {
            oss << config.escape_char << config.quote_char;
        } else {
            oss << c;
        }
    }
    
    oss << config.quote_char;
    return oss.str();
}

bool BatchWriter::needsQuoting(const std::string& field, const WriterConfig& config) {
    // EN: Check if field needs quoting
    // FR: Vérifie si le champ a besoin de quotes
    if (config.always_quote) {
        return true;
    }
    
    if (field.empty() && config.quote_empty_fields) {
        return true;
    }
    
    return field.find(config.delimiter) != std::string::npos ||
           field.find(config.quote_char) != std::string::npos ||
           field.find('\n') != std::string::npos ||
           field.find('\r') != std::string::npos ||
           (!field.empty() && (field.front() == ' ' || field.back() == ' '));
}

WriterError BatchWriter::createBackupFile(const std::string& filename, const std::string& backup_suffix) {
    // EN: Create backup of existing file
    // FR: Crée une sauvegarde du fichier existant
    if (!std::filesystem::exists(filename)) {
        return WriterError::SUCCESS; // EN: No file to backup / FR: Pas de fichier à sauvegarder
    }
    
    std::string backup_filename = filename + backup_suffix;
    
    try {
        std::filesystem::copy_file(filename, backup_filename, 
                                  std::filesystem::copy_options::overwrite_existing);
        return WriterError::SUCCESS;
    } catch (const std::filesystem::filesystem_error& e) {
        return WriterError::FILE_WRITE_ERROR;
    }
}

bool BatchWriter::isValidFilename(const std::string& filename) {
    // EN: Validate filename
    // FR: Valide le nom de fichier
    if (filename.empty()) {
        return false;
    }
    
    // EN: Check for invalid characters (basic check)
    // FR: Vérifie les caractères invalides (vérification basique)
    static const std::string invalid_chars = "<>:\"|?*";
    for (char c : invalid_chars) {
        if (filename.find(c) != std::string::npos) {
            return false;
        }
    }
    
    return true;
}

size_t BatchWriter::estimateCompressedSize(size_t original_size, CompressionType compression) {
    // EN: Estimate compressed size
    // FR: Estime la taille compressée
    switch (compression) {
        case CompressionType::GZIP:
        case CompressionType::ZLIB:
            return static_cast<size_t>(original_size * 0.3); // EN: Rough estimate / FR: Estimation approximative
        case CompressionType::NONE:
        case CompressionType::AUTO:
        default:
            return original_size;
    }
}

WriterError BatchWriter::setCompressionLevel(int level) {
    // EN: Set compression level
    // FR: Définit le niveau de compression
    if (level < 1 || level > 9) {
        reportError(WriterError::INVALID_CONFIGURATION, "Invalid compression level: " + std::to_string(level));
        return WriterError::INVALID_CONFIGURATION;
    }
    
    config_.compression_level = level;
    return WriterError::SUCCESS;
}

WriterError BatchWriter::enableCompression(CompressionType type, int level) {
    // EN: Enable compression with specified type and level
    // FR: Active la compression avec le type et niveau spécifiés
    if (level < 1 || level > 9) {
        reportError(WriterError::INVALID_CONFIGURATION, "Invalid compression level: " + std::to_string(level));
        return WriterError::INVALID_CONFIGURATION;
    }
    
    config_.compression = type;
    config_.compression_level = level;
    return WriterError::SUCCESS;
}

WriterError BatchWriter::disableCompression() {
    // EN: Disable compression
    // FR: Désactive la compression
    config_.compression = CompressionType::NONE;
    return WriterError::SUCCESS;
}

bool BatchWriter::isCompressionEnabled() const {
    // EN: Check if compression is enabled
    // FR: Vérifie si la compression est activée
    return config_.compression != CompressionType::NONE;
}

void BatchWriter::startBackgroundFlush() {
    // EN: Start background flush thread
    // FR: Démarre le thread de flush en arrière-plan
    if (background_flush_running_) {
        return;
    }
    
    should_stop_background_ = false;
    background_flush_running_ = true;
    background_thread_ = std::make_unique<std::thread>(&BatchWriter::backgroundFlushWorker, this);
}

void BatchWriter::stopBackgroundFlush() {
    // EN: Stop background flush thread
    // FR: Arrête le thread de flush en arrière-plan
    if (!background_flush_running_) {
        return;
    }
    
    should_stop_background_ = true;
    flush_condition_.notify_all();
    
    if (background_thread_ && background_thread_->joinable()) {
        background_thread_->join();
    }
    
    background_flush_running_ = false;
    background_thread_.reset();
}

WriterError BatchWriter::recover() {
    // EN: Attempt to recover from error state
    // FR: Tente de récupérer de l'état d'erreur
    // EN: Basic recovery - clear buffers and try to reopen
    // FR: Récupération basique - vide les buffers et essaie de rouvrir
    clearBuffer();
    
    if (!current_filename_.empty() && !file_open_) {
        return openFileInternal(current_filename_);
    }
    
    return WriterError::SUCCESS;
}

void BatchWriter::setRetryPolicy(size_t max_attempts, std::chrono::milliseconds delay) {
    // EN: Set retry policy for error recovery
    // FR: Définit la politique de retry pour récupération d'erreur
    config_.max_retry_attempts = max_attempts;
    config_.retry_delay = delay;
}

// EN: Private implementation methods
// FR: Méthodes d'implémentation privées

WriterError BatchWriter::openFileInternal(const std::string& filename) {
    // EN: Internal file opening logic
    // FR: Logique interne d'ouverture de fichier
    auto& logger = BBP::Logger::getInstance();
    
    // EN: Create backup if requested
    // FR: Crée une sauvegarde si demandé
    if (config_.create_backup) {
        createBackupFile(filename, ".bak");
    }
    
    // EN: Detect compression from filename if AUTO
    // FR: Détecte la compression depuis le nom de fichier si AUTO
    CompressionType compression_type = config_.compression;
    if (compression_type == CompressionType::AUTO) {
        compression_type = config_.detectCompressionFromFilename(filename);
    }
    
    // EN: Open file for writing
    // FR: Ouvre le fichier pour écriture
    try {
        file_stream_ = std::make_unique<std::ofstream>(filename, std::ios::binary | std::ios::trunc);
        if (!file_stream_->is_open()) {
            reportError(WriterError::FILE_OPEN_ERROR, "Cannot open file for writing: " + filename);
            return WriterError::FILE_OPEN_ERROR;
        }
        
        output_stream_ = std::unique_ptr<std::ostream, std::function<void(std::ostream*)>>(file_stream_.get(), [](std::ostream*) {
            // EN: Custom deleter - don't delete file_stream_ twice
            // FR: Déléteur personnalisé - ne supprime pas file_stream_ deux fois
        });
        owns_stream_ = true;
        file_open_ = true;
        header_written_ = false;
        current_filename_ = filename;
        
        stats_.startTiming();
        
        logger.info("batch_writer", "Opened file for writing: " + filename);
        
        // EN: Write BOM if requested
        // FR: Écrit BOM si demandé
        if (config_.write_bom && config_.encoding == "UTF-8") {
            const char utf8_bom[] = {static_cast<char>(0xEF), static_cast<char>(0xBB), static_cast<char>(0xBF)};
            output_stream_->write(utf8_bom, 3);
        }
        
        return WriterError::SUCCESS;
        
    } catch (const std::exception& e) {
        reportError(WriterError::FILE_OPEN_ERROR, "Exception opening file: " + std::string(e.what()));
        return WriterError::FILE_OPEN_ERROR;
    }
}

WriterError BatchWriter::flushInternal() {
    // EN: Internal flush implementation
    // FR: Implémentation interne du flush
    if (!file_open_ || row_buffer_.empty()) {
        return WriterError::SUCCESS;
    }
    
    auto flush_start = std::chrono::high_resolution_clock::now();
    
    // EN: Format all buffered rows
    // FR: Formate toutes les lignes mises en buffer
    std::ostringstream output;
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        
        for (const auto& row : row_buffer_) {
            output << formatRow(row) << config_.line_ending;
        }
        
        row_buffer_.clear();
        current_buffer_size_ = 0;
    }
    
    std::string data = output.str();
    if (data.empty()) {
        return WriterError::SUCCESS;
    }
    
    // EN: Write to output stream (with compression if enabled)
    // FR: Écrit vers le stream de sortie (avec compression si activée)
    WriterError write_result = compressAndWrite(data);
    
    if (write_result == WriterError::SUCCESS) {
        output_stream_->flush();
        stats_.incrementFlushCount();
        stats_.addBytesWritten(data.size());
        
        auto flush_end = std::chrono::high_resolution_clock::now();
        stats_.recordFlushTime(std::chrono::duration<double>(flush_end - flush_start));
        
        last_flush_time_ = std::chrono::steady_clock::now();
        
        // EN: Update buffer utilization
        // FR: Met à jour l'utilisation du buffer
        stats_.recordBufferUtilization(getBufferUtilization());
        
        // EN: Call flush callback if registered
        // FR: Appelle le callback de flush si enregistré
        if (flush_callback_) {
            flush_callback_(stats_.getRowsWritten(), stats_.getBytesWritten());
        }
        
        reportProgress();
    }
    
    return write_result;
}

WriterError BatchWriter::writeRowInternal(const CsvRow& row) {
    // EN: Internal row writing logic
    // FR: Logique interne d'écriture de ligne
    if (row.isEmpty()) {
        stats_.incrementRowsSkipped();
        return WriterError::SUCCESS;
    }
    
    // EN: Check field size limits
    // FR: Vérifie les limites de taille de champ
    for (const auto& field : row) {
        if (field.size() > config_.max_field_size) {
            stats_.incrementRowsWithErrors();
            reportError(WriterError::BUFFER_OVERFLOW, 
                       "Field size exceeds maximum: " + std::to_string(field.size()));
            if (!config_.continue_on_error) {
                return WriterError::BUFFER_OVERFLOW;
            }
        }
    }
    
    // EN: Add row to buffer
    // FR: Ajoute la ligne au buffer
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        
        if (row_buffer_.size() >= config_.max_rows_in_buffer) {
            // EN: Buffer full, flush immediately
            // FR: Buffer plein, flush immédiatement
            WriterError flush_result = flushInternal();
            if (flush_result != WriterError::SUCCESS) {
                return flush_result;
            }
        }
        
        row_buffer_.push_back(row);
        
        // EN: Estimate size increase
        // FR: Estime l'augmentation de taille
        std::string formatted = formatRow(row);
        current_buffer_size_ += formatted.size() + config_.line_ending.size();
    }
    
    stats_.incrementRowsWritten();
    return WriterError::SUCCESS;
}

WriterError BatchWriter::compressAndWrite(const std::string& data) {
    // EN: Compress data if needed and write to stream
    // FR: Compresse les données si nécessaire et écrit vers le stream
    if (config_.compression == CompressionType::NONE) {
        // EN: No compression, write directly
        // FR: Pas de compression, écrit directement
        output_stream_->write(data.c_str(), data.size());
        if (output_stream_->fail()) {
            reportError(WriterError::FILE_WRITE_ERROR, "Error writing to file");
            return WriterError::FILE_WRITE_ERROR;
        }
        return WriterError::SUCCESS;
    }
    
    // EN: Compress and write
    // FR: Compresse et écrit
    auto compress_start = std::chrono::high_resolution_clock::now();
    
    std::string compressed = compressString(data, config_.compression, config_.compression_level);
    if (compressed.empty()) {
        reportError(WriterError::COMPRESSION_ERROR, "Failed to compress data");
        return WriterError::COMPRESSION_ERROR;
    }
    
    auto compress_end = std::chrono::high_resolution_clock::now();
    stats_.recordCompressionTime(std::chrono::duration<double>(compress_end - compress_start));
    stats_.addBytesCompressed(data.size(), compressed.size());
    
    output_stream_->write(compressed.c_str(), compressed.size());
    if (output_stream_->fail()) {
        reportError(WriterError::FILE_WRITE_ERROR, "Error writing compressed data to file");
        return WriterError::FILE_WRITE_ERROR;
    }
    
    return WriterError::SUCCESS;
}

std::string BatchWriter::formatRow(const CsvRow& row) const {
    // EN: Format row according to configuration
    // FR: Formate la ligne selon la configuration
    return row.toString(config_);
}

bool BatchWriter::shouldFlush() const {
    // EN: Check if flush is needed based on triggers
    // FR: Vérifie si un flush est nécessaire basé sur les déclencheurs
    switch (config_.flush_trigger) {
        case FlushTrigger::MANUAL:
            return false;
            
        case FlushTrigger::ROW_COUNT:
            return row_buffer_.size() >= config_.flush_row_threshold;
            
        case FlushTrigger::BUFFER_SIZE:
            return current_buffer_size_ >= config_.flush_size_threshold;
            
        case FlushTrigger::TIME_INTERVAL: {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_flush_time_);
            return elapsed >= config_.flush_interval;
        }
        
        case FlushTrigger::MIXED:
            return row_buffer_.size() >= config_.flush_row_threshold ||
                   current_buffer_size_ >= config_.flush_size_threshold ||
                   (std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - last_flush_time_) >= config_.flush_interval);
    }
    
    return false;
}

void BatchWriter::backgroundFlushWorker() {
    // EN: Background thread worker for automatic flush
    // FR: Worker de thread en arrière-plan pour flush automatique
    auto& logger = BBP::Logger::getInstance();
    logger.info("batch_writer", "Background flush thread started");
    
    while (!should_stop_background_) {
        std::unique_lock<std::mutex> lock(writer_mutex_);
        
        // EN: Wait for flush interval or stop signal
        // FR: Attend l'intervalle de flush ou signal d'arrêt
        if (flush_condition_.wait_for(lock, config_.flush_interval, [this] { return should_stop_background_.load(); })) {
            break; // EN: Stop requested / FR: Arrêt demandé
        }
        
        // EN: Check if flush is needed
        // FR: Vérifie si un flush est nécessaire
        if (file_open_ && shouldFlush()) {
            flushInternal();
        }
    }
    
    logger.info("batch_writer", "Background flush thread stopped");
}

void BatchWriter::reportError(WriterError error, const std::string& message) {
    // EN: Report error to callback and statistics
    // FR: Rapporte l'erreur au callback et aux statistiques
    auto& logger = BBP::Logger::getInstance();
    logger.error("batch_writer", "Writer error: " + message);
    
    stats_.recordError(error);
    
    if (error_callback_) {
        error_callback_(error, message);
    }
}

void BatchWriter::reportProgress() {
    // EN: Report progress to callback
    // FR: Rapporte la progression au callback
    if (progress_callback_) {
        double progress = 0.0; // EN: Progress calculation would depend on context / FR: Le calcul de progression dépendrait du contexte
        progress_callback_(stats_.getRowsWritten(), progress);
    }
}

void BatchWriter::updateBufferSize() {
    // EN: Update current buffer size estimate
    // FR: Met à jour l'estimation de taille actuelle du buffer
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    
    size_t estimated_size = 0;
    for (const auto& row : row_buffer_) {
        estimated_size += formatRow(row).size() + config_.line_ending.size();
    }
    
    current_buffer_size_ = estimated_size;
}

WriterError BatchWriter::retryOperation(std::function<WriterError()> operation) {
    // EN: Retry operation with exponential backoff
    // FR: Retry l'opération avec backoff exponentiel
    WriterError last_error = WriterError::SUCCESS;
    
    for (size_t attempt = 0; attempt < config_.max_retry_attempts; ++attempt) {
        last_error = operation();
        if (last_error == WriterError::SUCCESS) {
            return WriterError::SUCCESS;
        }
        
        // EN: Wait before retry (exponential backoff)
        // FR: Attendre avant retry (backoff exponentiel)
        auto delay = config_.retry_delay * (1 << attempt); // 2^attempt multiplier
        std::this_thread::sleep_for(delay);
    }
    
    return last_error;
}

std::string BatchWriter::compressString(const std::string& input, CompressionType type, int level) {
    // EN: Compress string using specified algorithm
    // FR: Compresse la chaîne en utilisant l'algorithme spécifié
    if (type == CompressionType::NONE || input.empty()) {
        return input;
    }
    
    // EN: Basic ZLIB compression implementation
    // FR: Implémentation basique de compression ZLIB
    if (type == CompressionType::ZLIB || type == CompressionType::GZIP) {
        uLongf compressed_size = compressBound(input.size());
        std::string compressed(compressed_size, '\0');
        
        int result = compress2(reinterpret_cast<Bytef*>(compressed.data()),
                              &compressed_size,
                              reinterpret_cast<const Bytef*>(input.c_str()),
                              input.size(),
                              level);
        
        if (result != Z_OK) {
            return std::string(); // EN: Compression failed / FR: Compression échouée
        }
        
        compressed.resize(compressed_size);
        return compressed;
    }
    
    return input; // EN: Fallback to no compression / FR: Retombe sur pas de compression
}

bool BatchWriter::hasEnoughDiskSpace(size_t required_bytes) const {
    // EN: Check if there's enough disk space
    // FR: Vérifie s'il y a assez d'espace disque
    if (current_filename_.empty()) {
        return true; // EN: Can't check, assume OK / FR: Ne peut pas vérifier, assume OK
    }
    
    try {
        auto space = std::filesystem::space(std::filesystem::path(current_filename_).parent_path());
        return space.available >= required_bytes * 2; // EN: 2x safety margin / FR: Marge de sécurité 2x
    } catch (...) {
        return true; // EN: Error checking, assume OK / FR: Erreur de vérification, assume OK
    }
}

std::string BatchWriter::generateTempFilename() const {
    // EN: Generate temporary filename
    // FR: Génère un nom de fichier temporaire
    return current_filename_ + config_.temp_file_suffix;
}

WriterError BatchWriter::atomicFileWrite(const std::string& filename, const std::string& data) {
    // EN: Write to temporary file then rename (atomic operation)
    // FR: Écrit vers fichier temporaire puis renomme (opération atomique)
    std::string temp_filename = filename + config_.temp_file_suffix;
    
    try {
        // EN: Write to temporary file
        // FR: Écrit vers le fichier temporaire
        std::ofstream temp_file(temp_filename, std::ios::binary);
        if (!temp_file.is_open()) {
            return WriterError::FILE_WRITE_ERROR;
        }
        
        temp_file.write(data.c_str(), data.size());
        temp_file.close();
        
        if (temp_file.fail()) {
            std::filesystem::remove(temp_filename);
            return WriterError::FILE_WRITE_ERROR;
        }
        
        // EN: Atomic rename
        // FR: Renommage atomique
        std::filesystem::rename(temp_filename, filename);
        return WriterError::SUCCESS;
        
    } catch (const std::exception&) {
        std::filesystem::remove(temp_filename);
        return WriterError::FILE_WRITE_ERROR;
    }
}

} // namespace CSV
} // namespace BBP