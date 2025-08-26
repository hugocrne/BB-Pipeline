// EN: Intelligent CSV merger implementation with deduplication and advanced merge strategies
// FR: Implémentation du moteur de fusion CSV intelligent avec déduplication et stratégies de fusion avancées

#include "csv/merger_engine.hpp"
#include "infrastructure/logging/logger.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <thread>
#include <iomanip>
#include <cctype>
#include <cmath>
#include <unordered_set>

namespace BBP {
namespace CSV {

// EN: MergeConfig implementation
// FR: Implémentation de MergeConfig

MergeConfig::MergeConfig() {
    // EN: Initialize with sensible defaults
    // FR: Initialise avec des valeurs par défaut sensées
    key_columns = {"id", "ID", "key", "KEY"};
    output_filepath = "merged_output.csv";
}

bool MergeConfig::isValid() const {
    // EN: Validate configuration parameters
    // FR: Valide les paramètres de configuration
    if (output_filepath.empty()) return false;
    if (memory_limit < 1024 * 1024) return false; // EN: Minimum 1MB / FR: Minimum 1MB
    if (chunk_size == 0) return false;
    if (fuzzy_threshold < 0.0 || fuzzy_threshold > 1.0) return false;
    if (max_threads == 0) return false;
    
    return true;
}

std::vector<std::string> MergeConfig::getValidationErrors() const {
    // EN: Return detailed validation errors
    // FR: Retourne les erreurs de validation détaillées
    std::vector<std::string> errors;
    
    if (output_filepath.empty()) {
        errors.push_back("Output filepath cannot be empty");
    }
    if (memory_limit < 1024 * 1024) {
        errors.push_back("Memory limit must be at least 1MB");
    }
    if (chunk_size == 0) {
        errors.push_back("Chunk size must be greater than 0");
    }
    if (fuzzy_threshold < 0.0 || fuzzy_threshold > 1.0) {
        errors.push_back("Fuzzy threshold must be between 0.0 and 1.0");
    }
    if (max_threads == 0) {
        errors.push_back("Maximum threads must be greater than 0");
    }
    
    return errors;
}

// EN: MergeStatistics implementation
// FR: Implémentation de MergeStatistics

MergeStatistics::MergeStatistics() {
    reset();
}

void MergeStatistics::reset() {
    // EN: Reset all statistics to initial state
    // FR: Remet toutes les statistiques à l'état initial
    total_rows_processed_ = 0;
    total_rows_output_ = 0;
    duplicates_removed_ = 0;
    conflicts_resolved_ = 0;
    files_processed_ = 0;
    total_bytes_processed_ = 0;
    total_duration_ = std::chrono::duration<double>(0);
    
    std::lock_guard<std::mutex> timing_lock(timing_mutex_);
    phase_timings_.clear();
    
    std::lock_guard<std::mutex> error_lock(error_mutex_);
    error_counts_.clear();
    error_messages_.clear();
}

void MergeStatistics::startTiming() {
    // EN: Start measuring merge time
    // FR: Commence à mesurer le temps de fusion
    start_time_ = std::chrono::high_resolution_clock::now();
}

void MergeStatistics::stopTiming() {
    // EN: Stop measuring merge time and update duration
    // FR: Arrête de mesurer le temps de fusion et met à jour la durée
    auto end_time = std::chrono::high_resolution_clock::now();
    total_duration_ = end_time - start_time_;
}

void MergeStatistics::recordPhaseTime(const std::string& phase, std::chrono::duration<double> duration) {
    // EN: Record time for specific merge phase
    // FR: Enregistre le temps pour une phase spécifique de fusion
    std::lock_guard<std::mutex> lock(timing_mutex_);
    phase_timings_[phase] += duration;
}

double MergeStatistics::getRowsPerSecond() const {
    // EN: Calculate merge throughput in rows per second
    // FR: Calcule le débit de fusion en lignes par seconde
    double duration_seconds = total_duration_.count();
    if (duration_seconds <= 0.0) return 0.0;
    return static_cast<double>(total_rows_processed_.load()) / duration_seconds;
}

double MergeStatistics::getBytesPerSecond() const {
    // EN: Calculate merge throughput in bytes per second
    // FR: Calcule le débit de fusion en octets par seconde
    double duration_seconds = total_duration_.count();
    if (duration_seconds <= 0.0) return 0.0;
    return static_cast<double>(total_bytes_processed_.load()) / duration_seconds;
}

double MergeStatistics::getDeduplicationRatio() const {
    // EN: Calculate deduplication efficiency ratio
    // FR: Calcule le ratio d'efficacité de déduplication
    size_t processed = total_rows_processed_.load();
    size_t removed = duplicates_removed_.load();
    if (processed == 0) return 0.0;
    return static_cast<double>(removed) / static_cast<double>(processed);
}

std::unordered_map<std::string, std::chrono::duration<double>> MergeStatistics::getPhaseTimings() const {
    // EN: Get copy of phase timings
    // FR: Obtient une copie des chronométrages de phases
    std::lock_guard<std::mutex> lock(timing_mutex_);
    return phase_timings_;
}

void MergeStatistics::recordError(MergeError error, const std::string& message) {
    // EN: Record error occurrence with message
    // FR: Enregistre l'occurrence d'une erreur avec message
    std::lock_guard<std::mutex> lock(error_mutex_);
    error_counts_[error]++;
    error_messages_.push_back(message);
}

std::unordered_map<MergeError, size_t> MergeStatistics::getErrorCounts() const {
    // EN: Get copy of error counts
    // FR: Obtient une copie des comptes d'erreurs
    std::lock_guard<std::mutex> lock(error_mutex_);
    return error_counts_;
}

std::vector<std::string> MergeStatistics::getErrorMessages() const {
    // EN: Get copy of error messages
    // FR: Obtient une copie des messages d'erreur
    std::lock_guard<std::mutex> lock(error_mutex_);
    return error_messages_;
}

std::string MergeStatistics::generateReport() const {
    // EN: Generate comprehensive statistics report
    // FR: Génère un rapport de statistiques complet
    std::ostringstream report;
    report << std::fixed << std::setprecision(2);
    
    report << "=== Merge Engine Statistics ===\n";
    report << "Total Duration: " << total_duration_.count() << " seconds\n";
    
    report << "Rows:\n";
    report << "  - Processed: " << total_rows_processed_.load() << "\n";
    report << "  - Output: " << total_rows_output_.load() << "\n";
    report << "  - Duplicates Removed: " << duplicates_removed_.load() << "\n";
    report << "  - Conflicts Resolved: " << conflicts_resolved_.load() << "\n";
    
    report << "Files:\n";
    report << "  - Processed: " << files_processed_.load() << "\n";
    report << "  - Total Bytes: " << total_bytes_processed_.load() << " bytes\n";
    
    report << "Performance:\n";
    report << "  - Rows/second: " << getRowsPerSecond() << "\n";
    report << "  - Bytes/second: " << getBytesPerSecond() << " (" 
           << (getBytesPerSecond() / 1024.0 / 1024.0) << " MB/s)\n";
    report << "  - Deduplication Ratio: " << (getDeduplicationRatio() * 100.0) << "%\n";
    
    // EN: Phase timings
    // FR: Chronométrages des phases
    auto phase_timings = getPhaseTimings();
    if (!phase_timings.empty()) {
        report << "Phase Timings:\n";
        for (const auto& [phase, duration] : phase_timings) {
            report << "  - " << phase << ": " << duration.count() << " seconds\n";
        }
    }
    
    // EN: Errors
    // FR: Erreurs
    auto error_counts = getErrorCounts();
    if (!error_counts.empty()) {
        report << "Errors:\n";
        for (const auto& [error, count] : error_counts) {
            report << "  - Error " << static_cast<int>(error) << ": " << count << " occurrences\n";
        }
    }
    
    return report.str();
}

// EN: DuplicateResolver implementation
// FR: Implémentation de DuplicateResolver

DuplicateResolver::DuplicateResolver(const MergeConfig& config) : config_(config) {
    // EN: Initialize duplicate resolver with configuration
    // FR: Initialise le résolveur de doublons avec la configuration
}

bool DuplicateResolver::areDuplicates(const std::vector<std::string>& row1, 
                                     const std::vector<std::string>& row2,
                                     const std::vector<std::string>& headers) const {
    // EN: Check if two rows are duplicates based on configured strategy
    // FR: Vérifie si deux lignes sont des doublons selon la stratégie configurée
    switch (config_.dedup_strategy) {
        case DeduplicationStrategy::NONE:
            return false;
        case DeduplicationStrategy::EXACT_MATCH:
            return exactMatch(row1, row2);
        case DeduplicationStrategy::KEY_BASED:
            return keyBasedMatch(row1, row2, headers);
        case DeduplicationStrategy::FUZZY_MATCH:
            return fuzzyMatch(row1, row2);
        case DeduplicationStrategy::CONTENT_HASH:
            return contentHashMatch(row1, row2);
        case DeduplicationStrategy::CUSTOM_FUNCTION:
            if (config_.custom_dedup_function) {
                return config_.custom_dedup_function(row1, row2);
            }
            return false;
        default:
            return false;
    }
}

std::vector<std::string> DuplicateResolver::resolveConflict(
    const std::vector<std::vector<std::string>>& conflicting_rows,
    const std::vector<std::string>& headers,
    const std::vector<InputSource>& /* sources */) const {
    
    // EN: Resolve conflict between duplicate rows based on strategy
    // FR: Résout le conflit entre lignes dupliquées basé sur la stratégie
    if (conflicting_rows.empty()) {
        return {};
    }
    
    if (conflicting_rows.size() == 1) {
        return conflicting_rows[0];
    }
    
    switch (config_.conflict_resolution) {
        case ConflictResolution::KEEP_FIRST:
            return conflicting_rows[0];
        case ConflictResolution::KEEP_LAST:
            return conflicting_rows.back();
        case ConflictResolution::KEEP_NEWEST:
        case ConflictResolution::KEEP_OLDEST:
            return selectByTimestamp(conflicting_rows, headers, 
                                   config_.conflict_resolution == ConflictResolution::KEEP_NEWEST);
        case ConflictResolution::MERGE_VALUES:
            return mergeValues(conflicting_rows, headers);
        case ConflictResolution::PRIORITY_SOURCE:
            // EN: Use source priority (would need source info passed in)
            // FR: Utilise la priorité de source (nécessiterait info source passée)
            return conflicting_rows[0]; // EN: Fallback / FR: Solution de repli
        case ConflictResolution::CUSTOM_RESOLVER:
            if (config_.custom_conflict_resolver) {
                return config_.custom_conflict_resolver(conflicting_rows);
            }
            return conflicting_rows[0];
        default:
            return conflicting_rows[0];
    }
}

std::string DuplicateResolver::generateKey(const std::vector<std::string>& row,
                                          const std::vector<std::string>& headers) const {
    // EN: Generate unique key for row based on key columns
    // FR: Génère une clé unique pour la ligne basée sur les colonnes clés
    std::ostringstream key_stream;
    
    for (const auto& key_column : config_.key_columns) {
        auto it = std::find(headers.begin(), headers.end(), key_column);
        if (it != headers.end()) {
            size_t index = std::distance(headers.begin(), it);
            if (index < row.size()) {
                std::string value = row[index];
                
                if (config_.trim_key_whitespace) {
                    value = MergeUtils::trim(value);
                }
                if (!config_.case_sensitive_keys) {
                    value = MergeUtils::toLower(value);
                }
                
                key_stream << value << "|";
            }
        }
    }
    
    return key_stream.str();
}

double DuplicateResolver::calculateSimilarity(const std::string& str1, const std::string& str2) const {
    // EN: Calculate similarity score between two strings using best available algorithm
    // FR: Calcule le score de similarité entre deux chaînes en utilisant le meilleur algorithme disponible
    if (str1 == str2) return 1.0;
    if (str1.empty() || str2.empty()) return 0.0;
    
    // EN: Use Levenshtein distance for primary similarity
    // FR: Utilise la distance de Levenshtein pour similarité primaire
    double levenshtein_sim = levenshteinSimilarity(str1, str2);
    
    // EN: Use Jaccard similarity for token-based comparison
    // FR: Utilise la similarité Jaccard pour comparaison basée sur tokens
    double jaccard_sim = jaccardSimilarity(str1, str2);
    
    // EN: Return weighted average
    // FR: Retourne moyenne pondérée
    return (levenshtein_sim * 0.7) + (jaccard_sim * 0.3);
}

bool DuplicateResolver::exactMatch(const std::vector<std::string>& row1, 
                                  const std::vector<std::string>& row2) const {
    // EN: Check for exact field-by-field match
    // FR: Vérifie la correspondance exacte champ par champ
    if (row1.size() != row2.size()) return false;
    
    for (size_t i = 0; i < row1.size(); ++i) {
        if (row1[i] != row2[i]) return false;
    }
    
    return true;
}

bool DuplicateResolver::keyBasedMatch(const std::vector<std::string>& row1,
                                     const std::vector<std::string>& row2,
                                     const std::vector<std::string>& headers) const {
    // EN: Check match based on key columns only
    // FR: Vérifie la correspondance basée uniquement sur les colonnes clés
    std::string key1 = generateKey(row1, headers);
    std::string key2 = generateKey(row2, headers);
    
    return !key1.empty() && key1 == key2;
}

bool DuplicateResolver::fuzzyMatch(const std::vector<std::string>& row1, 
                                  const std::vector<std::string>& row2) const {
    // EN: Check fuzzy match using similarity threshold
    // FR: Vérifie correspondance floue en utilisant seuil de similarité
    if (row1.size() != row2.size()) return false;
    
    double total_similarity = 0.0;
    size_t compared_fields = 0;
    
    for (size_t i = 0; i < row1.size(); ++i) {
        if (!row1[i].empty() || !row2[i].empty()) {
            total_similarity += calculateSimilarity(row1[i], row2[i]);
            compared_fields++;
        }
    }
    
    if (compared_fields == 0) return false;
    
    double avg_similarity = total_similarity / compared_fields;
    return avg_similarity >= config_.fuzzy_threshold;
}

bool DuplicateResolver::contentHashMatch(const std::vector<std::string>& row1,
                                        const std::vector<std::string>& row2) const {
    // EN: Check match using content hash comparison
    // FR: Vérifie correspondance en utilisant comparaison de hash de contenu
    return calculateHash(row1) == calculateHash(row2);
}

double DuplicateResolver::levenshteinSimilarity(const std::string& str1, const std::string& str2) const {
    // EN: Calculate Levenshtein distance similarity (0.0-1.0)
    // FR: Calcule la similarité distance de Levenshtein (0.0-1.0)
    const size_t len1 = str1.size();
    const size_t len2 = str2.size();
    
    if (len1 == 0) return len2 == 0 ? 1.0 : 0.0;
    if (len2 == 0) return 0.0;
    
    std::vector<std::vector<size_t>> dp(len1 + 1, std::vector<size_t>(len2 + 1));
    
    for (size_t i = 0; i <= len1; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= len2; ++j) dp[0][j] = j;
    
    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            size_t cost = (str1[i-1] == str2[j-1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i-1][j] + 1,      // deletion
                dp[i][j-1] + 1,      // insertion
                dp[i-1][j-1] + cost  // substitution
            });
        }
    }
    
    size_t max_len = std::max(len1, len2);
    return 1.0 - static_cast<double>(dp[len1][len2]) / max_len;
}

double DuplicateResolver::jaccardSimilarity(const std::string& str1, const std::string& str2) const {
    // EN: Calculate Jaccard similarity using character bigrams
    // FR: Calcule la similarité Jaccard en utilisant bigrammes de caractères
    std::unordered_set<std::string> bigrams1, bigrams2;
    
    // EN: Generate bigrams for str1
    // FR: Génère des bigrammes pour str1
    for (size_t i = 0; i < str1.size() - 1; ++i) {
        bigrams1.insert(str1.substr(i, 2));
    }
    
    // EN: Generate bigrams for str2
    // FR: Génère des bigrammes pour str2
    for (size_t i = 0; i < str2.size() - 1; ++i) {
        bigrams2.insert(str2.substr(i, 2));
    }
    
    if (bigrams1.empty() && bigrams2.empty()) return 1.0;
    if (bigrams1.empty() || bigrams2.empty()) return 0.0;
    
    // EN: Calculate intersection and union
    // FR: Calcule intersection et union
    size_t intersection = 0;
    for (const auto& bigram : bigrams1) {
        if (bigrams2.count(bigram) > 0) {
            intersection++;
        }
    }
    
    size_t union_size = bigrams1.size() + bigrams2.size() - intersection;
    return static_cast<double>(intersection) / union_size;
}

std::string DuplicateResolver::calculateHash(const std::vector<std::string>& row) const {
    // EN: Calculate hash of row content for fast comparison
    // FR: Calcule le hash du contenu de ligne pour comparaison rapide
    std::ostringstream content;
    for (const auto& field : row) {
        content << field << "|";
    }
    
    // EN: Simple hash function (for demo purposes, could use SHA256 for production)
    // FR: Fonction de hash simple (pour démo, pourrait utiliser SHA256 pour production)
    std::hash<std::string> hasher;
    return std::to_string(hasher(content.str()));
}

std::vector<std::string> DuplicateResolver::mergeValues(
    const std::vector<std::vector<std::string>>& rows,
    const std::vector<std::string>& /* headers */) const {
    
    // EN: Merge values from multiple rows intelligently
    // FR: Fusionne intelligemment les valeurs de plusieurs lignes
    if (rows.empty()) return {};
    if (rows.size() == 1) return rows[0];
    
    std::vector<std::string> merged_row;
    size_t max_cols = 0;
    
    // EN: Find maximum number of columns
    // FR: Trouve le nombre maximum de colonnes
    for (const auto& row : rows) {
        max_cols = std::max(max_cols, row.size());
    }
    
    merged_row.resize(max_cols);
    
    // EN: Merge each column
    // FR: Fusionne chaque colonne
    for (size_t col = 0; col < max_cols; ++col) {
        std::vector<std::string> values;
        
        // EN: Collect all non-empty values for this column
        // FR: Collecte toutes les valeurs non-vides pour cette colonne
        for (const auto& row : rows) {
            if (col < row.size() && !row[col].empty()) {
                values.push_back(row[col]);
            }
        }
        
        if (values.empty()) {
            merged_row[col] = "";
        } else if (values.size() == 1) {
            merged_row[col] = values[0];
        } else {
            // EN: Multiple values - choose the most common or longest
            // FR: Valeurs multiples - choisir la plus commune ou la plus longue
            std::unordered_map<std::string, int> value_counts;
            for (const auto& value : values) {
                value_counts[value]++;
            }
            
            // EN: Find most frequent value
            // FR: Trouve la valeur la plus fréquente
            auto most_frequent = std::max_element(value_counts.begin(), value_counts.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            
            merged_row[col] = most_frequent->first;
        }
    }
    
    return merged_row;
}

std::vector<std::string> DuplicateResolver::selectByTimestamp(
    const std::vector<std::vector<std::string>>& rows,
    const std::vector<std::string>& /* headers */,
    bool keep_newest) const {
    
    // EN: Select row based on timestamp column (newest or oldest)
    // FR: Sélectionne la ligne basée sur colonne timestamp (plus récente ou plus ancienne)
    if (rows.empty()) return {};
    if (rows.size() == 1) return rows[0];
    
    // EN: For now, just return first/last based on keep_newest
    // FR: Pour l'instant, retourne juste premier/dernier basé sur keep_newest
    // EN: TODO: Implement proper timestamp parsing and comparison
    // FR: TODO: Implémenter parsing et comparaison appropriés de timestamp
    return keep_newest ? rows.back() : rows[0];
}

// EN: MergerEngine implementation
// FR: Implémentation de MergerEngine

MergerEngine::MergerEngine() : MergerEngine(MergeConfig()) {
}

MergerEngine::MergerEngine(const MergeConfig& config) : config_(config) {
    // EN: Initialize merger engine with configuration
    // FR: Initialise le moteur de fusion avec la configuration
    duplicate_resolver_ = std::make_unique<DuplicateResolver>(config_);
}


void MergerEngine::setConfig(const MergeConfig& config) {
    // EN: Update configuration and reinitialize resolver
    // FR: Met à jour la configuration et réinitialise le résolveur
    std::lock_guard<std::mutex> lock(engine_mutex_);
    config_ = config;
    duplicate_resolver_ = std::make_unique<DuplicateResolver>(config_);
}

void MergerEngine::addInputSource(const InputSource& source) {
    // EN: Add single input source
    // FR: Ajoute une seule source d'entrée
    std::lock_guard<std::mutex> lock(engine_mutex_);
    input_sources_.push_back(source);
}

void MergerEngine::addInputSources(const std::vector<InputSource>& sources) {
    // EN: Add multiple input sources
    // FR: Ajoute plusieurs sources d'entrée
    std::lock_guard<std::mutex> lock(engine_mutex_);
    input_sources_.insert(input_sources_.end(), sources.begin(), sources.end());
}

void MergerEngine::clearInputSources() {
    // EN: Clear all input sources
    // FR: Efface toutes les sources d'entrée
    std::lock_guard<std::mutex> lock(engine_mutex_);
    input_sources_.clear();
}

MergeError MergerEngine::merge() {
    // EN: Perform merge operation to configured output file
    // FR: Effectue l'opération de fusion vers le fichier de sortie configuré
    return mergeToFile(config_.output_filepath);
}

MergeError MergerEngine::mergeToFile(const std::string& output_path) {
    // EN: Perform merge operation to specified file
    // FR: Effectue l'opération de fusion vers le fichier spécifié
    std::ofstream output_file(output_path);
    if (!output_file.is_open()) {
        reportError(MergeError::OUTPUT_ERROR, "Cannot open output file: " + output_path);
        return MergeError::OUTPUT_ERROR;
    }
    
    MergeError result = mergeToStream(output_file);
    output_file.close();
    
    return result;
}

MergeError MergerEngine::mergeToStream(std::ostream& output_stream) {
    // EN: Perform merge operation to output stream
    // FR: Effectue l'opération de fusion vers le stream de sortie
    std::lock_guard<std::mutex> lock(engine_mutex_);
    
    auto& logger = BBP::Logger::getInstance();
    logger.info("merger_engine", "Starting merge operation with " + std::to_string(input_sources_.size()) + " sources");
    
    stats_.reset();
    stats_.startTiming();
    
    // EN: Validate configuration
    // FR: Valide la configuration
    if (!config_.isValid()) {
        reportError(MergeError::INVALID_CONFIG, "Invalid merge configuration");
        return MergeError::INVALID_CONFIG;
    }
    
    // EN: Load and validate sources
    // FR: Charge et valide les sources
    MergeError load_result = loadAndValidateSources();
    if (load_result != MergeError::SUCCESS) {
        return load_result;
    }
    
    // EN: Perform the actual merge
    // FR: Effectue la fusion réelle
    MergeError merge_result = performMerge(output_stream);
    
    stats_.stopTiming();
    
    logger.info("merger_engine", "Merge operation completed - " + 
                std::to_string(stats_.getTotalRowsOutput()) + " rows written");
    
    return merge_result;
}

MergeError MergerEngine::loadAndValidateSources() {
    // EN: Load and validate all input sources
    // FR: Charge et valide toutes les sources d'entrée
    auto phase_start = std::chrono::high_resolution_clock::now();
    
    if (input_sources_.empty()) {
        reportError(MergeError::INVALID_CONFIG, "No input sources specified");
        return MergeError::INVALID_CONFIG;
    }
    
    // EN: Check that all source files exist
    // FR: Vérifie que tous les fichiers source existent
    for (const auto& source : input_sources_) {
        if (!MergeUtils::fileExists(source.filepath)) {
            reportError(MergeError::FILE_NOT_FOUND, "Source file not found: " + source.filepath);
            return MergeError::FILE_NOT_FOUND;
        }
    }
    
    // EN: Validate schema compatibility if required
    // FR: Valide la compatibilité de schéma si requis
    if (config_.strict_schema_validation && !validateSchemaCompatibility()) {
        reportError(MergeError::SCHEMA_MISMATCH, "Schema validation failed");
        return MergeError::SCHEMA_MISMATCH;
    }
    
    auto phase_end = std::chrono::high_resolution_clock::now();
    stats_.recordPhaseTime("validation", phase_end - phase_start);
    
    return MergeError::SUCCESS;
}

MergeError MergerEngine::performMerge(std::ostream& output_stream) {
    // EN: Perform the actual merge based on configured strategy
    // FR: Effectue la fusion réelle basée sur la stratégie configurée
    switch (config_.merge_strategy) {
        case MergeStrategy::APPEND:
            return appendMerge(output_stream);
        case MergeStrategy::SMART_MERGE:
            return smartMerge(output_stream);
        case MergeStrategy::PRIORITY_MERGE:
            return priorityMerge(output_stream);
        case MergeStrategy::TIME_BASED:
            return timeBasedMerge(output_stream);
        case MergeStrategy::SCHEMA_AWARE:
            return schemaAwareMerge(output_stream);
        default:
            return smartMerge(output_stream); // EN: Default fallback / FR: Solution de repli par défaut
    }
}

MergeError MergerEngine::appendMerge(std::ostream& output_stream) {
    // EN: Simple append merge - concatenate all files
    // FR: Fusion d'ajout simple - concatène tous les fichiers
    auto phase_start = std::chrono::high_resolution_clock::now();
    
    bool first_file = true;
    std::vector<std::string> merged_headers;
    
    for (const auto& source : input_sources_) {
        reportProgress(static_cast<double>(stats_.getFilesProcessed()) / input_sources_.size(), 
                      "Processing " + source.name);
        
        try {
            std::ifstream file(source.filepath);
            if (!file.is_open()) {
                reportError(MergeError::FILE_NOT_FOUND, "Cannot open file: " + source.filepath);
                continue;
            }
            
            std::string line;
            bool is_header = true;
            
            while (std::getline(file, line)) {
                stats_.addBytesProcessed(line.size() + 1);
                
                if (is_header && source.has_header) {
                    if (first_file) {
                        // EN: Write header from first file
                        // FR: Écrit l'en-tête du premier fichier
                        merged_headers = MergeUtils::parseCsvRow(line, source.delimiter);
                        output_stream << line << "\n";
                        first_file = false;
                    }
                    is_header = false;
                    continue;
                }
                
                // EN: Write data row
                // FR: Écrit ligne de données
                output_stream << line << "\n";
                stats_.incrementRowsOutput();
                stats_.incrementRowsProcessed();
            }
            
            file.close();
            stats_.incrementFilesProcessed();
            
        } catch (const std::exception& e) {
            reportError(MergeError::IO_ERROR, "Error processing file " + source.filepath + ": " + e.what());
        }
    }
    
    auto phase_end = std::chrono::high_resolution_clock::now();
    stats_.recordPhaseTime("append_merge", phase_end - phase_start);
    
    return MergeError::SUCCESS;
}

MergeError MergerEngine::smartMerge(std::ostream& output_stream) {
    // EN: Smart merge with deduplication and conflict resolution
    // FR: Fusion intelligente avec déduplication et résolution de conflits
    auto phase_start = std::chrono::high_resolution_clock::now();
    
    // EN: Load all data into memory for processing (TODO: implement streaming for large files)
    // FR: Charge toutes les données en mémoire pour traitement (TODO: implémenter streaming pour gros fichiers)
    std::vector<std::vector<std::string>> all_rows;
    std::vector<std::string> merged_headers;
    std::unordered_map<std::string, std::vector<size_t>> key_to_rows; // EN: Key -> row indices / FR: Clé -> indices de lignes
    
    // EN: First pass: collect all headers and harmonize schema
    // FR: Premier passage : collecte tous les en-têtes et harmonise le schéma
    merged_headers = inferMergedSchema();
    if (merged_headers.empty()) {
        reportError(MergeError::SCHEMA_MISMATCH, "Cannot infer merged schema");
        return MergeError::SCHEMA_MISMATCH;
    }
    
    // EN: Write merged header
    // FR: Écrit l'en-tête fusionné
    output_stream << MergeUtils::join(merged_headers, config_.output_delimiter) << "\n";
    
    // EN: Second pass: load and process all rows
    // FR: Second passage : charge et traite toutes les lignes
    for (const auto& source : input_sources_) {
        reportProgress(static_cast<double>(stats_.getFilesProcessed()) / input_sources_.size(), 
                      "Loading " + source.name);
        
        try {
            std::vector<std::vector<std::string>> source_rows = readCsvFile(source);
            
            for (const auto& row : source_rows) {
                stats_.incrementRowsProcessed();
                
                // EN: Generate key for deduplication
                // FR: Génère clé pour déduplication
                std::string key = duplicate_resolver_->generateKey(row, merged_headers);
                
                if (config_.dedup_strategy != DeduplicationStrategy::NONE && !key.empty()) {
                    // EN: Check for existing rows with same key
                    // FR: Vérifie lignes existantes avec même clé
                    if (key_to_rows.count(key) > 0) {
                        // EN: Found potential duplicate
                        // FR: Trouvé doublon potentiel
                        bool is_duplicate = false;
                        
                        for (size_t existing_idx : key_to_rows[key]) {
                            if (duplicate_resolver_->areDuplicates(row, all_rows[existing_idx], merged_headers)) {
                                // EN: Resolve conflict
                                // FR: Résout le conflit
                                std::vector<std::vector<std::string>> conflicting_rows = {all_rows[existing_idx], row};
                                all_rows[existing_idx] = duplicate_resolver_->resolveConflict(conflicting_rows, merged_headers, input_sources_);
                                
                                stats_.incrementDuplicatesRemoved();
                                stats_.incrementConflictsResolved();
                                is_duplicate = true;
                                break;
                            }
                        }
                        
                        if (!is_duplicate) {
                            // EN: Not actually a duplicate, add new row
                            // FR: Pas vraiment un doublon, ajoute nouvelle ligne
                            size_t new_idx = all_rows.size();
                            all_rows.push_back(row);
                            key_to_rows[key].push_back(new_idx);
                        }
                    } else {
                        // EN: First occurrence of this key
                        // FR: Première occurrence de cette clé
                        size_t new_idx = all_rows.size();
                        all_rows.push_back(row);
                        key_to_rows[key].push_back(new_idx);
                    }
                } else {
                    // EN: No deduplication, just add row
                    // FR: Pas de déduplication, ajoute juste la ligne
                    all_rows.push_back(row);
                }
            }
            
            stats_.incrementFilesProcessed();
            
        } catch (const std::exception& e) {
            reportError(MergeError::IO_ERROR, "Error processing file " + source.filepath + ": " + e.what());
        }
    }
    
    // EN: Write all processed rows
    // FR: Écrit toutes les lignes traitées
    for (const auto& row : all_rows) {
        if (writeRow(output_stream, row)) {
            stats_.incrementRowsOutput();
        }
    }
    
    auto phase_end = std::chrono::high_resolution_clock::now();
    stats_.recordPhaseTime("smart_merge", phase_end - phase_start);
    
    return MergeError::SUCCESS;
}

MergeError MergerEngine::priorityMerge(std::ostream& output_stream) {
    // EN: Priority-based merge using source priority weights
    // FR: Fusion basée sur priorité en utilisant poids de priorité de source
    
    // EN: Sort sources by priority (higher priority first)
    // FR: Trie les sources par priorité (priorité plus élevée en premier)
    std::vector<InputSource> sorted_sources = input_sources_;
    std::sort(sorted_sources.begin(), sorted_sources.end(), 
              [](const InputSource& a, const InputSource& b) { return a.priority > b.priority; });
    
    // EN: Use smart merge but with priority-ordered processing
    // FR: Utilise fusion intelligente mais avec traitement ordonné par priorité
    std::vector<InputSource> original_sources = input_sources_;
    input_sources_ = sorted_sources;
    
    MergeError result = smartMerge(output_stream);
    
    input_sources_ = original_sources; // EN: Restore original order / FR: Restaure l'ordre original
    
    return result;
}

MergeError MergerEngine::timeBasedMerge(std::ostream& output_stream) {
    // EN: Time-based merge using timestamp columns for ordering
    // FR: Fusion basée sur temps en utilisant colonnes timestamp pour ordonnancement
    
    // EN: For now, use smart merge with timestamp-aware conflict resolution
    // FR: Pour l'instant, utilise fusion intelligente avec résolution de conflit consciente du timestamp
    // EN: TODO: Implement proper time-based sorting and merging
    // FR: TODO: Implémenter tri et fusion basés sur temps appropriés
    
    return smartMerge(output_stream);
}

MergeError MergerEngine::schemaAwareMerge(std::ostream& output_stream) {
    // EN: Schema-aware merge with type validation and conversion
    // FR: Fusion consciente du schéma avec validation et conversion de type
    
    // EN: For now, use smart merge with strict schema validation
    // FR: Pour l'instant, utilise fusion intelligente avec validation stricte du schéma
    // EN: TODO: Implement proper type detection and conversion
    // FR: TODO: Implémenter détection et conversion de type appropriées
    
    return smartMerge(output_stream);
}

std::vector<std::string> MergerEngine::inferMergedSchema() const {
    // EN: Infer merged schema from all input sources
    // FR: Infère le schéma fusionné à partir de toutes les sources d'entrée
    std::vector<std::string> merged_schema;
    std::unordered_set<std::string> all_headers;
    
    // EN: Collect all unique headers from all sources
    // FR: Collecte tous les en-têtes uniques de toutes les sources
    for (const auto& source : input_sources_) {
        try {
            std::vector<std::string> headers = readCsvHeaders(source.filepath, source.delimiter);
            for (const auto& header : headers) {
                all_headers.insert(header);
            }
        } catch (const std::exception& e) {
            // EN: Skip problematic files
            // FR: Ignore les fichiers problématiques
            continue;
        }
    }
    
    // EN: Convert to vector (could be ordered by frequency or importance)
    // FR: Convertit en vecteur (pourrait être ordonné par fréquence ou importance)
    merged_schema.assign(all_headers.begin(), all_headers.end());
    std::sort(merged_schema.begin(), merged_schema.end());
    
    return merged_schema;
}

bool MergerEngine::validateSchemaCompatibility() const {
    // EN: Validate that all sources have compatible schemas
    // FR: Valide que toutes les sources ont des schémas compatibles
    if (input_sources_.empty()) return true;
    
    std::vector<std::string> reference_schema;
    
    try {
        reference_schema = readCsvHeaders(input_sources_[0].filepath, input_sources_[0].delimiter);
    } catch (const std::exception&) {
        return false;
    }
    
    // EN: Check all other sources against reference
    // FR: Vérifie toutes les autres sources contre la référence
    for (size_t i = 1; i < input_sources_.size(); ++i) {
        try {
            std::vector<std::string> current_schema = readCsvHeaders(input_sources_[i].filepath, input_sources_[i].delimiter);
            
            if (config_.strict_schema_validation && current_schema != reference_schema) {
                return false;
            }
        } catch (const std::exception&) {
            return false;
        }
    }
    
    return true;
}

std::vector<std::string> MergerEngine::readCsvHeaders(const std::string& filepath, char delimiter) const {
    // EN: Read CSV headers from file
    // FR: Lit les en-têtes CSV du fichier
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    
    std::string header_line;
    if (!std::getline(file, header_line)) {
        throw std::runtime_error("Cannot read header from file: " + filepath);
    }
    
    return MergeUtils::parseCsvRow(header_line, delimiter);
}

std::vector<std::vector<std::string>> MergerEngine::readCsvFile(const InputSource& source) const {
    // EN: Read entire CSV file into memory
    // FR: Lit le fichier CSV entier en mémoire
    std::vector<std::vector<std::string>> rows;
    std::ifstream file(source.filepath);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + source.filepath);
    }
    
    std::string line;
    bool skip_header = source.has_header;
    
    while (std::getline(file, line)) {
        if (skip_header) {
            skip_header = false;
            continue;
        }
        
        std::vector<std::string> row = MergeUtils::parseCsvRow(line, source.delimiter);
        
        // EN: Apply custom row filter if configured
        // FR: Applique le filtre de ligne personnalisé si configuré
        if (config_.custom_row_filter && !config_.custom_row_filter(row)) {
            continue;
        }
        
        rows.push_back(row);
    }
    
    return rows;
}

bool MergerEngine::writeRow(std::ostream& output_stream, const std::vector<std::string>& row) const {
    // EN: Write row to output stream in CSV format
    // FR: Écrit la ligne vers le stream de sortie au format CSV
    try {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) {
                output_stream << config_.output_delimiter;
            }
            
            std::string field = row[i];
            if (MergeUtils::needsQuoting(field, config_.output_delimiter)) {
                output_stream << '"' << MergeUtils::escapeCsvField(field, config_.output_delimiter) << '"';
            } else {
                output_stream << field;
            }
        }
        output_stream << "\n";
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void MergerEngine::reportProgress(double progress, const std::string& message) const {
    // EN: Report progress to callback if configured
    // FR: Rapporte la progression au callback si configuré
    if (progress_callback_) {
        progress_callback_(progress, message);
    }
}

void MergerEngine::reportError(MergeError error, const std::string& message) {
    // EN: Report error to callback and statistics
    // FR: Rapporte l'erreur au callback et aux statistiques
    auto& logger = BBP::Logger::getInstance();
    logger.error("merger_engine", "Merge error: " + message);
    
    stats_.recordError(error, message);
    
    if (error_callback_) {
        error_callback_(error, message);
    }
}

// EN: MergeUtils namespace implementation
// FR: Implémentation du namespace MergeUtils

namespace MergeUtils {

std::string trim(const std::string& str) {
    // EN: Remove leading and trailing whitespace
    // FR: Supprime les espaces en début et fin
    const char* whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

std::string toLower(const std::string& str) {
    // EN: Convert string to lowercase
    // FR: Convertit la chaîne en minuscules
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    // EN: Split string by delimiter
    // FR: Divise la chaîne par délimiteur
    std::vector<std::string> parts;
    std::stringstream ss(str);
    std::string part;
    
    while (std::getline(ss, part, delimiter)) {
        parts.push_back(part);
    }
    
    return parts;
}

std::string join(const std::vector<std::string>& parts, char delimiter) {
    // EN: Join string parts with delimiter
    // FR: Joint les parties de chaîne avec délimiteur
    std::ostringstream oss;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            oss << delimiter;
        }
        oss << parts[i];
    }
    return oss.str();
}

std::vector<std::string> parseCsvRow(const std::string& line, char delimiter) {
    // EN: Parse CSV row handling quoted fields
    // FR: Parse ligne CSV en gérant les champs entre guillemets
    std::vector<std::string> fields;
    std::string current_field;
    bool in_quotes = false;
    bool escape_next = false;
    
    for (char c : line) {
        if (escape_next) {
            current_field += c;
            escape_next = false;
        } else if (c == '"') {
            if (in_quotes && !current_field.empty() && current_field.back() == '"') {
                // EN: Escaped quote
                // FR: Guillemet échappé
                current_field += c;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (c == delimiter && !in_quotes) {
            fields.push_back(current_field);
            current_field.clear();
        } else {
            current_field += c;
        }
    }
    
    fields.push_back(current_field);
    return fields;
}

std::string escapeCsvField(const std::string& field, char /* delimiter */) {
    // EN: Escape CSV field by doubling quotes
    // FR: Échappe champ CSV en doublant les guillemets
    std::string escaped = field;
    size_t pos = 0;
    while ((pos = escaped.find('"', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\"\"");
        pos += 2;
    }
    return escaped;
}

bool needsQuoting(const std::string& field, char delimiter) {
    // EN: Check if field needs quoting
    // FR: Vérifie si le champ a besoin de guillemets
    return field.find(delimiter) != std::string::npos ||
           field.find('"') != std::string::npos ||
           field.find('\n') != std::string::npos ||
           field.find('\r') != std::string::npos;
}

bool fileExists(const std::string& filepath) {
    // EN: Check if file exists
    // FR: Vérifie si le fichier existe
    return std::filesystem::exists(filepath);
}

size_t getFileSize(const std::string& filepath) {
    // EN: Get file size in bytes
    // FR: Obtient la taille du fichier en octets
    try {
        return std::filesystem::file_size(filepath);
    } catch (const std::exception&) {
        return 0;
    }
}

std::string detectEncoding(const std::string& /* filepath */) {
    // EN: Detect file encoding (basic implementation)
    // FR: Détecte l'encodage du fichier (implémentation basique)
    // EN: TODO: Implement proper encoding detection
    // FR: TODO: Implémenter détection d'encodage appropriée
    return "UTF-8";
}

char detectDelimiter(const std::string& filepath) {
    // EN: Detect CSV delimiter by analyzing first few lines
    // FR: Détecte le délimiteur CSV en analysant les premières lignes
    std::ifstream file(filepath);
    if (!file.is_open()) return ',';
    
    std::string line;
    std::unordered_map<char, int> delimiter_counts;
    std::vector<char> candidates = {',', ';', '\t', '|'};
    
    // EN: Analyze first 10 lines
    // FR: Analyse les 10 premières lignes
    for (int i = 0; i < 10 && std::getline(file, line); ++i) {
        for (char candidate : candidates) {
            delimiter_counts[candidate] += std::count(line.begin(), line.end(), candidate);
        }
    }
    
    // EN: Return most frequent delimiter
    // FR: Retourne le délimiteur le plus fréquent
    auto max_it = std::max_element(delimiter_counts.begin(), delimiter_counts.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    return max_it != delimiter_counts.end() ? max_it->first : ',';
}

MergeUtils::DataType detectType(const std::string& value) {
    // EN: Detect data type of string value
    // FR: Détecte le type de donnée de la valeur chaîne
    if (value.empty()) return DataType::STRING;
    
    if (isNumeric(value)) {
        return value.find('.') != std::string::npos ? DataType::FLOAT : DataType::INTEGER;
    }
    
    if (value == "true" || value == "false" || value == "TRUE" || value == "FALSE") {
        return DataType::BOOLEAN;
    }
    
    if (isTimestamp(value)) return DataType::TIMESTAMP;
    if (isDate(value)) return DataType::DATE;
    
    return DataType::STRING;
}

bool isNumeric(const std::string& value) {
    // EN: Check if string represents a number
    // FR: Vérifie si la chaîne représente un nombre
    if (value.empty()) return false;
    
    std::regex numeric_regex(R"([+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?)");
    return std::regex_match(value, numeric_regex);
}

bool isDate(const std::string& value) {
    // EN: Check if string represents a date
    // FR: Vérifie si la chaîne représente une date
    if (value.empty()) return false;
    
    // EN: Basic date patterns (YYYY-MM-DD, DD/MM/YYYY, etc.)
    // FR: Patterns de date basiques (YYYY-MM-DD, DD/MM/YYYY, etc.)
    std::vector<std::regex> date_patterns = {
        std::regex(R"(\d{4}-\d{2}-\d{2})"),           // YYYY-MM-DD
        std::regex(R"(\d{2}/\d{2}/\d{4})"),           // DD/MM/YYYY
        std::regex(R"(\d{2}-\d{2}-\d{4})"),           // DD-MM-YYYY
    };
    
    for (const auto& pattern : date_patterns) {
        if (std::regex_match(value, pattern)) {
            return true;
        }
    }
    
    return false;
}

bool isTimestamp(const std::string& value) {
    // EN: Check if string represents a timestamp
    // FR: Vérifie si la chaîne représente un timestamp
    if (value.empty()) return false;
    
    // EN: Basic timestamp patterns
    // FR: Patterns de timestamp basiques
    std::vector<std::regex> timestamp_patterns = {
        std::regex(R"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})"),        // YYYY-MM-DD HH:MM:SS
        std::regex(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z?)"),      // ISO 8601
        std::regex(R"(\d{10})"),                                      // Unix timestamp
    };
    
    for (const auto& pattern : timestamp_patterns) {
        if (std::regex_match(value, pattern)) {
            return true;
        }
    }
    
    return false;
}

size_t estimateRowSize(const std::vector<std::string>& row) {
    // EN: Estimate memory size of row
    // FR: Estime la taille mémoire de la ligne
    size_t size = 0;
    for (const auto& field : row) {
        size += field.size() + sizeof(std::string); // EN: Approximate overhead / FR: Surcharge approximative
    }
    return size;
}

size_t estimateFileRows(const std::string& filepath) {
    // EN: Estimate number of rows in file
    // FR: Estime le nombre de lignes dans le fichier
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) return 0;
        
        // EN: Count lines in first 1KB and extrapolate
        // FR: Compte les lignes dans le premier 1KB et extrapole
        std::string buffer(1024, '\0');
        file.read(&buffer[0], 1024);
        size_t bytes_read = file.gcount();
        
        size_t line_count = std::count(buffer.begin(), buffer.begin() + bytes_read, '\n');
        size_t file_size = getFileSize(filepath);
        
        if (bytes_read > 0 && line_count > 0) {
            return (file_size * line_count) / bytes_read;
        }
        
        return 0;
    } catch (const std::exception&) {
        return 0;
    }
}

bool shouldUseParallelProcessing(size_t total_rows, size_t available_memory) {
    // EN: Determine if parallel processing would be beneficial
    // FR: Détermine si le traitement parallèle serait bénéfique
    const size_t MIN_ROWS_FOR_PARALLEL = 1000;
    const size_t MIN_MEMORY_FOR_PARALLEL = 100 * 1024 * 1024; // 100MB
    
    return total_rows >= MIN_ROWS_FOR_PARALLEL && available_memory >= MIN_MEMORY_FOR_PARALLEL;
}

} // namespace MergeUtils

} // namespace CSV
} // namespace BBP