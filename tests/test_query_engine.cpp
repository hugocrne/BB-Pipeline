#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <vector>
#include <chrono>
#include "csv/query_engine.hpp"

using namespace BBP::CSV;
using namespace testing;

// EN: Test fixture for Query Engine tests
// FR: Fixture de test pour les tests Query Engine
class QueryEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // EN: Create temporary directory for test files
        // FR: Créer un répertoire temporaire pour les fichiers de test
        test_dir = std::filesystem::temp_directory_path() / "query_engine_test";
        std::filesystem::create_directories(test_dir);
        
        // EN: Initialize query engine
        // FR: Initialiser le moteur de requêtes
        QueryEngine::Config config;
        config.enable_query_cache = true;
        config.auto_index = true;
        config.max_memory_mb = 100;
        engine = std::make_unique<QueryEngine>(config);
        
        // EN: Create sample data
        // FR: Créer des données d'exemple
        createSampleData();
    }
    
    void TearDown() override {
        // EN: Clean up temporary files
        // FR: Nettoyer les fichiers temporaires
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }
    
    // EN: Helper function to create CSV file
    // FR: Fonction d'aide pour créer un fichier CSV
    void createCSVFile(const std::string& filename, const std::vector<std::string>& lines) {
        std::ofstream file(test_dir / filename);
        for (const auto& line : lines) {
            file << line << "\n";
        }
    }
    
    // EN: Create sample test data
    // FR: Créer des données de test d'exemple
    void createSampleData() {
        // EN: Employee data
        // FR: Données d'employés
        std::vector<std::string> employee_headers = {"id", "name", "email", "department", "salary", "age"};
        std::vector<std::vector<std::string>> employee_data = {
            {"1", "Alice Johnson", "alice@company.com", "Engineering", "75000", "28"},
            {"2", "Bob Smith", "bob@company.com", "Marketing", "65000", "32"},
            {"3", "Charlie Brown", "charlie@company.com", "Engineering", "80000", "29"},
            {"4", "Diana Ross", "diana@company.com", "HR", "60000", "35"},
            {"5", "Eve Wilson", "eve@company.com", "Engineering", "85000", "27"}
        };
        engine->registerTable("employees", employee_headers, employee_data);
        
        // EN: Product data
        // FR: Données de produits
        std::vector<std::string> product_headers = {"id", "name", "category", "price", "stock"};
        std::vector<std::vector<std::string>> product_data = {
            {"1", "Laptop", "Electronics", "999.99", "50"},
            {"2", "Mouse", "Electronics", "29.99", "200"},
            {"3", "Keyboard", "Electronics", "79.99", "150"},
            {"4", "Chair", "Furniture", "199.99", "25"},
            {"5", "Desk", "Furniture", "399.99", "10"}
        };
        engine->registerTable("products", product_headers, product_data);
        
        // EN: Create CSV files for file loading tests
        // FR: Créer fichiers CSV pour tests de chargement de fichiers
        createCSVFile("test_data.csv", {
            "id,name,category,value",
            "1,Item A,Cat1,100",
            "2,Item B,Cat2,200",
            "3,Item C,Cat1,150",
            "4,Item D,Cat3,300"
        });
    }
    
    std::filesystem::path test_dir;
    std::unique_ptr<QueryEngine> engine;
};

// EN: Tests for QueryResult class
// FR: Tests pour la classe QueryResult
class QueryResultTest : public ::testing::Test {
protected:
    void SetUp() override {
        headers = {"id", "name", "email", "age"};
        result = std::make_unique<QueryResult>(headers);
        
        // EN: Add sample data
        // FR: Ajouter des données d'exemple
        result->addRow({"1", "Alice", "alice@test.com", "25"});
        result->addRow({"2", "Bob", "bob@test.com", "30"});
        result->addRow({"3", "Charlie", "charlie@test.com", "35"});
    }
    
    std::vector<std::string> headers;
    std::unique_ptr<QueryResult> result;
};

TEST_F(QueryResultTest, BasicAccessors) {
    EXPECT_EQ(result->getRowCount(), 3);
    EXPECT_EQ(result->getColumnCount(), 4);
    EXPECT_EQ(result->getHeaders(), headers);
    EXPECT_FALSE(result->isEmpty());
}

TEST_F(QueryResultTest, CellAccess) {
    // EN: Test cell access by index
    // FR: Tester l'accès aux cellules par index
    EXPECT_EQ(result->getCell(0, 1), "Alice");
    EXPECT_EQ(result->getCell(1, 0), "2");
    EXPECT_EQ(result->getCell(2, 3), "35");
    
    // EN: Test cell access by column name
    // FR: Tester l'accès aux cellules par nom de colonne
    EXPECT_EQ(result->getCell(0, "name"), "Alice");
    EXPECT_EQ(result->getCell(1, "id"), "2");
    EXPECT_EQ(result->getCell(2, "age"), "35");
}

TEST_F(QueryResultTest, ColumnAccess) {
    // EN: Test column access by name
    // FR: Tester l'accès aux colonnes par nom
    auto name_column = result->getColumn("name");
    EXPECT_EQ(name_column.size(), 3);
    EXPECT_EQ(name_column[0], "Alice");
    EXPECT_EQ(name_column[1], "Bob");
    EXPECT_EQ(name_column[2], "Charlie");
    
    // EN: Test column access by index
    // FR: Tester l'accès aux colonnes par index
    auto id_column = result->getColumn(0);
    EXPECT_EQ(id_column.size(), 3);
    EXPECT_EQ(id_column[0], "1");
    EXPECT_EQ(id_column[1], "2");
    EXPECT_EQ(id_column[2], "3");
}

TEST_F(QueryResultTest, Sorting) {
    // EN: Test sorting by single column
    // FR: Tester tri par colonne unique
    result->sortBy("age", SortDirection::ASC);
    EXPECT_EQ(result->getCell(0, "name"), "Alice");  // age 25
    EXPECT_EQ(result->getCell(1, "name"), "Bob");    // age 30
    EXPECT_EQ(result->getCell(2, "name"), "Charlie"); // age 35
    
    result->sortBy("age", SortDirection::DESC);
    EXPECT_EQ(result->getCell(0, "name"), "Charlie"); // age 35
    EXPECT_EQ(result->getCell(1, "name"), "Bob");    // age 30
    EXPECT_EQ(result->getCell(2, "name"), "Alice");  // age 25
    
    // EN: Test multi-column sorting
    // FR: Tester tri multi-colonnes
    result->addRow({"4", "Alice", "alice2@test.com", "25"}); // Same name and age as first Alice
    
    std::vector<OrderByColumn> sort_spec = {
        {"name", SortDirection::ASC},
        {"email", SortDirection::ASC}
    };
    result->sortBy(sort_spec);
    
    // EN: Both Alice entries should be together, sorted by email
    // FR: Les deux entrées Alice devraient être ensemble, triées par email
    EXPECT_EQ(result->getCell(0, "name"), "Alice");
    EXPECT_EQ(result->getCell(1, "name"), "Alice");
    EXPECT_LT(result->getCell(0, "email"), result->getCell(1, "email"));
}

TEST_F(QueryResultTest, Slicing) {
    // EN: Test result slicing
    // FR: Tester découpage de résultat
    auto slice = result->slice(1, 2);
    EXPECT_EQ(slice.getRowCount(), 2);
    EXPECT_EQ(slice.getCell(0, "name"), "Bob");
    EXPECT_EQ(slice.getCell(1, "name"), "Charlie");
    
    // EN: Test edge cases
    // FR: Tester cas limites
    auto empty_slice = result->slice(10, 5);
    EXPECT_TRUE(empty_slice.isEmpty());
    
    auto partial_slice = result->slice(2, 10);
    EXPECT_EQ(partial_slice.getRowCount(), 1);
    EXPECT_EQ(partial_slice.getCell(0, "name"), "Charlie");
}

TEST_F(QueryResultTest, OutputFormats) {
    // EN: Test CSV output
    // FR: Tester sortie CSV
    std::string csv = result->toCSV();
    EXPECT_THAT(csv, HasSubstr("\"id\",\"name\",\"email\",\"age\""));
    EXPECT_THAT(csv, HasSubstr("\"Alice\""));
    EXPECT_THAT(csv, HasSubstr("\"Bob\""));
    
    // EN: Test JSON output
    // FR: Tester sortie JSON
    std::string json = result->toJSON();
    EXPECT_THAT(json, HasSubstr("{\"data\":"));
    EXPECT_THAT(json, HasSubstr("\"Alice\""));
    EXPECT_THAT(json, HasSubstr("\"count\":3"));
    
    // EN: Test table output
    // FR: Tester sortie tableau
    std::string table = result->toTable();
    EXPECT_THAT(table, HasSubstr("Alice"));
    EXPECT_THAT(table, HasSubstr("Bob"));
    EXPECT_THAT(table, HasSubstr("(3 rows)"));
}

// EN: Tests for QueryParser class
// FR: Tests pour la classe QueryParser
class QueryParserTest : public ::testing::Test {
protected:
    QueryParser parser;
    SqlQuery query;
};

TEST_F(QueryParserTest, BasicSelectParsing) {
    // EN: Test simple SELECT statement
    // FR: Tester déclaration SELECT simple
    std::string sql = "SELECT id, name FROM users";
    EXPECT_EQ(parser.parse(sql, query), QueryError::SUCCESS);
    
    EXPECT_EQ(query.table, "users");
    EXPECT_EQ(query.columns.size(), 2);
    EXPECT_EQ(query.columns[0].column, "id");
    EXPECT_EQ(query.columns[1].column, "name");
    
    // EN: Test SELECT *
    // FR: Tester SELECT *
    sql = "SELECT * FROM products";
    EXPECT_EQ(parser.parse(sql, query), QueryError::SUCCESS);
    EXPECT_EQ(query.columns.size(), 1);
    EXPECT_EQ(query.columns[0].column, "*");
}

TEST_F(QueryParserTest, WhereClauseParsing) {
    // EN: Test WHERE with simple condition
    // FR: Tester WHERE avec condition simple
    std::string sql = "SELECT * FROM users WHERE age > 25";
    EXPECT_EQ(parser.parse(sql, query), QueryError::SUCCESS);
    
    EXPECT_EQ(query.where.size(), 1);
    EXPECT_EQ(query.where[0].column, "age");
    EXPECT_EQ(query.where[0].operator_, SqlOperator::GREATER_THAN);
    
    // EN: Test WHERE with multiple conditions
    // FR: Tester WHERE avec conditions multiples
    sql = "SELECT * FROM users WHERE age > 25 AND name = 'Alice'";
    EXPECT_EQ(parser.parse(sql, query), QueryError::SUCCESS);
    EXPECT_EQ(query.where.size(), 2);
    EXPECT_EQ(query.where[0].logical_op, LogicalOperator::AND);
}

TEST_F(QueryParserTest, OrderByParsing) {
    // EN: Test ORDER BY parsing
    // FR: Tester analyse ORDER BY
    std::string sql = "SELECT * FROM users ORDER BY name ASC, age DESC";
    EXPECT_EQ(parser.parse(sql, query), QueryError::SUCCESS);
    
    EXPECT_EQ(query.order_by.size(), 2);
    EXPECT_EQ(query.order_by[0].column, "name");
    EXPECT_EQ(query.order_by[0].direction, SortDirection::ASC);
    EXPECT_EQ(query.order_by[1].column, "age");
    EXPECT_EQ(query.order_by[1].direction, SortDirection::DESC);
}

TEST_F(QueryParserTest, LimitOffsetParsing) {
    // EN: Test LIMIT and OFFSET parsing
    // FR: Tester analyse LIMIT et OFFSET
    std::string sql = "SELECT * FROM users LIMIT 10 OFFSET 5";
    EXPECT_EQ(parser.parse(sql, query), QueryError::SUCCESS);
    
    EXPECT_EQ(query.limit, 10);
    EXPECT_EQ(query.offset, 5);
}

TEST_F(QueryParserTest, AggregateFunctions) {
    // EN: Test aggregate function parsing
    // FR: Tester analyse fonctions d'agrégation
    std::string sql = "SELECT COUNT(id), AVG(salary) FROM employees";
    EXPECT_EQ(parser.parse(sql, query), QueryError::SUCCESS);
    
    EXPECT_EQ(query.columns.size(), 2);
    EXPECT_EQ(query.columns[0].aggregate, AggregateFunction::COUNT);
    EXPECT_EQ(query.columns[0].column, "id");
    EXPECT_EQ(query.columns[1].aggregate, AggregateFunction::AVG);
    EXPECT_EQ(query.columns[1].column, "salary");
}

TEST_F(QueryParserTest, ErrorHandling) {
    // EN: Test syntax errors
    // FR: Tester erreurs de syntaxe
    std::string sql = "INVALID QUERY";
    EXPECT_EQ(parser.parse(sql, query), QueryError::SYNTAX_ERROR);
    EXPECT_FALSE(parser.getLastError().empty());
    
    sql = "SELECT FROM users"; // Missing columns
    EXPECT_EQ(parser.parse(sql, query), QueryError::SYNTAX_ERROR);
    
    sql = "SELECT * users"; // Missing FROM
    EXPECT_EQ(parser.parse(sql, query), QueryError::SYNTAX_ERROR);
}

// EN: Tests for QueryEngine execution
// FR: Tests pour l'exécution QueryEngine
TEST_F(QueryEngineTest, BasicQueries) {
    // EN: Test simple SELECT * query
    // FR: Tester requête SELECT * simple
    auto result = engine->execute("SELECT * FROM employees");
    EXPECT_EQ(result.getRowCount(), 5);
    EXPECT_EQ(result.getColumnCount(), 6);
    
    // EN: Test column selection
    // FR: Tester sélection de colonnes
    result = engine->execute("SELECT name, department FROM employees");
    EXPECT_EQ(result.getRowCount(), 5);
    EXPECT_EQ(result.getColumnCount(), 2);
    EXPECT_EQ(result.getHeaders()[0], "name");
    EXPECT_EQ(result.getHeaders()[1], "department");
}

TEST_F(QueryEngineTest, WhereConditions) {
    // EN: Test WHERE with equality
    // FR: Tester WHERE avec égalité
    auto result = engine->execute("SELECT * FROM employees WHERE department = 'Engineering'");
    EXPECT_EQ(result.getRowCount(), 3);
    
    // EN: Test WHERE with comparison
    // FR: Tester WHERE avec comparaison
    result = engine->execute("SELECT * FROM employees WHERE salary > '70000'");
    EXPECT_GE(result.getRowCount(), 1);
    
    // EN: Test WHERE with multiple conditions
    // FR: Tester WHERE avec conditions multiples
    result = engine->execute("SELECT * FROM employees WHERE department = 'Engineering' AND salary > '75000'");
    EXPECT_GE(result.getRowCount(), 1);
}

TEST_F(QueryEngineTest, OrderByQueries) {
    // EN: Test ORDER BY ascending
    // FR: Tester ORDER BY croissant
    auto result = engine->execute("SELECT name FROM employees ORDER BY name ASC");
    EXPECT_EQ(result.getRowCount(), 5);
    
    // EN: Verify ordering
    // FR: Vérifier l'ordre
    auto names = result.getColumn("name");
    for (size_t i = 1; i < names.size(); ++i) {
        EXPECT_LE(names[i-1], names[i]);
    }
    
    // EN: Test ORDER BY descending
    // FR: Tester ORDER BY décroissant
    result = engine->execute("SELECT name FROM employees ORDER BY name DESC");
    names = result.getColumn("name");
    for (size_t i = 1; i < names.size(); ++i) {
        EXPECT_GE(names[i-1], names[i]);
    }
}

TEST_F(QueryEngineTest, LimitOffset) {
    // EN: Test LIMIT
    // FR: Tester LIMIT
    auto result = engine->execute("SELECT * FROM employees LIMIT 3");
    EXPECT_EQ(result.getRowCount(), 3);
    
    // EN: Test LIMIT with OFFSET
    // FR: Tester LIMIT avec OFFSET
    result = engine->execute("SELECT * FROM employees LIMIT 2 OFFSET 2");
    EXPECT_EQ(result.getRowCount(), 2);
}

TEST_F(QueryEngineTest, AggregateFunctions) {
    // EN: Test COUNT
    // FR: Tester COUNT
    auto result = engine->execute("SELECT COUNT(id) FROM employees");
    EXPECT_EQ(result.getRowCount(), 1);
    EXPECT_EQ(result.getCell(0, 0), "5");
    
    // EN: Test COUNT with WHERE
    // FR: Tester COUNT avec WHERE
    result = engine->execute("SELECT COUNT(id) FROM employees WHERE department = 'Engineering'");
    EXPECT_EQ(result.getCell(0, 0), "3");
    
    // EN: Test AVG (if implemented)
    // FR: Tester AVG (si implémenté)
    result = engine->execute("SELECT AVG(salary) FROM employees");
    EXPECT_EQ(result.getRowCount(), 1);
    // EN: Average should be calculated correctly
    // FR: La moyenne devrait être calculée correctement
    double avg = std::stod(result.getCell(0, 0));
    EXPECT_GT(avg, 0);
}

TEST_F(QueryEngineTest, DistinctQueries) {
    // EN: Test DISTINCT
    // FR: Tester DISTINCT
    auto result = engine->execute("SELECT DISTINCT department FROM employees");
    EXPECT_LE(result.getRowCount(), 3); // HR, Engineering, Marketing
    
    // EN: Verify all departments are unique
    // FR: Vérifier que tous les départements sont uniques
    auto departments = result.getColumn("department");
    std::set<std::string> unique_depts(departments.begin(), departments.end());
    EXPECT_EQ(departments.size(), unique_depts.size());
}

TEST_F(QueryEngineTest, FileLoadingQueries) {
    // EN: Test loading table from CSV file
    // FR: Tester chargement de table depuis fichier CSV
    std::string csv_file = test_dir / "test_data.csv";
    auto error = engine->loadTable("test_table", csv_file);
    EXPECT_EQ(error, QueryError::SUCCESS);
    
    // EN: Query the loaded table
    // FR: Requêter la table chargée
    auto result = engine->execute("SELECT * FROM test_table");
    EXPECT_EQ(result.getRowCount(), 4);
    EXPECT_EQ(result.getColumnCount(), 4);
    
    // EN: Test specific query on loaded data
    // FR: Tester requête spécifique sur données chargées
    result = engine->execute("SELECT name FROM test_table WHERE category = 'Cat1'");
    EXPECT_EQ(result.getRowCount(), 2);
}

TEST_F(QueryEngineTest, IndexingOptimization) {
    // EN: Create index on frequently queried column
    // FR: Créer index sur colonne fréquemment requêtée
    IndexConfig config;
    config.column = "department";
    config.type = IndexType::HASH;
    
    auto error = engine->createIndex("employees", config);
    EXPECT_EQ(error, QueryError::SUCCESS);
    
    // EN: Query using indexed column should be optimized
    // FR: Requête utilisant colonne indexée devrait être optimisée
    auto result = engine->execute("SELECT * FROM employees WHERE department = 'Engineering'");
    EXPECT_EQ(result.getRowCount(), 3);
    
    // EN: Check that index is being used (via query plan)
    // FR: Vérifier que l'index est utilisé (via plan de requête)
    std::string plan = engine->explainQuery("SELECT * FROM employees WHERE department = 'Engineering'");
    EXPECT_THAT(plan, HasSubstr("INDEXED"));
}

TEST_F(QueryEngineTest, QueryCaching) {
    // EN: Execute same query twice
    // FR: Exécuter même requête deux fois
    std::string sql = "SELECT * FROM employees WHERE department = 'Engineering'";
    
    auto result1 = engine->execute(sql);
    auto result2 = engine->execute(sql);
    
    // EN: Results should be identical
    // FR: Résultats devraient être identiques
    EXPECT_EQ(result1.getRowCount(), result2.getRowCount());
    EXPECT_EQ(result1.getColumnCount(), result2.getColumnCount());
    
    // EN: Second query should be faster (cached)
    // FR: Deuxième requête devrait être plus rapide (mise en cache)
    EXPECT_TRUE(result2.getStatistics().query_cached);
}

TEST_F(QueryEngineTest, ErrorHandling) {
    // EN: Test non-existent table
    // FR: Tester table inexistante
    auto result = engine->execute("SELECT * FROM nonexistent_table");
    EXPECT_TRUE(result.isEmpty());
    
    // EN: Test invalid SQL
    // FR: Tester SQL invalide
    result = engine->execute("INVALID SQL QUERY");
    EXPECT_TRUE(result.isEmpty());
    
    // EN: Test non-existent column
    // FR: Tester colonne inexistante
    result = engine->execute("SELECT nonexistent_column FROM employees");
    // EN: Should return empty values for non-existent columns
    // FR: Devrait retourner valeurs vides pour colonnes inexistantes
    EXPECT_EQ(result.getRowCount(), 5);
}

TEST_F(QueryEngineTest, PerformanceAndStatistics) {
    // EN: Execute multiple queries and check statistics
    // FR: Exécuter plusieurs requêtes et vérifier statistiques
    for (int i = 0; i < 10; ++i) {
        engine->execute("SELECT * FROM employees WHERE id = '" + std::to_string(i % 5 + 1) + "'");
    }
    
    auto stats = engine->getStatistics();
    EXPECT_EQ(stats.total_queries_executed, 10);
    EXPECT_GT(stats.total_execution_time.count(), 0);
    EXPECT_GT(stats.total_rows_processed, 0);
    
    // EN: Some queries should hit cache
    // FR: Certaines requêtes devraient toucher le cache
    EXPECT_GT(stats.cache_hits, 0);
}

// EN: Tests for QueryUtils functions
// FR: Tests pour les fonctions QueryUtils
class QueryUtilsTest : public ::testing::Test {};

TEST_F(QueryUtilsTest, ValueConversion) {
    // EN: Test string to QueryValue conversion
    // FR: Tester conversion chaîne vers QueryValue
    auto value = QueryUtils::stringToQueryValue("123");
    EXPECT_TRUE(std::holds_alternative<int64_t>(value));
    EXPECT_EQ(std::get<int64_t>(value), 123);
    
    value = QueryUtils::stringToQueryValue("123.45");
    EXPECT_TRUE(std::holds_alternative<double>(value));
    EXPECT_DOUBLE_EQ(std::get<double>(value), 123.45);
    
    value = QueryUtils::stringToQueryValue("true");
    EXPECT_TRUE(std::holds_alternative<bool>(value));
    EXPECT_TRUE(std::get<bool>(value));
    
    value = QueryUtils::stringToQueryValue("hello");
    EXPECT_TRUE(std::holds_alternative<std::string>(value));
    EXPECT_EQ(std::get<std::string>(value), "hello");
}

TEST_F(QueryUtilsTest, ValueComparison) {
    // EN: Test value comparison
    // FR: Tester comparaison de valeurs
    QueryValue a = std::string("10");
    QueryValue b = std::string("20");
    
    EXPECT_TRUE(QueryUtils::compareValues(a, b, SqlOperator::LESS_THAN));
    EXPECT_FALSE(QueryUtils::compareValues(a, b, SqlOperator::GREATER_THAN));
    EXPECT_TRUE(QueryUtils::compareValues(a, b, SqlOperator::NOT_EQUALS));
    
    // EN: Test with numeric values
    // FR: Tester avec valeurs numériques
    a = static_cast<int64_t>(10);
    b = static_cast<int64_t>(20);
    
    EXPECT_TRUE(QueryUtils::compareValues(a, b, SqlOperator::LESS_THAN));
    EXPECT_FALSE(QueryUtils::compareValues(a, b, SqlOperator::GREATER_THAN));
}

TEST_F(QueryUtilsTest, StringUtilities) {
    // EN: Test string escaping
    // FR: Tester échappement de chaînes
    std::string escaped = QueryUtils::escapeString("It's a \"test\"");
    EXPECT_THAT(escaped, HasSubstr("\\'"));
    EXPECT_THAT(escaped, HasSubstr("\\\""));
    
    // EN: Test numeric detection
    // FR: Tester détection numérique
    EXPECT_TRUE(QueryUtils::isNumeric("123"));
    EXPECT_TRUE(QueryUtils::isNumeric("123.45"));
    EXPECT_TRUE(QueryUtils::isNumeric("-123"));
    EXPECT_FALSE(QueryUtils::isNumeric("abc"));
    EXPECT_FALSE(QueryUtils::isNumeric("12.34.56"));
}

TEST_F(QueryUtilsTest, FormatUtilities) {
    // EN: Test duration formatting
    // FR: Tester formatage de durée
    auto duration = std::chrono::milliseconds(1500);
    std::string formatted = QueryUtils::formatDuration(duration);
    EXPECT_THAT(formatted, HasSubstr("1.5"));
    EXPECT_THAT(formatted, HasSubstr("s"));
    
    // EN: Test memory size formatting
    // FR: Tester formatage de taille mémoire
    std::string size = QueryUtils::formatMemorySize(1024 * 1024);
    EXPECT_THAT(size, HasSubstr("1"));
    EXPECT_THAT(size, HasSubstr("MB"));
    
    // EN: Test number formatting
    // FR: Tester formatage de nombres
    std::string number = QueryUtils::formatNumber(1234567);
    EXPECT_THAT(number, HasSubstr(","));
}

// EN: Integration tests combining multiple features
// FR: Tests d'intégration combinant plusieurs fonctionnalités
class QueryIntegrationTest : public QueryEngineTest {
protected:
    void SetUp() override {
        QueryEngineTest::SetUp();
        
        // EN: Create more complex test data
        // FR: Créer données de test plus complexes
        createComplexTestData();
    }
    
    void createComplexTestData() {
        // EN: Sales data with dates and multiple relationships
        // FR: Données de ventes avec dates et relations multiples
        std::vector<std::string> sales_headers = {"id", "employee_id", "product_id", "quantity", "sale_date", "amount"};
        std::vector<std::vector<std::string>> sales_data = {
            {"1", "1", "1", "2", "2024-01-15", "1999.98"},
            {"2", "2", "2", "5", "2024-01-16", "149.95"},
            {"3", "3", "1", "1", "2024-01-17", "999.99"},
            {"4", "1", "3", "3", "2024-01-18", "239.97"},
            {"5", "4", "4", "1", "2024-01-19", "199.99"},
            {"6", "2", "5", "2", "2024-01-20", "799.98"},
            {"7", "5", "2", "10", "2024-01-21", "299.90"},
            {"8", "3", "1", "1", "2024-01-22", "999.99"}
        };
        engine->registerTable("sales", sales_headers, sales_data);
    }
};

TEST_F(QueryIntegrationTest, ComplexAnalyticalQueries) {
    // EN: Test complex query with multiple conditions
    // FR: Tester requête complexe avec conditions multiples
    auto result = engine->execute(
        "SELECT * FROM sales WHERE amount > '500' AND quantity >= '2' ORDER BY amount DESC LIMIT 5"
    );
    
    EXPECT_GT(result.getRowCount(), 0);
    EXPECT_LE(result.getRowCount(), 5);
    
    // EN: Verify results are sorted by amount descending
    // FR: Vérifier que résultats sont triés par montant décroissant
    if (result.getRowCount() > 1) {
        auto amounts = result.getColumn("amount");
        for (size_t i = 1; i < amounts.size(); ++i) {
            double prev = std::stod(amounts[i-1]);
            double curr = std::stod(amounts[i]);
            EXPECT_GE(prev, curr);
        }
    }
}

TEST_F(QueryIntegrationTest, AggregationWithFiltering) {
    // EN: Test aggregation with WHERE clause
    // FR: Tester agrégation avec clause WHERE
    auto result = engine->execute("SELECT COUNT(id) FROM sales WHERE amount > '500'");
    EXPECT_EQ(result.getRowCount(), 1);
    
    int high_value_sales = std::stoi(result.getCell(0, 0));
    EXPECT_GT(high_value_sales, 0);
    
    // EN: Test sum aggregation
    // FR: Tester agrégation somme
    result = engine->execute("SELECT SUM(amount) FROM sales");
    EXPECT_EQ(result.getRowCount(), 1);
    
    double total = std::stod(result.getCell(0, 0));
    EXPECT_GT(total, 0);
}

TEST_F(QueryIntegrationTest, MultiTableOperations) {
    // EN: Test querying multiple tables separately
    // FR: Tester requête de tables multiples séparément
    auto emp_result = engine->execute("SELECT COUNT(*) FROM employees");
    auto sales_result = engine->execute("SELECT COUNT(*) FROM sales");
    auto product_result = engine->execute("SELECT COUNT(*) FROM products");
    
    EXPECT_EQ(std::stoi(emp_result.getCell(0, 0)), 5);
    EXPECT_EQ(std::stoi(sales_result.getCell(0, 0)), 8);
    EXPECT_EQ(std::stoi(product_result.getCell(0, 0)), 5);
}

TEST_F(QueryIntegrationTest, PerformanceWithIndexes) {
    // EN: Create indexes on frequently used columns
    // FR: Créer index sur colonnes fréquemment utilisées
    IndexConfig emp_index;
    emp_index.column = "id";
    emp_index.type = IndexType::HASH;
    engine->createIndex("employees", emp_index);
    
    IndexConfig sales_index;
    sales_index.column = "employee_id";
    sales_index.type = IndexType::HASH;
    engine->createIndex("sales", sales_index);
    
    // EN: Execute queries that should benefit from indexes
    // FR: Exécuter requêtes qui devraient bénéficier des index
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 1; i <= 5; ++i) {
        engine->execute("SELECT * FROM employees WHERE id = '" + std::to_string(i) + "'");
        engine->execute("SELECT * FROM sales WHERE employee_id = '" + std::to_string(i) + "'");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // EN: With indexes, queries should execute reasonably fast
    // FR: Avec index, requêtes devraient s'exécuter raisonnablement vite
    EXPECT_LT(duration.count(), 1000); // Less than 1 second for 10 queries
}

TEST_F(QueryIntegrationTest, QueryPlanAnalysis) {
    // EN: Test query plan generation
    // FR: Tester génération de plan de requête
    std::string plan = engine->explainQuery("SELECT * FROM employees WHERE department = 'Engineering'");
    
    EXPECT_THAT(plan, HasSubstr("Query Execution Plan"));
    EXPECT_THAT(plan, HasSubstr("Table: employees"));
    EXPECT_THAT(plan, HasSubstr("WHERE conditions"));
    
    // EN: Create index and check plan changes
    // FR: Créer index et vérifier changements de plan
    IndexConfig config;
    config.column = "department";
    config.type = IndexType::HASH;
    engine->createIndex("employees", config);
    
    plan = engine->explainQuery("SELECT * FROM employees WHERE department = 'Engineering'");
    EXPECT_THAT(plan, HasSubstr("INDEXED"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}