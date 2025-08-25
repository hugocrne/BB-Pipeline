// EN: Implementation of the ConfigManager class. Provides YAML configuration parsing, validation, and templates.
// FR: Implémentation de la classe ConfigManager. Fournit le parsing de configuration YAML, validation et templates.

#include "../../../include/infrastructure/config/config_manager.hpp"
#include "../../../include/infrastructure/logging/logger.hpp"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <regex>
#include <cstdlib>

namespace BBP {

// EN: ConfigValue template implementation for type conversion.
// FR: Implémentation de template ConfigValue pour la conversion de types.
template<typename T>
T ConfigValue::as() const {
    if (!value_) {
        throw std::runtime_error("ConfigValue is empty");
    }
    try {
        return std::get<T>(*value_);
    } catch (const std::bad_variant_access& e) {
        throw std::runtime_error("ConfigValue type mismatch");
    }
}

template<typename T>
std::optional<T> ConfigValue::tryAs() const {
    if (!value_) {
        return std::nullopt;
    }
    try {
        return std::get<T>(*value_);
    } catch (const std::bad_variant_access&) {
        return std::nullopt;
    }
}

template<typename T>
T ConfigValue::asOrDefault(const T& default_value) const {
    auto result = tryAs<T>();
    return result ? *result : default_value;
}

std::string ConfigValue::toString() const {
    if (!value_) {
        return "<empty>";
    }
    
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
            std::string result = "[";
            for (size_t i = 0; i < v.size(); ++i) {
                if (i > 0) result += ", ";
                result += v[i];
            }
            result += "]";
            return result;
        }
        return "<unknown>";
    }, *value_);
}

// ConfigSection implementation
void ConfigSection::set(const std::string& key, const ConfigValue& value) {
    values_[key] = value;
}

ConfigValue ConfigSection::get(const std::string& key) const {
    auto it = values_.find(key);
    return it != values_.end() ? it->second : ConfigValue();
}

bool ConfigSection::has(const std::string& key) const {
    return values_.find(key) != values_.end();
}

void ConfigSection::remove(const std::string& key) {
    values_.erase(key);
}

std::vector<std::string> ConfigSection::keys() const {
    std::vector<std::string> result;
    for (const auto& [key, _] : values_) {
        result.push_back(key);
    }
    return result;
}

void ConfigSection::merge(const ConfigSection& other, bool overwrite) {
    for (const auto& [key, value] : other.values_) {
        if (overwrite || !has(key)) {
            set(key, value);
        }
    }
}

// ConfigManager implementation
// EN: Get the singleton configuration manager instance.
// FR: Obtient l'instance singleton du gestionnaire de configuration.
ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

// EN: Destructor stops file watching thread.
// FR: Le destructeur arrête le thread de surveillance de fichier.
ConfigManager::~ConfigManager() {
    stopWatcher();
}

// EN: Load configuration from YAML file with error handling.
// FR: Charge la configuration depuis un fichier YAML avec gestion d'erreur.
bool ConfigManager::loadFromFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (!std::filesystem::exists(filename)) {
            LOG_ERROR("config", "Configuration file not found: " + filename);
            return false;
        }
        
        YAML::Node yaml = YAML::LoadFile(filename);
        
        // EN: Clear existing configuration before loading new one.
        // FR: Efface la configuration existante avant de charger la nouvelle.
        sections_.clear();
        
        // EN: Process YAML nodes and convert to ConfigSections.
        // FR: Traite les nœuds YAML et les convertit en ConfigSections.
        for (const auto& section : yaml) {
            std::string section_name = section.first.as<std::string>();
            ConfigSection config_section;
            
            if (section.second.IsMap()) {
                for (const auto& item : section.second) {
                    std::string key = item.first.as<std::string>();
                    ConfigValue value = parseYamlValue(item.second);
                    config_section.set(key, value);
                }
            } else {
                ConfigValue value = parseYamlValue(section.second);
                config_section.set("value", value);
            }
            
            sections_[section_name] = config_section;
        }
        
        // EN: Store file path for potential file watching.
        // FR: Stocke le chemin du fichier pour une surveillance potentielle.
        watched_file_ = filename;
        if (std::filesystem::exists(filename)) {
            last_write_time_ = std::filesystem::last_write_time(filename);
        }
        
        LOG_INFO("config", "Configuration loaded from: " + filename);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("config", "Failed to load configuration: " + std::string(e.what()));
        return false;
    }
}

ConfigValue ConfigManager::parseYamlValue(const YAML::Node& node) {
    if (node.IsScalar()) {
        try {
            // Try to parse as bool first
            if (node.as<std::string>() == "true" || node.as<std::string>() == "false") {
                return ConfigValue(node.as<bool>());
            }
            
            // Try to parse as int
            std::string str_val = node.as<std::string>();
            if (str_val.find('.') == std::string::npos) {
                try {
                    int int_val = node.as<int>();
                    return ConfigValue(int_val);
                } catch (...) {
                    // Not an int, continue
                }
            }
            
            // Try to parse as double
            try {
                double double_val = node.as<double>();
                return ConfigValue(double_val);
            } catch (...) {
                // Not a double, treat as string
            }
            
            // Default to string
            std::string str_value = node.as<std::string>();
            return ConfigValue(expandVariables(str_value));
            
        } catch (const std::exception& e) {
            return ConfigValue(node.as<std::string>());
        }
    } else if (node.IsSequence()) {
        std::vector<std::string> array_value;
        for (const auto& item : node) {
            array_value.push_back(item.as<std::string>());
        }
        return ConfigValue(array_value);
    }
    
    return ConfigValue(node.as<std::string>());
}

bool ConfigManager::loadFromString(const std::string& yaml_content) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        YAML::Node yaml = YAML::Load(yaml_content);
        
        sections_.clear();
        
        for (const auto& section : yaml) {
            std::string section_name = section.first.as<std::string>();
            ConfigSection config_section;
            
            if (section.second.IsMap()) {
                for (const auto& item : section.second) {
                    std::string key = item.first.as<std::string>();
                    ConfigValue value = parseYamlValue(item.second);
                    config_section.set(key, value);
                }
            }
            
            sections_[section_name] = config_section;
        }
        
        LOG_INFO("config", "Configuration loaded from string");
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("config", "Failed to load configuration from string: " + std::string(e.what()));
        return false;
    }
}

bool ConfigManager::saveToFile(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        YAML::Emitter emitter;
        emitter << YAML::BeginMap;
        
        for (const auto& [section_name, section] : sections_) {
            emitter << YAML::Key << section_name;
            emitter << YAML::Value << YAML::BeginMap;
            
            for (const std::string& key : section.keys()) {
                ConfigValue value = section.get(key);
                emitter << YAML::Key << key;
                emitter << YAML::Value;
                
                if (auto bool_val = value.tryAs<bool>()) {
                    emitter << *bool_val;
                } else if (auto int_val = value.tryAs<int>()) {
                    emitter << *int_val;
                } else if (auto double_val = value.tryAs<double>()) {
                    emitter << *double_val;
                } else if (auto str_val = value.tryAs<std::string>()) {
                    emitter << *str_val;
                } else if (auto array_val = value.tryAs<std::vector<std::string>>()) {
                    emitter << YAML::BeginSeq;
                    for (const auto& item : *array_val) {
                        emitter << item;
                    }
                    emitter << YAML::EndSeq;
                }
            }
            
            emitter << YAML::EndMap;
        }
        
        emitter << YAML::EndMap;
        
        std::ofstream file(filename);
        file << emitter.c_str();
        file.close();
        
        LOG_INFO("config", "Configuration saved to: " + filename);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR("config", "Failed to save configuration: " + std::string(e.what()));
        return false;
    }
}

void ConfigManager::loadEnvironmentOverrides(const std::string& prefix) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // This is a simplified version - in a real implementation you'd iterate through environment variables
    LOG_INFO("config", "Loading environment overrides with prefix: " + prefix);
    
    // Example: BBP_DATABASE_HOST would override database.host
    char* env_value = getenv((prefix + "DATABASE_HOST").c_str());
    if (env_value) {
        set("database", "host", ConfigValue(std::string(env_value)));
        LOG_INFO("config", "Environment override applied: database.host");
    }
    
    env_value = getenv((prefix + "RATE_LIMIT").c_str());
    if (env_value) {
        set("rate_limit", "default", ConfigValue(std::stoi(env_value)));
        LOG_INFO("config", "Environment override applied: rate_limit.default");
    }
}

void ConfigManager::addTemplate(const std::string& name, const ConfigSection& template_config) {
    std::lock_guard<std::mutex> lock(mutex_);
    templates_[name] = template_config;
    LOG_INFO("config", "Template added: " + name);
}

bool ConfigManager::applyTemplate(const std::string& template_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = templates_.find(template_name);
    if (it == templates_.end()) {
        LOG_ERROR("config", "Template not found: " + template_name);
        return false;
    }
    
    // Apply template to all sections
    for (auto& [section_name, section] : sections_) {
        section.merge(it->second, false); // Don't overwrite existing values
    }
    
    LOG_INFO("config", "Template applied: " + template_name);
    return true;
}

void ConfigManager::addValidationRules(const std::vector<ValidationRule>& rules) {
    std::lock_guard<std::mutex> lock(mutex_);
    validation_rules_.insert(validation_rules_.end(), rules.begin(), rules.end());
    LOG_INFO("config", "Added " + std::to_string(rules.size()) + " validation rules");
}

bool ConfigManager::validate(std::vector<std::string>& errors) const {
    std::lock_guard<std::mutex> lock(mutex_);
    errors.clear();
    
    for (const auto& rule : validation_rules_) {
        // Parse key to get section and key
        size_t dot_pos = rule.key.find('.');
        std::string section_name = (dot_pos != std::string::npos) ? 
            rule.key.substr(0, dot_pos) : "default";
        std::string key_name = (dot_pos != std::string::npos) ? 
            rule.key.substr(dot_pos + 1) : rule.key;
        
        ConfigValue value = get(section_name, key_name);
        
        if (!value.isValid()) {
            if (rule.required) {
                errors.push_back("Required configuration missing: " + rule.key);
            }
            continue;
        }
        
        std::string error;
        if (!validateValue(rule.key, value, rule, error)) {
            errors.push_back(error);
        }
    }
    
    return errors.empty();
}

ConfigValue ConfigManager::get(const std::string& key) const {
    return get("default", key);
}

ConfigValue ConfigManager::get(const std::string& section, const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto section_it = sections_.find(section);
    if (section_it != sections_.end()) {
        return section_it->second.get(key);
    }
    
    return ConfigValue();
}

void ConfigManager::set(const std::string& key, const ConfigValue& value) {
    set("default", key, value);
}

void ConfigManager::set(const std::string& section, const std::string& key, const ConfigValue& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    sections_[section].set(key, value);
}

bool ConfigManager::has(const std::string& key) const {
    return has("default", key);
}

bool ConfigManager::has(const std::string& section, const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto section_it = sections_.find(section);
    if (section_it != sections_.end()) {
        return section_it->second.has(key);
    }
    
    return false;
}

void ConfigManager::remove(const std::string& key) {
    remove("default", key);
}

void ConfigManager::remove(const std::string& section, const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto section_it = sections_.find(section);
    if (section_it != sections_.end()) {
        section_it->second.remove(key);
    }
}

ConfigSection ConfigManager::getSection(const std::string& section) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = sections_.find(section);
    return it != sections_.end() ? it->second : ConfigSection();
}

void ConfigManager::setSection(const std::string& section, const ConfigSection& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    sections_[section] = config;
}

std::vector<std::string> ConfigManager::getSectionNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> names;
    for (const auto& [name, _] : sections_) {
        names.push_back(name);
    }
    return names;
}

void ConfigManager::merge(const ConfigManager& other, bool overwrite) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::lock_guard<std::mutex> other_lock(other.mutex_);
    
    for (const auto& [section_name, section] : other.sections_) {
        if (overwrite || sections_.find(section_name) == sections_.end()) {
            sections_[section_name] = section;
        } else {
            sections_[section_name].merge(section, overwrite);
        }
    }
}

void ConfigManager::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    sections_.clear();
    templates_.clear();
    validation_rules_.clear();
    stopWatcher();
}

std::string ConfigManager::dump() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    for (const auto& [section_name, section] : sections_) {
        oss << "[" << section_name << "]\n";
        for (const std::string& key : section.keys()) {
            ConfigValue value = section.get(key);
            oss << "  " << key << " = " << value.toString() << "\n";
        }
        oss << "\n";
    }
    return oss.str();
}

void ConfigManager::enableWatching(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (watching_enabled_) {
        stopWatcher();
    }
    
    watched_file_ = filename;
    watching_enabled_ = true;
    
    if (std::filesystem::exists(filename)) {
        last_write_time_ = std::filesystem::last_write_time(filename);
    }
    
    startWatcher();
    LOG_INFO("config", "File watching enabled for: " + filename);
}

void ConfigManager::disableWatching() {
    std::lock_guard<std::mutex> lock(mutex_);
    stopWatcher();
    watching_enabled_ = false;
    LOG_INFO("config", "File watching disabled");
}

void ConfigManager::setReloadCallback(std::function<void(const ConfigManager&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    reload_callback_ = callback;
}

void ConfigManager::startWatcher() {
    should_stop_watching_ = false;
    watcher_thread_ = std::make_unique<std::thread>([this]() {
        checkFileChanges();
    });
}

void ConfigManager::stopWatcher() {
    if (watcher_thread_) {
        should_stop_watching_ = true;
        watcher_thread_->join();
        watcher_thread_.reset();
    }
}

void ConfigManager::checkFileChanges() {
    while (!should_stop_watching_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (watched_file_.empty() || !std::filesystem::exists(watched_file_)) {
            continue;
        }
        
        try {
            auto current_time = std::filesystem::last_write_time(watched_file_);
            if (current_time != last_write_time_) {
                last_write_time_ = current_time;
                
                LOG_INFO("config", "Configuration file changed, reloading: " + watched_file_);
                
                if (loadFromFile(watched_file_)) {
                    if (reload_callback_) {
                        reload_callback_(*this);
                    }
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("config", "Error checking file changes: " + std::string(e.what()));
        }
    }
}

bool ConfigManager::validateValue(const std::string& key, const ConfigValue& value, 
                                 const ValidationRule& rule, std::string& error) const {
    // Type validation
    if (rule.type == "bool" && !value.tryAs<bool>()) {
        error = "Configuration " + key + " must be a boolean";
        return false;
    } else if (rule.type == "int" && !value.tryAs<int>()) {
        error = "Configuration " + key + " must be an integer";
        return false;
    } else if (rule.type == "double" && !value.tryAs<double>()) {
        error = "Configuration " + key + " must be a number";
        return false;
    } else if (rule.type == "string" && !value.tryAs<std::string>()) {
        error = "Configuration " + key + " must be a string";
        return false;
    } else if (rule.type == "array" && !value.tryAs<std::vector<std::string>>()) {
        error = "Configuration " + key + " must be an array";
        return false;
    }
    
    // Range validation for numeric types
    if ((rule.type == "int" || rule.type == "double") && 
        (rule.min_value || rule.max_value)) {
        double numeric_value = 0.0;
        if (auto int_val = value.tryAs<int>()) {
            numeric_value = static_cast<double>(*int_val);
        } else if (auto double_val = value.tryAs<double>()) {
            numeric_value = *double_val;
        }
        
        if (rule.min_value && numeric_value < *rule.min_value) {
            error = "Configuration " + key + " must be >= " + std::to_string(*rule.min_value);
            return false;
        }
        if (rule.max_value && numeric_value > *rule.max_value) {
            error = "Configuration " + key + " must be <= " + std::to_string(*rule.max_value);
            return false;
        }
    }
    
    // Allowed values validation
    if (!rule.allowed_values.empty()) {
        std::string str_value = value.toString();
        bool found = false;
        for (const auto& allowed : rule.allowed_values) {
            if (str_value == allowed) {
                found = true;
                break;
            }
        }
        if (!found) {
            error = "Configuration " + key + " must be one of: ";
            for (size_t i = 0; i < rule.allowed_values.size(); ++i) {
                if (i > 0) error += ", ";
                error += rule.allowed_values[i];
            }
            return false;
        }
    }
    
    return true;
}

std::string ConfigManager::expandVariables(const std::string& value) const {
    std::string result = value;
    std::regex var_regex(R"(\$\{([^}]+)\})");
    std::smatch match;
    
    while (std::regex_search(result, match, var_regex)) {
        std::string var_name = match[1].str();
        std::string var_value = getEnvironmentVariable(var_name);
        
        if (!var_value.empty()) {
            result.replace(match.position(), match.length(), var_value);
        } else {
            // Leave variable as-is if not found
            break;
        }
    }
    
    return result;
}

std::string ConfigManager::getEnvironmentVariable(const std::string& name) const {
    const char* env_value = getenv(name.c_str());
    return env_value ? std::string(env_value) : "";
}

// Explicit template instantiations for non-specialized methods only
template std::optional<bool> ConfigValue::tryAs<bool>() const;
template std::optional<int> ConfigValue::tryAs<int>() const;
template std::optional<double> ConfigValue::tryAs<double>() const;
template std::optional<std::string> ConfigValue::tryAs<std::string>() const;
template std::optional<std::vector<std::string>> ConfigValue::tryAs<std::vector<std::string>>() const;

template bool ConfigValue::asOrDefault<bool>(const bool&) const;
template int ConfigValue::asOrDefault<int>(const int&) const;
template double ConfigValue::asOrDefault<double>(const double&) const;
template std::string ConfigValue::asOrDefault<std::string>(const std::string&) const;
template std::vector<std::string> ConfigValue::asOrDefault<std::vector<std::string>>(const std::vector<std::string>&) const;

} // namespace BBP