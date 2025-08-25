// EN: Schema Validator implementation for BB-Pipeline CSV Engine - Strict validation with versioning
// FR: Implémentation Schema Validator pour BB-Pipeline CSV Engine - Validation stricte avec versioning

#include "csv/schema_validator.hpp"
#include "infrastructure/logging/logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <cmath>

namespace BBP::CSV {

// EN: CsvSchema implementation
// FR: Implémentation CsvSchema
CsvSchema::CsvSchema(const std::string& schema_name, const SchemaVersion& version) 
    : name_(schema_name), version_(version) {
    
    auto& logger = Logger::getInstance();
    logger.debug("CsvSchema created - Name: " + schema_name + ", Version: " + version.toString(), "csv_schema");
}

CsvSchema& CsvSchema::addField(const SchemaField& field) {
    // EN: Check for duplicate field names
    // FR: Vérifie les noms de champs dupliqués
    if (field_map_.find(field.name) != field_map_.end()) {
        throw std::invalid_argument("Field '" + field.name + "' already exists in schema '" + name_ + "'");
    }
    
    fields_.push_back(field);
    updateFieldMapping();
    
    auto& logger = Logger::getInstance();
    logger.debug("Field added to schema '" + name_ + "': " + field.name + 
                " (type: " + std::to_string(static_cast<int>(field.type)) + ")", "csv_schema");
    
    return *this;
}

CsvSchema& CsvSchema::addField(const std::string& name, DataType type, const FieldConstraints& constraints) {
    SchemaField field;
    field.name = name;
    field.type = type;
    field.constraints = constraints;
    field.position = fields_.size();
    
    return addField(field);
}

const SchemaField* CsvSchema::getField(const std::string& name) const {
    auto it = field_map_.find(name);
    if (it != field_map_.end()) {
        return &fields_[it->second];
    }
    
    // EN: Check aliases
    // FR: Vérifie les alias
    for (const auto& field : fields_) {
        for (const auto& alias : field.aliases) {
            if (alias == name) {
                return &field;
            }
        }
    }
    
    return nullptr;
}

const SchemaField* CsvSchema::getFieldByPosition(size_t position) const {
    if (position < fields_.size()) {
        return &fields_[position];
    }
    return nullptr;
}

bool CsvSchema::isCompatibleWith(const SchemaVersion& other_version) const {
    // EN: Major version must match, minor can be backward compatible
    // FR: Version majeure doit correspondre, mineure peut être rétro-compatible
    return version_.major == other_version.major && version_.minor >= other_version.minor;
}

std::string CsvSchema::toJson() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"name\": \"" << name_ << "\",\n";
    json << "  \"version\": {\n";
    json << "    \"major\": " << version_.major << ",\n";
    json << "    \"minor\": " << version_.minor << ",\n";
    json << "    \"patch\": " << version_.patch << "\n";
    json << "  },\n";
    json << "  \"description\": \"" << description_ << "\",\n";
    json << "  \"strict_mode\": " << (strict_mode_ ? "true" : "false") << ",\n";
    json << "  \"allow_extra_columns\": " << (allow_extra_columns_ ? "true" : "false") << ",\n";
    json << "  \"header_required\": " << (header_required_ ? "true" : "false") << ",\n";
    json << "  \"fields\": [\n";
    
    for (size_t i = 0; i < fields_.size(); ++i) {
        const auto& field = fields_[i];
        json << "    {\n";
        json << "      \"name\": \"" << field.name << "\",\n";
        json << "      \"type\": \"" << static_cast<int>(field.type) << "\",\n";
        json << "      \"position\": " << field.position << ",\n";
        json << "      \"required\": " << (field.constraints.required ? "true" : "false") << "\n";
        json << "    }";
        if (i < fields_.size() - 1) json << ",";
        json << "\n";
    }
    
    json << "  ]\n";
    json << "}";
    
    return json.str();
}

std::unique_ptr<CsvSchema> CsvSchema::fromJson(const std::string& json_str) {
    // EN: Basic JSON parsing - in production, use a proper JSON library
    // FR: Parsing JSON basique - en production, utiliser une vraie librairie JSON
    auto& logger = Logger::getInstance();
    logger.warn("CsvSchema::fromJson - Basic implementation, use proper JSON library in production", "csv_schema");
    
    // EN: For now, return nullptr to indicate not implemented
    // FR: Pour l'instant, retourne nullptr pour indiquer non implémenté
    (void)json_str;
    return nullptr;
}

bool CsvSchema::isValid() const {
    return !fields_.empty() && !name_.empty();
}

std::vector<std::string> CsvSchema::getValidationIssues() const {
    std::vector<std::string> issues;
    
    if (name_.empty()) {
        issues.push_back("Schema name cannot be empty");
    }
    
    if (fields_.empty()) {
        issues.push_back("Schema must have at least one field");
    }
    
    // EN: Check for duplicate positions
    // FR: Vérifie les positions dupliquées
    std::unordered_set<size_t> positions;
    for (const auto& field : fields_) {
        if (positions.find(field.position) != positions.end()) {
            issues.push_back("Duplicate position " + std::to_string(field.position) + " found");
        }
        positions.insert(field.position);
    }
    
    return issues;
}

void CsvSchema::updateFieldMapping() {
    field_map_.clear();
    for (size_t i = 0; i < fields_.size(); ++i) {
        field_map_[fields_[i].name] = i;
    }
}

// EN: CsvSchemaValidator implementation
// FR: Implémentation CsvSchemaValidator
CsvSchemaValidator::CsvSchemaValidator() {
    auto& logger = Logger::getInstance();
    logger.debug("CsvSchemaValidator initialized", "csv_schema_validator");
    
    // EN: Register built-in validators
    // FR: Enregistre les validateurs intégrés
    registerCustomValidator("non_empty", [](const std::string& value) {
        return !value.empty();
    });
    
    registerCustomValidator("alphanumeric", [](const std::string& value) {
        return std::all_of(value.begin(), value.end(), [](char c) {
            return std::isalnum(c);
        });
    });
    
    registerCustomValidator("numeric", [](const std::string& value) {
        return std::all_of(value.begin(), value.end(), [](char c) {
            return std::isdigit(c) || c == '.' || c == '-' || c == '+';
        });
    });
}

void CsvSchemaValidator::registerSchema(std::unique_ptr<CsvSchema> schema) {
    if (!schema) {
        throw std::invalid_argument("Cannot register null schema");
    }
    
    const std::string& name = schema->getName();
    const std::string version_key = versionToKey(schema->getVersion());
    
    schemas_[name][version_key] = std::move(schema);
    
    auto& logger = Logger::getInstance();
    logger.info("Schema registered - Name: " + name + ", Version: " + version_key, "csv_schema_validator");
}

const CsvSchema* CsvSchemaValidator::getSchema(const std::string& name, const SchemaVersion& version) const {
    auto schema_it = schemas_.find(name);
    if (schema_it == schemas_.end()) {
        return nullptr;
    }
    
    const std::string version_key = versionToKey(version);
    auto version_it = schema_it->second.find(version_key);
    if (version_it != schema_it->second.end()) {
        return version_it->second.get();
    }
    
    // EN: If exact version not found, try to find latest compatible version
    // FR: Si version exacte non trouvée, essaie de trouver la dernière version compatible
    const CsvSchema* latest_compatible = nullptr;
    SchemaVersion latest_version{0, 0, 0};
    
    for (const auto& version_pair : schema_it->second) {
        const CsvSchema* candidate = version_pair.second.get();
        if (candidate->isCompatibleWith(version) && candidate->getVersion() >= latest_version) {
            latest_compatible = candidate;
            latest_version = candidate->getVersion();
        }
    }
    
    return latest_compatible;
}

std::vector<std::string> CsvSchemaValidator::getAvailableSchemas() const {
    std::vector<std::string> schemas;
    for (const auto& schema_pair : schemas_) {
        schemas.push_back(schema_pair.first);
    }
    std::sort(schemas.begin(), schemas.end());
    return schemas;
}

std::vector<SchemaVersion> CsvSchemaValidator::getSchemaVersions(const std::string& name) const {
    std::vector<SchemaVersion> versions;
    
    auto schema_it = schemas_.find(name);
    if (schema_it != schemas_.end()) {
        for (const auto& version_pair : schema_it->second) {
            versions.push_back(version_pair.second->getVersion());
        }
        std::sort(versions.begin(), versions.end());
    }
    
    return versions;
}

ValidationResult CsvSchemaValidator::validateCsvFile(const std::string& file_path, const std::string& schema_name, 
                                                    const SchemaVersion& version) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::ifstream file(file_path);
    if (!file.is_open()) {
        ValidationResult result;
        result.is_valid = false;
        addValidationError(result, ValidationError::Severity::FATAL, "", 0, 0, 
                         "Cannot open file: " + file_path);
        return result;
    }
    
    auto result = validateCsvStream(file, schema_name, version);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.validation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    auto& logger = Logger::getInstance();
    logger.info("CSV file validation completed - File: " + file_path + 
                ", Valid: " + (result.is_valid ? "true" : "false") + 
                ", Duration: " + std::to_string(result.validation_duration.count()) + "ms", 
                "csv_schema_validator");
    
    return result;
}

ValidationResult CsvSchemaValidator::validateCsvContent(const std::string& csv_content, const std::string& schema_name, 
                                                       const SchemaVersion& version) {
    std::istringstream stream(csv_content);
    return validateCsvStream(stream, schema_name, version);
}

ValidationResult CsvSchemaValidator::validateCsvStream(std::istream& stream, const std::string& schema_name, 
                                                      const SchemaVersion& version) {
    ValidationResult result;
    result.schema_version = version;
    
    // EN: Get schema
    // FR: Obtient le schéma
    const CsvSchema* schema = getSchema(schema_name, version);
    if (!schema) {
        result.is_valid = false;
        addValidationError(result, ValidationError::Severity::FATAL, "", 0, 0, 
                         "Schema not found: " + schema_name + " v" + version.toString());
        return result;
    }
    
    std::string line;
    size_t row_number = 0;
    bool header_validated = false;
    
    while (std::getline(stream, line)) {
        row_number++;
        
        if (line.empty()) {
            continue; // EN: Skip empty lines / FR: Ignore les lignes vides
        }
        
        std::vector<std::string> row = parseCsvRow(line);
        
        // EN: Validate header if required
        // FR: Valide l'en-tête si requis
        if (!header_validated && schema->isHeaderRequired()) {
            if (validateHeader(row, *schema, result)) {
                header_validated = true;
                continue; // EN: Header processed, continue to next row / FR: En-tête traité, continue à la ligne suivante
            } else {
                result.is_valid = false;
                if (stop_on_first_error_) break;
            }
        }
        
        // EN: Count only data rows (excluding header)
        // FR: Compte seulement les lignes de données (excluant l'en-tête)
        result.total_rows++;
        
        // EN: Validate data row
        // FR: Valide la ligne de données
        if (validateRow(row, *schema, row_number, result)) {
            result.valid_rows++;
        } else {
            result.error_rows++;
            result.is_valid = false;
            if (stop_on_first_error_) break;
        }
    }
    
    // EN: Final validation summary
    // FR: Résumé final de validation
    if (result.errors.empty()) {
        result.is_valid = true;
    }
    
    // EN: Count warnings
    // FR: Compte les avertissements
    for (const auto& error : result.errors) {
        if (error.severity == ValidationError::Severity::WARNING) {
            result.warning_rows++;
        }
    }
    
    return result;
}

bool CsvSchemaValidator::validateHeader(const std::vector<std::string>& header, const CsvSchema& schema, ValidationResult& result) {
    bool valid = true;
    const auto& fields = schema.getFields();
    
    // EN: Check if all required fields are present
    // FR: Vérifie si tous les champs requis sont présents
    for (const auto& field : fields) {
        bool found = false;
        for (size_t i = 0; i < header.size(); ++i) {
            if (header[i] == field.name) {
                found = true;
                break;
            }
            // EN: Check aliases
            // FR: Vérifie les alias
            for (const auto& alias : field.aliases) {
                if (header[i] == alias) {
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        
        if (!found && field.constraints.required) {
            addValidationError(result, ValidationError::Severity::ERROR, field.name, 1, 0,
                             "Required field '" + field.name + "' not found in header");
            valid = false;
        }
    }
    
    // EN: Check for extra columns if not allowed
    // FR: Vérifie les colonnes supplémentaires si non autorisées
    if (!schema.getAllowExtraColumns()) {
        for (size_t i = 0; i < header.size(); ++i) {
            const std::string& col_name = header[i];
            if (!schema.getField(col_name)) {
                addValidationError(result, ValidationError::Severity::WARNING, col_name, 1, i + 1,
                                 "Extra column '" + col_name + "' not defined in schema");
                if (schema.isStrictMode()) {
                    valid = false;
                }
            }
        }
    }
    
    return valid;
}

bool CsvSchemaValidator::validateRow(const std::vector<std::string>& row, const CsvSchema& schema, 
                                    size_t row_number, ValidationResult& result) {
    bool valid = true;
    const auto& fields = schema.getFields();
    
    // EN: Validate each field
    // FR: Valide chaque champ
    for (size_t i = 0; i < fields.size(); ++i) {
        const auto& field = fields[i];
        std::string value;
        
        if (i < row.size()) {
            value = trim(row[i]);
        }
        
        if (!validateField(value, field, row_number, i + 1, result)) {
            valid = false;
        }
    }
    
    return valid;
}

bool CsvSchemaValidator::validateField(const std::string& value, const SchemaField& field, size_t row_number, 
                                      size_t column_number, ValidationResult& result) {
    // EN: Check if field is empty
    // FR: Vérifie si le champ est vide
    bool is_empty = isEmptyValue(value);
    
    if (is_empty) {
        if (field.constraints.required) {
            addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                             "Required field '" + field.name + "' is empty", value);
            return false;
        } else if (!field.constraints.default_value.empty()) {
            // EN: Use default value
            // FR: Utilise la valeur par défaut
            return true;
        }
        return true; // EN: Empty but not required / FR: Vide mais pas requis
    }
    
    // EN: Validate based on data type
    // FR: Valide selon le type de données
    switch (field.type) {
        case DataType::STRING:
            return validateString(value, field, row_number, column_number, result);
        case DataType::INTEGER:
            return validateInteger(value, field, row_number, column_number, result);
        case DataType::FLOAT:
            return validateFloat(value, field, row_number, column_number, result);
        case DataType::BOOLEAN:
            return validateBoolean(value, field, row_number, column_number, result);
        case DataType::DATE:
            return validateDate(value, field, row_number, column_number, result);
        case DataType::DATETIME:
            return validateDateTime(value, field, row_number, column_number, result);
        case DataType::EMAIL:
            return validateEmail(value, field, row_number, column_number, result);
        case DataType::URL:
            return validateUrl(value, field, row_number, column_number, result);
        case DataType::IP_ADDRESS:
            return validateIpAddress(value, field, row_number, column_number, result);
        case DataType::UUID:
            return validateUuid(value, field, row_number, column_number, result);
        case DataType::ENUM:
            return validateEnum(value, field, row_number, column_number, result);
        case DataType::CUSTOM:
            return validateCustom(value, field, row_number, column_number, result);
        default:
            addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                             "Unknown data type for field '" + field.name + "'", value);
            return false;
    }
}

void CsvSchemaValidator::registerCustomValidator(const std::string& name, std::function<bool(const std::string&)> validator) {
    custom_validators_[name] = validator;
    
    auto& logger = Logger::getInstance();
    logger.debug("Custom validator registered: " + name, "csv_schema_validator");
}

std::function<bool(const std::string&)> CsvSchemaValidator::getCustomValidator(const std::string& name) const {
    auto it = custom_validators_.find(name);
    return it != custom_validators_.end() ? it->second : nullptr;
}

std::string CsvSchemaValidator::generateValidationReport(const ValidationResult& result, bool detailed) const {
    std::ostringstream report;
    
    report << "=== CSV Validation Report ===\n";
    report << "Schema Version: " << result.schema_version.toString() << "\n";
    report << "Total Rows: " << result.total_rows << "\n";
    report << "Valid Rows: " << result.valid_rows << "\n";
    report << "Error Rows: " << result.error_rows << "\n";
    report << "Warning Rows: " << result.warning_rows << "\n";
    report << "Success Rate: " << std::fixed << std::setprecision(2) << result.getSuccessRate() << "%\n";
    report << "Validation Duration: " << result.validation_duration.count() << "ms\n";
    report << "Overall Status: " << (result.is_valid ? "VALID" : "INVALID") << "\n";
    
    if (!result.errors.empty()) {
        report << "\n=== Errors Summary ===\n";
        
        // EN: Group errors by field
        // FR: Groupe les erreurs par champ
        std::unordered_map<std::string, std::vector<ValidationError>> errors_by_field;
        for (const auto& error : result.errors) {
            errors_by_field[error.field_name].push_back(error);
        }
        
        for (const auto& field_errors : errors_by_field) {
            report << "Field '" << field_errors.first << "': " << field_errors.second.size() << " error(s)\n";
            
            if (detailed) {
                for (const auto& error : field_errors.second) {
                    report << "  Row " << error.row_number << ", Col " << error.column_number 
                           << ": " << error.message;
                    if (!error.actual_value.empty()) {
                        report << " (value: '" << error.actual_value << "')";
                    }
                    report << "\n";
                }
            }
        }
    }
    
    return report.str();
}

std::string CsvSchemaValidator::generateSchemaDocumentation(const std::string& schema_name, const SchemaVersion& version) const {
    const CsvSchema* schema = getSchema(schema_name, version);
    if (!schema) {
        return "Schema not found: " + schema_name + " v" + version.toString();
    }
    
    std::ostringstream doc;
    doc << "=== Schema Documentation ===\n";
    doc << "Name: " << schema->getName() << "\n";
    doc << "Version: " << schema->getVersion().toString() << "\n";
    doc << "Description: " << schema->getDescription() << "\n";
    doc << "Strict Mode: " << (schema->isStrictMode() ? "Yes" : "No") << "\n";
    doc << "Allow Extra Columns: " << (schema->getAllowExtraColumns() ? "Yes" : "No") << "\n";
    doc << "Header Required: " << (schema->isHeaderRequired() ? "Yes" : "No") << "\n";
    
    doc << "\n=== Fields ===\n";
    const auto& fields = schema->getFields();
    for (const auto& field : fields) {
        doc << "Field: " << field.name << "\n";
        doc << "  Type: " << static_cast<int>(field.type) << "\n";
        doc << "  Position: " << field.position << "\n";
        doc << "  Required: " << (field.constraints.required ? "Yes" : "No") << "\n";
        doc << "  Description: " << field.constraints.description << "\n";
        if (!field.constraints.default_value.empty()) {
            doc << "  Default: " << field.constraints.default_value << "\n";
        }
        doc << "\n";
    }
    
    return doc.str();
}

// EN: Validation helper implementations
// FR: Implémentations des helpers de validation

bool CsvSchemaValidator::validateString(const std::string& value, const SchemaField& field, size_t row_number, 
                                       size_t column_number, ValidationResult& result) {
    bool valid = true;
    
    // EN: Check length constraints
    // FR: Vérifie les contraintes de longueur
    if (field.constraints.min_length && value.length() < field.constraints.min_length.value()) {
        addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                         "String too short (min: " + std::to_string(field.constraints.min_length.value()) + ")", 
                         value, "min_length=" + std::to_string(field.constraints.min_length.value()));
        valid = false;
    }
    
    if (field.constraints.max_length && value.length() > field.constraints.max_length.value()) {
        addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                         "String too long (max: " + std::to_string(field.constraints.max_length.value()) + ")", 
                         value, "max_length=" + std::to_string(field.constraints.max_length.value()));
        valid = false;
    }
    
    // EN: Check pattern if specified
    // FR: Vérifie le pattern si spécifié
    if (field.constraints.pattern && !std::regex_match(value, field.constraints.pattern.value())) {
        addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                         "String does not match required pattern", value, "pattern_match");
        valid = false;
    }
    
    return valid;
}

bool CsvSchemaValidator::validateInteger(const std::string& value, const SchemaField& field, size_t row_number, 
                                        size_t column_number, ValidationResult& result) {
    try {
        int64_t int_value = std::stoll(value);
        
        // EN: Check range constraints
        // FR: Vérifie les contraintes de plage
        if (field.constraints.min_value && int_value < field.constraints.min_value.value()) {
            addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                             "Integer too small (min: " + std::to_string(field.constraints.min_value.value()) + ")", 
                             value);
            return false;
        }
        
        if (field.constraints.max_value && int_value > field.constraints.max_value.value()) {
            addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                             "Integer too large (max: " + std::to_string(field.constraints.max_value.value()) + ")", 
                             value);
            return false;
        }
        
        return true;
    } catch (const std::exception&) {
        addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                         "Invalid integer format", value, "integer");
        return false;
    }
}

bool CsvSchemaValidator::validateFloat(const std::string& value, const SchemaField& field, size_t row_number, 
                                      size_t column_number, ValidationResult& result) {
    try {
        double float_value = std::stod(value);
        
        if (std::isnan(float_value) || std::isinf(float_value)) {
            addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                             "Invalid float value (NaN or Inf)", value);
            return false;
        }
        
        // EN: Check range constraints
        // FR: Vérifie les contraintes de plage
        if (field.constraints.min_value && float_value < field.constraints.min_value.value()) {
            addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                             "Float too small (min: " + std::to_string(field.constraints.min_value.value()) + ")", 
                             value);
            return false;
        }
        
        if (field.constraints.max_value && float_value > field.constraints.max_value.value()) {
            addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                             "Float too large (max: " + std::to_string(field.constraints.max_value.value()) + ")", 
                             value);
            return false;
        }
        
        return true;
    } catch (const std::exception&) {
        addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                         "Invalid float format", value, "float");
        return false;
    }
}

bool CsvSchemaValidator::validateBoolean(const std::string& value, const SchemaField& field, size_t row_number, 
                                        size_t column_number, ValidationResult& result) {
    std::string lower_value = toLower(value);
    
    const std::unordered_set<std::string> true_values = {"true", "1", "yes", "y", "on"};
    const std::unordered_set<std::string> false_values = {"false", "0", "no", "n", "off"};
    
    if (true_values.find(lower_value) != true_values.end() || 
        false_values.find(lower_value) != false_values.end()) {
        return true;
    }
    
    addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                     "Invalid boolean format", value, "true/false, 1/0, yes/no, y/n, on/off");
    return false;
}

bool CsvSchemaValidator::validateDate(const std::string& value, const SchemaField& field, size_t row_number, 
                                     size_t column_number, ValidationResult& result) {
    // EN: Basic date format validation (YYYY-MM-DD)
    // FR: Validation basique de format de date (YYYY-MM-DD)
    std::regex date_pattern(R"(\d{4}-\d{2}-\d{2})");
    
    if (!std::regex_match(value, date_pattern)) {
        addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                         "Invalid date format", value, "YYYY-MM-DD");
        return false;
    }
    
    // EN: Additional date validation could be added here
    // FR: Validation de date additionnelle pourrait être ajoutée ici
    
    return true;
}

bool CsvSchemaValidator::validateDateTime(const std::string& value, const SchemaField& field, size_t row_number, 
                                         size_t column_number, ValidationResult& result) {
    // EN: Basic datetime format validation (ISO 8601)
    // FR: Validation basique de format datetime (ISO 8601)
    std::regex datetime_pattern(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})?)");
    
    if (!std::regex_match(value, datetime_pattern)) {
        addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                         "Invalid datetime format", value, "ISO 8601 (YYYY-MM-DDTHH:MM:SS)");
        return false;
    }
    
    return true;
}

bool CsvSchemaValidator::validateEmail(const std::string& value, const SchemaField& field, size_t row_number, 
                                      size_t column_number, ValidationResult& result) {
    // EN: Basic email format validation
    // FR: Validation basique de format email
    std::regex email_pattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    
    if (!std::regex_match(value, email_pattern)) {
        addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                         "Invalid email format", value, "user@domain.com");
        return false;
    }
    
    return true;
}

bool CsvSchemaValidator::validateUrl(const std::string& value, const SchemaField& field, size_t row_number, 
                                    size_t column_number, ValidationResult& result) {
    // EN: Basic URL format validation
    // FR: Validation basique de format URL
    std::regex url_pattern(R"(https?://[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}(/.*)?$)");
    
    if (!std::regex_match(value, url_pattern)) {
        addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                         "Invalid URL format", value, "http(s)://domain.com/path");
        return false;
    }
    
    return true;
}

bool CsvSchemaValidator::validateIpAddress(const std::string& value, const SchemaField& field, size_t row_number, 
                                          size_t column_number, ValidationResult& result) {
    // EN: Basic IPv4 format validation
    // FR: Validation basique de format IPv4
    std::regex ipv4_pattern(R"(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})");
    
    if (std::regex_match(value, ipv4_pattern)) {
        // EN: Validate octets are in range 0-255
        // FR: Valide que les octets sont dans la plage 0-255
        std::istringstream ss(value);
        std::string octet;
        while (std::getline(ss, octet, '.')) {
            int num = std::stoi(octet);
            if (num < 0 || num > 255) {
                addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                                 "Invalid IP address octet range", value, "0-255 per octet");
                return false;
            }
        }
        return true;
    }
    
    // EN: Basic IPv6 format validation (simplified)
    // FR: Validation basique de format IPv6 (simplifiée)
    std::regex ipv6_pattern(R"([0-9a-fA-F:]{2,39})");
    if (std::regex_match(value, ipv6_pattern)) {
        return true;
    }
    
    addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                     "Invalid IP address format", value, "IPv4 (x.x.x.x) or IPv6");
    return false;
}

bool CsvSchemaValidator::validateUuid(const std::string& value, const SchemaField& field, size_t row_number, 
                                     size_t column_number, ValidationResult& result) {
    // EN: UUID format validation
    // FR: Validation de format UUID
    std::regex uuid_pattern(R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})");
    
    if (!std::regex_match(value, uuid_pattern)) {
        addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                         "Invalid UUID format", value, "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx");
        return false;
    }
    
    return true;
}

bool CsvSchemaValidator::validateEnum(const std::string& value, const SchemaField& field, size_t row_number, 
                                     size_t column_number, ValidationResult& result) {
    if (field.constraints.enum_values.find(value) == field.constraints.enum_values.end()) {
        std::ostringstream valid_values;
        bool first = true;
        for (const auto& valid_value : field.constraints.enum_values) {
            if (!first) valid_values << ", ";
            valid_values << valid_value;
            first = false;
        }
        
        addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                         "Value not in allowed enum set", value, valid_values.str());
        return false;
    }
    
    return true;
}

bool CsvSchemaValidator::validateCustom(const std::string& value, const SchemaField& field, size_t row_number, 
                                       size_t column_number, ValidationResult& result) {
    if (field.constraints.custom_validator) {
        if (!field.constraints.custom_validator(value)) {
            addValidationError(result, ValidationError::Severity::ERROR, field.name, row_number, column_number,
                             "Custom validation failed", value, "custom_validator");
            return false;
        }
    }
    
    return true;
}

// EN: Utility function implementations
// FR: Implémentations des fonctions utilitaires

std::vector<std::string> CsvSchemaValidator::parseCsvRow(const std::string& row_str) const {
    std::vector<std::string> fields;
    std::string current_field;
    bool in_quotes = false;
    
    for (size_t i = 0; i < row_str.length(); ++i) {
        char c = row_str[i];
        
        if (c == '"' && !in_quotes) {
            in_quotes = true;
        } else if (c == '"' && in_quotes) {
            // EN: Check for escaped quote
            // FR: Vérifie les guillemets échappés
            if (i + 1 < row_str.length() && row_str[i + 1] == '"') {
                current_field += '"';
                ++i; // EN: Skip next quote / FR: Ignore le guillemet suivant
            } else {
                in_quotes = false;
            }
        } else if (c == ',' && !in_quotes) {
            fields.push_back(current_field);
            current_field.clear();
        } else {
            current_field += c;
        }
    }
    
    fields.push_back(current_field);
    return fields;
}

void CsvSchemaValidator::addValidationError(ValidationResult& result, ValidationError::Severity severity, 
                                          const std::string& field_name, size_t row_number, size_t column_number,
                                          const std::string& message, const std::string& actual_value,
                                          const std::string& expected_format) const {
    ValidationError error(severity, field_name, row_number, column_number, message, actual_value, expected_format);
    result.errors.push_back(error);
    
    // EN: Update field error count
    // FR: Met à jour le compte d'erreur par champ
    result.field_error_counts[field_name]++;
    
    // EN: Check max errors per field limit
    // FR: Vérifie la limite max d'erreurs par champ
    if (result.field_error_counts[field_name] >= max_errors_per_field_) {
        ValidationError limit_error(ValidationError::Severity::WARNING, field_name, row_number, column_number,
                                   "Maximum error count reached for field '" + field_name + 
                                   "', further errors will be suppressed");
        result.errors.push_back(limit_error);
    }
}

bool CsvSchemaValidator::isEmptyValue(const std::string& value) const {
    std::string trimmed = trim(value);
    return trimmed.empty() || trimmed == "NULL" || trimmed == "null" || trimmed == "N/A" || trimmed == "";
}

std::string CsvSchemaValidator::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string CsvSchemaValidator::toLower(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string CsvSchemaValidator::versionToKey(const SchemaVersion& version) const {
    return std::to_string(version.major) + "." + 
           std::to_string(version.minor) + "." + 
           std::to_string(version.patch);
}

SchemaVersion CsvSchemaValidator::keyToVersion(const std::string& key) const {
    SchemaVersion version;
    std::istringstream ss(key);
    std::string part;
    
    if (std::getline(ss, part, '.')) {
        version.major = static_cast<uint32_t>(std::stoul(part));
    }
    if (std::getline(ss, part, '.')) {
        version.minor = static_cast<uint32_t>(std::stoul(part));
    }
    if (std::getline(ss, part)) {
        version.patch = static_cast<uint32_t>(std::stoul(part));
    }
    
    return version;
}

// EN: SchemaUtils namespace implementation
// FR: Implémentation du namespace SchemaUtils
namespace SchemaUtils {

SchemaField createStringField(const std::string& name, size_t position, bool required,
                             size_t min_length, size_t max_length) {
    SchemaField field(name, DataType::STRING, position);
    field.constraints.required = required;
    if (min_length > 0) field.constraints.min_length = min_length;
    if (max_length != SIZE_MAX) field.constraints.max_length = max_length;
    return field;
}

SchemaField createIntegerField(const std::string& name, size_t position, bool required,
                              int64_t min_value, int64_t max_value) {
    SchemaField field(name, DataType::INTEGER, position);
    field.constraints.required = required;
    if (min_value != std::numeric_limits<int64_t>::min()) {
        field.constraints.min_value = static_cast<double>(min_value);
    }
    if (max_value != std::numeric_limits<int64_t>::max()) {
        field.constraints.max_value = static_cast<double>(max_value);
    }
    return field;
}

SchemaField createFloatField(const std::string& name, size_t position, bool required,
                            double min_value, double max_value) {
    SchemaField field(name, DataType::FLOAT, position);
    field.constraints.required = required;
    if (min_value != std::numeric_limits<double>::lowest()) {
        field.constraints.min_value = min_value;
    }
    if (max_value != std::numeric_limits<double>::max()) {
        field.constraints.max_value = max_value;
    }
    return field;
}

SchemaField createBooleanField(const std::string& name, size_t position, bool required) {
    SchemaField field(name, DataType::BOOLEAN, position);
    field.constraints.required = required;
    return field;
}

SchemaField createDateField(const std::string& name, size_t position, bool required) {
    SchemaField field(name, DataType::DATE, position);
    field.constraints.required = required;
    return field;
}

SchemaField createEmailField(const std::string& name, size_t position, bool required) {
    SchemaField field(name, DataType::EMAIL, position);
    field.constraints.required = required;
    return field;
}

SchemaField createUrlField(const std::string& name, size_t position, bool required) {
    SchemaField field(name, DataType::URL, position);
    field.constraints.required = required;
    return field;
}

SchemaField createIpField(const std::string& name, size_t position, bool required) {
    SchemaField field(name, DataType::IP_ADDRESS, position);
    field.constraints.required = required;
    return field;
}

SchemaField createUuidField(const std::string& name, size_t position, bool required) {
    SchemaField field(name, DataType::UUID, position);
    field.constraints.required = required;
    return field;
}

SchemaField createEnumField(const std::string& name, size_t position, const std::vector<std::string>& values, bool required) {
    SchemaField field(name, DataType::ENUM, position);
    field.constraints.required = required;
    for (const auto& value : values) {
        field.constraints.enum_values.insert(value);
    }
    return field;
}

// EN: Create schemas for BB-Pipeline modules
// FR: Créer des schémas pour les modules BB-Pipeline

std::unique_ptr<CsvSchema> createScopeSchema() {
    auto schema = std::make_unique<CsvSchema>("scope", SchemaVersion{1, 0, 0, "Initial scope schema"});
    schema->setDescription("Schema for data/scope.csv - defines authorized target scope for BB-Pipeline");
    
    schema->addField(createStringField("domain", 0, true, 1, 253));  // EN: Domain name / FR: Nom de domaine
    schema->addField(createStringField("program", 1, true, 1, 100)); // EN: Bug bounty program / FR: Programme bug bounty
    schema->addField(createEnumField("status", 2, {"active", "inactive", "paused"}, true)); // EN: Scope status / FR: Statut du scope
    schema->addField(createStringField("notes", 3, false, 0, 500));  // EN: Optional notes / FR: Notes optionnelles
    
    return schema;
}

std::unique_ptr<CsvSchema> createSubdomainsSchema() {
    auto schema = std::make_unique<CsvSchema>("subdomains", SchemaVersion{1, 0, 0, "Subdomain enumeration results"});
    schema->setDescription("Schema for 01_subdomains.csv - subdomain enumeration results");
    
    schema->addField(createStringField("subdomain", 0, true, 1, 253));
    schema->addField(createStringField("source", 1, true, 1, 50));
    schema->addField(createBooleanField("wildcard", 2, false));
    schema->addField(createDateField("discovered_at", 3, true));
    
    return schema;
}

std::unique_ptr<CsvSchema> createProbeSchema() {
    auto schema = std::make_unique<CsvSchema>("probe", SchemaVersion{1, 0, 0, "HTTP probing results"});
    schema->setDescription("Schema for 02_probe.csv - HTTP probing results");
    
    schema->addField(createUrlField("url", 0, true));
    schema->addField(createIntegerField("status_code", 1, true, 100, 599));
    schema->addField(createIntegerField("content_length", 2, false, 0));
    schema->addField(createStringField("title", 3, false, 0, 200));
    schema->addField(createStringField("technologies", 4, false, 0, 500));
    
    return schema;
}

std::unique_ptr<CsvSchema> createHeadlessSchema() {
    auto schema = std::make_unique<CsvSchema>("headless", SchemaVersion{1, 0, 0, "Headless analysis results"});
    schema->setDescription("Schema for 03_headless.csv - headless browser analysis results");
    
    schema->addField(createUrlField("url", 0, true));
    schema->addField(createStringField("js_frameworks", 1, false, 0, 200));
    schema->addField(createStringField("forms", 2, false, 0, 1000));
    schema->addField(createStringField("xhr_endpoints", 3, false, 0, 2000));
    schema->addField(createBooleanField("spa_detected", 4, false));
    
    return schema;
}

std::unique_ptr<CsvSchema> createDiscoverySchema() {
    auto schema = std::make_unique<CsvSchema>("discovery", SchemaVersion{1, 0, 0, "Content discovery results"});
    schema->setDescription("Schema for 04_discovery.csv - content discovery and bruteforce results");
    
    schema->addField(createUrlField("url", 0, true));
    schema->addField(createIntegerField("status_code", 1, true, 100, 599));
    schema->addField(createIntegerField("content_length", 2, false, 0));
    schema->addField(createStringField("content_type", 3, false, 0, 100));
    schema->addField(createEnumField("discovery_type", 4, {"directory", "file", "backup", "config", "source"}, true));
    
    return schema;
}

std::unique_ptr<CsvSchema> createJsIntelSchema() {
    auto schema = std::make_unique<CsvSchema>("jsintel", SchemaVersion{1, 0, 0, "JavaScript intelligence results"});
    schema->setDescription("Schema for 05_jsintel.csv - JavaScript analysis and intelligence");
    
    schema->addField(createUrlField("js_url", 0, true));
    schema->addField(createStringField("endpoints", 1, false, 0, 2000));
    schema->addField(createStringField("api_keys", 2, false, 0, 1000));
    schema->addField(createStringField("secrets", 3, false, 0, 500));
    schema->addField(createStringField("comments", 4, false, 0, 1000));
    
    return schema;
}

std::unique_ptr<CsvSchema> createApiCatalogSchema() {
    auto schema = std::make_unique<CsvSchema>("api_catalog", SchemaVersion{1, 0, 0, "API catalog results"});
    schema->setDescription("Schema for 06_api_catalog.csv - API discovery and catalog");
    
    schema->addField(createUrlField("api_endpoint", 0, true));
    schema->addField(createEnumField("api_type", 1, {"REST", "GraphQL", "SOAP", "gRPC"}, true));
    schema->addField(createStringField("methods", 2, true, 1, 100));
    schema->addField(createStringField("parameters", 3, false, 0, 2000));
    schema->addField(createBooleanField("authentication_required", 4, false));
    
    return schema;
}

std::unique_ptr<CsvSchema> createApiFindingsSchema() {
    auto schema = std::make_unique<CsvSchema>("api_findings", SchemaVersion{1, 0, 0, "API security findings"});
    schema->setDescription("Schema for 07_api_findings.csv - API security testing results");
    
    schema->addField(createUrlField("api_endpoint", 0, true));
    schema->addField(createEnumField("vulnerability_type", 1, {"IDOR", "SQLi", "XSS", "Auth_Bypass", "Rate_Limit"}, true));
    schema->addField(createEnumField("severity", 2, {"Low", "Medium", "High", "Critical"}, true));
    schema->addField(createStringField("description", 3, true, 1, 1000));
    schema->addField(createStringField("proof_of_concept", 4, false, 0, 2000));
    
    return schema;
}

std::unique_ptr<CsvSchema> createMobileIntelSchema() {
    auto schema = std::make_unique<CsvSchema>("mobile_intel", SchemaVersion{1, 0, 0, "Mobile intelligence results"});
    schema->setDescription("Schema for 08_mobile_intel.csv - mobile application analysis");
    
    schema->addField(createStringField("app_package", 0, true, 1, 200));
    schema->addField(createEnumField("platform", 1, {"Android", "iOS"}, true));
    schema->addField(createStringField("endpoints", 2, false, 0, 2000));
    schema->addField(createStringField("certificates", 3, false, 0, 1000));
    schema->addField(createStringField("deep_links", 4, false, 0, 1000));
    
    return schema;
}

std::unique_ptr<CsvSchema> createChangesSchema() {
    auto schema = std::make_unique<CsvSchema>("changes", SchemaVersion{1, 0, 0, "Change monitoring results"});
    schema->setDescription("Schema for 09_changes.csv - change detection and monitoring");
    
    schema->addField(createUrlField("url", 0, true));
    schema->addField(createEnumField("change_type", 1, {"content", "certificate", "dns", "headers", "technology"}, true));
    schema->addField(createStringField("previous_value", 2, false, 0, 1000));
    schema->addField(createStringField("current_value", 3, false, 0, 1000));
    schema->addField(createDateField("detected_at", 4, true));
    
    return schema;
}

std::unique_ptr<CsvSchema> createFinalRankedSchema() {
    auto schema = std::make_unique<CsvSchema>("final_ranked", SchemaVersion{1, 0, 0, "Final ranked results"});
    schema->setDescription("Schema for 99_final_ranked.csv - final aggregated and ranked findings");
    
    schema->addField(createUrlField("target", 0, true));
    schema->addField(createEnumField("finding_type", 1, {"vulnerability", "information", "technology", "endpoint"}, true));
    schema->addField(createEnumField("severity", 2, {"Info", "Low", "Medium", "High", "Critical"}, true));
    schema->addField(createFloatField("risk_score", 3, true, 0.0, 10.0));
    schema->addField(createStringField("description", 4, true, 1, 1000));
    schema->addField(createStringField("source_module", 5, true, 1, 50));
    
    return schema;
}

bool canMigrateSchema(const SchemaVersion& from, const SchemaVersion& to) {
    // EN: Can only migrate within same major version, and to newer or same version
    // FR: Peut seulement migrer dans la même version majeure, et vers version plus récente ou identique
    return from.major == to.major && from <= to;
}

std::unique_ptr<CsvSchema> migrateSchema(const CsvSchema& source, const SchemaVersion& target_version) {
    // EN: Basic schema migration - copy and update version
    // FR: Migration basique de schéma - copie et met à jour la version
    auto migrated = std::make_unique<CsvSchema>(source.getName(), target_version);
    migrated->setDescription(source.getDescription() + " (migrated from v" + source.getVersion().toString() + ")");
    
    // EN: Copy all fields
    // FR: Copie tous les champs
    for (const auto& field : source.getFields()) {
        migrated->addField(field);
    }
    
    migrated->setStrictMode(source.isStrictMode());
    migrated->setAllowExtraColumns(source.getAllowExtraColumns());
    migrated->setHeaderRequired(source.isHeaderRequired());
    
    return migrated;
}

} // namespace SchemaUtils

} // namespace BBP::CSV