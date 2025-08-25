#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <optional>
#include <filesystem>
#include <functional>
#include <thread>
#include <atomic>

// Forward declaration
namespace YAML { class Node; }

namespace BBP {

// EN: Type-safe configuration value wrapper supporting multiple data types.
// FR: Wrapper de valeur de configuration type-safe supportant plusieurs types de données.
class ConfigValue {
public:
    using ValueType = std::variant<bool, int, double, std::string, std::vector<std::string>>;
    
    ConfigValue() = default;
    
    template<typename T>
    ConfigValue(const T& value) : value_(value) {}
    
    // EN: Get value as specific type (throws if type mismatch).
    // FR: Obtient la valeur comme type spécifique (lance exception si type incorrect).
    template<typename T>
    T as() const;
    
    // EN: Try to get value as specific type (returns nullopt if type mismatch).
    // FR: Tente d'obtenir la valeur comme type spécifique (retourne nullopt si type incorrect).
    template<typename T>
    std::optional<T> tryAs() const;
    
    // EN: Get value as specific type or return default if type mismatch.
    // FR: Obtient la valeur comme type spécifique ou retourne défaut si type incorrect.
    template<typename T>
    T asOrDefault(const T& default_value) const;
    
    // EN: Check if value is valid (not empty).
    // FR: Vérifie si la valeur est valide (non vide).
    bool isValid() const { return value_.has_value(); }
    
    // EN: Convert value to string representation.
    // FR: Convertit la valeur en représentation chaîne.
    std::string toString() const;
    
private:
    std::optional<ValueType> value_;
};

// EN: Configuration section containing key-value pairs.
// FR: Section de configuration contenant des paires clé-valeur.
class ConfigSection {
public:
    ConfigSection() = default;
    
    // EN: Set a configuration value.
    // FR: Définit une valeur de configuration.
    void set(const std::string& key, const ConfigValue& value);
    
    // EN: Get a configuration value.
    // FR: Obtient une valeur de configuration.
    ConfigValue get(const std::string& key) const;
    
    // EN: Check if key exists in section.
    // FR: Vérifie si la clé existe dans la section.
    bool has(const std::string& key) const;
    
    // EN: Remove a key from section.
    // FR: Supprime une clé de la section.
    void remove(const std::string& key);
    
    // EN: Get all keys in section.
    // FR: Obtient toutes les clés de la section.
    std::vector<std::string> keys() const;
    
    // EN: Get number of values in section.
    // FR: Obtient le nombre de valeurs dans la section.
    size_t size() const { return values_.size(); }
    
    // EN: Check if section is empty.
    // FR: Vérifie si la section est vide.
    bool empty() const { return values_.empty(); }
    
    // EN: Merge another section into this one.
    // FR: Fusionne une autre section dans celle-ci.
    void merge(const ConfigSection& other, bool overwrite = true);
    
private:
    std::unordered_map<std::string, ConfigValue> values_;
};

// EN: Main configuration manager with YAML parsing, validation, and templates.
// FR: Gestionnaire de configuration principal avec parsing YAML, validation et templates.
class ConfigManager {
public:
    // EN: Validation rule structure for configuration values.
    // FR: Structure de règle de validation pour les valeurs de configuration.
    struct ValidationRule {
        std::string key;
        std::string type; // "bool", "int", "double", "string", "array"
        bool required = false;
        std::optional<std::string> default_value;
        std::optional<double> min_value;
        std::optional<double> max_value;
        std::vector<std::string> allowed_values;
        std::string description;
    };
    
    // EN: Get the singleton instance.
    // FR: Obtient l'instance singleton.
    static ConfigManager& getInstance();
    
    // EN: Load configuration from YAML file.
    // FR: Charge la configuration depuis un fichier YAML.
    bool loadFromFile(const std::string& filename);
    
    // EN: Load configuration from YAML string.
    // FR: Charge la configuration depuis une chaîne YAML.
    bool loadFromString(const std::string& yaml_content);
    
    // EN: Save current configuration to YAML file.
    // FR: Sauvegarde la configuration actuelle vers un fichier YAML.
    bool saveToFile(const std::string& filename) const;
    
    // EN: Load environment variable overrides.
    // FR: Charge les surcharges de variables d'environnement.
    void loadEnvironmentOverrides(const std::string& prefix = "BBP_");
    
    // EN: Add a configuration template.
    // FR: Ajoute un template de configuration.
    void addTemplate(const std::string& name, const ConfigSection& template_config);
    
    // EN: Apply a configuration template.
    // FR: Applique un template de configuration.
    bool applyTemplate(const std::string& template_name);
    
    // EN: Add validation rules for configuration values.
    // FR: Ajoute des règles de validation pour les valeurs de configuration.
    void addValidationRules(const std::vector<ValidationRule>& rules);
    
    // EN: Validate current configuration against rules.
    // FR: Valide la configuration actuelle contre les règles.
    bool validate(std::vector<std::string>& errors) const;
    
    // EN: Get configuration value from default section.
    // FR: Obtient une valeur de configuration de la section par défaut.
    ConfigValue get(const std::string& key) const;
    
    // EN: Get configuration value from specific section.
    // FR: Obtient une valeur de configuration d'une section spécifique.
    ConfigValue get(const std::string& section, const std::string& key) const;
    
    // EN: Set configuration value in default section.
    // FR: Définit une valeur de configuration dans la section par défaut.
    void set(const std::string& key, const ConfigValue& value);
    
    // EN: Set configuration value in specific section.
    // FR: Définit une valeur de configuration dans une section spécifique.
    void set(const std::string& section, const std::string& key, const ConfigValue& value);
    
    // EN: Check if key exists in default section.
    // FR: Vérifie si la clé existe dans la section par défaut.
    bool has(const std::string& key) const;
    
    // EN: Check if key exists in specific section.
    // FR: Vérifie si la clé existe dans une section spécifique.
    bool has(const std::string& section, const std::string& key) const;
    
    // EN: Remove key from default section.
    // FR: Supprime la clé de la section par défaut.
    void remove(const std::string& key);
    
    // EN: Remove key from specific section.
    // FR: Supprime la clé d'une section spécifique.
    void remove(const std::string& section, const std::string& key);
    
    // EN: Get entire configuration section.
    // FR: Obtient une section de configuration entière.
    ConfigSection getSection(const std::string& section) const;
    
    // EN: Set entire configuration section.
    // FR: Définit une section de configuration entière.
    void setSection(const std::string& section, const ConfigSection& config);
    
    // EN: Get list of all section names.
    // FR: Obtient la liste de tous les noms de sections.
    std::vector<std::string> getSectionNames() const;
    
    // EN: Merge another configuration manager into this one.
    // FR: Fusionne un autre gestionnaire de configuration dans celui-ci.
    void merge(const ConfigManager& other, bool overwrite = true);
    
    // EN: Reset all configuration data.
    // FR: Remet à zéro toutes les données de configuration.
    void reset();
    
    // EN: Dump current configuration as string for debugging.
    // FR: Vide la configuration actuelle en chaîne pour débogage.
    std::string dump() const;
    
    // EN: Enable file watching for automatic reload.
    // FR: Active la surveillance de fichier pour rechargement automatique.
    void enableWatching(const std::string& filename);
    
    // EN: Disable file watching.
    // FR: Désactive la surveillance de fichier.
    void disableWatching();
    
    // EN: Check if file watching is enabled.
    // FR: Vérifie si la surveillance de fichier est activée.
    bool isWatching() const { return watching_enabled_; }
    
    // EN: Set callback function for configuration reload events.
    // FR: Définit la fonction callback pour les événements de rechargement de configuration.
    void setReloadCallback(std::function<void(const ConfigManager&)> callback);
    
private:
    ConfigManager() = default;
    ~ConfigManager();
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    // EN: Start file watching thread.
    // FR: Démarre le thread de surveillance de fichier.
    void startWatcher();
    
    // EN: Stop file watching thread.
    // FR: Arrête le thread de surveillance de fichier.
    void stopWatcher();
    
    // EN: Check for file changes in watcher thread.
    // FR: Vérifie les changements de fichier dans le thread de surveillance.
    void checkFileChanges();
    
    // EN: Validate a single configuration value against rule.
    // FR: Valide une valeur de configuration individuelle contre une règle.
    bool validateValue(const std::string& key, const ConfigValue& value, 
                      const ValidationRule& rule, std::string& error) const;
    
    // EN: Expand environment variables in configuration strings.
    // FR: Étend les variables d'environnement dans les chaînes de configuration.
    std::string expandVariables(const std::string& value) const;
    
    // EN: Get environment variable value.
    // FR: Obtient la valeur d'une variable d'environnement.
    std::string getEnvironmentVariable(const std::string& name) const;
    
    // EN: Parse YAML node into ConfigValue.
    // FR: Parse un nœud YAML en ConfigValue.
    ConfigValue parseYamlValue(const YAML::Node& node);
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ConfigSection> sections_;
    std::unordered_map<std::string, ConfigSection> templates_;
    std::vector<ValidationRule> validation_rules_;
    
    // EN: File watching members.
    // FR: Membres de surveillance de fichier.
    bool watching_enabled_ = false;
    std::string watched_file_;
    std::filesystem::file_time_type last_write_time_;
    std::unique_ptr<std::thread> watcher_thread_;
    std::atomic<bool> should_stop_watching_{false};
    std::function<void(const ConfigManager&)> reload_callback_;
};

// Template specializations
template<>
inline bool ConfigValue::as<bool>() const {
    if (!value_) throw std::runtime_error("ConfigValue is empty");
    return std::get<bool>(*value_);
}

template<>
inline int ConfigValue::as<int>() const {
    if (!value_) throw std::runtime_error("ConfigValue is empty");
    return std::get<int>(*value_);
}

template<>
inline double ConfigValue::as<double>() const {
    if (!value_) throw std::runtime_error("ConfigValue is empty");
    return std::get<double>(*value_);
}

template<>
inline std::string ConfigValue::as<std::string>() const {
    if (!value_) throw std::runtime_error("ConfigValue is empty");
    return std::get<std::string>(*value_);
}

template<>
inline std::vector<std::string> ConfigValue::as<std::vector<std::string>>() const {
    if (!value_) throw std::runtime_error("ConfigValue is empty");
    return std::get<std::vector<std::string>>(*value_);
}

#define CONFIG_GET(key) BBP::ConfigManager::getInstance().get(key)
#define CONFIG_GET_SECTION(section, key) BBP::ConfigManager::getInstance().get(section, key)
#define CONFIG_SET(key, value) BBP::ConfigManager::getInstance().set(key, BBP::ConfigValue(value))
#define CONFIG_SET_SECTION(section, key, value) BBP::ConfigManager::getInstance().set(section, key, BBP::ConfigValue(value))

} // namespace BBP