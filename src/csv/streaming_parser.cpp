// EN: High-performance streaming CSV parser implementation - processes large files with constant memory usage
// FR: Implémentation du parser CSV streaming haute performance - traite de gros fichiers avec usage mémoire constant

#include "csv/streaming_parser.hpp"
#include "infrastructure/logging/logger.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <codecvt>
#include <locale>
#include <regex>
#include <cstring>

namespace BBP {
namespace CSV {

// EN: ParsedRow implementation
// FR: Implémentation de ParsedRow

ParsedRow::ParsedRow(size_t row_number, std::vector<std::string> fields, std::vector<std::string> headers)
    : row_number_(row_number), fields_(std::move(fields)), headers_(std::move(headers)) {
    initializeHeaderMap();
}

const std::string& ParsedRow::operator[](size_t index) const {
    return getField(index);
}

const std::string& ParsedRow::getField(size_t index) const {
    static const std::string empty_string;
    if (index >= fields_.size()) {
        return empty_string;
    }
    return fields_[index];
}

std::optional<std::string> ParsedRow::getFieldSafe(size_t index) const {
    if (index >= fields_.size()) {
        return std::nullopt;
    }
    return fields_[index];
}

const std::string& ParsedRow::operator[](const std::string& header) const {
    return getField(header);
}

const std::string& ParsedRow::getField(const std::string& header) const {
    static const std::string empty_string;
    auto it = header_map_.find(header);
    if (it == header_map_.end()) {
        return empty_string;
    }
    return getField(it->second);
}

std::optional<std::string> ParsedRow::getFieldSafe(const std::string& header) const {
    auto it = header_map_.find(header);
    if (it == header_map_.end()) {
        return std::nullopt;
    }
    return getFieldSafe(it->second);
}

std::string ParsedRow::toString() const {
    std::ostringstream oss;
    oss << "Row " << row_number_ << ": [";
    for (size_t i = 0; i < fields_.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << fields_[i] << "\"";
    }
    oss << "]";
    return oss.str();
}

void ParsedRow::initializeHeaderMap() {
    // EN: Create header name to index mapping for efficient field access
    // FR: Crée le mappage nom d'en-tête vers index pour accès efficace aux champs
    header_map_.clear();
    for (size_t i = 0; i < headers_.size(); ++i) {
        header_map_[headers_[i]] = i;
    }
}

// EN: ParserStatistics implementation
// FR: Implémentation de ParserStatistics

ParserStatistics::ParserStatistics() {
    reset();
}

void ParserStatistics::reset() {
    // EN: Reset all statistics to initial state
    // FR: Remet toutes les statistiques à l'état initial
    rows_parsed_ = 0;
    rows_skipped_ = 0;
    rows_with_errors_ = 0;
    bytes_read_ = 0;
    parsing_duration_ = std::chrono::duration<double>(0);
    total_field_count_ = 0;
    min_field_count_ = SIZE_MAX;
    max_field_count_ = 0;
}

void ParserStatistics::startTiming() {
    // EN: Start measuring parsing time
    // FR: Commence à mesurer le temps de parsing
    start_time_ = std::chrono::high_resolution_clock::now();
}

void ParserStatistics::stopTiming() {
    // EN: Stop measuring parsing time and update duration
    // FR: Arrête de mesurer le temps de parsing et met à jour la durée
    auto end_time = std::chrono::high_resolution_clock::now();
    parsing_duration_ = end_time - start_time_;
}

void ParserStatistics::recordFieldCount(size_t count) {
    // EN: Record field count statistics for this row
    // FR: Enregistre les statistiques de nombre de champs pour cette ligne
    total_field_count_ += count;
    
    // EN: Update min/max field counts (thread-safe atomic operations)
    // FR: Met à jour les nombres de champs min/max (opérations atomiques thread-safe)
    size_t current_min = min_field_count_.load();
    while (count < current_min && !min_field_count_.compare_exchange_weak(current_min, count)) {
        // EN: Loop until successful CAS or value is no longer minimal
        // FR: Boucle jusqu'à CAS réussi ou valeur n'est plus minimale
    }
    
    size_t current_max = max_field_count_.load();
    while (count > current_max && !max_field_count_.compare_exchange_weak(current_max, count)) {
        // EN: Loop until successful CAS or value is no longer maximal
        // FR: Boucle jusqu'à CAS réussi ou valeur n'est plus maximale
    }
}

double ParserStatistics::getRowsPerSecond() const {
    // EN: Calculate parsing throughput in rows per second
    // FR: Calcule le débit de parsing en lignes par seconde
    double duration_seconds = parsing_duration_.count();
    if (duration_seconds <= 0.0) return 0.0;
    return static_cast<double>(rows_parsed_) / duration_seconds;
}

double ParserStatistics::getBytesPerSecond() const {
    // EN: Calculate parsing throughput in bytes per second
    // FR: Calcule le débit de parsing en octets par seconde
    double duration_seconds = parsing_duration_.count();
    if (duration_seconds <= 0.0) return 0.0;
    return static_cast<double>(bytes_read_) / duration_seconds;
}

double ParserStatistics::getAverageFieldCount() const {
    // EN: Calculate average number of fields per row
    // FR: Calcule le nombre moyen de champs par ligne
    if (rows_parsed_ == 0) return 0.0;
    return static_cast<double>(total_field_count_.load()) / static_cast<double>(rows_parsed_.load());
}

std::string ParserStatistics::generateReport() const {
    // EN: Generate comprehensive statistics report
    // FR: Génère un rapport de statistiques complet
    std::ostringstream report;
    report << std::fixed << std::setprecision(2);
    
    report << "=== Streaming Parser Statistics ===\n";
    report << "Parsing Duration: " << parsing_duration_.count() << " seconds\n";
    report << "Rows Processed:\n";
    report << "  - Successfully parsed: " << rows_parsed_ << "\n";
    report << "  - Skipped: " << rows_skipped_ << "\n";
    report << "  - With errors: " << rows_with_errors_ << "\n";
    report << "  - Total: " << (rows_parsed_ + rows_skipped_ + rows_with_errors_) << "\n";
    
    report << "Performance:\n";
    report << "  - Bytes read: " << bytes_read_ << " bytes\n";
    report << "  - Rows/second: " << getRowsPerSecond() << "\n";
    report << "  - Bytes/second: " << getBytesPerSecond() << " (" 
           << (getBytesPerSecond() / 1024.0 / 1024.0) << " MB/s)\n";
    
    report << "Field Statistics:\n";
    report << "  - Average fields per row: " << getAverageFieldCount() << "\n";
    if (min_field_count_ != SIZE_MAX) {
        report << "  - Min fields per row: " << min_field_count_ << "\n";
    }
    report << "  - Max fields per row: " << max_field_count_ << "\n";
    
    return report.str();
}

// EN: StreamingParser implementation
// FR: Implémentation de StreamingParser

StreamingParser::StreamingParser() : config_() {
    // EN: Initialize with default configuration
    // FR: Initialise avec configuration par défaut
    initializeBuffer();
}

StreamingParser::StreamingParser(const ParserConfig& config) : config_(config) {
    // EN: Initialize with custom configuration
    // FR: Initialise avec configuration personnalisée
    initializeBuffer();
}

StreamingParser::~StreamingParser() {
    // EN: Ensure parsing is stopped and thread is joined
    // FR: S'assure que le parsing est arrêté et le thread rejoint
    if (is_parsing_) {
        stopParsing();
        if (parsing_thread_ && parsing_thread_->joinable()) {
            parsing_thread_->join();
        }
    }
}

StreamingParser::StreamingParser(StreamingParser&& other) noexcept
    : config_(std::move(other.config_))
    , row_callback_(std::move(other.row_callback_))
    , progress_callback_(std::move(other.progress_callback_))
    , error_callback_(std::move(other.error_callback_))
    , parsing_thread_(std::move(other.parsing_thread_))
    , buffer_(std::move(other.buffer_))
    , buffer_pos_(other.buffer_pos_)
    , buffer_size_(other.buffer_size_)
    , current_row_(std::move(other.current_row_))
    , headers_(std::move(other.headers_))
    , current_row_number_(other.current_row_number_)
    , total_file_size_(other.total_file_size_)
    , detected_encoding_(other.detected_encoding_) {
    
    // EN: Transfer atomic state
    // FR: Transfère l'état atomique
    is_parsing_.store(other.is_parsing_.load());
    is_paused_.store(other.is_paused_.load());
    should_stop_.store(other.should_stop_.load());
    
    // EN: Reset other object
    // FR: Remet l'autre objet à zéro
    other.buffer_pos_ = 0;
    other.buffer_size_ = 0;
    other.current_row_number_ = 0;
    other.total_file_size_ = 0;
    other.is_parsing_.store(false);
    other.is_paused_.store(false);
    other.should_stop_.store(false);
}

StreamingParser& StreamingParser::operator=(StreamingParser&& other) noexcept {
    if (this != &other) {
        // EN: Stop current parsing if active
        // FR: Arrête le parsing actuel s'il est actif
        if (is_parsing_) {
            stopParsing();
            if (parsing_thread_ && parsing_thread_->joinable()) {
                parsing_thread_->join();
            }
        }
        
        // EN: Move all members
        // FR: Déplace tous les membres
        config_ = std::move(other.config_);
        row_callback_ = std::move(other.row_callback_);
        progress_callback_ = std::move(other.progress_callback_);
        error_callback_ = std::move(other.error_callback_);
        // EN: Reset stats instead of moving (atomic members cannot be moved)
        // FR: Reset stats au lieu de les déplacer (membres atomiques ne peuvent pas être déplacés)
        stats_.reset();
        parsing_thread_ = std::move(other.parsing_thread_);
        buffer_ = std::move(other.buffer_);
        buffer_pos_ = other.buffer_pos_;
        buffer_size_ = other.buffer_size_;
        current_row_ = std::move(other.current_row_);
        headers_ = std::move(other.headers_);
        current_row_number_ = other.current_row_number_;
        total_file_size_ = other.total_file_size_;
        detected_encoding_ = other.detected_encoding_;
        
        // EN: Transfer atomic state
        // FR: Transfère l'état atomique
        is_parsing_.store(other.is_parsing_.load());
        is_paused_.store(other.is_paused_.load());
        should_stop_.store(other.should_stop_.load());
        
        // EN: Reset other object
        // FR: Remet l'autre objet à zéro
        other.buffer_pos_ = 0;
        other.buffer_size_ = 0;
        other.current_row_number_ = 0;
        other.total_file_size_ = 0;
        other.is_parsing_.store(false);
        other.is_paused_.store(false);
        other.should_stop_.store(false);
    }
    return *this;
}

void StreamingParser::setConfig(const ParserConfig& config) {
    // EN: Update configuration (only when not parsing)
    // FR: Met à jour la configuration (seulement quand pas en parsing)
    if (is_parsing_) {
        reportError(ParserError::THREAD_ERROR, "Cannot change configuration while parsing is active", 0);
        return;
    }
    
    config_ = config;
    initializeBuffer();
}

ParserError StreamingParser::parseFile(const std::string& file_path) {
    // EN: Synchronous file parsing
    // FR: Parsing synchrone de fichier
    if (is_parsing_) {
        return ParserError::THREAD_ERROR;
    }
    
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        reportError(ParserError::FILE_NOT_FOUND, "Cannot open file: " + file_path, 0);
        return ParserError::FILE_NOT_FOUND;
    }
    
    // EN: Get file size for progress tracking
    // FR: Obtient la taille du fichier pour suivi de progression
    total_file_size_ = getFileSize(file_path);
    
    auto& logger = Logger::getInstance();
    logger.info("streaming_parser", "Starting to parse file: " + file_path + 
                " (size: " + std::to_string(total_file_size_) + " bytes)");
    
    return parseStream(file);
}

ParserError StreamingParser::parseStream(std::istream& stream) {
    // EN: Synchronous stream parsing
    // FR: Parsing synchrone de stream
    if (is_parsing_) {
        return ParserError::THREAD_ERROR;
    }
    
    setParsingState(true, false);
    
    auto result = parseInternal(stream);
    
    setParsingState(false, false);
    return result;
}

ParserError StreamingParser::parseString(const std::string& csv_content) {
    // EN: Parse CSV content from string
    // FR: Parse le contenu CSV depuis une chaîne
    std::istringstream stream(csv_content);
    total_file_size_ = csv_content.size();
    return parseStream(stream);
}

ParserError StreamingParser::parseFileAsync(const std::string& file_path) {
    // EN: Asynchronous file parsing
    // FR: Parsing asynchrone de fichier
    if (is_parsing_) {
        return ParserError::THREAD_ERROR;
    }
    
    total_file_size_ = getFileSize(file_path);
    
    // EN: Start parsing in background thread
    // FR: Démarre le parsing dans un thread en arrière-plan
    parsing_thread_ = std::make_unique<std::thread>([this, file_path]() {
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            reportError(ParserError::FILE_NOT_FOUND, "Cannot open file: " + file_path, 0);
            setParsingState(false, false);
            return;
        }
        
        asyncParsingWorker(file);
    });
    
    return ParserError::SUCCESS;
}

ParserError StreamingParser::parseStreamAsync(std::istream& stream) {
    // EN: Asynchronous stream parsing
    // FR: Parsing asynchrone de stream
    if (is_parsing_) {
        return ParserError::THREAD_ERROR;
    }
    
    // EN: Start parsing in background thread
    // FR: Démarre le parsing dans un thread en arrière-plan
    parsing_thread_ = std::make_unique<std::thread>([this, &stream]() {
        asyncParsingWorker(stream);
    });
    
    return ParserError::SUCCESS;
}

void StreamingParser::pauseParsing() {
    // EN: Pause background parsing
    // FR: Met en pause le parsing en arrière-plan
    std::lock_guard<std::mutex> lock(parsing_mutex_);
    is_paused_ = true;
}

void StreamingParser::resumeParsing() {
    // EN: Resume paused parsing
    // FR: Reprend le parsing en pause
    {
        std::lock_guard<std::mutex> lock(parsing_mutex_);
        is_paused_ = false;
    }
    parsing_cv_.notify_all();
}

void StreamingParser::stopParsing() {
    // EN: Stop background parsing
    // FR: Arrête le parsing en arrière-plan
    {
        std::lock_guard<std::mutex> lock(parsing_mutex_);
        should_stop_ = true;
        is_paused_ = false;
    }
    parsing_cv_.notify_all();
}

ParserError StreamingParser::waitForCompletion() {
    // EN: Wait for async parsing to complete
    // FR: Attend que le parsing asynchrone se termine
    if (parsing_thread_ && parsing_thread_->joinable()) {
        parsing_thread_->join();
        parsing_thread_.reset();
    }
    
    return ParserError::SUCCESS;
}

// EN: Static utility methods
// FR: Méthodes utilitaires statiques

EncodingType StreamingParser::detectEncoding(const std::string& file_path) {
    // EN: Detect file encoding by examining BOM and content
    // FR: Détecte l'encodage du fichier en examinant BOM et contenu
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        return EncodingType::UTF8; // EN: Default fallback / FR: Fallback par défaut
    }
    
    return detectEncoding(file);
}

EncodingType StreamingParser::detectEncoding(std::istream& stream) {
    // EN: Detect stream encoding by examining BOM and content
    // FR: Détecte l'encodage du stream en examinant BOM et contenu
    auto original_pos = stream.tellg();
    stream.seekg(0);
    
    // EN: Read first few bytes to check for BOM
    // FR: Lit les premiers octets pour vérifier le BOM
    unsigned char bom[4] = {0};
    stream.read(reinterpret_cast<char*>(bom), 4);
    stream.seekg(original_pos);
    
    // EN: Check for UTF-16 LE BOM (FF FE)
    // FR: Vérifie le BOM UTF-16 LE (FF FE)
    if (bom[0] == 0xFF && bom[1] == 0xFE) {
        return EncodingType::UTF16_LE;
    }
    
    // EN: Check for UTF-16 BE BOM (FE FF)
    // FR: Vérifie le BOM UTF-16 BE (FE FF)
    if (bom[0] == 0xFE && bom[1] == 0xFF) {
        return EncodingType::UTF16_BE;
    }
    
    // EN: Check for UTF-8 BOM (EF BB BF)
    // FR: Vérifie le BOM UTF-8 (EF BB BF)
    if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {
        return EncodingType::UTF8;
    }
    
    // EN: No BOM detected, assume UTF-8 (most common)
    // FR: Aucun BOM détecté, assume UTF-8 (plus courant)
    return EncodingType::UTF8;
}

std::vector<std::string> StreamingParser::parseRow(const std::string& row, const ParserConfig& config) {
    // EN: Static method to parse a single CSV row
    // FR: Méthode statique pour parser une seule ligne CSV
    std::vector<std::string> fields;
    if (row.empty()) {
        return fields;
    }
    
    size_t pos = 0;
    bool in_quotes = false;
    std::string current_field;
    
    while (pos < row.length()) {
        char c = row[pos];
        
        if (c == config.quote_char) {
            if (in_quotes && pos + 1 < row.length() && row[pos + 1] == config.quote_char) {
                // EN: Escaped quote within quoted field
                // FR: Quote échappée dans un champ quoté
                current_field += config.quote_char;
                pos += 2;
            } else {
                // EN: Toggle quote state
                // FR: Bascule l'état de quote
                in_quotes = !in_quotes;
                pos++;
            }
        } else if (c == config.delimiter && !in_quotes) {
            // EN: Field delimiter outside quotes
            // FR: Délimiteur de champ en dehors des quotes
            if (config.trim_whitespace) {
                // EN: Trim leading and trailing whitespace
                // FR: Supprime les espaces en début et fin
                size_t start = current_field.find_first_not_of(" \t\r\n");
                if (start != std::string::npos) {
                    size_t end = current_field.find_last_not_of(" \t\r\n");
                    current_field = current_field.substr(start, end - start + 1);
                } else {
                    current_field.clear();
                }
            }
            fields.push_back(current_field);
            current_field.clear();
            pos++;
        } else {
            // EN: Regular character
            // FR: Caractère normal
            current_field += c;
            pos++;
        }
    }
    
    // EN: Add the last field
    // FR: Ajoute le dernier champ
    if (config.trim_whitespace) {
        size_t start = current_field.find_first_not_of(" \t\r\n");
        if (start != std::string::npos) {
            size_t end = current_field.find_last_not_of(" \t\r\n");
            current_field = current_field.substr(start, end - start + 1);
        } else {
            current_field.clear();
        }
    }
    fields.push_back(current_field);
    
    return fields;
}

std::string StreamingParser::escapeField(const std::string& field, const ParserConfig& config) {
    // EN: Escape field for CSV output
    // FR: Échappe le champ pour sortie CSV
    bool needs_quoting = field.find(config.delimiter) != std::string::npos ||
                        field.find(config.quote_char) != std::string::npos ||
                        field.find('\n') != std::string::npos ||
                        field.find('\r') != std::string::npos;
    
    if (!needs_quoting) {
        return field;
    }
    
    std::string escaped = field;
    // EN: Escape quotes by doubling them
    // FR: Échappe les quotes en les doublant
    size_t pos = 0;
    while ((pos = escaped.find(config.quote_char, pos)) != std::string::npos) {
        escaped.insert(pos, 1, config.quote_char);
        pos += 2;
    }
    
    return config.quote_char + escaped + config.quote_char;
}

bool StreamingParser::isQuotedField(const std::string& field, char quote_char) {
    // EN: Check if field is properly quoted
    // FR: Vérifie si le champ est correctement quoté
    return field.length() >= 2 && 
           field.front() == quote_char && 
           field.back() == quote_char;
}

size_t StreamingParser::getFileSize(const std::string& file_path) {
    // EN: Get file size in bytes
    // FR: Obtient la taille du fichier en octets
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return 0;
    }
    return static_cast<size_t>(file.tellg());
}

// EN: Private implementation methods
// FR: Méthodes d'implémentation privées

ParserError StreamingParser::parseInternal(std::istream& stream) {
    // EN: Core internal parsing logic
    // FR: Logique de parsing interne principale
    auto& logger = Logger::getInstance();
    logger.info("streaming_parser", "Starting internal parsing");
    
    stats_.startTiming();
    
    // EN: Handle encoding detection and conversion
    // FR: Gère la détection et conversion d'encodage
    ParserError encoding_result = handleEncoding(stream);
    if (encoding_result != ParserError::SUCCESS) {
        stats_.stopTiming();
        return encoding_result;
    }
    
    // EN: Initialize parsing state
    // FR: Initialise l'état de parsing
    current_row_number_ = 0;
    headers_.clear();
    
    // EN: Process file in chunks
    // FR: Traite le fichier par chunks
    ParserError result = ParserError::SUCCESS;
    try {
        while (!checkShouldStop() && stream.good()) {
            // EN: Handle pause state
            // FR: Gère l'état de pause
            {
                std::unique_lock<std::mutex> lock(parsing_mutex_);
                parsing_cv_.wait(lock, [this] { return !is_paused_ || should_stop_; });
                if (should_stop_) break;
            }
            
            result = processBuffer(stream);
            if (result != ParserError::SUCCESS && config_.strict_mode) {
                break;
            }
        }
    } catch (const std::exception& e) {
        logger.error("streaming_parser", "Exception during parsing: " + std::string(e.what()));
        result = ParserError::CALLBACK_ERROR;
    }
    
    stats_.stopTiming();
    logger.info("streaming_parser", "Parsing completed. " + 
                std::to_string(stats_.getRowsParsed()) + " rows processed");
    
    return result;
}

ParserError StreamingParser::processBuffer(std::istream& stream) {
    // EN: Process data from stream buffer
    // FR: Traite les données du buffer de stream
    ParserError fill_result = fillBuffer(stream);
    if (fill_result != ParserError::SUCCESS && fill_result != ParserError::SUCCESS) {
        return fill_result;
    }
    
    // EN: Extract and process complete rows from buffer
    // FR: Extrait et traite les lignes complètes du buffer
    while (buffer_pos_ < buffer_size_) {
        std::string row = extractNextRow();
        if (row.empty()) {
            break; // EN: No complete row available / FR: Aucune ligne complète disponible
        }
        
        current_row_number_++;
        
        // EN: Skip empty rows if configured
        // FR: Ignore les lignes vides si configuré
        if (config_.skip_empty_rows && row.empty()) {
            stats_.incrementRowsSkipped();
            continue;
        }
        
        ParserError row_result = processRow(row, current_row_number_);
        if (row_result != ParserError::SUCCESS && config_.strict_mode) {
            return row_result;
        }
        
        // EN: Report progress periodically
        // FR: Rapporte la progression périodiquement
        if (current_row_number_ % 1000 == 0) {
            reportProgress(stats_.getBytesRead());
        }
    }
    
    return ParserError::SUCCESS;
}

ParserError StreamingParser::processRow(const std::string& row_data, size_t row_number) {
    // EN: Process a single CSV row
    // FR: Traite une seule ligne CSV
    if (row_data.empty() && config_.skip_empty_rows) {
        stats_.incrementRowsSkipped();
        return ParserError::SUCCESS;
    }
    
    try {
        std::vector<std::string> fields = parseRowFields(row_data);
        
        // EN: Handle header row
        // FR: Gère la ligne d'en-tête
        if (config_.has_header && row_number == 1) {
            headers_ = fields;
            stats_.incrementRowsSkipped(); // EN: Header is not counted as data row / FR: En-tête n'est pas comptée comme ligne de données
            return ParserError::SUCCESS;
        }
        
        // EN: Create parsed row object
        // FR: Crée l'objet ligne analysée
        ParsedRow parsed_row(row_number, std::move(fields), headers_);
        stats_.incrementRowsParsed();
        stats_.recordFieldCount(parsed_row.getFieldCount());
        
        // EN: Call user callback if provided
        // FR: Appelle le callback utilisateur si fourni
        if (row_callback_) {
            bool continue_parsing = row_callback_(parsed_row, ParserError::SUCCESS);
            if (!continue_parsing) {
                return ParserError::SUCCESS; // EN: User requested stop / FR: Utilisateur demande l'arrêt
            }
        }
        
        return ParserError::SUCCESS;
        
    } catch (const std::exception& e) {
        stats_.incrementRowsWithErrors();
        reportError(ParserError::MALFORMED_ROW, "Error parsing row " + std::to_string(row_number) + ": " + e.what(), row_number);
        
        if (config_.strict_mode) {
            return ParserError::MALFORMED_ROW;
        }
        
        return ParserError::SUCCESS; // EN: Continue in non-strict mode / FR: Continue en mode non-strict
    }
}

std::vector<std::string> StreamingParser::parseRowFields(const std::string& row_data) const {
    // EN: Parse fields from a row string
    // FR: Analyse les champs d'une chaîne de ligne
    return parseRow(row_data, config_);
}

std::string StreamingParser::processField(const std::string& field_data) const {
    // EN: Process individual field data (unquoting, unescaping)
    // FR: Traite les données de champ individuelles (suppression quotes, échappement)
    std::string processed = field_data;
    
    // EN: Remove quotes if field is quoted
    // FR: Supprime les quotes si le champ est quoté
    if (isQuotedField(processed, config_.quote_char)) {
        processed = processed.substr(1, processed.length() - 2);
        
        // EN: Unescape doubled quotes
        // FR: Supprime l'échappement des quotes doublées
        size_t pos = 0;
        std::string double_quote(2, config_.quote_char);
        while ((pos = processed.find(double_quote, pos)) != std::string::npos) {
            processed.replace(pos, 2, std::string(1, config_.quote_char));
            pos += 1;
        }
    }
    
    return processed;
}

bool StreamingParser::isRowComplete(const std::string& row_data) const {
    // EN: Check if row is complete (all quotes are properly closed)
    // FR: Vérifie si la ligne est complète (toutes les quotes sont correctement fermées)
    bool in_quotes = false;
    
    for (char c : row_data) {
        if (c == config_.quote_char) {
            in_quotes = !in_quotes;
        }
    }
    
    return !in_quotes; // EN: Complete if not inside quotes / FR: Complète si pas à l'intérieur de quotes
}

void StreamingParser::initializeBuffer() {
    // EN: Initialize internal read buffer
    // FR: Initialise le buffer de lecture interne
    buffer_ = std::make_unique<char[]>(config_.buffer_size);
    buffer_pos_ = 0;
    buffer_size_ = 0;
}

ParserError StreamingParser::fillBuffer(std::istream& stream) {
    // EN: Fill internal buffer with data from stream
    // FR: Remplit le buffer interne avec des données du stream
    if (!stream.good()) {
        return ParserError::FILE_READ_ERROR;
    }
    
    // EN: Move remaining data to beginning of buffer
    // FR: Déplace les données restantes au début du buffer
    if (buffer_pos_ > 0 && buffer_pos_ < buffer_size_) {
        size_t remaining = buffer_size_ - buffer_pos_;
        std::memmove(buffer_.get(), buffer_.get() + buffer_pos_, remaining);
        buffer_size_ = remaining;
        buffer_pos_ = 0;
    } else if (buffer_pos_ >= buffer_size_) {
        buffer_pos_ = 0;
        buffer_size_ = 0;
    }
    
    // EN: Read new data into buffer
    // FR: Lit de nouvelles données dans le buffer
    size_t space_available = config_.buffer_size - buffer_size_;
    if (space_available > 0) {
        stream.read(buffer_.get() + buffer_size_, space_available);
        size_t bytes_read = static_cast<size_t>(stream.gcount());
        buffer_size_ += bytes_read;
        stats_.addBytesRead(bytes_read);
    }
    
    return ParserError::SUCCESS;
}

std::string StreamingParser::extractNextRow() {
    // EN: Extract next complete row from buffer
    // FR: Extrait la prochaine ligne complète du buffer
    if (buffer_pos_ >= buffer_size_) {
        return std::string(); // EN: No data available / FR: Aucune donnée disponible
    }
    
    // EN: Find end of line
    // FR: Trouve la fin de ligne
    size_t line_start = buffer_pos_;
    size_t line_end = line_start;
    bool in_quotes = false;
    
    while (line_end < buffer_size_) {
        char c = buffer_[line_end];
        
        if (c == config_.quote_char) {
            in_quotes = !in_quotes;
        } else if ((c == '\n' || c == '\r') && !in_quotes) {
            break;
        }
        
        line_end++;
    }
    
    // EN: Check if we have a complete line
    // FR: Vérifie si on a une ligne complète
    if (line_end >= buffer_size_ && in_quotes) {
        return std::string(); // EN: Incomplete quoted field / FR: Champ quoté incomplet
    }
    
    // EN: Extract line
    // FR: Extrait la ligne
    std::string line(buffer_.get() + line_start, line_end - line_start);
    
    // EN: Update buffer position, skip line endings
    // FR: Met à jour la position du buffer, ignore les fins de ligne
    buffer_pos_ = line_end;
    while (buffer_pos_ < buffer_size_ && (buffer_[buffer_pos_] == '\n' || buffer_[buffer_pos_] == '\r')) {
        buffer_pos_++;
    }
    
    return line;
}

ParserError StreamingParser::handleEncoding(std::istream& stream) {
    // EN: Handle file encoding detection and conversion
    // FR: Gère la détection et conversion d'encodage de fichier
    if (config_.encoding == EncodingType::AUTO_DETECT) {
        detected_encoding_ = detectEncoding(stream);
    } else {
        detected_encoding_ = config_.encoding;
    }
    
    auto& logger = Logger::getInstance();
    logger.info("streaming_parser", "Detected encoding: " + std::to_string(static_cast<int>(detected_encoding_)));
    
    // EN: For now, we support UTF-8 primarily. UTF-16 support can be added later.
    // FR: Pour l'instant, on supporte principalement UTF-8. Le support UTF-16 peut être ajouté plus tard.
    if (detected_encoding_ != EncodingType::UTF8 && detected_encoding_ != EncodingType::ASCII) {
        logger.warn("streaming_parser", "Non-UTF8 encoding detected but not fully supported yet");
    }
    
    return ParserError::SUCCESS;
}

std::string StreamingParser::convertEncoding(const std::string& input, EncodingType from, EncodingType to) {
    // EN: Convert string between encodings (placeholder implementation)
    // FR: Convertit une chaîne entre encodages (implémentation placeholder)
    if (from == to) {
        return input;
    }
    
    // EN: Basic implementation - full encoding support would require additional libraries
    // FR: Implémentation basique - le support complet d'encodage nécessiterait des bibliothèques additionnelles
    return input;
}

void StreamingParser::reportError(ParserError error, const std::string& message, size_t row_number) {
    // EN: Report error to user callback and logger
    // FR: Rapporte l'erreur au callback utilisateur et logger
    auto& logger = Logger::getInstance();
    logger.error("streaming_parser", "Parser error: " + message + " (row " + std::to_string(row_number) + ")");
    
    if (error_callback_) {
        error_callback_(error, message, row_number);
    }
}

void StreamingParser::reportProgress(size_t bytes_processed) {
    // EN: Report parsing progress to user callback
    // FR: Rapporte la progression de parsing au callback utilisateur
    if (progress_callback_ && total_file_size_ > 0) {
        double progress_percent = static_cast<double>(bytes_processed) / static_cast<double>(total_file_size_) * 100.0;
        progress_callback_(stats_.getRowsParsed(), bytes_processed, progress_percent);
    }
}

void StreamingParser::asyncParsingWorker(std::istream& stream) {
    // EN: Background thread worker for async parsing
    // FR: Worker de thread en arrière-plan pour parsing asynchrone
    setParsingState(true, false);
    
    parseInternal(stream);
    
    setParsingState(false, false);
    
    // EN: Final progress report
    // FR: Rapport de progression final
    if (progress_callback_) {
        progress_callback_(stats_.getRowsParsed(), stats_.getBytesRead(), 100.0);
    }
}

void StreamingParser::setParsingState(bool parsing, bool paused) {
    // EN: Thread-safe parsing state management
    // FR: Gestion d'état de parsing thread-safe
    std::lock_guard<std::mutex> lock(parsing_mutex_);
    is_parsing_ = parsing;
    is_paused_ = paused;
    should_stop_ = false;
}

bool StreamingParser::checkShouldStop() const {
    // EN: Check if parsing should be stopped
    // FR: Vérifie si le parsing devrait être arrêté
    return should_stop_.load();
}

} // namespace CSV
} // namespace BBP