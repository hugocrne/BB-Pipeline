// EN: High-performance streaming CSV parser for processing large files without loading them entirely into memory
// FR: Parser CSV streaming haute performance pour traiter de gros fichiers sans les charger entièrement en mémoire

#pragma once

#include <string>
#include <vector>
#include <functional>
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
#include <iomanip>

namespace BBP {
namespace CSV {

// EN: Forward declarations
// FR: Déclarations anticipées
class StreamingParser;
class ParsedRow;
class ParserStatistics;

// EN: Encoding types supported by the parser
// FR: Types d'encodage supportés par le parser
enum class EncodingType {
    UTF8,           // EN: UTF-8 encoding / FR: Encodage UTF-8
    UTF16_LE,       // EN: UTF-16 Little Endian / FR: UTF-16 Little Endian
    UTF16_BE,       // EN: UTF-16 Big Endian / FR: UTF-16 Big Endian
    ASCII,          // EN: ASCII encoding / FR: Encodage ASCII
    AUTO_DETECT     // EN: Automatic encoding detection / FR: Détection automatique d'encodage
};

// EN: Parser error types
// FR: Types d'erreur du parser
enum class ParserError {
    SUCCESS,                    // EN: No error / FR: Aucune erreur
    FILE_NOT_FOUND,            // EN: Input file not found / FR: Fichier d'entrée introuvable
    FILE_READ_ERROR,           // EN: Error reading file / FR: Erreur de lecture du fichier
    ENCODING_ERROR,            // EN: Encoding detection/conversion error / FR: Erreur de détection/conversion d'encodage
    MALFORMED_ROW,             // EN: Row parsing error / FR: Erreur de parsing de ligne
    BUFFER_OVERFLOW,           // EN: Internal buffer overflow / FR: Débordement de buffer interne
    MEMORY_ALLOCATION_ERROR,   // EN: Memory allocation failure / FR: Échec d'allocation mémoire
    CALLBACK_ERROR,            // EN: User callback function error / FR: Erreur de fonction callback utilisateur
    THREAD_ERROR               // EN: Threading/concurrency error / FR: Erreur de threading/concurrence
};

// EN: Parser configuration options
// FR: Options de configuration du parser
struct ParserConfig {
    char delimiter{','};                    // EN: Field delimiter character / FR: Caractère délimiteur de champ
    char quote_char{'"'};                   // EN: Quote character for escaped fields / FR: Caractère de quote pour champs échappés
    char escape_char{'"'};                  // EN: Escape character / FR: Caractère d'échappement
    bool has_header{true};                  // EN: First row is header / FR: Première ligne est l'en-tête
    bool strict_mode{false};                // EN: Strict parsing (fail on malformed rows) / FR: Parsing strict (échec sur lignes malformées)
    bool trim_whitespace{true};             // EN: Trim leading/trailing whitespace / FR: Supprimer espaces en début/fin
    bool skip_empty_rows{true};             // EN: Skip empty rows / FR: Ignorer les lignes vides
    size_t buffer_size{8192};               // EN: Internal buffer size in bytes / FR: Taille du buffer interne en octets
    size_t max_field_size{1048576};         // EN: Maximum field size (1MB default) / FR: Taille maximum de champ (1MB par défaut)
    size_t max_row_size{10485760};          // EN: Maximum row size (10MB default) / FR: Taille maximum de ligne (10MB par défaut)
    EncodingType encoding{EncodingType::AUTO_DETECT}; // EN: Input file encoding / FR: Encodage du fichier d'entrée
    bool enable_parallel_processing{false}; // EN: Enable multi-threaded parsing / FR: Activer le parsing multi-thread
    size_t thread_count{0};                 // EN: Number of threads (0 = auto-detect) / FR: Nombre de threads (0 = auto-détection)
    
    // EN: Default constructor with sensible defaults
    // FR: Constructeur par défaut avec valeurs par défaut sensées
    ParserConfig() = default;
};

// EN: Represents a parsed CSV row with field access methods
// FR: Représente une ligne CSV analysée avec méthodes d'accès aux champs
class ParsedRow {
public:
    // EN: Constructor
    // FR: Constructeur
    ParsedRow(size_t row_number, std::vector<std::string> fields, std::vector<std::string> headers = {});
    
    // EN: Copy constructor and assignment
    // FR: Constructeur de copie et assignation
    ParsedRow(const ParsedRow& other) = default;
    ParsedRow& operator=(const ParsedRow& other) = default;
    ParsedRow(ParsedRow&& other) noexcept = default;
    ParsedRow& operator=(ParsedRow&& other) noexcept = default;
    
    // EN: Destructor
    // FR: Destructeur
    ~ParsedRow() = default;
    
    // EN: Field access by index
    // FR: Accès aux champs par index
    const std::string& operator[](size_t index) const;
    const std::string& getField(size_t index) const;
    std::optional<std::string> getFieldSafe(size_t index) const;
    
    // EN: Field access by header name (if headers are available)
    // FR: Accès aux champs par nom d'en-tête (si en-têtes disponibles)
    const std::string& operator[](const std::string& header) const;
    const std::string& getField(const std::string& header) const;
    std::optional<std::string> getFieldSafe(const std::string& header) const;
    
    // EN: Row information
    // FR: Informations sur la ligne
    size_t getRowNumber() const { return row_number_; }
    size_t getFieldCount() const { return fields_.size(); }
    const std::vector<std::string>& getFields() const { return fields_; }
    const std::vector<std::string>& getHeaders() const { return headers_; }
    bool hasHeaders() const { return !headers_.empty(); }
    
    // EN: Field type conversion helpers
    // FR: Assistants de conversion de type de champ
    template<typename T>
    std::optional<T> getFieldAs(size_t index) const;
    
    template<typename T>
    std::optional<T> getFieldAs(const std::string& header) const;
    
    // EN: Row validation
    // FR: Validation de ligne
    bool isValid() const { return !fields_.empty(); }
    bool isEmpty() const { return fields_.empty() || (fields_.size() == 1 && fields_[0].empty()); }
    
    // EN: String representation
    // FR: Représentation en chaîne
    std::string toString() const;
    
private:
    size_t row_number_;                     // EN: 1-based row number / FR: Numéro de ligne base 1
    std::vector<std::string> fields_;       // EN: Field values / FR: Valeurs des champs
    std::vector<std::string> headers_;      // EN: Header names / FR: Noms des en-têtes
    std::unordered_map<std::string, size_t> header_map_; // EN: Header name to index mapping / FR: Mappage nom d'en-tête vers index
    
    // EN: Initialize header mapping
    // FR: Initialise le mappage des en-têtes
    void initializeHeaderMap();
};

// EN: Parser statistics and performance metrics
// FR: Statistiques du parser et métriques de performance
class ParserStatistics {
public:
    // EN: Default constructor
    // FR: Constructeur par défaut
    ParserStatistics();
    
    // EN: Reset all statistics
    // FR: Remet à zéro toutes les statistiques
    void reset();
    
    // EN: Start timing
    // FR: Démarre le chronométrage
    void startTiming();
    
    // EN: Stop timing
    // FR: Arrête le chronométrage
    void stopTiming();
    
    // EN: Update statistics
    // FR: Met à jour les statistiques
    void incrementRowsParsed() { rows_parsed_++; }
    void incrementRowsSkipped() { rows_skipped_++; }
    void incrementRowsWithErrors() { rows_with_errors_++; }
    void addBytesRead(size_t bytes) { bytes_read_ += bytes; }
    void recordFieldCount(size_t count);
    
    // EN: Getters
    // FR: Accesseurs
    size_t getRowsParsed() const { return rows_parsed_; }
    size_t getRowsSkipped() const { return rows_skipped_; }
    size_t getRowsWithErrors() const { return rows_with_errors_; }
    size_t getBytesRead() const { return bytes_read_; }
    std::chrono::duration<double> getParsingDuration() const { return parsing_duration_; }
    double getRowsPerSecond() const;
    double getBytesPerSecond() const;
    double getAverageFieldCount() const;
    size_t getMinFieldCount() const { return min_field_count_; }
    size_t getMaxFieldCount() const { return max_field_count_; }
    
    // EN: Generate report
    // FR: Génère un rapport
    std::string generateReport() const;
    
private:
    std::atomic<size_t> rows_parsed_{0};        // EN: Number of successfully parsed rows / FR: Nombre de lignes analysées avec succès
    std::atomic<size_t> rows_skipped_{0};       // EN: Number of skipped rows / FR: Nombre de lignes ignorées
    std::atomic<size_t> rows_with_errors_{0};   // EN: Number of rows with parsing errors / FR: Nombre de lignes avec erreurs de parsing
    std::atomic<size_t> bytes_read_{0};         // EN: Total bytes read from file / FR: Total d'octets lus du fichier
    std::chrono::high_resolution_clock::time_point start_time_; // EN: Parsing start time / FR: Heure de début du parsing
    std::chrono::duration<double> parsing_duration_{0};         // EN: Total parsing duration / FR: Durée totale du parsing
    std::atomic<size_t> total_field_count_{0};  // EN: Total field count for average calculation / FR: Nombre total de champs pour calcul moyenne
    std::atomic<size_t> min_field_count_{SIZE_MAX}; // EN: Minimum field count per row / FR: Nombre minimum de champs par ligne
    std::atomic<size_t> max_field_count_{0};    // EN: Maximum field count per row / FR: Nombre maximum de champs par ligne
    mutable std::mutex stats_mutex_;            // EN: Mutex for thread-safe statistics / FR: Mutex pour statistiques thread-safe
};

// EN: Row callback function type
// FR: Type de fonction callback de ligne
using RowCallback = std::function<bool(const ParsedRow& row, ParserError error)>;

// EN: Progress callback function type (called periodically with current progress)
// FR: Type de fonction callback de progression (appelée périodiquement avec progression actuelle)
using ProgressCallback = std::function<void(size_t rows_processed, size_t bytes_read, double progress_percent)>;

// EN: Error callback function type
// FR: Type de fonction callback d'erreur
using ErrorCallback = std::function<void(ParserError error, const std::string& message, size_t row_number)>;

// EN: High-performance streaming CSV parser
// FR: Parser CSV streaming haute performance
class StreamingParser {
public:
    // EN: Constructor with default configuration
    // FR: Constructeur avec configuration par défaut
    StreamingParser();
    
    // EN: Constructor with custom configuration
    // FR: Constructeur avec configuration personnalisée
    explicit StreamingParser(const ParserConfig& config);
    
    // EN: Destructor
    // FR: Destructeur
    ~StreamingParser();
    
    // EN: Copy constructor and assignment (deleted for performance)
    // FR: Constructeur de copie et assignation (supprimés pour performance)
    StreamingParser(const StreamingParser&) = delete;
    StreamingParser& operator=(const StreamingParser&) = delete;
    
    // EN: Move constructor and assignment
    // FR: Constructeur de déplacement et assignation
    StreamingParser(StreamingParser&& other) noexcept;
    StreamingParser& operator=(StreamingParser&& other) noexcept;
    
    // EN: Configuration management
    // FR: Gestion de la configuration
    void setConfig(const ParserConfig& config);
    const ParserConfig& getConfig() const { return config_; }
    
    // EN: Callback registration
    // FR: Enregistrement des callbacks
    void setRowCallback(RowCallback callback) { row_callback_ = std::move(callback); }
    void setProgressCallback(ProgressCallback callback) { progress_callback_ = std::move(callback); }
    void setErrorCallback(ErrorCallback callback) { error_callback_ = std::move(callback); }
    
    // EN: Main parsing methods
    // FR: Méthodes principales de parsing
    ParserError parseFile(const std::string& file_path);
    ParserError parseStream(std::istream& stream);
    ParserError parseString(const std::string& csv_content);
    
    // EN: Async parsing methods (returns immediately, parsing happens in background)
    // FR: Méthodes de parsing asynchrone (retourne immédiatement, parsing en arrière-plan)
    ParserError parseFileAsync(const std::string& file_path);
    ParserError parseStreamAsync(std::istream& stream);
    
    // EN: Control methods for async parsing
    // FR: Méthodes de contrôle pour parsing asynchrone
    void pauseParsing();
    void resumeParsing();
    void stopParsing();
    bool isParsing() const { return is_parsing_; }
    bool isPaused() const { return is_paused_; }
    
    // EN: Wait for async parsing to complete
    // FR: Attend que le parsing asynchrone se termine
    ParserError waitForCompletion();
    
    // EN: Statistics and monitoring
    // FR: Statistiques et surveillance
    const ParserStatistics& getStatistics() const { return stats_; }
    void resetStatistics() { stats_.reset(); }
    
    // EN: Utility methods
    // FR: Méthodes utilitaires
    static EncodingType detectEncoding(const std::string& file_path);
    static EncodingType detectEncoding(std::istream& stream);
    static std::vector<std::string> parseRow(const std::string& row, const ParserConfig& config = ParserConfig{});
    static std::string escapeField(const std::string& field, const ParserConfig& config = ParserConfig{});
    static bool isQuotedField(const std::string& field, char quote_char = '"');
    
    // EN: File size estimation for progress tracking
    // FR: Estimation de taille de fichier pour suivi de progression
    static size_t getFileSize(const std::string& file_path);
    
private:
    ParserConfig config_;                   // EN: Parser configuration / FR: Configuration du parser
    RowCallback row_callback_;              // EN: Row processing callback / FR: Callback de traitement de ligne
    ProgressCallback progress_callback_;    // EN: Progress reporting callback / FR: Callback de rapport de progression
    ErrorCallback error_callback_;          // EN: Error handling callback / FR: Callback de gestion d'erreur
    ParserStatistics stats_;                // EN: Parsing statistics / FR: Statistiques de parsing
    
    // EN: Threading and async support
    // FR: Support threading et asynchrone
    std::unique_ptr<std::thread> parsing_thread_; // EN: Background parsing thread / FR: Thread de parsing en arrière-plan
    std::atomic<bool> is_parsing_{false};   // EN: Parsing in progress flag / FR: Flag de parsing en cours
    std::atomic<bool> is_paused_{false};    // EN: Parsing paused flag / FR: Flag de parsing en pause
    std::atomic<bool> should_stop_{false};  // EN: Stop request flag / FR: Flag de demande d'arrêt
    std::mutex parsing_mutex_;              // EN: Parsing synchronization mutex / FR: Mutex de synchronisation de parsing
    std::condition_variable parsing_cv_;    // EN: Parsing condition variable / FR: Variable de condition de parsing
    
    // EN: Internal buffer management
    // FR: Gestion de buffer interne
    std::unique_ptr<char[]> buffer_;        // EN: Internal read buffer / FR: Buffer de lecture interne
    size_t buffer_pos_{0};                  // EN: Current position in buffer / FR: Position actuelle dans le buffer
    size_t buffer_size_{0};                 // EN: Current buffer size / FR: Taille actuelle du buffer
    std::string current_row_;               // EN: Current row being parsed / FR: Ligne actuelle en cours de parsing
    std::vector<std::string> headers_;      // EN: Cached header names / FR: Noms d'en-têtes mis en cache
    
    // EN: Parsing state
    // FR: État du parsing
    size_t current_row_number_{0};          // EN: Current row number (1-based) / FR: Numéro de ligne actuelle (base 1)
    size_t total_file_size_{0};             // EN: Total file size for progress calculation / FR: Taille totale du fichier pour calcul progression
    EncodingType detected_encoding_{EncodingType::UTF8}; // EN: Detected file encoding / FR: Encodage détecté du fichier
    
    // EN: Core parsing methods
    // FR: Méthodes de parsing principales
    ParserError parseInternal(std::istream& stream);
    ParserError processBuffer(std::istream& stream);
    ParserError processRow(const std::string& row_data, size_t row_number);
    
    // EN: Row parsing helpers
    // FR: Assistants de parsing de ligne
    std::vector<std::string> parseRowFields(const std::string& row_data) const;
    std::string processField(const std::string& field_data) const;
    bool isRowComplete(const std::string& row_data) const;
    
    // EN: Buffer management
    // FR: Gestion de buffer
    void initializeBuffer();
    ParserError fillBuffer(std::istream& stream);
    std::string extractNextRow();
    
    // EN: Encoding handling
    // FR: Gestion d'encodage
    ParserError handleEncoding(std::istream& stream);
    std::string convertEncoding(const std::string& input, EncodingType from, EncodingType to);
    
    // EN: Error handling
    // FR: Gestion d'erreur
    void reportError(ParserError error, const std::string& message, size_t row_number = 0);
    void reportProgress(size_t bytes_processed);
    
    // EN: Async parsing implementation
    // FR: Implémentation du parsing asynchrone
    void asyncParsingWorker(std::istream& stream);
    
    // EN: Thread-safe parsing state management
    // FR: Gestion d'état de parsing thread-safe
    void setParsingState(bool parsing, bool paused = false);
    bool checkShouldStop() const;
};

// EN: Template implementations for type conversion
// FR: Implémentations de template pour conversion de type
template<typename T>
std::optional<T> ParsedRow::getFieldAs(size_t index) const {
    auto field = getFieldSafe(index);
    if (!field.has_value()) {
        return std::nullopt;
    }
    
    try {
        if constexpr (std::is_same_v<T, int>) {
            return std::stoi(field.value());
        } else if constexpr (std::is_same_v<T, long>) {
            return std::stol(field.value());
        } else if constexpr (std::is_same_v<T, long long>) {
            return std::stoll(field.value());
        } else if constexpr (std::is_same_v<T, float>) {
            return std::stof(field.value());
        } else if constexpr (std::is_same_v<T, double>) {
            return std::stod(field.value());
        } else if constexpr (std::is_same_v<T, bool>) {
            const std::string& value = field.value();
            if (value == "true" || value == "1" || value == "yes" || value == "on") {
                return true;
            } else if (value == "false" || value == "0" || value == "no" || value == "off") {
                return false;
            }
            return std::nullopt;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return field.value();
        } else {
            // EN: Unsupported type
            // FR: Type non supporté
            static_assert(sizeof(T) == 0, "Unsupported type for field conversion");
            return std::nullopt;
        }
    } catch (...) {
        return std::nullopt;
    }
}

template<typename T>
std::optional<T> ParsedRow::getFieldAs(const std::string& header) const {
    auto it = header_map_.find(header);
    if (it == header_map_.end()) {
        return std::nullopt;
    }
    return getFieldAs<T>(it->second);
}

} // namespace CSV
} // namespace BBP