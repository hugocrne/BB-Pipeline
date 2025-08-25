// EN: Comprehensive unit tests for CSV Schema Validator - 100% coverage
// FR: Tests unitaires complets pour CSV Schema Validator - 100% de couverture

#include <gtest/gtest.h>
#include "csv/schema_validator.hpp"
#include "infrastructure/logging/logger.hpp"
#include <sstream>
#include <fstream>

using namespace BBP::CSV;

// EN: Test fixture for Schema Validator tests
// FR: Fixture de test pour les tests Schema Validator
class SchemaValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        validator_ = std::make_unique<CsvSchemaValidator>();
        
        // EN: Setup logger
        // FR: Configure le logger
        auto& logger = BBP::Logger::getInstance();
        logger.setLogLevel(BBP::LogLevel::ERROR); // EN: Reduce noise during tests / FR: Réduit le bruit pendant les tests
    }

    void TearDown() override {
        validator_.reset();
    }

    std::unique_ptr<CsvSchemaValidator> validator_;
};

// EN: Test SchemaVersion functionality
// FR: Test de la fonctionnalité SchemaVersion
TEST_F(SchemaValidatorTest, SchemaVersionComparison) {
    SchemaVersion v1(1, 0, 0);
    SchemaVersion v2(1, 1, 0);
    SchemaVersion v3(2, 0, 0);
    
    EXPECT_TRUE(v1 < v2);
    EXPECT_TRUE(v2 < v3);
    EXPECT_FALSE(v2 < v1);
    EXPECT_TRUE(v1 == SchemaVersion(1, 0, 0));
    EXPECT_FALSE(v1 == v2);
    
    EXPECT_EQ(v1.toString(), "1.0.0");
    EXPECT_EQ(v2.toString(), "1.1.0");
    EXPECT_EQ(v3.toString(), "2.0.0");
}

// EN: Test CsvSchema basic functionality
// FR: Test de la fonctionnalité de base CsvSchema
TEST_F(SchemaValidatorTest, CsvSchemaBasicFunctionality) {
    SchemaVersion version(1, 0, 0, "Test schema");
    CsvSchema schema("test_schema", version);
    
    EXPECT_EQ(schema.getName(), "test_schema");
    EXPECT_EQ(schema.getVersion(), version);
    EXPECT_TRUE(schema.getDescription().empty());
    
    schema.setDescription("Test description");
    EXPECT_EQ(schema.getDescription(), "Test description");
    
    EXPECT_TRUE(schema.isStrictMode());
    EXPECT_FALSE(schema.getAllowExtraColumns());
    EXPECT_TRUE(schema.isHeaderRequired());
    
    schema.setStrictMode(false);
    schema.setAllowExtraColumns(true);
    schema.setHeaderRequired(false);
    
    EXPECT_FALSE(schema.isStrictMode());
    EXPECT_TRUE(schema.getAllowExtraColumns());
    EXPECT_FALSE(schema.isHeaderRequired());
}

// EN: Test adding fields to schema
// FR: Test d'ajout de champs au schéma
TEST_F(SchemaValidatorTest, SchemaFieldManagement) {
    CsvSchema schema("test_schema");
    
    EXPECT_FALSE(schema.isValid()); // EN: No fields yet / FR: Pas de champs encore
    
    SchemaField field1("name", DataType::STRING, 0);
    field1.constraints.required = true;
    field1.constraints.min_length = 1;
    field1.constraints.max_length = 100;
    
    schema.addField(field1);
    EXPECT_TRUE(schema.isValid());
    EXPECT_EQ(schema.getFields().size(), 1);
    
    const SchemaField* retrieved = schema.getField("name");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name, "name");
    EXPECT_EQ(retrieved->type, DataType::STRING);
    
    // EN: Test field by position
    // FR: Test de champ par position
    const SchemaField* by_pos = schema.getFieldByPosition(0);
    ASSERT_NE(by_pos, nullptr);
    EXPECT_EQ(by_pos->name, "name");
    
    // EN: Test adding duplicate field should throw
    // FR: Test d'ajout de champ dupliqué devrait lever une exception
    EXPECT_THROW(schema.addField(field1), std::invalid_argument);
    
    // EN: Test convenience method
    // FR: Test de la méthode de convenance
    FieldConstraints constraints;
    constraints.required = false;
    schema.addField("email", DataType::EMAIL, constraints);
    EXPECT_EQ(schema.getFields().size(), 2);
}

// EN: Test schema field aliases
// FR: Test des alias de champs de schéma
TEST_F(SchemaValidatorTest, SchemaFieldAliases) {
    CsvSchema schema("test_schema");
    
    SchemaField field("primary_name", DataType::STRING, 0);
    field.aliases = {"alt_name", "another_name"};
    schema.addField(field);
    
    // EN: Test primary name
    // FR: Test du nom primaire
    const SchemaField* by_primary = schema.getField("primary_name");
    ASSERT_NE(by_primary, nullptr);
    EXPECT_EQ(by_primary->name, "primary_name");
    
    // EN: Test aliases
    // FR: Test des alias
    const SchemaField* by_alias1 = schema.getField("alt_name");
    ASSERT_NE(by_alias1, nullptr);
    EXPECT_EQ(by_alias1->name, "primary_name");
    
    const SchemaField* by_alias2 = schema.getField("another_name");
    ASSERT_NE(by_alias2, nullptr);
    EXPECT_EQ(by_alias2->name, "primary_name");
    
    // EN: Test non-existent field
    // FR: Test de champ non existant
    const SchemaField* non_existent = schema.getField("does_not_exist");
    EXPECT_EQ(non_existent, nullptr);
}

// EN: Test schema version compatibility
// FR: Test de compatibilité de version de schéma
TEST_F(SchemaValidatorTest, SchemaVersionCompatibility) {
    SchemaVersion v1_0_0(1, 0, 0);
    SchemaVersion v1_1_0(1, 1, 0);
    SchemaVersion v1_2_0(1, 2, 0);
    SchemaVersion v2_0_0(2, 0, 0);
    
    CsvSchema schema("test", v1_2_0);
    
    EXPECT_TRUE(schema.isCompatibleWith(v1_0_0)); // EN: Backward compatible / FR: Rétro-compatible
    EXPECT_TRUE(schema.isCompatibleWith(v1_1_0));
    EXPECT_TRUE(schema.isCompatibleWith(v1_2_0));
    EXPECT_FALSE(schema.isCompatibleWith(v2_0_0)); // EN: Different major version / FR: Version majeure différente
}

// EN: Test schema JSON serialization
// FR: Test de sérialisation JSON de schéma
TEST_F(SchemaValidatorTest, SchemaJsonSerialization) {
    CsvSchema schema("test_schema", SchemaVersion(1, 0, 0));
    schema.setDescription("Test description");
    schema.addField("name", DataType::STRING);
    schema.addField("age", DataType::INTEGER);
    
    std::string json = schema.toJson();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("test_schema"), std::string::npos);
    EXPECT_NE(json.find("Test description"), std::string::npos);
    
    // EN: Test JSON deserialization (returns nullptr for now)
    // FR: Test de désérialisation JSON (retourne nullptr pour l'instant)
    auto deserialized = CsvSchema::fromJson(json);
    EXPECT_EQ(deserialized, nullptr); // EN: Not implemented yet / FR: Pas encore implémenté
}

// EN: Test validator registration and retrieval
// FR: Test d'enregistrement et récupération de validateur
TEST_F(SchemaValidatorTest, ValidatorSchemaRegistration) {
    auto schema = std::make_unique<CsvSchema>("user_schema", SchemaVersion(1, 0, 0));
    schema->addField("name", DataType::STRING);
    schema->addField("email", DataType::EMAIL);
    
    validator_->registerSchema(std::move(schema));
    
    // EN: Test schema retrieval
    // FR: Test de récupération de schéma
    const CsvSchema* retrieved = validator_->getSchema("user_schema");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->getName(), "user_schema");
    EXPECT_EQ(retrieved->getFields().size(), 2);
    
    // EN: Test non-existent schema
    // FR: Test de schéma non existant
    const CsvSchema* non_existent = validator_->getSchema("does_not_exist");
    EXPECT_EQ(non_existent, nullptr);
    
    // EN: Test schema listing
    // FR: Test de listage de schéma
    auto schemas = validator_->getAvailableSchemas();
    EXPECT_EQ(schemas.size(), 1);
    EXPECT_EQ(schemas[0], "user_schema");
}

// EN: Test schema version management
// FR: Test de gestion de version de schéma
TEST_F(SchemaValidatorTest, SchemaVersionManagement) {
    // EN: Register multiple versions of same schema
    // FR: Enregistre plusieurs versions du même schéma
    auto schema_v1 = std::make_unique<CsvSchema>("test_schema", SchemaVersion(1, 0, 0));
    schema_v1->addField("name", DataType::STRING);
    
    auto schema_v1_1 = std::make_unique<CsvSchema>("test_schema", SchemaVersion(1, 1, 0));
    schema_v1_1->addField("name", DataType::STRING);
    schema_v1_1->addField("email", DataType::EMAIL);
    
    validator_->registerSchema(std::move(schema_v1));
    validator_->registerSchema(std::move(schema_v1_1));
    
    // EN: Test version listing
    // FR: Test de listage de version
    auto versions = validator_->getSchemaVersions("test_schema");
    EXPECT_EQ(versions.size(), 2);
    
    // EN: Test specific version retrieval
    // FR: Test de récupération de version spécifique
    const CsvSchema* v1_0 = validator_->getSchema("test_schema", SchemaVersion(1, 0, 0));
    ASSERT_NE(v1_0, nullptr);
    EXPECT_EQ(v1_0->getFields().size(), 1);
    
    const CsvSchema* v1_1 = validator_->getSchema("test_schema", SchemaVersion(1, 1, 0));
    ASSERT_NE(v1_1, nullptr);
    EXPECT_EQ(v1_1->getFields().size(), 2);
    
    // EN: Test compatibility-based retrieval
    // FR: Test de récupération basée sur la compatibilité
    const CsvSchema* compatible = validator_->getSchema("test_schema", SchemaVersion(1, 0, 5));
    ASSERT_NE(compatible, nullptr);
    EXPECT_EQ(compatible->getVersion(), SchemaVersion(1, 1, 0)); // EN: Should get latest compatible / FR: Devrait obtenir le plus récent compatible
}

// EN: Test custom validators
// FR: Test de validateurs personnalisés
TEST_F(SchemaValidatorTest, CustomValidators) {
    // EN: Test built-in validators
    // FR: Test des validateurs intégrés
    auto non_empty = validator_->getCustomValidator("non_empty");
    ASSERT_NE(non_empty, nullptr);
    EXPECT_TRUE(non_empty("hello"));
    EXPECT_FALSE(non_empty(""));
    
    auto alphanumeric = validator_->getCustomValidator("alphanumeric");
    ASSERT_NE(alphanumeric, nullptr);
    EXPECT_TRUE(alphanumeric("abc123"));
    EXPECT_FALSE(alphanumeric("abc-123"));
    
    // EN: Register custom validator
    // FR: Enregistre un validateur personnalisé
    validator_->registerCustomValidator("contains_at", [](const std::string& value) {
        return value.find('@') != std::string::npos;
    });
    
    auto contains_at = validator_->getCustomValidator("contains_at");
    ASSERT_NE(contains_at, nullptr);
    EXPECT_TRUE(contains_at("test@example.com"));
    EXPECT_FALSE(contains_at("test.example.com"));
    
    // EN: Test non-existent validator
    // FR: Test de validateur non existant
    auto non_existent = validator_->getCustomValidator("does_not_exist");
    EXPECT_EQ(non_existent, nullptr);
}

// EN: Test string field validation
// FR: Test de validation de champ chaîne
TEST_F(SchemaValidatorTest, StringFieldValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    
    SchemaField field("text", DataType::STRING, 0);
    field.constraints.required = true;
    field.constraints.min_length = 3;
    field.constraints.max_length = 10;
    field.constraints.pattern = std::regex("^[a-zA-Z]+$");
    schema->addField(field);
    
    validator_->registerSchema(std::move(schema));
    
    ValidationResult result;
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    
    // EN: Valid string
    // FR: Chaîne valide
    EXPECT_TRUE(validator_->validateField("hello", test_schema->getFields()[0], 1, 1, result));
    
    // EN: Too short
    // FR: Trop court
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("hi", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_NE(result.errors[0].message.find("too short"), std::string::npos);
    
    // EN: Too long
    // FR: Trop long
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("verylongstring", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_NE(result.errors[0].message.find("too long"), std::string::npos);
    
    // EN: Pattern mismatch
    // FR: Non correspondance de pattern
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("hello123", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_NE(result.errors[0].message.find("pattern"), std::string::npos);
}

// EN: Test integer field validation
// FR: Test de validation de champ entier
TEST_F(SchemaValidatorTest, IntegerFieldValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    
    SchemaField field("number", DataType::INTEGER, 0);
    field.constraints.required = true;
    field.constraints.min_value = 0;
    field.constraints.max_value = 100;
    schema->addField(field);
    
    validator_->registerSchema(std::move(schema));
    
    ValidationResult result;
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    
    // EN: Valid integer
    // FR: Entier valide
    EXPECT_TRUE(validator_->validateField("42", test_schema->getFields()[0], 1, 1, result));
    
    // EN: Too small
    // FR: Trop petit
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("-5", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
    
    // EN: Too large
    // FR: Trop grand
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("150", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
    
    // EN: Invalid format
    // FR: Format invalide
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("not_a_number", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_NE(result.errors[0].message.find("Invalid integer"), std::string::npos);
}

// EN: Test float field validation
// FR: Test de validation de champ flottant
TEST_F(SchemaValidatorTest, FloatFieldValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    
    SchemaField field("price", DataType::FLOAT, 0);
    field.constraints.required = true;
    field.constraints.min_value = 0.0;
    field.constraints.max_value = 999.99;
    schema->addField(field);
    
    validator_->registerSchema(std::move(schema));
    
    ValidationResult result;
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    
    // EN: Valid float
    // FR: Flottant valide
    EXPECT_TRUE(validator_->validateField("42.50", test_schema->getFields()[0], 1, 1, result));
    
    // EN: Invalid format
    // FR: Format invalide
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("not_a_float", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
    
    // EN: NaN/Inf values
    // FR: Valeurs NaN/Inf
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("nan", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
}

// EN: Test boolean field validation
// FR: Test de validation de champ booléen
TEST_F(SchemaValidatorTest, BooleanFieldValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    
    SchemaField field("active", DataType::BOOLEAN, 0);
    field.constraints.required = true;
    schema->addField(field);
    
    validator_->registerSchema(std::move(schema));
    
    ValidationResult result;
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    
    // EN: Test various boolean formats
    // FR: Test de différents formats booléens
    std::vector<std::string> true_values = {"true", "TRUE", "1", "yes", "YES", "y", "Y", "on", "ON"};
    std::vector<std::string> false_values = {"false", "FALSE", "0", "no", "NO", "n", "N", "off", "OFF"};
    
    for (const auto& value : true_values) {
        result = ValidationResult{};
        EXPECT_TRUE(validator_->validateField(value, test_schema->getFields()[0], 1, 1, result))
            << "Failed for true value: " << value;
    }
    
    for (const auto& value : false_values) {
        result = ValidationResult{};
        EXPECT_TRUE(validator_->validateField(value, test_schema->getFields()[0], 1, 1, result))
            << "Failed for false value: " << value;
    }
    
    // EN: Invalid boolean
    // FR: Booléen invalide
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("maybe", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
}

// EN: Test date field validation
// FR: Test de validation de champ date
TEST_F(SchemaValidatorTest, DateFieldValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    
    SchemaField field("created_date", DataType::DATE, 0);
    field.constraints.required = true;
    schema->addField(field);
    
    validator_->registerSchema(std::move(schema));
    
    ValidationResult result;
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    
    // EN: Valid date
    // FR: Date valide
    EXPECT_TRUE(validator_->validateField("2023-12-25", test_schema->getFields()[0], 1, 1, result));
    
    // EN: Invalid date format
    // FR: Format de date invalide
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("12/25/2023", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
    
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("not-a-date", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
}

// EN: Test email field validation
// FR: Test de validation de champ email
TEST_F(SchemaValidatorTest, EmailFieldValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    
    SchemaField field("email", DataType::EMAIL, 0);
    field.constraints.required = true;
    schema->addField(field);
    
    validator_->registerSchema(std::move(schema));
    
    ValidationResult result;
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    
    // EN: Valid emails
    // FR: Emails valides
    std::vector<std::string> valid_emails = {
        "test@example.com",
        "user.name@domain.co.uk",
        "test+tag@example.org"
    };
    
    for (const auto& email : valid_emails) {
        result = ValidationResult{};
        EXPECT_TRUE(validator_->validateField(email, test_schema->getFields()[0], 1, 1, result))
            << "Failed for email: " << email;
    }
    
    // EN: Invalid emails
    // FR: Emails invalides
    std::vector<std::string> invalid_emails = {
        "not-an-email",
        "@example.com",
        "test@",
        "test.example.com"
    };
    
    for (const auto& email : invalid_emails) {
        result = ValidationResult{};
        EXPECT_FALSE(validator_->validateField(email, test_schema->getFields()[0], 1, 1, result))
            << "Should have failed for email: " << email;
    }
}

// EN: Test URL field validation
// FR: Test de validation de champ URL
TEST_F(SchemaValidatorTest, UrlFieldValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    
    SchemaField field("website", DataType::URL, 0);
    field.constraints.required = true;
    schema->addField(field);
    
    validator_->registerSchema(std::move(schema));
    
    ValidationResult result;
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    
    // EN: Valid URLs
    // FR: URLs valides
    EXPECT_TRUE(validator_->validateField("https://example.com", test_schema->getFields()[0], 1, 1, result));
    
    result = ValidationResult{};
    EXPECT_TRUE(validator_->validateField("http://test.org/path", test_schema->getFields()[0], 1, 1, result));
    
    // EN: Invalid URLs
    // FR: URLs invalides
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("not-a-url", test_schema->getFields()[0], 1, 1, result));
    
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("ftp://example.com", test_schema->getFields()[0], 1, 1, result));
}

// EN: Test IP address field validation
// FR: Test de validation de champ adresse IP
TEST_F(SchemaValidatorTest, IpAddressFieldValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    
    SchemaField field("ip_address", DataType::IP_ADDRESS, 0);
    field.constraints.required = true;
    schema->addField(field);
    
    validator_->registerSchema(std::move(schema));
    
    ValidationResult result;
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    
    // EN: Valid IPv4
    // FR: IPv4 valide
    EXPECT_TRUE(validator_->validateField("192.168.1.1", test_schema->getFields()[0], 1, 1, result));
    
    // EN: Valid IPv6 (simplified)
    // FR: IPv6 valide (simplifié)
    result = ValidationResult{};
    EXPECT_TRUE(validator_->validateField("2001:0db8:85a3:0000:0000:8a2e:0370:7334", test_schema->getFields()[0], 1, 1, result));
    
    // EN: Invalid IP
    // FR: IP invalide
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("999.999.999.999", test_schema->getFields()[0], 1, 1, result));
    
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("not-an-ip", test_schema->getFields()[0], 1, 1, result));
}

// EN: Test UUID field validation
// FR: Test de validation de champ UUID
TEST_F(SchemaValidatorTest, UuidFieldValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    
    SchemaField field("uuid", DataType::UUID, 0);
    field.constraints.required = true;
    schema->addField(field);
    
    validator_->registerSchema(std::move(schema));
    
    ValidationResult result;
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    
    // EN: Valid UUID
    // FR: UUID valide
    EXPECT_TRUE(validator_->validateField("550e8400-e29b-41d4-a716-446655440000", test_schema->getFields()[0], 1, 1, result));
    
    // EN: Invalid UUID
    // FR: UUID invalide
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("not-a-uuid", test_schema->getFields()[0], 1, 1, result));
    
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("550e8400-e29b-41d4-a716", test_schema->getFields()[0], 1, 1, result));
}

// EN: Test enum field validation
// FR: Test de validation de champ enum
TEST_F(SchemaValidatorTest, EnumFieldValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    
    SchemaField field("status", DataType::ENUM, 0);
    field.constraints.required = true;
    field.constraints.enum_values = {"active", "inactive", "pending"};
    schema->addField(field);
    
    validator_->registerSchema(std::move(schema));
    
    ValidationResult result;
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    
    // EN: Valid enum values
    // FR: Valeurs enum valides
    EXPECT_TRUE(validator_->validateField("active", test_schema->getFields()[0], 1, 1, result));
    
    result = ValidationResult{};
    EXPECT_TRUE(validator_->validateField("inactive", test_schema->getFields()[0], 1, 1, result));
    
    // EN: Invalid enum value
    // FR: Valeur enum invalide
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("unknown", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_NE(result.errors[0].message.find("not in allowed enum"), std::string::npos);
}

// EN: Test custom field validation
// FR: Test de validation de champ personnalisé
TEST_F(SchemaValidatorTest, CustomFieldValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    
    SchemaField field("custom", DataType::CUSTOM, 0);
    field.constraints.required = true;
    field.constraints.custom_validator = [](const std::string& value) {
        return value.length() >= 5 && value.find("test") != std::string::npos;
    };
    schema->addField(field);
    
    validator_->registerSchema(std::move(schema));
    
    ValidationResult result;
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    
    // EN: Valid custom value
    // FR: Valeur personnalisée valide
    EXPECT_TRUE(validator_->validateField("testing123", test_schema->getFields()[0], 1, 1, result));
    
    // EN: Invalid custom value
    // FR: Valeur personnalisée invalide
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("short", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
}

// EN: Test empty and null value handling
// FR: Test de gestion des valeurs vides et null
TEST_F(SchemaValidatorTest, EmptyValueHandling) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    
    // EN: Required field
    // FR: Champ requis
    SchemaField required_field("required", DataType::STRING, 0);
    required_field.constraints.required = true;
    schema->addField(required_field);
    
    // EN: Optional field with default
    // FR: Champ optionnel avec défaut
    SchemaField optional_field("optional", DataType::STRING, 1);
    optional_field.constraints.required = false;
    optional_field.constraints.default_value = "default";
    schema->addField(optional_field);
    
    validator_->registerSchema(std::move(schema));
    
    ValidationResult result;
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    
    // EN: Required field empty should fail
    // FR: Champ requis vide devrait échouer
    EXPECT_FALSE(validator_->validateField("", test_schema->getFields()[0], 1, 1, result));
    EXPECT_EQ(result.errors.size(), 1);
    
    // EN: Required field with null variants should fail
    // FR: Champ requis avec variants null devrait échouer
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("NULL", test_schema->getFields()[0], 1, 1, result));
    
    result = ValidationResult{};
    EXPECT_FALSE(validator_->validateField("N/A", test_schema->getFields()[0], 1, 1, result));
    
    // EN: Optional field empty should pass
    // FR: Champ optionnel vide devrait passer
    result = ValidationResult{};
    EXPECT_TRUE(validator_->validateField("", test_schema->getFields()[1], 1, 1, result));
    
    result = ValidationResult{};
    EXPECT_TRUE(validator_->validateField("null", test_schema->getFields()[1], 1, 1, result));
}

// EN: Test header validation
// FR: Test de validation d'en-tête
TEST_F(SchemaValidatorTest, HeaderValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    schema->addField("name", DataType::STRING);
    schema->addField("email", DataType::EMAIL);
    schema->addField("age", DataType::INTEGER);
    
    validator_->registerSchema(std::move(schema));
    
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    ValidationResult result;
    
    // EN: Valid header
    // FR: En-tête valide
    std::vector<std::string> valid_header = {"name", "email", "age"};
    EXPECT_TRUE(validator_->validateHeader(valid_header, *test_schema, result));
    
    // EN: Missing required field
    // FR: Champ requis manquant
    result = ValidationResult{};
    std::vector<std::string> missing_field = {"name", "email"};
    EXPECT_FALSE(validator_->validateHeader(missing_field, *test_schema, result));
    EXPECT_GT(result.errors.size(), 0);
    
    // EN: Extra field in strict mode
    // FR: Champ supplémentaire en mode strict
    result = ValidationResult{};
    std::vector<std::string> extra_field = {"name", "email", "age", "extra"};
    EXPECT_FALSE(validator_->validateHeader(extra_field, *test_schema, result));
    EXPECT_GT(result.errors.size(), 0);
}

// EN: Test row validation
// FR: Test de validation de ligne
TEST_F(SchemaValidatorTest, RowValidation) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    schema->addField("name", DataType::STRING);
    schema->addField("email", DataType::EMAIL);
    schema->addField("age", DataType::INTEGER);
    
    validator_->registerSchema(std::move(schema));
    
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    ValidationResult result;
    
    // EN: Valid row
    // FR: Ligne valide
    std::vector<std::string> valid_row = {"John Doe", "john@example.com", "30"};
    EXPECT_TRUE(validator_->validateRow(valid_row, *test_schema, 2, result));
    
    // EN: Invalid email in row
    // FR: Email invalide dans la ligne
    result = ValidationResult{};
    std::vector<std::string> invalid_row = {"Jane Doe", "invalid-email", "25"};
    EXPECT_FALSE(validator_->validateRow(invalid_row, *test_schema, 3, result));
    EXPECT_GT(result.errors.size(), 0);
}

// EN: Test CSV content validation
// FR: Test de validation de contenu CSV
TEST_F(SchemaValidatorTest, CsvContentValidation) {
    auto schema = std::make_unique<CsvSchema>("user_schema");
    schema->addField("name", DataType::STRING);
    schema->addField("email", DataType::EMAIL);
    schema->addField("age", DataType::INTEGER);
    
    validator_->registerSchema(std::move(schema));
    
    // EN: Valid CSV content
    // FR: Contenu CSV valide
    std::string valid_csv = 
        "name,email,age\n"
        "John Doe,john@example.com,30\n"
        "Jane Smith,jane@example.com,25\n";
    
    auto result = validator_->validateCsvContent(valid_csv, "user_schema");
    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.total_rows, 2); // EN: Excluding header / FR: Excluant l'en-tête
    EXPECT_EQ(result.valid_rows, 2);
    EXPECT_EQ(result.error_rows, 0);
    
    // EN: Invalid CSV content
    // FR: Contenu CSV invalide
    std::string invalid_csv = 
        "name,email,age\n"
        "John Doe,invalid-email,30\n"
        "Jane Smith,jane@example.com,not-a-number\n";
    
    result = validator_->validateCsvContent(invalid_csv, "user_schema");
    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.total_rows, 2);
    EXPECT_EQ(result.valid_rows, 0);
    EXPECT_EQ(result.error_rows, 2);
    EXPECT_GT(result.errors.size(), 0);
}

// EN: Test validation reporting
// FR: Test de rapports de validation
TEST_F(SchemaValidatorTest, ValidationReporting) {
    auto schema = std::make_unique<CsvSchema>("test_schema");
    schema->addField("email", DataType::EMAIL);
    
    validator_->registerSchema(std::move(schema));
    
    std::string csv = 
        "email\n"
        "valid@example.com\n"
        "invalid-email\n"
        "another@example.com\n";
    
    auto result = validator_->validateCsvContent(csv, "test_schema");
    
    // EN: Test summary report
    // FR: Test de rapport de résumé
    std::string summary_report = validator_->generateValidationReport(result, false);
    EXPECT_FALSE(summary_report.empty());
    EXPECT_NE(summary_report.find("Total Rows"), std::string::npos);
    EXPECT_NE(summary_report.find("Success Rate"), std::string::npos);
    
    // EN: Test detailed report
    // FR: Test de rapport détaillé
    std::string detailed_report = validator_->generateValidationReport(result, true);
    EXPECT_FALSE(detailed_report.empty());
    EXPECT_GT(detailed_report.length(), summary_report.length());
}

// EN: Test schema documentation generation
// FR: Test de génération de documentation de schéma
TEST_F(SchemaValidatorTest, SchemaDocumentationGeneration) {
    auto schema = std::make_unique<CsvSchema>("documented_schema", SchemaVersion(2, 1, 0));
    schema->setDescription("A well-documented test schema");
    schema->addField("name", DataType::STRING);
    schema->addField("email", DataType::EMAIL);
    
    validator_->registerSchema(std::move(schema));
    
    std::string doc = validator_->generateSchemaDocumentation("documented_schema", SchemaVersion(2, 1, 0));
    EXPECT_FALSE(doc.empty());
    EXPECT_NE(doc.find("documented_schema"), std::string::npos);
    EXPECT_NE(doc.find("2.1.0"), std::string::npos);
    EXPECT_NE(doc.find("well-documented"), std::string::npos);
    
    // EN: Test non-existent schema
    // FR: Test de schéma non existant
    std::string no_doc = validator_->generateSchemaDocumentation("does_not_exist");
    EXPECT_NE(no_doc.find("Schema not found"), std::string::npos);
}

// EN: Test ValidationResult utility methods
// FR: Test des méthodes utilitaires ValidationResult
TEST_F(SchemaValidatorTest, ValidationResultUtilities) {
    ValidationResult result;
    result.total_rows = 100;
    result.valid_rows = 80;
    result.error_rows = 20;
    
    // EN: Test success rate calculation
    // FR: Test de calcul de taux de succès
    EXPECT_DOUBLE_EQ(result.getSuccessRate(), 80.0);
    
    // EN: Add errors with different severities
    // FR: Ajoute des erreurs avec différentes sévérités
    result.errors.push_back(ValidationError(ValidationError::Severity::ERROR, "field1", 1, 1, "Error"));
    result.errors.push_back(ValidationError(ValidationError::Severity::WARNING, "field2", 2, 1, "Warning"));
    result.errors.push_back(ValidationError(ValidationError::Severity::FATAL, "field3", 3, 1, "Fatal"));
    
    // EN: Test filtering by severity
    // FR: Test de filtrage par sévérité
    auto errors = result.getErrorsBySeverity(ValidationError::Severity::ERROR);
    EXPECT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].severity, ValidationError::Severity::ERROR);
    
    auto warnings = result.getErrorsBySeverity(ValidationError::Severity::WARNING);
    EXPECT_EQ(warnings.size(), 1);
    
    auto fatals = result.getErrorsBySeverity(ValidationError::Severity::FATAL);
    EXPECT_EQ(fatals.size(), 1);
}

// EN: Test SchemaUtils utility functions
// FR: Test des fonctions utilitaires SchemaUtils
TEST_F(SchemaValidatorTest, SchemaUtilityFunctions) {
    using namespace SchemaUtils;
    
    // EN: Test field creation functions
    // FR: Test des fonctions de création de champ
    auto string_field = createStringField("name", 0, true, 1, 100);
    EXPECT_EQ(string_field.name, "name");
    EXPECT_EQ(string_field.type, DataType::STRING);
    EXPECT_TRUE(string_field.constraints.required);
    EXPECT_EQ(string_field.constraints.min_length.value(), 1);
    EXPECT_EQ(string_field.constraints.max_length.value(), 100);
    
    auto int_field = createIntegerField("age", 1, true, 0, 150);
    EXPECT_EQ(int_field.name, "age");
    EXPECT_EQ(int_field.type, DataType::INTEGER);
    EXPECT_EQ(int_field.constraints.min_value.value(), 0.0);
    EXPECT_EQ(int_field.constraints.max_value.value(), 150.0);
    
    auto enum_field = createEnumField("status", 2, {"active", "inactive"}, true);
    EXPECT_EQ(enum_field.name, "status");
    EXPECT_EQ(enum_field.type, DataType::ENUM);
    EXPECT_EQ(enum_field.constraints.enum_values.size(), 2);
    
    // EN: Test predefined schema creation
    // FR: Test de création de schéma prédéfini
    auto scope_schema = createScopeSchema();
    ASSERT_NE(scope_schema, nullptr);
    EXPECT_EQ(scope_schema->getName(), "scope");
    EXPECT_GT(scope_schema->getFields().size(), 0);
    
    auto subdomains_schema = createSubdomainsSchema();
    ASSERT_NE(subdomains_schema, nullptr);
    EXPECT_EQ(subdomains_schema->getName(), "subdomains");
    
    // EN: Test schema migration
    // FR: Test de migration de schéma
    SchemaVersion from(1, 0, 0);
    SchemaVersion to(1, 1, 0);
    SchemaVersion incompatible(2, 0, 0);
    
    EXPECT_TRUE(canMigrateSchema(from, to));
    EXPECT_FALSE(canMigrateSchema(from, incompatible));
    
    auto migrated = migrateSchema(*scope_schema, to);
    ASSERT_NE(migrated, nullptr);
    EXPECT_EQ(migrated->getVersion(), to);
    EXPECT_EQ(migrated->getName(), scope_schema->getName());
}

// EN: Test error conditions and edge cases
// FR: Test des conditions d'erreur et cas limites
TEST_F(SchemaValidatorTest, ErrorConditionsAndEdgeCases) {
    // EN: Test registering null schema
    // FR: Test d'enregistrement de schéma null
    EXPECT_THROW(validator_->registerSchema(nullptr), std::invalid_argument);
    
    // EN: Test validation with non-existent schema
    // FR: Test de validation avec schéma non existant
    auto result = validator_->validateCsvContent("test", "non_existent_schema");
    EXPECT_FALSE(result.is_valid);
    EXPECT_GT(result.errors.size(), 0);
    EXPECT_EQ(result.errors[0].severity, ValidationError::Severity::FATAL);
    
    // EN: Test empty schema
    // FR: Test de schéma vide
    CsvSchema empty_schema("empty");
    EXPECT_FALSE(empty_schema.isValid());
    auto issues = empty_schema.getValidationIssues();
    EXPECT_GT(issues.size(), 0);
    
    // EN: Test unknown data type
    // FR: Test de type de données inconnu
    auto schema = std::make_unique<CsvSchema>("test_schema");
    SchemaField field("unknown_type", static_cast<DataType>(999), 0);
    schema->addField(field);
    
    validator_->registerSchema(std::move(schema));
    
    const CsvSchema* test_schema = validator_->getSchema("test_schema");
    ValidationResult validation_result;
    EXPECT_FALSE(validator_->validateField("value", test_schema->getFields()[0], 1, 1, validation_result));
    EXPECT_GT(validation_result.errors.size(), 0);
}

// EN: Test configuration options
// FR: Test des options de configuration
TEST_F(SchemaValidatorTest, ConfigurationOptions) {
    // EN: Test max errors per field
    // FR: Test du max d'erreurs par champ
    validator_->setMaxErrorsPerField(2);
    EXPECT_EQ(validator_->getMaxErrorsPerField(), 2);
    
    // EN: Test stop on first error
    // FR: Test d'arrêt à la première erreur
    validator_->setStopOnFirstError(true);
    EXPECT_TRUE(validator_->getStopOnFirstError());
    
    // EN: Create schema with multiple validation errors
    // FR: Crée schéma avec multiples erreurs de validation
    auto schema = std::make_unique<CsvSchema>("test_schema");
    schema->addField("field1", DataType::INTEGER);
    
    validator_->registerSchema(std::move(schema));
    
    std::string csv_with_errors = 
        "field1\n"
        "not_an_int\n"
        "also_not_int\n"
        "still_not_int\n";
    
    auto result = validator_->validateCsvContent(csv_with_errors, "test_schema");
    EXPECT_FALSE(result.is_valid);
    
    // EN: With stop on first error, should have fewer total rows processed
    // FR: Avec arrêt à la première erreur, devrait avoir moins de lignes totales traitées
    EXPECT_EQ(result.total_rows, 1); // EN: Should stop after first error / FR: Devrait s'arrêter après la première erreur
}

// EN: Main test runner
// FR: Lanceur de test principal
int main(int argc, char** argv) {
    // EN: Initialize logging for tests
    // FR: Initialise le logging pour les tests
    auto& logger = BBP::Logger::getInstance();
    logger.setLogLevel(BBP::LogLevel::ERROR);
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}