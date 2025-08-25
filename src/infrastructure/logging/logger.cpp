// EN: Implementation of the Logger class. Provides thread-safe NDJSON logging with correlation IDs.
// FR: Implémentation de la classe Logger. Fournit un logging NDJSON thread-safe avec IDs de corrélation.

#include "../../../include/infrastructure/logging/logger.hpp"
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

namespace BBP {

// EN: Get the singleton logger instance.
// FR: Obtient l'instance singleton du logger.
Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

// EN: Destructor ensures all logs are flushed.
// FR: Le destructeur assure que tous les logs sont vidés.
Logger::~Logger() {
    flush();
}

// EN: Set the minimum log level for filtering.
// FR: Définit le niveau de log minimum pour le filtrage.
void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_level_ = level;
}

// EN: Set output file and disable console output.
// FR: Définit le fichier de sortie et désactive la sortie console.
void Logger::setOutputFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_file_) {
        log_file_->close();
    }
    log_file_ = std::make_unique<std::ofstream>(filename, std::ios::app);
    if (!log_file_->is_open()) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
        log_file_.reset();
    } else {
        console_output_ = false;
    }
}

// EN: Set correlation ID for all subsequent log entries.
// FR: Définit l'ID de corrélation pour toutes les entrées de log suivantes.
void Logger::setCorrelationId(const std::string& correlation_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    correlation_id_ = correlation_id;
}

// EN: Add global metadata included in all log entries.
// FR: Ajoute des métadonnées globales incluses dans toutes les entrées de log.
void Logger::addGlobalMetadata(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    global_metadata_[key] = value;
}

// EN: Log message with specified level (no metadata).
// FR: Enregistre un message avec le niveau spécifié (sans métadonnées).
void Logger::log(LogLevel level, const std::string& module, const std::string& message) {
    log(level, module, message, {});
}

// EN: Log message with specified level and metadata.
// FR: Enregistre un message avec le niveau spécifié et des métadonnées.
void Logger::log(LogLevel level, const std::string& module, const std::string& message,
                 const std::unordered_map<std::string, std::string>& metadata) {
    if (level < current_level_) {
        return;
    }
    
    LogEntry entry;
    entry.timestamp = std::chrono::system_clock::now();
    entry.level = level;
    entry.message = message;
    entry.correlation_id = correlation_id_;
    entry.module = module;
    entry.thread_id = getThreadId();
    entry.metadata = metadata;
    
    // EN: Merge global metadata, preserving entry-specific metadata.
    // FR: Fusionne les métadonnées globales, préservant les métadonnées spécifiques à l'entrée.
    for (const auto& [key, value] : global_metadata_) {
        if (entry.metadata.find(key) == entry.metadata.end()) {
            entry.metadata[key] = value;
        }
    }
    
    writeEntry(entry);
}

void Logger::debug(const std::string& module, const std::string& message) {
    log(LogLevel::DEBUG, module, message);
}

void Logger::info(const std::string& module, const std::string& message) {
    log(LogLevel::INFO, module, message);
}

void Logger::warn(const std::string& module, const std::string& message) {
    log(LogLevel::WARN, module, message);
}

void Logger::error(const std::string& module, const std::string& message) {
    log(LogLevel::ERROR, module, message);
}

void Logger::debug(const std::string& module, const std::string& message,
                   const std::unordered_map<std::string, std::string>& metadata) {
    log(LogLevel::DEBUG, module, message, metadata);
}

void Logger::info(const std::string& module, const std::string& message,
                  const std::unordered_map<std::string, std::string>& metadata) {
    log(LogLevel::INFO, module, message, metadata);
}

void Logger::warn(const std::string& module, const std::string& message,
                  const std::unordered_map<std::string, std::string>& metadata) {
    log(LogLevel::WARN, module, message, metadata);
}

void Logger::error(const std::string& module, const std::string& message,
                   const std::unordered_map<std::string, std::string>& metadata) {
    log(LogLevel::ERROR, module, message, metadata);
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (log_file_) {
        log_file_->flush();
    }
    if (console_output_) {
        std::cout.flush();
    }
}

// EN: Generate a UUID-like correlation ID for request tracing.
// FR: Génère un ID de corrélation similaire à UUID pour le traçage des requêtes.
std::string Logger::generateCorrelationId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            ss << "-";
        }
        ss << dis(gen);
    }
    return ss.str();
}

// EN: Write log entry to configured output (file or console).
// FR: Écrit l'entrée de log vers la sortie configurée (fichier ou console).
void Logger::writeEntry(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string ndjson = formatAsNDJSON(entry);
    
    if (log_file_ && log_file_->is_open()) {
        *log_file_ << ndjson << std::endl;
    }
    
    if (console_output_) {
        std::cout << ndjson << std::endl;
    }
}

std::string Logger::formatAsNDJSON(const LogEntry& entry) {
    std::ostringstream json;
    json << "{";
    json << "\"timestamp\":\"" << timestampToISO8601(entry.timestamp) << "\",";
    json << "\"level\":\"" << levelToString(entry.level) << "\",";
    json << "\"message\":\"" << entry.message << "\",";
    json << "\"module\":\"" << entry.module << "\",";
    json << "\"thread_id\":\"" << entry.thread_id << "\"";
    
    if (!entry.correlation_id.empty()) {
        json << ",\"correlation_id\":\"" << entry.correlation_id << "\"";
    }
    
    for (const auto& [key, value] : entry.metadata) {
        json << ",\"" << key << "\":\"" << value << "\"";
    }
    
    json << "}";
    return json.str();
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

std::string Logger::timestampToISO8601(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;
    
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z";
    return ss.str();
}

std::string Logger::getThreadId() {
    std::ostringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

} // namespace BBP