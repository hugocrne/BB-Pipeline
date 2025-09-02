// EN: Config Override System Implementation for BB-Pipeline - CLI-based configuration parameter overrides
// FR: Implémentation du Système de Surcharge de Configuration pour BB-Pipeline - Surcharges de paramètres via CLI

#include "infrastructure/cli/config_override.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <random>

namespace BBP {
namespace CLI {

// EN: ConfigOverrideParser Implementation using PIMPL pattern for better encapsulation
// FR: Implémentation ConfigOverrideParser utilisant le motif PIMPL pour une meilleure encapsulation
class ConfigOverrideParser::ConfigOverrideParserImpl {
public:
    explicit ConfigOverrideParserImpl(ConfigOverrideEventCallback event_callback)
        : event_callback_(event_callback)
        , help_header_("BB-Pipeline Configuration Override System")
        , help_footer_("For more information, visit: https://github.com/bb-pipeline/bb-pipeline")
        , version_("1.0.0")
        , build_info_("Development Build") {
    }
    
    ~ConfigOverrideParserImpl() = default;
    
    // EN: Option management methods
    // FR: Méthodes de gestion d'options
    void addOption(const CliOptionDefinition& option_def) {
        std::lock_guard<std::mutex> lock(options_mutex_);
        
        // EN: Validate option definition before adding
        // FR: Valider la définition d'option avant d'ajouter
        if (option_def.long_name.empty()) {
            throw std::invalid_argument("Option long name cannot be empty");
        }
        
        if (options_by_long_name_.find(option_def.long_name) != options_by_long_name_.end()) {
            throw std::invalid_argument("Option with long name '" + option_def.long_name + "' already exists");
        }
        
        if (option_def.short_name && options_by_short_name_.find(*option_def.short_name) != options_by_short_name_.end()) {
            throw std::invalid_argument("Option with short name '-" + std::string(1, *option_def.short_name) + "' already exists");
        }
        
        option_definitions_.push_back(option_def);
        options_by_long_name_[option_def.long_name] = option_def;
        
        if (option_def.short_name) {
            options_by_short_name_[*option_def.short_name] = option_def;
        }
        
        emitEvent(ConfigOverrideEventType::OPTION_PARSED, "", 
                 "Added option definition: " + option_def.long_name);
    }
    
    void addOptions(const std::vector<CliOptionDefinition>& option_defs) {
        for (const auto& option_def : option_defs) {
            addOption(option_def);
        }
    }
    
    void removeOption(const std::string& long_name) {
        std::lock_guard<std::mutex> lock(options_mutex_);
        
        auto it = options_by_long_name_.find(long_name);
        if (it == options_by_long_name_.end()) {
            return;
        }
        
        const auto& option_def = it->second;
        
        // EN: Remove from all containers
        // FR: Supprimer de tous les conteneurs
        options_by_long_name_.erase(it);
        
        if (option_def.short_name) {
            options_by_short_name_.erase(*option_def.short_name);
        }
        
        option_definitions_.erase(
            std::remove_if(option_definitions_.begin(), option_definitions_.end(),
                [&long_name](const CliOptionDefinition& def) {
                    return def.long_name == long_name;
                }),
            option_definitions_.end()
        );
    }
    
    void clearOptions() {
        std::lock_guard<std::mutex> lock(options_mutex_);
        option_definitions_.clear();
        options_by_long_name_.clear();
        options_by_short_name_.clear();
    }
    
    // EN: Pre-defined option sets
    // FR: Ensembles d'options pré-définis
    void addStandardOptions() {
        std::vector<CliOptionDefinition> standard_options = {
            {
                .long_name = "config",
                .short_name = 'c',
                .type = CliOptionType::STRING,
                .description = "EN: Configuration file path / FR: Chemin du fichier de configuration",
                .config_path = "_config_file",
                .default_value = "configs/defaults.yaml",
                .category = "General"
            },
            {
                .long_name = "threads",
                .short_name = 't',
                .type = CliOptionType::INTEGER,
                .description = "EN: Number of threads to use / FR: Nombre de threads à utiliser",
                .config_path = "pipeline.max_threads",
                .default_value = "200",
                .constraint = CliOptionConstraint::POSITIVE,
                .min_value = 1,
                .max_value = 1000,
                .category = "Performance"
            },
            {
                .long_name = "timeout",
                .short_name = 'T',
                .type = CliOptionType::INTEGER,
                .description = "EN: Default timeout in seconds / FR: Timeout par défaut en secondes",
                .config_path = "pipeline.default_timeout",
                .default_value = "30",
                .constraint = CliOptionConstraint::POSITIVE,
                .min_value = 1,
                .max_value = 3600,
                .category = "Network"
            },
            {
                .long_name = "output",
                .short_name = 'o',
                .type = CliOptionType::STRING,
                .description = "EN: Output directory path / FR: Chemin du répertoire de sortie",
                .config_path = "_output_directory",
                .default_value = "out",
                .category = "General"
            },
            {
                .long_name = "dry-run",
                .short_name = 'n',
                .type = CliOptionType::BOOLEAN,
                .description = "EN: Show execution plan without running / FR: Afficher le plan d'exécution sans exécuter",
                .config_path = "_dry_run",
                .default_value = "false",
                .category = "General"
            },
            {
                .long_name = "verbose",
                .short_name = 'v',
                .type = CliOptionType::BOOLEAN,
                .description = "EN: Enable verbose output / FR: Activer la sortie détaillée",
                .config_path = "logging.level",
                .default_value = "info",
                .category = "Logging"
            },
            {
                .long_name = "help",
                .short_name = 'h',
                .type = CliOptionType::BOOLEAN,
                .description = "EN: Show this help message / FR: Afficher ce message d'aide",
                .config_path = "_help",
                .category = "General",
                .hidden = true
            },
            {
                .long_name = "version",
                .short_name = 'V',
                .type = CliOptionType::BOOLEAN,
                .description = "EN: Show version information / FR: Afficher les informations de version",
                .config_path = "_version",
                .category = "General",
                .hidden = true
            }
        };
        
        addOptions(standard_options);
    }
    
    void addLoggingOptions() {
        std::vector<CliOptionDefinition> logging_options = {
            {
                .long_name = "log-level",
                .type = CliOptionType::STRING,
                .description = "EN: Logging level (debug, info, warn, error) / FR: Niveau de journalisation (debug, info, warn, error)",
                .config_path = "logging.level",
                .default_value = "info",
                .constraint = CliOptionConstraint::ENUM_VALUES,
                .enum_values = {"debug", "info", "warn", "error"},
                .category = "Logging"
            },
            {
                .long_name = "log-format",
                .type = CliOptionType::STRING,
                .description = "EN: Log output format (ndjson, text) / FR: Format de sortie des logs (ndjson, text)",
                .config_path = "logging.format",
                .default_value = "ndjson",
                .constraint = CliOptionConstraint::ENUM_VALUES,
                .enum_values = {"ndjson", "text"},
                .category = "Logging"
            },
            {
                .long_name = "log-file",
                .type = CliOptionType::STRING,
                .description = "EN: Log file path / FR: Chemin du fichier de log",
                .config_path = "logging.file",
                .default_value = "bb-pipeline.log",
                .category = "Logging"
            },
            {
                .long_name = "correlation-id",
                .type = CliOptionType::BOOLEAN,
                .description = "EN: Include correlation IDs in logs / FR: Inclure les IDs de corrélation dans les logs",
                .config_path = "logging.correlation_id",
                .default_value = "true",
                .category = "Logging"
            }
        };
        
        addOptions(logging_options);
    }
    
    void addNetworkingOptions() {
        std::vector<CliOptionDefinition> network_options = {
            {
                .long_name = "rate-limit",
                .short_name = 'r',
                .type = CliOptionType::INTEGER,
                .description = "EN: Requests per second rate limit / FR: Limite de requêtes par seconde",
                .config_path = "rate_limit.default_rps",
                .default_value = "10",
                .constraint = CliOptionConstraint::POSITIVE,
                .min_value = 1,
                .max_value = 1000,
                .category = "Network"
            },
            {
                .long_name = "user-agent",
                .short_name = 'u',
                .type = CliOptionType::STRING,
                .description = "EN: HTTP User-Agent string / FR: Chaîne User-Agent HTTP",
                .config_path = "http.user_agent",
                .default_value = "BB-Pipeline/1.0.0 (https://github.com/bb-pipeline/bb-pipeline)",
                .category = "Network"
            },
            {
                .long_name = "connect-timeout",
                .type = CliOptionType::INTEGER,
                .description = "EN: Connection timeout in seconds / FR: Timeout de connexion en secondes",
                .config_path = "http.connect_timeout",
                .default_value = "10",
                .constraint = CliOptionConstraint::POSITIVE,
                .min_value = 1,
                .max_value = 300,
                .category = "Network"
            },
            {
                .long_name = "max-redirects",
                .type = CliOptionType::INTEGER,
                .description = "EN: Maximum number of redirects to follow / FR: Nombre maximum de redirections à suivre",
                .config_path = "http.max_redirects",
                .default_value = "3",
                .constraint = CliOptionConstraint::NON_NEGATIVE,
                .min_value = 0,
                .max_value = 20,
                .category = "Network"
            },
            {
                .long_name = "verify-ssl",
                .type = CliOptionType::BOOLEAN,
                .description = "EN: Verify SSL certificates / FR: Vérifier les certificats SSL",
                .config_path = "http.verify_ssl",
                .default_value = "true",
                .category = "Network"
            },
            {
                .long_name = "dns-servers",
                .type = CliOptionType::STRING_LIST,
                .description = "EN: DNS servers to use (comma-separated) / FR: Serveurs DNS à utiliser (séparés par virgule)",
                .config_path = "dns.servers",
                .default_value = "8.8.8.8,1.1.1.1,9.9.9.9",
                .category = "Network"
            }
        };
        
        addOptions(network_options);
    }
    
    void addPerformanceOptions() {
        std::vector<CliOptionDefinition> perf_options = {
            {
                .long_name = "cache-enable",
                .type = CliOptionType::BOOLEAN,
                .description = "EN: Enable response caching / FR: Activer le cache des réponses",
                .config_path = "cache.enabled",
                .default_value = "true",
                .category = "Performance"
            },
            {
                .long_name = "cache-ttl",
                .type = CliOptionType::INTEGER,
                .description = "EN: Cache TTL in seconds / FR: TTL du cache en secondes",
                .config_path = "cache.ttl",
                .default_value = "3600",
                .constraint = CliOptionConstraint::POSITIVE,
                .min_value = 60,
                .max_value = 86400,
                .category = "Performance"
            },
            {
                .long_name = "cache-size",
                .type = CliOptionType::INTEGER,
                .description = "EN: Maximum cache size in MB / FR: Taille maximale du cache en MB",
                .config_path = "cache.max_size_mb",
                .default_value = "100",
                .constraint = CliOptionConstraint::POSITIVE,
                .min_value = 10,
                .max_value = 10000,
                .category = "Performance"
            },
            {
                .long_name = "memory-limit",
                .type = CliOptionType::INTEGER,
                .description = "EN: Memory limit in MB / FR: Limite mémoire en MB",
                .config_path = "_memory_limit_mb",
                .default_value = "2048",
                .constraint = CliOptionConstraint::POSITIVE,
                .min_value = 128,
                .max_value = 32768,
                .category = "Performance"
            }
        };
        
        addOptions(perf_options);
    }
    
    // EN: Main parsing implementation
    // FR: Implémentation d'analyse principale
    CliParseResult parse(const std::vector<std::string>& arguments) {
        auto start_time = std::chrono::steady_clock::now();
        
        CliParseResult result{};
        result.status = CliParseStatus::SUCCESS;  // Initialize status explicitly
        result.total_arguments_processed = arguments.size();
        
        emitEvent(ConfigOverrideEventType::PARSING_STARTED, "", 
                 "Starting CLI parsing with " + std::to_string(arguments.size()) + " arguments");
        
        try {
            // EN: Process each argument
            // FR: Traiter chaque argument
            for (size_t i = 0; i < arguments.size(); ++i) {
                const std::string& arg = arguments[i];
                
                if (arg.empty()) continue;
                
                // EN: Check for help or version flags first
                // FR: Vérifier d'abord les drapeaux d'aide ou de version
                if (arg == "--help" || arg == "-h") {
                    result.status = CliParseStatus::HELP_REQUESTED;
                    result.help_text = generateHelpText();
                    emitEvent(ConfigOverrideEventType::HELP_DISPLAYED, "help", "Help requested");
                    break;
                }
                
                if (arg == "--version" || arg == "-V") {
                    result.status = CliParseStatus::VERSION_REQUESTED;
                    result.version_text = generateVersionText();
                    emitEvent(ConfigOverrideEventType::HELP_DISPLAYED, "version", "Version requested");
                    break;
                }
                
                // EN: Parse option arguments
                // FR: Analyser les arguments d'options
                if (ConfigOverrideUtils::isLongOption(arg) || ConfigOverrideUtils::isShortOption(arg)) {
                    auto parse_option_result = parseOption(arg, arguments, i);
                    
                    if (!parse_option_result.first.option_name.empty()) {
                        result.parsed_options.push_back(parse_option_result.first);
                        result.overrides[parse_option_result.first.config_path] = parse_option_result.first.config_value;
                        result.overrides_applied++;
                        
                        emitEvent(ConfigOverrideEventType::OPTION_PARSED, 
                                parse_option_result.first.option_name,
                                "Parsed option: " + parse_option_result.first.option_name);
                    } else {
                        result.errors.push_back(parse_option_result.second);
                        // Set specific status based on error type
                        if (parse_option_result.second.find("requires a value") != std::string::npos) {
                            result.status = CliParseStatus::MISSING_VALUE;
                        } else {
                            result.status = CliParseStatus::INVALID_OPTION;
                        }
                    }
                    
                    i = parse_option_result.first.raw_values.size() > 1 ? i + 1 : i; // EN: Skip next arg if it was consumed / FR: Ignorer l'argument suivant s'il a été consommé
                } else {
                    // EN: Unknown positional argument
                    // FR: Argument positionnel inconnu
                    result.warnings.push_back("Unknown positional argument: " + arg);
                }
            }
            
            // EN: Set default success status if no special status was set
            // FR: Définir le statut de succès par défaut si aucun statut spécial n'a été défini
            if (result.status == CliParseStatus::SUCCESS && result.errors.empty()) {
                result.status = CliParseStatus::SUCCESS;
            } else if (!result.errors.empty() && result.status == CliParseStatus::SUCCESS) {
                result.status = CliParseStatus::INVALID_OPTION;
            }
            
        } catch (const std::exception& e) {
            result.errors.push_back("Parsing error: " + std::string(e.what()));
            result.status = CliParseStatus::INVALID_OPTION;
            emitEvent(ConfigOverrideEventType::ERROR_OCCURRED, "", 
                     "Exception during parsing: " + std::string(e.what()));
        }
        
        auto end_time = std::chrono::steady_clock::now();
        result.parse_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        emitEvent(ConfigOverrideEventType::PARSING_COMPLETED, "", 
                 "CLI parsing completed with status: " + ConfigOverrideUtils::cliParseStatusToString(result.status));
        
        return result;
    }
    
    // EN: Helper method to parse individual options
    // FR: Méthode d'aide pour analyser les options individuelles
    std::pair<CliOptionValue, std::string> parseOption(const std::string& arg, 
                                                       const std::vector<std::string>& all_args, 
                                                       size_t current_index) {
        CliOptionValue option_value;
        std::string error_message;
        
        std::lock_guard<std::mutex> lock(options_mutex_);
        
        // EN: Extract option name
        // FR: Extraire le nom d'option
        std::string option_name = ConfigOverrideUtils::extractOptionName(arg);
        
        // EN: Find option definition
        // FR: Trouver la définition d'option
        CliOptionDefinition* option_def = nullptr;
        
        if (ConfigOverrideUtils::isLongOption(arg)) {
            auto it = options_by_long_name_.find(option_name);
            if (it != options_by_long_name_.end()) {
                option_def = &it->second;
            }
        } else if (ConfigOverrideUtils::isShortOption(arg) && option_name.length() == 1) {
            auto it = options_by_short_name_.find(option_name[0]);
            if (it != options_by_short_name_.end()) {
                option_def = &it->second;
            }
        }
        
        if (!option_def) {
            error_message = "Unknown option: " + arg;
            return {option_value, error_message};
        }
        
        // EN: Set basic option information
        // FR: Définir les informations d'option de base
        option_value.option_name = option_def->long_name;
        option_value.type = option_def->type;
        option_value.config_path = option_def->config_path;
        
        // EN: Handle boolean options (flags)
        // FR: Gérer les options booléennes (drapeaux)
        if (option_def->type == CliOptionType::BOOLEAN) {
            option_value.raw_values = {"true"};
            option_value.config_value = ConfigValue(true);
            return {option_value, ""};
        }
        
        // EN: Handle options that require values
        // FR: Gérer les options qui nécessitent des valeurs
        if (current_index + 1 >= all_args.size()) {
            error_message = "Option " + arg + " requires a value";
            return {option_value, error_message};
        }
        
        std::string value = all_args[current_index + 1];
        
        // EN: Validate and convert value
        // FR: Valider et convertir la valeur
        std::string validation_error;
        if (!ConfigOverrideUtils::validateCliValue(value, option_def->type, *option_def, validation_error)) {
            error_message = "Invalid value for option " + arg + ": " + validation_error;
            return {option_value, error_message};
        }
        
        option_value.raw_values = {value};
        option_value.config_value = ConfigOverrideUtils::parseCliValue(value, option_def->type);
        
        return {option_value, ""};
    }
    
    // EN: Help text generation
    // FR: Génération de texte d'aide
    std::string generateHelpText() const {
        std::ostringstream help;
        
        help << help_header_ << "\n\n";
        help << "Usage: bbpctl [OPTIONS] COMMAND\n\n";
        
        // EN: Group options by category
        // FR: Grouper les options par catégorie
        std::map<std::string, std::vector<CliOptionDefinition>> options_by_category;
        
        std::lock_guard<std::mutex> lock(options_mutex_);
        for (const auto& opt : option_definitions_) {
            if (!opt.hidden) {
                options_by_category[opt.category].push_back(opt);
            }
        }
        
        // EN: Generate help for each category
        // FR: Générer l'aide pour chaque catégorie
        for (const auto& [category, options] : options_by_category) {
            help << category << " Options:\n";
            
            for (const auto& opt : options) {
                help << ConfigOverrideUtils::formatOptionHelp(opt, 80) << "\n";
            }
            
            help << "\n";
        }
        
        help << help_footer_ << "\n";
        
        return help.str();
    }
    
    std::string generateVersionText() const {
        std::ostringstream version;
        version << "BB-Pipeline " << version_;
        if (!build_info_.empty()) {
            version << " (" << build_info_ << ")";
        }
        version << "\n";
        return version.str();
    }
    
    // EN: Event emission helper
    // FR: Assistant d'émission d'événements
    void emitEvent(ConfigOverrideEventType type, const std::string& option_name, const std::string& message) {
        if (!event_callback_) return;
        
        ConfigOverrideEvent event;
        event.type = type;
        event.timestamp = std::chrono::system_clock::now();
        event.operation_id = generateOperationId();
        event.option_name = option_name;
        event.message = message;
        event.success = (type != ConfigOverrideEventType::ERROR_OCCURRED);
        
        event_callback_(event);
    }
    
    std::string generateOperationId() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(100000, 999999);
        return "CLI_" + std::to_string(dis(gen));
    }
    
    // EN: Member variables
    // FR: Variables membres
    mutable std::mutex options_mutex_;
    std::vector<CliOptionDefinition> option_definitions_;
    std::unordered_map<std::string, CliOptionDefinition> options_by_long_name_;
    std::unordered_map<char, CliOptionDefinition> options_by_short_name_;
    
    ConfigOverrideEventCallback event_callback_;
    std::string help_header_;
    std::string help_footer_;
    std::string version_;
    std::string build_info_;
};

// EN: ConfigOverrideParser public interface implementation
// FR: Implémentation de l'interface publique ConfigOverrideParser
ConfigOverrideParser::ConfigOverrideParser(ConfigOverrideEventCallback event_callback) 
    : impl_(std::make_unique<ConfigOverrideParserImpl>(event_callback)) {
}

ConfigOverrideParser::~ConfigOverrideParser() = default;

void ConfigOverrideParser::addOption(const CliOptionDefinition& option_def) {
    impl_->addOption(option_def);
}

void ConfigOverrideParser::addOptions(const std::vector<CliOptionDefinition>& option_defs) {
    impl_->addOptions(option_defs);
}

void ConfigOverrideParser::removeOption(const std::string& long_name) {
    impl_->removeOption(long_name);
}

void ConfigOverrideParser::clearOptions() {
    impl_->clearOptions();
}

void ConfigOverrideParser::addStandardOptions() {
    impl_->addStandardOptions();
}

void ConfigOverrideParser::addLoggingOptions() {
    impl_->addLoggingOptions();
}

void ConfigOverrideParser::addNetworkingOptions() {
    impl_->addNetworkingOptions();
}

void ConfigOverrideParser::addPerformanceOptions() {
    impl_->addPerformanceOptions();
}

CliParseResult ConfigOverrideParser::parse(int argc, char* argv[]) {
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i) { // EN: Skip program name / FR: Ignorer le nom du programme
        arguments.emplace_back(argv[i]);
    }
    return parse(arguments);
}

CliParseResult ConfigOverrideParser::parse(const std::vector<std::string>& arguments) {
    return impl_->parse(arguments);
}

std::string ConfigOverrideParser::generateHelpText(const std::string& program_name) const {
    // EN: Simple implementation - could be enhanced to use program_name parameter
    // FR: Implémentation simple - pourrait être améliorée pour utiliser le paramètre program_name
    (void)program_name;
    return impl_->generateHelpText();
}

std::string ConfigOverrideParser::generateVersionText() const {
    return impl_->generateVersionText();
}

// EN: Utility functions implementation
// FR: Implémentation des fonctions utilitaires
namespace ConfigOverrideUtils {

std::string cliOptionTypeToString(CliOptionType type) {
    switch (type) {
        case CliOptionType::BOOLEAN: return "BOOLEAN";
        case CliOptionType::INTEGER: return "INTEGER";
        case CliOptionType::DOUBLE: return "DOUBLE";
        case CliOptionType::STRING: return "STRING";
        case CliOptionType::STRING_LIST: return "STRING_LIST";
        default: return "UNKNOWN";
    }
}

CliOptionType stringToCliOptionType(const std::string& str) {
    if (str == "BOOLEAN") return CliOptionType::BOOLEAN;
    if (str == "INTEGER") return CliOptionType::INTEGER;
    if (str == "DOUBLE") return CliOptionType::DOUBLE;
    if (str == "STRING") return CliOptionType::STRING;
    if (str == "STRING_LIST") return CliOptionType::STRING_LIST;
    throw std::invalid_argument("Unknown CliOptionType: " + str);
}

std::string cliParseStatusToString(CliParseStatus status) {
    switch (status) {
        case CliParseStatus::SUCCESS: return "SUCCESS";
        case CliParseStatus::HELP_REQUESTED: return "HELP_REQUESTED";
        case CliParseStatus::VERSION_REQUESTED: return "VERSION_REQUESTED";
        case CliParseStatus::INVALID_OPTION: return "INVALID_OPTION";
        case CliParseStatus::MISSING_VALUE: return "MISSING_VALUE";
        case CliParseStatus::INVALID_VALUE: return "INVALID_VALUE";
        case CliParseStatus::CONSTRAINT_VIOLATION: return "CONSTRAINT_VIOLATION";
        case CliParseStatus::CONFIG_FILE_ERROR: return "CONFIG_FILE_ERROR";
        case CliParseStatus::DUPLICATE_OPTION: return "DUPLICATE_OPTION";
        default: return "UNKNOWN";
    }
}

bool isValidConfigPath(const std::string& path) {
    if (path.empty()) return false;
    
    // EN: Check for valid characters and structure
    // FR: Vérifier les caractères et la structure valides
    std::regex valid_path_regex(R"(^[a-zA-Z_][a-zA-Z0-9_]*(\.[a-zA-Z_][a-zA-Z0-9_]*)*$)");
    return std::regex_match(path, valid_path_regex);
}

std::vector<std::string> splitConfigPath(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    
    while (std::getline(ss, part, '.')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    
    return parts;
}

std::string joinConfigPath(const std::vector<std::string>& parts) {
    if (parts.empty()) return "";
    
    std::ostringstream oss;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) oss << ".";
        oss << parts[i];
    }
    
    return oss.str();
}

std::string normalizeConfigPath(const std::string& path) {
    auto parts = splitConfigPath(path);
    return joinConfigPath(parts);
}

ConfigValue parseCliValue(const std::string& raw_value, CliOptionType type) {
    switch (type) {
        case CliOptionType::BOOLEAN: {
            std::string lower = raw_value;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return ConfigValue(lower == "true" || lower == "1" || lower == "yes" || lower == "on");
        }
        
        case CliOptionType::INTEGER: {
            return ConfigValue(std::stoi(raw_value));
        }
        
        case CliOptionType::DOUBLE: {
            return ConfigValue(std::stod(raw_value));
        }
        
        case CliOptionType::STRING: {
            return ConfigValue(raw_value);
        }
        
        case CliOptionType::STRING_LIST: {
            std::vector<std::string> values;
            std::stringstream ss(raw_value);
            std::string item;
            
            while (std::getline(ss, item, ',')) {
                // EN: Trim whitespace
                // FR: Supprimer les espaces
                item.erase(0, item.find_first_not_of(" \t"));
                item.erase(item.find_last_not_of(" \t") + 1);
                if (!item.empty()) {
                    values.push_back(item);
                }
            }
            
            return ConfigValue(values);
        }
        
        default:
            throw std::invalid_argument("Unknown CliOptionType");
    }
}

bool validateCliValue(const std::string& raw_value, CliOptionType type, 
                     const CliOptionDefinition& definition, std::string& error_message) {
    try {
        switch (type) {
            case CliOptionType::BOOLEAN: {
                std::string lower = raw_value;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (lower != "true" && lower != "false" && lower != "1" && lower != "0" && 
                    lower != "yes" && lower != "no" && lower != "on" && lower != "off") {
                    error_message = "Boolean value must be true/false, 1/0, yes/no, or on/off";
                    return false;
                }
                break;
            }
            
            case CliOptionType::INTEGER: {
                int value = std::stoi(raw_value);
                if (definition.constraint == CliOptionConstraint::POSITIVE && value <= 0) {
                    error_message = "Value must be positive";
                    return false;
                }
                if (definition.constraint == CliOptionConstraint::NON_NEGATIVE && value < 0) {
                    error_message = "Value must be non-negative";
                    return false;
                }
                if (definition.constraint == CliOptionConstraint::RANGE) {
                    if (definition.min_value && value < *definition.min_value) {
                        error_message = "Value must be >= " + std::to_string(*definition.min_value);
                        return false;
                    }
                    if (definition.max_value && value > *definition.max_value) {
                        error_message = "Value must be <= " + std::to_string(*definition.max_value);
                        return false;
                    }
                }
                break;
            }
            
            case CliOptionType::DOUBLE: {
                double value = std::stod(raw_value);
                if (definition.constraint == CliOptionConstraint::POSITIVE && value <= 0.0) {
                    error_message = "Value must be positive";
                    return false;
                }
                if (definition.constraint == CliOptionConstraint::NON_NEGATIVE && value < 0.0) {
                    error_message = "Value must be non-negative";
                    return false;
                }
                if (definition.constraint == CliOptionConstraint::RANGE) {
                    if (definition.min_value && value < *definition.min_value) {
                        error_message = "Value must be >= " + std::to_string(*definition.min_value);
                        return false;
                    }
                    if (definition.max_value && value > *definition.max_value) {
                        error_message = "Value must be <= " + std::to_string(*definition.max_value);
                        return false;
                    }
                }
                break;
            }
            
            case CliOptionType::STRING: {
                if (definition.constraint == CliOptionConstraint::ENUM_VALUES) {
                    if (definition.enum_values.find(raw_value) == definition.enum_values.end()) {
                        error_message = "Value must be one of: ";
                        bool first = true;
                        for (const auto& valid_value : definition.enum_values) {
                            if (!first) error_message += ", ";
                            error_message += valid_value;
                            first = false;
                        }
                        return false;
                    }
                }
                if (definition.constraint == CliOptionConstraint::REGEX_MATCH && definition.regex_pattern) {
                    std::regex pattern(*definition.regex_pattern);
                    if (!std::regex_match(raw_value, pattern)) {
                        error_message = "Value does not match required pattern";
                        return false;
                    }
                }
                break;
            }
            
            case CliOptionType::STRING_LIST: {
                // EN: For string lists, validate each item separately
                // FR: Pour les listes de chaînes, valider chaque élément séparément
                std::vector<std::string> values;
                std::stringstream ss(raw_value);
                std::string item;
                
                while (std::getline(ss, item, ',')) {
                    item.erase(0, item.find_first_not_of(" \t"));
                    item.erase(item.find_last_not_of(" \t") + 1);
                    if (!item.empty()) {
                        values.push_back(item);
                    }
                }
                
                if (values.empty()) {
                    error_message = "String list cannot be empty";
                    return false;
                }
                break;
            }
        }
        
    } catch (const std::exception& e) {
        error_message = "Invalid format: " + std::string(e.what());
        return false;
    }
    
    return true;
}

std::string formatOptionHelp(const CliOptionDefinition& option, size_t /*max_width*/) {
    std::ostringstream help;
    
    // EN: Format option name(s)
    // FR: Formater le(s) nom(s) d'option
    std::string option_names = "  ";
    if (option.short_name) {
        option_names += "-" + std::string(1, *option.short_name) + ", ";
    }
    option_names += "--" + option.long_name;
    
    // EN: Add value type hint
    // FR: Ajouter l'indication de type de valeur
    if (option.type != CliOptionType::BOOLEAN) {
        option_names += " <" + cliOptionTypeToString(option.type) + ">";
    }
    
    // EN: Format description
    // FR: Formater la description
    size_t name_width = 30; // EN: Fixed width for option names / FR: Largeur fixe pour les noms d'options
    if (option_names.length() > name_width - 2) {
        help << option_names << "\n" << std::string(name_width, ' ') << option.description;
    } else {
        help << std::left << std::setw(name_width) << option_names << option.description;
    }
    
    // EN: Add default value if available
    // FR: Ajouter la valeur par défaut si disponible
    if (option.default_value && !option.default_value->empty()) {
        help << " (default: " << *option.default_value << ")";
    }
    
    return help.str();
}

bool isShortOption(const std::string& arg) {
    return arg.length() >= 2 && arg[0] == '-' && arg[1] != '-' && std::isalpha(arg[1]);
}

bool isLongOption(const std::string& arg) {
    return arg.length() >= 3 && arg.substr(0, 2) == "--" && std::isalpha(arg[2]);
}

std::string extractOptionName(const std::string& arg) {
    if (isLongOption(arg)) {
        return arg.substr(2); // EN: Remove -- prefix / FR: Supprimer le préfixe --
    } else if (isShortOption(arg)) {
        return arg.substr(1, 1); // EN: Get single character after - / FR: Obtenir le caractère unique après -
    }
    return "";
}

} // namespace ConfigOverrideUtils

// EN: ConfigOverrideValidator Implementation
// FR: Implémentation ConfigOverrideValidator
class ConfigOverrideValidator::ConfigOverrideValidatorImpl {
public:
    ConfigOverrideValidatorImpl() {
        // EN: Set up default validation rules
        // FR: Configurer les règles de validation par défaut
        setupDefaultValidationRules();
    }
    
    ConfigOverrideValidationResult validateOverrides(
        const std::unordered_map<std::string, ConfigValue>& overrides,
        const ConfigManager* /*base_config*/) const {
        
        ConfigOverrideValidationResult result;
        
        for (const auto& [path, value] : overrides) {
            // EN: Validate configuration path
            // FR: Valider le chemin de configuration
            if (!ConfigOverrideUtils::isValidConfigPath(path)) {
                result.errors.push_back("Invalid configuration path: " + path);
                result.is_valid = false;
                continue;
            }
            
            // EN: Check for deprecated paths
            // FR: Vérifier les chemins obsolètes
            if (isDeprecatedPath(path)) {
                result.deprecated_paths.insert(path);
                result.warnings.push_back("Configuration path is deprecated: " + path);
                
                auto replacement = getReplacementPath(path);
                if (replacement) {
                    result.warnings.push_back("Consider using: " + *replacement);
                }
            }
            
            // EN: Apply custom validation rules
            // FR: Appliquer les règles de validation personnalisées
            for (const auto& [pattern, rule] : validation_rules_) {
                std::regex rule_regex(pattern);
                if (std::regex_match(path, rule_regex)) {
                    std::string error_message;
                    if (!rule(path, value, error_message)) {
                        result.errors.push_back("Validation failed for " + path + ": " + error_message);
                        result.is_valid = false;
                    }
                }
            }
        }
        
        // EN: Check for conflicting overrides
        // FR: Vérifier les surcharges conflictuelles
        detectConflictingOverrides(overrides, result);
        
        return result;
    }
    
    void addValidationRule(const std::string& config_path_pattern, 
                          std::function<bool(const std::string&, const ConfigValue&, std::string&)> rule) {
        std::lock_guard<std::mutex> lock(rules_mutex_);
        validation_rules_[config_path_pattern] = rule;
    }
    
    void removeValidationRule(const std::string& config_path_pattern) {
        std::lock_guard<std::mutex> lock(rules_mutex_);
        validation_rules_.erase(config_path_pattern);
    }
    
    void clearValidationRules() {
        std::lock_guard<std::mutex> lock(rules_mutex_);
        validation_rules_.clear();
    }
    
    void addDeprecatedPath(const std::string& old_path, const std::string& new_path) {
        std::lock_guard<std::mutex> lock(paths_mutex_);
        deprecated_paths_[old_path] = new_path;
    }
    
    bool isDeprecatedPath(const std::string& path) const {
        std::lock_guard<std::mutex> lock(paths_mutex_);
        return deprecated_paths_.find(path) != deprecated_paths_.end();
    }
    
    std::optional<std::string> getReplacementPath(const std::string& deprecated_path) const {
        std::lock_guard<std::mutex> lock(paths_mutex_);
        auto it = deprecated_paths_.find(deprecated_path);
        if (it != deprecated_paths_.end() && !it->second.empty()) {
            return it->second;
        }
        return std::nullopt;
    }
    
private:
    void setupDefaultValidationRules() {
        // EN: Add common validation rules
        // FR: Ajouter des règles de validation communes
        
        // EN: Thread count validation
        // FR: Validation du nombre de threads
        addValidationRule(R"(.*\.threads|.*\.max_threads)", 
            [](const std::string&, const ConfigValue& value, std::string& error) {
                try {
                    int threads = value.as<int>();
                    if (threads <= 0 || threads > 10000) {
                        error = "Thread count must be between 1 and 10000";
                        return false;
                    }
                } catch (...) {
                    error = "Invalid thread count value";
                    return false;
                }
                return true;
            });
        
        // EN: Timeout validation
        // FR: Validation des timeouts
        addValidationRule(R"(.*\.timeout)", 
            [](const std::string&, const ConfigValue& value, std::string& error) {
                try {
                    int timeout = value.as<int>();
                    if (timeout <= 0 || timeout > 86400) {
                        error = "Timeout must be between 1 and 86400 seconds";
                        return false;
                    }
                } catch (...) {
                    error = "Invalid timeout value";
                    return false;
                }
                return true;
            });
        
        // EN: Port validation
        // FR: Validation des ports
        addValidationRule(R"(.*\.port)", 
            [](const std::string&, const ConfigValue& value, std::string& error) {
                try {
                    int port = value.as<int>();
                    if (port < 1 || port > 65535) {
                        error = "Port must be between 1 and 65535";
                        return false;
                    }
                } catch (...) {
                    error = "Invalid port value";
                    return false;
                }
                return true;
            });
        
        // EN: URL validation
        // FR: Validation des URLs
        addValidationRule(R"(.*\.url|.*\.endpoint)", 
            [](const std::string&, const ConfigValue& value, std::string& error) {
                try {
                    std::string url = value.as<std::string>();
                    if (url.empty() || (url.find("http://") != 0 && url.find("https://") != 0)) {
                        error = "URL must start with http:// or https://";
                        return false;
                    }
                } catch (...) {
                    error = "Invalid URL value";
                    return false;
                }
                return true;
            });
    }
    
    void detectConflictingOverrides(const std::unordered_map<std::string, ConfigValue>& overrides,
                                   ConfigOverrideValidationResult& result) const {
        // EN: Check for common conflicting patterns
        // FR: Vérifier les motifs de conflit courants
        
        std::vector<std::pair<std::string, std::string>> conflict_patterns = {
            {"cache.enabled", "cache.ttl"},        // EN: Cache disabled but TTL set / FR: Cache désactivé mais TTL défini
            {"logging.output", "logging.file"}     // EN: Output to stdout but file specified / FR: Sortie vers stdout mais fichier spécifié
        };
        
        for (const auto& [pattern1, pattern2] : conflict_patterns) {
            auto it1 = overrides.find(pattern1);
            auto it2 = overrides.find(pattern2);
            
            if (it1 != overrides.end() && it2 != overrides.end()) {
                // EN: Apply specific conflict logic
                // FR: Appliquer la logique de conflit spécifique
                if (pattern1 == "cache.enabled" && pattern2 == "cache.ttl") {
                    try {
                        bool cache_enabled = it1->second.as<bool>();
                        if (!cache_enabled) {
                            result.conflicting_overrides[pattern1] = pattern2;
                            result.warnings.push_back("Cache TTL specified but cache is disabled");
                        }
                    } catch (...) {
                        // EN: Ignore type conversion errors here
                        // FR: Ignorer les erreurs de conversion de type ici
                    }
                }
            }
        }
    }
    
    mutable std::mutex rules_mutex_;
    mutable std::mutex paths_mutex_;
    
    std::map<std::string, std::function<bool(const std::string&, const ConfigValue&, std::string&)>> validation_rules_;
    std::map<std::string, std::string> deprecated_paths_;
    std::map<std::string, std::string> alternative_paths_;
};

ConfigOverrideValidator::ConfigOverrideValidator() 
    : impl_(std::make_unique<ConfigOverrideValidatorImpl>()) {
}

ConfigOverrideValidator::~ConfigOverrideValidator() = default;

ConfigOverrideValidationResult ConfigOverrideValidator::validateOverrides(
    const std::unordered_map<std::string, ConfigValue>& overrides,
    const ConfigManager* base_config) const {
    return impl_->validateOverrides(overrides, base_config);
}

void ConfigOverrideValidator::addValidationRule(const std::string& config_path_pattern, ValidationRule rule) {
    impl_->addValidationRule(config_path_pattern, rule);
}

void ConfigOverrideValidator::removeValidationRule(const std::string& config_path_pattern) {
    impl_->removeValidationRule(config_path_pattern);
}

void ConfigOverrideValidator::clearValidationRules() {
    impl_->clearValidationRules();
}

void ConfigOverrideValidator::addDeprecatedPath(const std::string& old_path, const std::string& new_path) {
    impl_->addDeprecatedPath(old_path, new_path);
}

bool ConfigOverrideValidator::isValidConfigPath(const std::string& path) const {
    return ConfigOverrideUtils::isValidConfigPath(path);
}

bool ConfigOverrideValidator::isDeprecatedPath(const std::string& path) const {
    return impl_->isDeprecatedPath(path);
}

std::optional<std::string> ConfigOverrideValidator::getReplacementPath(const std::string& deprecated_path) const {
    return impl_->getReplacementPath(deprecated_path);
}

// EN: ConfigOverrideManager Implementation
// FR: Implémentation ConfigOverrideManager
class ConfigOverrideManager::ConfigOverrideManagerImpl {
public:
    ConfigOverrideManagerImpl(std::shared_ptr<ConfigManager> config_manager, 
                             ConfigOverrideEventCallback event_callback)
        : config_manager_(config_manager)
        , parser_(event_callback)
        , validator_()
        , event_callback_(event_callback) {
        
        // EN: Set up parser with all standard options
        // FR: Configurer l'analyseur avec toutes les options standard
        parser_.addStandardOptions();
        parser_.addLoggingOptions();
        parser_.addNetworkingOptions();
        parser_.addPerformanceOptions();
    }
    
    CliParseResult processCliArguments(const std::vector<std::string>& arguments) {
        emitEvent(ConfigOverrideEventType::PARSING_STARTED, "", 
                 "Processing " + std::to_string(arguments.size()) + " CLI arguments");
        
        auto parse_result = parser_.parse(arguments);
        
        if (parse_result.status == CliParseStatus::SUCCESS && !parse_result.overrides.empty()) {
            // EN: Validate overrides
            // FR: Valider les surcharges
            emitEvent(ConfigOverrideEventType::VALIDATION_STARTED, "", "Validating configuration overrides");
            
            auto validation_result = validator_.validateOverrides(parse_result.overrides, config_manager_.get());
            
            if (!validation_result.is_valid) {
                parse_result.status = CliParseStatus::CONSTRAINT_VIOLATION;
                parse_result.errors.insert(parse_result.errors.end(), 
                                         validation_result.errors.begin(), 
                                         validation_result.errors.end());
            }
            
            // EN: Add warnings from validation
            // FR: Ajouter les avertissements de la validation
            parse_result.warnings.insert(parse_result.warnings.end(),
                                        validation_result.warnings.begin(),
                                        validation_result.warnings.end());
            
            emitEvent(ConfigOverrideEventType::VALIDATION_COMPLETED, "", 
                     std::string("Validation completed with status: ") + (validation_result.is_valid ? "valid" : "invalid"));
            
            // EN: Apply overrides if validation passed
            // FR: Appliquer les surcharges si la validation a réussi
            if (validation_result.is_valid) {
                if (applyOverrides(parse_result.overrides)) {
                    updateStatistics(parse_result.overrides);
                } else {
                    parse_result.status = CliParseStatus::CONFIG_FILE_ERROR;
                    parse_result.errors.push_back("Failed to apply configuration overrides");
                }
            }
        }
        
        return parse_result;
    }
    
    bool applyOverrides(const std::unordered_map<std::string, ConfigValue>& overrides) {
        std::lock_guard<std::mutex> lock(overrides_mutex_);
        
        try {
            for (const auto& [path, value] : overrides) {
                // EN: Skip internal CLI options (prefixed with _)
                // FR: Ignorer les options CLI internes (préfixées par _)
                if (path.empty() || path[0] == '_') {
                    continue;
                }
                
                // EN: Apply override to configuration manager
                // FR: Appliquer la surcharge au gestionnaire de configuration
                if (config_manager_) {
                    // EN: This would integrate with the existing ConfigManager
                    // FR: Ceci s'intégrerait avec le ConfigManager existant
                    // config_manager_->setValue(path, value);
                }
                
                current_overrides_[path] = value;
                
                emitEvent(ConfigOverrideEventType::OVERRIDE_APPLIED, "", 
                         "Applied override: " + path);
            }
            
            return true;
            
        } catch (const std::exception& e) {
            emitEvent(ConfigOverrideEventType::ERROR_OCCURRED, "", 
                     "Failed to apply overrides: " + std::string(e.what()));
            return false;
        }
    }
    
    bool applyOverridesFromParseResult(const CliParseResult& parse_result) {
        return applyOverrides(parse_result.overrides);
    }
    
    void clearOverrides() {
        std::lock_guard<std::mutex> lock(overrides_mutex_);
        current_overrides_.clear();
        
        // EN: Reset statistics
        // FR: Réinitialiser les statistiques
        statistics_.active_overrides = 0;
        statistics_.recent_override_paths.clear();
    }
    
    std::unordered_map<std::string, ConfigValue> getCurrentOverrides() const {
        std::lock_guard<std::mutex> lock(overrides_mutex_);
        return current_overrides_;
    }
    
    bool hasOverride(const std::string& config_path) const {
        std::lock_guard<std::mutex> lock(overrides_mutex_);
        return current_overrides_.find(config_path) != current_overrides_.end();
    }
    
    std::optional<ConfigValue> getOverrideValue(const std::string& config_path) const {
        std::lock_guard<std::mutex> lock(overrides_mutex_);
        auto it = current_overrides_.find(config_path);
        if (it != current_overrides_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    bool removeOverride(const std::string& config_path) {
        std::lock_guard<std::mutex> lock(overrides_mutex_);
        auto it = current_overrides_.find(config_path);
        if (it != current_overrides_.end()) {
            current_overrides_.erase(it);
            statistics_.active_overrides = current_overrides_.size();
            return true;
        }
        return false;
    }
    
    ConfigOverrideManager::OverrideStatistics getStatistics() const {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        return statistics_;
    }
    
    void resetStatistics() {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        statistics_ = OverrideStatistics{};
    }
    
private:
    void updateStatistics(const std::unordered_map<std::string, ConfigValue>& overrides) {
        std::lock_guard<std::mutex> lock(statistics_mutex_);
        
        statistics_.total_overrides_applied += overrides.size();
        statistics_.active_overrides = current_overrides_.size();
        statistics_.last_override_time = std::chrono::system_clock::now();
        
        // EN: Update counts by path
        // FR: Mettre à jour les compteurs par chemin
        for (const auto& [path, _] : overrides) {
            statistics_.override_counts_by_path[path]++;
            
            // EN: Add to recent overrides (keep last 10)
            // FR: Ajouter aux surcharges récentes (garder les 10 dernières)
            statistics_.recent_override_paths.push_back(path);
            if (statistics_.recent_override_paths.size() > 10) {
                statistics_.recent_override_paths.erase(statistics_.recent_override_paths.begin());
            }
        }
    }
    
    void emitEvent(ConfigOverrideEventType type, const std::string& option_name, const std::string& message) {
        if (!event_callback_) return;
        
        ConfigOverrideEvent event;
        event.type = type;
        event.timestamp = std::chrono::system_clock::now();
        event.operation_id = "MGR_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        event.option_name = option_name;
        event.message = message;
        event.success = (type != ConfigOverrideEventType::ERROR_OCCURRED);
        
        event_callback_(event);
    }
    
    std::shared_ptr<ConfigManager> config_manager_;
    ConfigOverrideParser parser_;
    ConfigOverrideValidator validator_;
    ConfigOverrideEventCallback event_callback_;
    
    mutable std::mutex overrides_mutex_;
    mutable std::mutex statistics_mutex_;
    
    std::unordered_map<std::string, ConfigValue> current_overrides_;
    OverrideStatistics statistics_;
};

ConfigOverrideManager::ConfigOverrideManager(std::shared_ptr<ConfigManager> config_manager, 
                                            ConfigOverrideEventCallback event_callback)
    : impl_(std::make_unique<ConfigOverrideManagerImpl>(config_manager, event_callback)) {
}

ConfigOverrideManager::~ConfigOverrideManager() = default;

CliParseResult ConfigOverrideManager::processCliArguments(int argc, char* argv[]) {
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; ++i) {
        arguments.emplace_back(argv[i]);
    }
    return processCliArguments(arguments);
}

CliParseResult ConfigOverrideManager::processCliArguments(const std::vector<std::string>& arguments) {
    return impl_->processCliArguments(arguments);
}

bool ConfigOverrideManager::applyOverrides(const std::unordered_map<std::string, ConfigValue>& overrides) {
    return impl_->applyOverrides(overrides);
}

bool ConfigOverrideManager::applyOverridesFromParseResult(const CliParseResult& parse_result) {
    return impl_->applyOverridesFromParseResult(parse_result);
}

void ConfigOverrideManager::clearOverrides() {
    impl_->clearOverrides();
}

std::unordered_map<std::string, ConfigValue> ConfigOverrideManager::getCurrentOverrides() const {
    return impl_->getCurrentOverrides();
}

bool ConfigOverrideManager::hasOverride(const std::string& config_path) const {
    return impl_->hasOverride(config_path);
}

std::optional<ConfigValue> ConfigOverrideManager::getOverrideValue(const std::string& config_path) const {
    return impl_->getOverrideValue(config_path);
}

bool ConfigOverrideManager::removeOverride(const std::string& config_path) {
    return impl_->removeOverride(config_path);
}

ConfigOverrideManager::OverrideStatistics ConfigOverrideManager::getStatistics() const {
    return impl_->getStatistics();
}

void ConfigOverrideManager::resetStatistics() {
    impl_->resetStatistics();
}

} // namespace CLI
} // namespace BBP