// EN: Config Override System for BB-Pipeline - CLI-based configuration parameter overrides
// FR: Système de Surcharge de Configuration pour BB-Pipeline - Surcharges de paramètres via CLI

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <regex>
#include <set>

#include "infrastructure/config/config_manager.hpp"
#include "infrastructure/logging/logger.hpp"

namespace BBP {
namespace CLI {

// EN: Forward declarations
// FR: Déclarations avancées
class ConfigOverrideParser;
class ConfigOverrideManager;
class ConfigOverrideValidator;

// EN: CLI option types and specifications
// FR: Types et spécifications des options CLI
enum class CliOptionType {
    BOOLEAN,        // EN: Boolean flag / FR: Drapeau booléen
    INTEGER,        // EN: Integer value / FR: Valeur entière
    DOUBLE,         // EN: Double precision float / FR: Flottant double précision
    STRING,         // EN: String value / FR: Valeur chaîne
    STRING_LIST     // EN: Comma-separated string list / FR: Liste de chaînes séparées par virgule
};

// EN: CLI option value constraints
// FR: Contraintes de valeur d'option CLI
enum class CliOptionConstraint {
    NONE,           // EN: No constraints / FR: Aucune contrainte
    POSITIVE,       // EN: Must be positive (>0) / FR: Doit être positif (>0)
    NON_NEGATIVE,   // EN: Must be non-negative (>=0) / FR: Doit être non-négatif (>=0)
    RANGE,          // EN: Must be within specified range / FR: Doit être dans la plage spécifiée
    REGEX_MATCH,    // EN: Must match regex pattern / FR: Doit correspondre au motif regex
    ENUM_VALUES     // EN: Must be one of predefined values / FR: Doit être l'une des valeurs prédéfinies
};

// EN: CLI parsing result status
// FR: Statut de résultat d'analyse CLI
enum class CliParseStatus {
    SUCCESS,                // EN: Parsing completed successfully / FR: Analyse terminée avec succès
    HELP_REQUESTED,         // EN: Help was requested / FR: Aide demandée
    VERSION_REQUESTED,      // EN: Version was requested / FR: Version demandée
    INVALID_OPTION,         // EN: Invalid option provided / FR: Option invalide fournie
    MISSING_VALUE,          // EN: Required value missing / FR: Valeur requise manquante
    INVALID_VALUE,          // EN: Invalid value format / FR: Format de valeur invalide
    CONSTRAINT_VIOLATION,   // EN: Value constraint violation / FR: Violation de contrainte de valeur
    CONFIG_FILE_ERROR,      // EN: Configuration file error / FR: Erreur de fichier de configuration
    DUPLICATE_OPTION        // EN: Duplicate option specified / FR: Option dupliquée spécifiée
};

// EN: CLI option definition structure
// FR: Structure de définition d'option CLI
struct CliOptionDefinition {
    std::string long_name;                          // EN: Long option name (--example) / FR: Nom d'option long (--exemple)
    std::optional<char> short_name;                 // EN: Short option name (-e) / FR: Nom d'option court (-e)
    CliOptionType type;                            // EN: Option value type / FR: Type de valeur d'option
    std::string description;                       // EN: Option description for help / FR: Description d'option pour l'aide
    std::string config_path;                       // EN: Configuration path (e.g., "http.timeout") / FR: Chemin de configuration (ex: "http.timeout")
    std::optional<std::string> default_value;      // EN: Default value as string / FR: Valeur par défaut en chaîne
    bool required = false;                         // EN: Whether option is required / FR: Si l'option est requise
    bool hidden = false;                           // EN: Hide from help output / FR: Masquer de la sortie d'aide
    
    // EN: Validation constraints
    // FR: Contraintes de validation
    CliOptionConstraint constraint = CliOptionConstraint::NONE;
    std::optional<double> min_value;               // EN: Minimum numeric value / FR: Valeur numérique minimale
    std::optional<double> max_value;               // EN: Maximum numeric value / FR: Valeur numérique maximale
    std::optional<std::string> regex_pattern;     // EN: Regex validation pattern / FR: Motif de validation regex
    std::set<std::string> enum_values;             // EN: Valid enum values / FR: Valeurs d'énumération valides
    
    // EN: Advanced options
    // FR: Options avancées
    bool repeatable = false;                       // EN: Can be specified multiple times / FR: Peut être spécifié plusieurs fois
    std::string category = "General";              // EN: Help category / FR: Catégorie d'aide
    int priority = 0;                              // EN: Display priority in help / FR: Priorité d'affichage dans l'aide
};

// EN: Parsed CLI option value
// FR: Valeur d'option CLI analysée
struct CliOptionValue {
    std::string option_name;                       // EN: Option name that was parsed / FR: Nom d'option qui a été analysé
    CliOptionType type;                           // EN: Parsed value type / FR: Type de valeur analysée
    std::vector<std::string> raw_values;          // EN: Raw string values from command line / FR: Valeurs chaîne brutes de la ligne de commande
    ConfigValue config_value;                     // EN: Converted configuration value / FR: Valeur de configuration convertie
    std::string config_path;                      // EN: Configuration path for override / FR: Chemin de configuration pour surcharge
    bool is_default = false;                      // EN: Whether using default value / FR: Si utilise la valeur par défaut
};

// EN: CLI parsing result containing all parsed options and status
// FR: Résultat d'analyse CLI contenant toutes les options analysées et le statut
struct CliParseResult {
    CliParseStatus status;                        // EN: Overall parsing status / FR: Statut d'analyse global
    std::vector<CliOptionValue> parsed_options;   // EN: Successfully parsed options / FR: Options analysées avec succès
    std::vector<std::string> errors;              // EN: Parsing error messages / FR: Messages d'erreur d'analyse
    std::vector<std::string> warnings;            // EN: Parsing warning messages / FR: Messages d'avertissement d'analyse
    std::unordered_map<std::string, ConfigValue> overrides; // EN: Configuration overrides map / FR: Carte des surcharges de configuration
    
    std::string help_text;                        // EN: Generated help text (if requested) / FR: Texte d'aide généré (si demandé)
    std::string version_text;                     // EN: Version information (if requested) / FR: Information de version (si demandée)
    
    // EN: Statistics
    // FR: Statistiques
    size_t total_arguments_processed = 0;         // EN: Total CLI arguments processed / FR: Total d'arguments CLI traités
    size_t overrides_applied = 0;                // EN: Number of overrides applied / FR: Nombre de surcharges appliquées
    std::chrono::milliseconds parse_duration{0}; // EN: Time taken to parse / FR: Temps pris pour analyser
};

// EN: Configuration override validation result
// FR: Résultat de validation de surcharge de configuration
struct ConfigOverrideValidationResult {
    bool is_valid = true;                         // EN: Whether all overrides are valid / FR: Si toutes les surcharges sont valides
    std::vector<std::string> errors;              // EN: Validation errors / FR: Erreurs de validation
    std::vector<std::string> warnings;            // EN: Validation warnings / FR: Avertissements de validation
    std::map<std::string, std::string> conflicting_overrides; // EN: Conflicting override pairs / FR: Paires de surcharges conflictuelles
    std::set<std::string> deprecated_paths;       // EN: Deprecated configuration paths / FR: Chemins de configuration obsolètes
};

// EN: CLI event types for monitoring and logging
// FR: Types d'événements CLI pour surveillance et journalisation
enum class ConfigOverrideEventType {
    PARSING_STARTED,        // EN: CLI parsing started / FR: Analyse CLI démarrée
    PARSING_COMPLETED,      // EN: CLI parsing completed / FR: Analyse CLI terminée
    OPTION_PARSED,          // EN: Individual option parsed / FR: Option individuelle analysée
    VALIDATION_STARTED,     // EN: Validation started / FR: Validation démarrée
    VALIDATION_COMPLETED,   // EN: Validation completed / FR: Validation terminée
    OVERRIDE_APPLIED,       // EN: Configuration override applied / FR: Surcharge de configuration appliquée
    HELP_DISPLAYED,         // EN: Help information displayed / FR: Information d'aide affichée
    ERROR_OCCURRED          // EN: Error occurred during processing / FR: Erreur survenue pendant le traitement
};

// EN: Event data structure for monitoring CLI operations
// FR: Structure de données d'événement pour surveiller les opérations CLI
struct ConfigOverrideEvent {
    ConfigOverrideEventType type;                 // EN: Event type / FR: Type d'événement
    std::chrono::system_clock::time_point timestamp; // EN: Event timestamp / FR: Horodatage d'événement
    std::string operation_id;                     // EN: Unique operation identifier / FR: Identifiant unique d'opération
    std::string option_name;                      // EN: Related option name / FR: Nom d'option associé
    std::string message;                          // EN: Event message / FR: Message d'événement
    std::map<std::string, std::string> metadata;  // EN: Additional event metadata / FR: Métadonnées d'événement supplémentaires
    std::chrono::milliseconds duration{0};       // EN: Operation duration / FR: Durée d'opération
    bool success = true;                          // EN: Operation success status / FR: Statut de succès d'opération
};

// EN: Event callback function type
// FR: Type de fonction de rappel d'événement
using ConfigOverrideEventCallback = std::function<void(const ConfigOverrideEvent&)>;

// EN: Main configuration override parser for handling CLI arguments
// FR: Analyseur principal de surcharge de configuration pour gérer les arguments CLI
class ConfigOverrideParser {
public:
    // EN: Constructor with optional event callback
    // FR: Constructeur avec rappel d'événement optionnel
    explicit ConfigOverrideParser(ConfigOverrideEventCallback event_callback = nullptr);
    
    // EN: Destructor
    // FR: Destructeur
    ~ConfigOverrideParser();
    
    // EN: Non-copyable and non-movable for thread safety
    // FR: Non-copiable et non-déplaçable pour la sécurité des threads
    ConfigOverrideParser(const ConfigOverrideParser&) = delete;
    ConfigOverrideParser& operator=(const ConfigOverrideParser&) = delete;
    ConfigOverrideParser(ConfigOverrideParser&&) = delete;
    ConfigOverrideParser& operator=(ConfigOverrideParser&&) = delete;
    
    // EN: Option definition management
    // FR: Gestion des définitions d'options
    void addOption(const CliOptionDefinition& option_def);
    void addOptions(const std::vector<CliOptionDefinition>& option_defs);
    void removeOption(const std::string& long_name);
    void clearOptions();
    
    // EN: Pre-defined option sets for common use cases
    // FR: Ensembles d'options pré-définis pour cas d'usage courants
    void addStandardOptions();                    // EN: Add standard BB-Pipeline options / FR: Ajouter les options BB-Pipeline standard
    void addLoggingOptions();                     // EN: Add logging-related options / FR: Ajouter les options liées à la journalisation
    void addNetworkingOptions();                  // EN: Add networking-related options / FR: Ajouter les options liées au réseau
    void addPerformanceOptions();                 // EN: Add performance-related options / FR: Ajouter les options liées aux performances
    
    // EN: CLI parsing operations
    // FR: Opérations d'analyse CLI
    CliParseResult parse(int argc, char* argv[]);
    CliParseResult parse(const std::vector<std::string>& arguments);
    
    // EN: Help and version generation
    // FR: Génération d'aide et de version
    std::string generateHelpText(const std::string& program_name = "bbpctl") const;
    std::string generateVersionText() const;
    
    // EN: Validation and error checking
    // FR: Validation et vérification d'erreurs
    bool validateOptionDefinitions() const;
    std::vector<std::string> getDefinitionErrors() const;
    
    // EN: Configuration and customization
    // FR: Configuration et personnalisation
    void setHelpHeader(const std::string& header);
    void setHelpFooter(const std::string& footer);
    void setVersionInfo(const std::string& version, const std::string& build_info = "");
    void setEventCallback(ConfigOverrideEventCallback callback);
    
    // EN: Query option definitions
    // FR: Interroger les définitions d'options
    std::vector<CliOptionDefinition> getOptionDefinitions() const;
    std::optional<CliOptionDefinition> getOptionDefinition(const std::string& name) const;
    bool hasOption(const std::string& name) const;
    
private:
    class ConfigOverrideParserImpl;
    std::unique_ptr<ConfigOverrideParserImpl> impl_;
};

// EN: Configuration override validator for ensuring override compatibility
// FR: Validateur de surcharge de configuration pour assurer la compatibilité des surcharges
class ConfigOverrideValidator {
public:
    // EN: Constructor
    // FR: Constructeur
    ConfigOverrideValidator();
    
    // EN: Destructor
    // FR: Destructeur
    ~ConfigOverrideValidator();
    
    // EN: Validate configuration overrides
    // FR: Valider les surcharges de configuration
    ConfigOverrideValidationResult validateOverrides(
        const std::unordered_map<std::string, ConfigValue>& overrides,
        const ConfigManager* base_config = nullptr
    ) const;
    
    // EN: Add custom validation rules
    // FR: Ajouter des règles de validation personnalisées
    using ValidationRule = std::function<bool(const std::string& path, const ConfigValue& value, std::string& error_message)>;
    void addValidationRule(const std::string& config_path_pattern, ValidationRule rule);
    void removeValidationRule(const std::string& config_path_pattern);
    void clearValidationRules();
    
    // EN: Configuration path management
    // FR: Gestion des chemins de configuration
    void addDeprecatedPath(const std::string& old_path, const std::string& new_path = "");
    void addAlternativePath(const std::string& primary_path, const std::string& alternative_path);
    void setPathConstraints(const std::string& path, CliOptionConstraint constraint, 
                           double min_val = 0, double max_val = 0);
    
    // EN: Utility functions
    // FR: Fonctions utilitaires
    bool isValidConfigPath(const std::string& path) const;
    bool isDeprecatedPath(const std::string& path) const;
    std::optional<std::string> getReplacementPath(const std::string& deprecated_path) const;
    
private:
    class ConfigOverrideValidatorImpl;
    std::unique_ptr<ConfigOverrideValidatorImpl> impl_;
};

// EN: Main configuration override manager integrating parsing, validation, and application
// FR: Gestionnaire principal de surcharge de configuration intégrant analyse, validation et application
class ConfigOverrideManager {
public:
    // EN: Constructor with ConfigManager integration
    // FR: Constructeur avec intégration ConfigManager
    explicit ConfigOverrideManager(std::shared_ptr<ConfigManager> config_manager, 
                                  ConfigOverrideEventCallback event_callback = nullptr);
    
    // EN: Destructor
    // FR: Destructeur
    ~ConfigOverrideManager();
    
    // EN: Non-copyable and non-movable
    // FR: Non-copiable et non-déplaçable
    ConfigOverrideManager(const ConfigOverrideManager&) = delete;
    ConfigOverrideManager& operator=(const ConfigOverrideManager&) = delete;
    ConfigOverrideManager(ConfigOverrideManager&&) = delete;
    ConfigOverrideManager& operator=(ConfigOverrideManager&&) = delete;
    
    // EN: Main CLI processing workflow
    // FR: Flux de travail principal de traitement CLI
    CliParseResult processCliArguments(int argc, char* argv[]);
    CliParseResult processCliArguments(const std::vector<std::string>& arguments);
    
    // EN: Configuration override operations
    // FR: Opérations de surcharge de configuration
    bool applyOverrides(const std::unordered_map<std::string, ConfigValue>& overrides);
    bool applyOverridesFromParseResult(const CliParseResult& parse_result);
    void clearOverrides();
    void resetToDefaults();
    
    // EN: Override management
    // FR: Gestion des surcharges
    std::unordered_map<std::string, ConfigValue> getCurrentOverrides() const;
    bool hasOverride(const std::string& config_path) const;
    std::optional<ConfigValue> getOverrideValue(const std::string& config_path) const;
    bool removeOverride(const std::string& config_path);
    
    // EN: Configuration access
    // FR: Accès à la configuration
    std::shared_ptr<ConfigManager> getConfigManager() const;
    ConfigOverrideParser& getParser();
    const ConfigOverrideParser& getParser() const;
    ConfigOverrideValidator& getValidator();
    const ConfigOverrideValidator& getValidator() const;
    
    // EN: Statistics and monitoring
    // FR: Statistiques et surveillance
    struct OverrideStatistics {
        size_t total_overrides_applied = 0;       // EN: Total overrides applied / FR: Total des surcharges appliquées
        size_t active_overrides = 0;              // EN: Currently active overrides / FR: Surcharges actuellement actives
        std::chrono::system_clock::time_point last_override_time; // EN: Last override timestamp / FR: Horodatage dernière surcharge
        std::map<std::string, size_t> override_counts_by_path;    // EN: Override counts by path / FR: Compteurs de surcharges par chemin
        std::vector<std::string> recent_override_paths;           // EN: Recently overridden paths / FR: Chemins récemment surchargés
    };
    
    OverrideStatistics getStatistics() const;
    void resetStatistics();
    
    // EN: Event callback management
    // FR: Gestion des rappels d'événements
    void setEventCallback(ConfigOverrideEventCallback callback);
    void clearEventCallback();
    
private:
    class ConfigOverrideManagerImpl;
    std::unique_ptr<ConfigOverrideManagerImpl> impl_;
};

// EN: Utility functions for configuration override operations
// FR: Fonctions utilitaires pour les opérations de surcharge de configuration
namespace ConfigOverrideUtils {
    
    // EN: String conversion utilities
    // FR: Utilitaires de conversion de chaînes
    std::string cliOptionTypeToString(CliOptionType type);
    CliOptionType stringToCliOptionType(const std::string& str);
    
    std::string cliOptionConstraintToString(CliOptionConstraint constraint);
    CliOptionConstraint stringToCliOptionConstraint(const std::string& str);
    
    std::string cliParseStatusToString(CliParseStatus status);
    
    // EN: Configuration path utilities
    // FR: Utilitaires de chemin de configuration
    bool isValidConfigPath(const std::string& path);
    std::vector<std::string> splitConfigPath(const std::string& path);
    std::string joinConfigPath(const std::vector<std::string>& parts);
    std::string normalizeConfigPath(const std::string& path);
    
    // EN: Value conversion utilities
    // FR: Utilitaires de conversion de valeurs
    ConfigValue parseCliValue(const std::string& raw_value, CliOptionType type);
    std::vector<ConfigValue> parseCliValueList(const std::vector<std::string>& raw_values, CliOptionType type);
    
    bool validateCliValue(const std::string& raw_value, CliOptionType type, 
                         const CliOptionDefinition& definition, std::string& error_message);
    
    // EN: Help text formatting utilities
    // FR: Utilitaires de formatage de texte d'aide
    std::string formatOptionHelp(const CliOptionDefinition& option, size_t max_width = 80);
    std::string formatHelpCategory(const std::string& category, 
                                  const std::vector<CliOptionDefinition>& options, 
                                  size_t max_width = 80);
    
    // EN: Command line argument utilities
    // FR: Utilitaires d'arguments de ligne de commande
    std::vector<std::string> splitArgumentString(const std::string& args);
    std::string escapeArgumentString(const std::string& arg);
    bool isShortOption(const std::string& arg);
    bool isLongOption(const std::string& arg);
    std::string extractOptionName(const std::string& arg);
    
} // namespace ConfigOverrideUtils

} // namespace CLI
} // namespace BBP