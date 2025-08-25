// EN: Schema Validator for BB-Pipeline CSV Engine - Strict I/O contract validation with versioning
// FR: Validateur de schéma pour BB-Pipeline CSV Engine - Validation stricte contrats E/S avec versioning

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <functional>
#include <regex>
#include <chrono>
#include <memory>
#include <variant>
#include <type_traits>
#include <limits>

namespace BBP::CSV {

// EN: Data types supported in CSV schema validation
// FR: Types de données supportés dans la validation de schéma CSV
enum class DataType {
    STRING,         // EN: String/text data / FR: Données texte/chaîne
    INTEGER,        // EN: Integer numbers / FR: Nombres entiers
    FLOAT,          // EN: Floating point numbers / FR: Nombres à virgule flottante
    BOOLEAN,        // EN: Boolean values (true/false, 1/0, yes/no) / FR: Valeurs booléennes
    DATE,           // EN: Date values (YYYY-MM-DD) / FR: Valeurs de date
    DATETIME,       // EN: DateTime values (ISO 8601) / FR: Valeurs de date et heure
    EMAIL,          // EN: Email addresses / FR: Adresses email
    URL,            // EN: URLs / FR: URLs
    IP_ADDRESS,     // EN: IP addresses (IPv4/IPv6) / FR: Adresses IP
    UUID,           // EN: UUID values / FR: Valeurs UUID
    ENUM,           // EN: Enumerated values from predefined set / FR: Valeurs énumérées d'un ensemble prédéfini
    CUSTOM          // EN: Custom validation with user-defined function / FR: Validation personnalisée avec fonction définie par l'utilisateur
};

// EN: Field constraints for validation
// FR: Contraintes de champ pour validation
struct FieldConstraints {
    bool required{true};                               // EN: Field is required / FR: Champ requis
    std::optional<size_t> min_length;                  // EN: Minimum string length / FR: Longueur minimum de chaîne
    std::optional<size_t> max_length;                  // EN: Maximum string length / FR: Longueur maximum de chaîne
    std::optional<double> min_value;                   // EN: Minimum numeric value / FR: Valeur numérique minimum
    std::optional<double> max_value;                   // EN: Maximum numeric value / FR: Valeur numérique maximum
    std::optional<std::regex> pattern;                 // EN: Regex pattern for validation / FR: Pattern regex pour validation
    std::unordered_set<std::string> enum_values;       // EN: Valid enum values / FR: Valeurs enum valides
    std::function<bool(const std::string&)> custom_validator; // EN: Custom validation function / FR: Fonction de validation personnalisée
    std::string format;                                // EN: Format specification / FR: Spécification de format
    std::string description;                           // EN: Field description / FR: Description du champ
    std::string default_value;                         // EN: Default value if empty / FR: Valeur par défaut si vide
};

// EN: Schema field definition
// FR: Définition de champ de schéma
struct SchemaField {
    std::string name;                                  // EN: Field name / FR: Nom du champ
    DataType type;                                     // EN: Data type / FR: Type de données
    FieldConstraints constraints;                      // EN: Validation constraints / FR: Contraintes de validation
    size_t position{0};                               // EN: Column position (0-based) / FR: Position de colonne (base 0)
    std::vector<std::string> aliases;                 // EN: Alternative field names / FR: Noms de champ alternatifs
    
    SchemaField() = default;
    SchemaField(const std::string& field_name, DataType field_type, size_t pos = 0) 
        : name(field_name), type(field_type), position(pos) {}
};

// EN: Schema version information
// FR: Information de version de schéma
struct SchemaVersion {
    uint32_t major{1};                                // EN: Major version number / FR: Numéro de version majeure
    uint32_t minor{0};                                // EN: Minor version number / FR: Numéro de version mineure
    uint32_t patch{0};                                // EN: Patch version number / FR: Numéro de version patch
    std::string description;                          // EN: Version description / FR: Description de version
    std::chrono::system_clock::time_point created_at; // EN: Creation timestamp / FR: Timestamp de création
    
    SchemaVersion() : created_at(std::chrono::system_clock::now()) {}
    SchemaVersion(uint32_t maj, uint32_t min, uint32_t pat, const std::string& desc = "") 
        : major(maj), minor(min), patch(pat), description(desc), created_at(std::chrono::system_clock::now()) {}
        
    // EN: Version comparison
    // FR: Comparaison de version
    bool operator==(const SchemaVersion& other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
    }
    
    bool operator<(const SchemaVersion& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        return patch < other.patch;
    }
    
    bool operator<=(const SchemaVersion& other) const {
        return *this < other || *this == other;
    }
    
    bool operator>(const SchemaVersion& other) const {
        return !(*this <= other);
    }
    
    bool operator>=(const SchemaVersion& other) const {
        return !(*this < other);
    }
    
    std::string toString() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
};

// EN: Validation error details
// FR: Détails d'erreur de validation
struct ValidationError {
    enum class Severity {
        WARNING,    // EN: Non-critical issue / FR: Problème non critique
        ERROR,      // EN: Critical validation failure / FR: Échec de validation critique
        FATAL       // EN: Fatal error preventing processing / FR: Erreur fatale empêchant le traitement
    };
    
    Severity severity{Severity::ERROR};               // EN: Error severity / FR: Sévérité de l'erreur
    std::string field_name;                           // EN: Field that failed validation / FR: Champ qui a échoué à la validation
    size_t row_number{0};                            // EN: Row number (1-based) / FR: Numéro de ligne (base 1)
    size_t column_number{0};                         // EN: Column number (1-based) / FR: Numéro de colonne (base 1)
    std::string message;                             // EN: Error message / FR: Message d'erreur
    std::string actual_value;                        // EN: Actual value that failed / FR: Valeur actuelle qui a échoué
    std::string expected_format;                     // EN: Expected format/constraint / FR: Format/contrainte attendu
    std::string context;                             // EN: Additional context / FR: Contexte additionnel
    
    ValidationError() = default;
    ValidationError(Severity sev, const std::string& field, size_t row, size_t col, 
                   const std::string& msg, const std::string& actual = "", const std::string& expected = "")
        : severity(sev), field_name(field), row_number(row), column_number(col), 
          message(msg), actual_value(actual), expected_format(expected) {}
};

// EN: Validation result summary
// FR: Résumé des résultats de validation
struct ValidationResult {
    bool is_valid{true};                             // EN: Overall validation status / FR: Statut de validation global
    size_t total_rows{0};                           // EN: Total number of rows processed / FR: Nombre total de lignes traitées
    size_t valid_rows{0};                           // EN: Number of valid rows / FR: Nombre de lignes valides
    size_t error_rows{0};                           // EN: Number of rows with errors / FR: Nombre de lignes avec erreurs
    size_t warning_rows{0};                         // EN: Number of rows with warnings / FR: Nombre de lignes avec avertissements
    std::vector<ValidationError> errors;            // EN: List of all validation errors / FR: Liste de toutes les erreurs de validation
    std::chrono::milliseconds validation_duration{0}; // EN: Time taken for validation / FR: Temps pris pour la validation
    std::unordered_map<std::string, size_t> field_error_counts; // EN: Error count per field / FR: Compte d'erreurs par champ
    SchemaVersion schema_version;                   // EN: Schema version used / FR: Version de schéma utilisée
    
    // EN: Get errors by severity
    // FR: Obtient les erreurs par sévérité
    std::vector<ValidationError> getErrorsBySeverity(ValidationError::Severity severity) const {
        std::vector<ValidationError> filtered;
        for (const auto& error : errors) {
            if (error.severity == severity) {
                filtered.push_back(error);
            }
        }
        return filtered;
    }
    
    // EN: Get summary statistics
    // FR: Obtient les statistiques de résumé
    double getSuccessRate() const {
        return total_rows > 0 ? static_cast<double>(valid_rows) / total_rows * 100.0 : 0.0;
    }
};

// EN: CSV schema definition with versioning support
// FR: Définition de schéma CSV avec support de versioning
class CsvSchema {
public:
    // EN: Constructor with schema name and version
    // FR: Constructeur avec nom de schéma et version
    explicit CsvSchema(const std::string& schema_name, const SchemaVersion& version = SchemaVersion{});
    
    // EN: Add field to schema
    // FR: Ajouter un champ au schéma
    CsvSchema& addField(const SchemaField& field);
    CsvSchema& addField(const std::string& name, DataType type, const FieldConstraints& constraints = {});
    
    // EN: Get schema fields
    // FR: Obtenir les champs de schéma
    const std::vector<SchemaField>& getFields() const { return fields_; }
    const SchemaField* getField(const std::string& name) const;
    const SchemaField* getFieldByPosition(size_t position) const;
    
    // EN: Schema metadata
    // FR: Métadonnées de schéma
    const std::string& getName() const { return name_; }
    const SchemaVersion& getVersion() const { return version_; }
    void setDescription(const std::string& description) { description_ = description; }
    const std::string& getDescription() const { return description_; }
    
    // EN: Schema configuration
    // FR: Configuration de schéma
    void setStrictMode(bool strict) { strict_mode_ = strict; }
    bool isStrictMode() const { return strict_mode_; }
    void setAllowExtraColumns(bool allow) { allow_extra_columns_ = allow; }
    bool getAllowExtraColumns() const { return allow_extra_columns_; }
    void setHeaderRequired(bool required) { header_required_ = required; }
    bool isHeaderRequired() const { return header_required_; }
    
    // EN: Version compatibility
    // FR: Compatibilité de version
    bool isCompatibleWith(const SchemaVersion& other_version) const;
    
    // EN: Schema serialization
    // FR: Sérialisation de schéma
    std::string toJson() const;
    static std::unique_ptr<CsvSchema> fromJson(const std::string& json_str);
    
    // EN: Schema validation
    // FR: Validation de schéma
    bool isValid() const;
    std::vector<std::string> getValidationIssues() const;

private:
    std::string name_;                               // EN: Schema name / FR: Nom du schéma
    SchemaVersion version_;                          // EN: Schema version / FR: Version du schéma
    std::string description_;                        // EN: Schema description / FR: Description du schéma
    std::vector<SchemaField> fields_;               // EN: Schema fields / FR: Champs du schéma
    std::unordered_map<std::string, size_t> field_map_; // EN: Field name to index mapping / FR: Mappage nom de champ vers index
    
    // EN: Schema validation options
    // FR: Options de validation de schéma
    bool strict_mode_{true};                        // EN: Strict validation mode / FR: Mode de validation strict
    bool allow_extra_columns_{false};              // EN: Allow extra columns not in schema / FR: Autoriser colonnes supplémentaires
    bool header_required_{true};                   // EN: Header row required / FR: Ligne d'en-tête requise
    
    // EN: Update internal field mapping
    // FR: Met à jour le mappage interne des champs
    void updateFieldMapping();
};

// EN: Main CSV Schema Validator class
// FR: Classe principale du validateur de schéma CSV
class CsvSchemaValidator {
public:
    // EN: Constructor
    // FR: Constructeur
    CsvSchemaValidator();
    
    // EN: Schema management
    // FR: Gestion de schéma
    void registerSchema(std::unique_ptr<CsvSchema> schema);
    const CsvSchema* getSchema(const std::string& name, const SchemaVersion& version = SchemaVersion{}) const;
    std::vector<std::string> getAvailableSchemas() const;
    std::vector<SchemaVersion> getSchemaVersions(const std::string& name) const;
    
    // EN: Validation methods
    // FR: Méthodes de validation
    ValidationResult validateCsvFile(const std::string& file_path, const std::string& schema_name, 
                                   const SchemaVersion& version = SchemaVersion{});
    ValidationResult validateCsvContent(const std::string& csv_content, const std::string& schema_name, 
                                      const SchemaVersion& version = SchemaVersion{});
    ValidationResult validateCsvStream(std::istream& stream, const std::string& schema_name, 
                                     const SchemaVersion& version = SchemaVersion{});
    
    // EN: Row-by-row validation
    // FR: Validation ligne par ligne
    bool validateHeader(const std::vector<std::string>& header, const CsvSchema& schema, ValidationResult& result);
    bool validateRow(const std::vector<std::string>& row, const CsvSchema& schema, size_t row_number, ValidationResult& result);
    
    // EN: Field validation
    // FR: Validation de champ
    bool validateField(const std::string& value, const SchemaField& field, size_t row_number, 
                      size_t column_number, ValidationResult& result);
    
    // EN: Configuration
    // FR: Configuration
    void setMaxErrorsPerField(size_t max_errors) { max_errors_per_field_ = max_errors; }
    size_t getMaxErrorsPerField() const { return max_errors_per_field_; }
    void setStopOnFirstError(bool stop) { stop_on_first_error_ = stop; }
    bool getStopOnFirstError() const { return stop_on_first_error_; }
    
    // EN: Custom validators
    // FR: Validateurs personnalisés
    void registerCustomValidator(const std::string& name, std::function<bool(const std::string&)> validator);
    std::function<bool(const std::string&)> getCustomValidator(const std::string& name) const;
    
    // EN: Statistics and reporting
    // FR: Statistiques et rapports
    std::string generateValidationReport(const ValidationResult& result, bool detailed = false) const;
    std::string generateSchemaDocumentation(const std::string& schema_name, const SchemaVersion& version = SchemaVersion{}) const;

private:
    // EN: Schema storage by name and version
    // FR: Stockage de schéma par nom et version
    std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<CsvSchema>>> schemas_;
    
    // EN: Custom validators
    // FR: Validateurs personnalisés
    std::unordered_map<std::string, std::function<bool(const std::string&)>> custom_validators_;
    
    // EN: Configuration options
    // FR: Options de configuration
    size_t max_errors_per_field_{10};               // EN: Max errors to report per field / FR: Max erreurs à rapporter par champ
    bool stop_on_first_error_{false};              // EN: Stop validation on first error / FR: Arrêter validation à la première erreur
    
    // EN: Internal validation helpers
    // FR: Helpers de validation interne
    bool validateString(const std::string& value, const SchemaField& field, size_t row_number, 
                       size_t column_number, ValidationResult& result);
    bool validateInteger(const std::string& value, const SchemaField& field, size_t row_number, 
                        size_t column_number, ValidationResult& result);
    bool validateFloat(const std::string& value, const SchemaField& field, size_t row_number, 
                      size_t column_number, ValidationResult& result);
    bool validateBoolean(const std::string& value, const SchemaField& field, size_t row_number, 
                        size_t column_number, ValidationResult& result);
    bool validateDate(const std::string& value, const SchemaField& field, size_t row_number, 
                     size_t column_number, ValidationResult& result);
    bool validateDateTime(const std::string& value, const SchemaField& field, size_t row_number, 
                         size_t column_number, ValidationResult& result);
    bool validateEmail(const std::string& value, const SchemaField& field, size_t row_number, 
                      size_t column_number, ValidationResult& result);
    bool validateUrl(const std::string& value, const SchemaField& field, size_t row_number, 
                    size_t column_number, ValidationResult& result);
    bool validateIpAddress(const std::string& value, const SchemaField& field, size_t row_number, 
                          size_t column_number, ValidationResult& result);
    bool validateUuid(const std::string& value, const SchemaField& field, size_t row_number, 
                     size_t column_number, ValidationResult& result);
    bool validateEnum(const std::string& value, const SchemaField& field, size_t row_number, 
                     size_t column_number, ValidationResult& result);
    bool validateCustom(const std::string& value, const SchemaField& field, size_t row_number, 
                       size_t column_number, ValidationResult& result);
    
    // EN: Utility functions
    // FR: Fonctions utilitaires
    std::vector<std::string> parseCsvRow(const std::string& row_str) const;
    void addValidationError(ValidationResult& result, ValidationError::Severity severity, 
                           const std::string& field_name, size_t row_number, size_t column_number,
                           const std::string& message, const std::string& actual_value = "",
                           const std::string& expected_format = "") const;
    bool isEmptyValue(const std::string& value) const;
    std::string trim(const std::string& str) const;
    std::string toLower(const std::string& str) const;
    
    // EN: Schema version management
    // FR: Gestion de version de schéma
    std::string versionToKey(const SchemaVersion& version) const;
    SchemaVersion keyToVersion(const std::string& key) const;
};

// EN: Utility functions for schema creation
// FR: Fonctions utilitaires pour création de schéma
namespace SchemaUtils {
    
    // EN: Create common field types
    // FR: Créer des types de champs courants
    SchemaField createStringField(const std::string& name, size_t position, bool required = true,
                                 size_t min_length = 0, size_t max_length = SIZE_MAX);
    SchemaField createIntegerField(const std::string& name, size_t position, bool required = true,
                                  int64_t min_value = std::numeric_limits<int64_t>::min(),
                                  int64_t max_value = std::numeric_limits<int64_t>::max());
    SchemaField createFloatField(const std::string& name, size_t position, bool required = true,
                                double min_value = std::numeric_limits<double>::lowest(),
                                double max_value = std::numeric_limits<double>::max());
    SchemaField createBooleanField(const std::string& name, size_t position, bool required = true);
    SchemaField createDateField(const std::string& name, size_t position, bool required = true);
    SchemaField createEmailField(const std::string& name, size_t position, bool required = true);
    SchemaField createUrlField(const std::string& name, size_t position, bool required = true);
    SchemaField createIpField(const std::string& name, size_t position, bool required = true);
    SchemaField createUuidField(const std::string& name, size_t position, bool required = true);
    SchemaField createEnumField(const std::string& name, size_t position, const std::vector<std::string>& values, bool required = true);
    
    // EN: Create schemas for BB-Pipeline modules
    // FR: Créer des schémas pour les modules BB-Pipeline
    std::unique_ptr<CsvSchema> createScopeSchema();              // data/scope.csv
    std::unique_ptr<CsvSchema> createSubdomainsSchema();         // 01_subdomains.csv
    std::unique_ptr<CsvSchema> createProbeSchema();              // 02_probe.csv
    std::unique_ptr<CsvSchema> createHeadlessSchema();           // 03_headless.csv
    std::unique_ptr<CsvSchema> createDiscoverySchema();          // 04_discovery.csv
    std::unique_ptr<CsvSchema> createJsIntelSchema();            // 05_jsintel.csv
    std::unique_ptr<CsvSchema> createApiCatalogSchema();         // 06_api_catalog.csv
    std::unique_ptr<CsvSchema> createApiFindingsSchema();        // 07_api_findings.csv
    std::unique_ptr<CsvSchema> createMobileIntelSchema();        // 08_mobile_intel.csv
    std::unique_ptr<CsvSchema> createChangesSchema();            // 09_changes.csv
    std::unique_ptr<CsvSchema> createFinalRankedSchema();        // 99_final_ranked.csv
    
    // EN: Version migration utilities
    // FR: Utilitaires de migration de version
    bool canMigrateSchema(const SchemaVersion& from, const SchemaVersion& to);
    std::unique_ptr<CsvSchema> migrateSchema(const CsvSchema& source, const SchemaVersion& target_version);
    
} // namespace SchemaUtils

} // namespace BBP::CSV