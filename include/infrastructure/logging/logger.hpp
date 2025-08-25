#pragma once

#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>

namespace BBP {

// EN: Log levels enumeration.
// FR: Énumération des niveaux de log.
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3
};

// EN: Thread-safe singleton logger with NDJSON output and correlation IDs.
// FR: Logger singleton thread-safe avec sortie NDJSON et IDs de corrélation.
class Logger {
public:
    // EN: Structure representing a log entry.
    // FR: Structure représentant une entrée de log.
    struct LogEntry {
        std::chrono::system_clock::time_point timestamp;
        LogLevel level;
        std::string message;
        std::string correlation_id;
        std::string module;
        std::string thread_id;
        std::unordered_map<std::string, std::string> metadata;
    };

    // EN: Get the singleton instance.
    // FR: Obtient l'instance singleton.
    static Logger& getInstance();
    
    // EN: Set the minimum log level.
    // FR: Définit le niveau de log minimum.
    void setLogLevel(LogLevel level);
    
    // EN: Set output file for logging (disables console output).
    // FR: Définit le fichier de sortie (désactive la sortie console).
    void setOutputFile(const std::string& filename);
    
    // EN: Set correlation ID for all subsequent log entries.
    // FR: Définit l'ID de corrélation pour toutes les entrées suivantes.
    void setCorrelationId(const std::string& correlation_id);
    
    // EN: Add global metadata that will be included in all log entries.
    // FR: Ajoute des métadonnées globales incluses dans toutes les entrées.
    void addGlobalMetadata(const std::string& key, const std::string& value);
    
    // EN: Log a message with specified level.
    // FR: Enregistre un message avec le niveau spécifié.
    void log(LogLevel level, const std::string& module, const std::string& message);
    void log(LogLevel level, const std::string& module, const std::string& message, 
             const std::unordered_map<std::string, std::string>& metadata);
    
    // EN: Log convenience methods for different levels.
    // FR: Méthodes de log pratiques pour différents niveaux.
    void debug(const std::string& module, const std::string& message);
    void info(const std::string& module, const std::string& message);
    void warn(const std::string& module, const std::string& message);
    void error(const std::string& module, const std::string& message);
    
    // EN: Log convenience methods with metadata.
    // FR: Méthodes de log pratiques avec métadonnées.
    void debug(const std::string& module, const std::string& message, 
               const std::unordered_map<std::string, std::string>& metadata);
    void info(const std::string& module, const std::string& message, 
              const std::unordered_map<std::string, std::string>& metadata);
    void warn(const std::string& module, const std::string& message, 
              const std::unordered_map<std::string, std::string>& metadata);
    void error(const std::string& module, const std::string& message, 
               const std::unordered_map<std::string, std::string>& metadata);
    
    // EN: Flush all pending log entries to output.
    // FR: Vide toutes les entrées en attente vers la sortie.
    void flush();
    
    // EN: Generate a new correlation ID (UUID-like format).
    // FR: Génère un nouvel ID de corrélation (format UUID).
    std::string generateCorrelationId();

private:
    Logger() = default;
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // EN: Write a log entry to the configured output.
    // FR: Écrit une entrée de log vers la sortie configurée.
    void writeEntry(const LogEntry& entry);
    
    // EN: Format log entry as NDJSON string.
    // FR: Formate l'entrée de log en chaîne NDJSON.
    std::string formatAsNDJSON(const LogEntry& entry);
    
    // EN: Convert log level enum to string.
    // FR: Convertit l'enum niveau de log en chaîne.
    std::string levelToString(LogLevel level);
    
    // EN: Convert timestamp to ISO8601 format.
    // FR: Convertit le timestamp au format ISO8601.
    std::string timestampToISO8601(const std::chrono::system_clock::time_point& tp);
    
    // EN: Get current thread ID as string.
    // FR: Obtient l'ID du thread courant en chaîne.
    std::string getThreadId();
    
    LogLevel current_level_ = LogLevel::INFO;
    std::string correlation_id_;
    std::unordered_map<std::string, std::string> global_metadata_;
    std::unique_ptr<std::ofstream> log_file_;
    std::mutex mutex_;
    bool console_output_ = true;
};

#define LOG_DEBUG(module, message) BBP::Logger::getInstance().debug(module, message)
#define LOG_INFO(module, message) BBP::Logger::getInstance().info(module, message)
#define LOG_WARN(module, message) BBP::Logger::getInstance().warn(module, message)
#define LOG_ERROR(module, message) BBP::Logger::getInstance().error(module, message)

#define LOG_DEBUG_META(module, message, metadata) BBP::Logger::getInstance().debug(module, message, metadata)
#define LOG_INFO_META(module, message, metadata) BBP::Logger::getInstance().info(module, message, metadata)
#define LOG_WARN_META(module, message, metadata) BBP::Logger::getInstance().warn(module, message, metadata)
#define LOG_ERROR_META(module, message, metadata) BBP::Logger::getInstance().error(module, message, metadata)

} // namespace BBP