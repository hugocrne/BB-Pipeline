#pragma once

// EN: Query Engine for CSV files with SQL-like syntax and fast indexing
// FR: Moteur de requêtes pour fichiers CSV avec syntaxe SQL et indexation rapide

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <functional>
#include <regex>
#include <chrono>
#include <mutex>
#include <atomic>
#include <variant>

namespace BBP {
namespace CSV {

// EN: Forward declarations
// FR: Déclarations anticipées
class QueryResult;
class QueryExecutor;
class IndexManager;

// EN: SQL operator types for query processing
// FR: Types d'opérateurs SQL pour traitement de requêtes
enum class SqlOperator {
    EQUALS = 0,         // EN: = / FR: =
    NOT_EQUALS,         // EN: != or <> / FR: != ou <>
    LESS_THAN,          // EN: < / FR: <
    LESS_EQUAL,         // EN: <= / FR: <=
    GREATER_THAN,       // EN: > / FR: >
    GREATER_EQUAL,      // EN: >= / FR: >=
    LIKE,              // EN: LIKE pattern matching / FR: LIKE correspondance de motifs
    NOT_LIKE,          // EN: NOT LIKE / FR: NOT LIKE
    IN,                // EN: IN (list) / FR: IN (liste)
    NOT_IN,            // EN: NOT IN (list) / FR: NOT IN (liste)
    IS_NULL,           // EN: IS NULL / FR: IS NULL
    IS_NOT_NULL,       // EN: IS NOT NULL / FR: IS NOT NULL
    REGEX,             // EN: REGEX matching / FR: Correspondance REGEX
    BETWEEN            // EN: BETWEEN range / FR: BETWEEN plage
};

// EN: Logical operators for combining conditions
// FR: Opérateurs logiques pour combiner les conditions
enum class LogicalOperator {
    AND = 0,           // EN: AND conjunction / FR: Conjonction AND
    OR,                // EN: OR disjunction / FR: Disjonction OR
    NOT                // EN: NOT negation / FR: Négation NOT
};

// EN: Index types for optimization
// FR: Types d'index pour optimisation
enum class IndexType {
    NONE = 0,          // EN: No indexing / FR: Pas d'indexation
    HASH,              // EN: Hash index for equality lookups / FR: Index hash pour recherches d'égalité
    BTREE,             // EN: B-Tree index for range queries / FR: Index B-Tree pour requêtes de plage
    FULL_TEXT,         // EN: Full-text index for text search / FR: Index texte intégral pour recherche textuelle
    COMPOSITE          // EN: Multi-column composite index / FR: Index composé multi-colonnes
};

// EN: Aggregation function types
// FR: Types de fonctions d'agrégation
enum class AggregateFunction {
    NONE = 0,          // EN: No aggregation / FR: Pas d'agrégation
    COUNT,             // EN: COUNT rows / FR: COMPTER les lignes
    SUM,               // EN: SUM values / FR: SOMME des valeurs
    AVG,               // EN: AVERAGE values / FR: MOYENNE des valeurs
    MIN,               // EN: MINIMUM value / FR: Valeur MINIMALE
    MAX,               // EN: MAXIMUM value / FR: Valeur MAXIMALE
    DISTINCT,          // EN: DISTINCT values / FR: Valeurs DISTINCTES
    GROUP_CONCAT       // EN: GROUP_CONCAT concatenation / FR: Concaténation GROUP_CONCAT
};

// EN: Sort direction for ORDER BY
// FR: Direction de tri pour ORDER BY
enum class SortDirection {
    ASC = 0,           // EN: Ascending order / FR: Ordre croissant
    DESC               // EN: Descending order / FR: Ordre décroissant
};

// EN: Query execution error types
// FR: Types d'erreurs d'exécution de requêtes
enum class QueryError {
    SUCCESS = 0,       // EN: Query executed successfully / FR: Requête exécutée avec succès
    SYNTAX_ERROR,      // EN: SQL syntax error / FR: Erreur de syntaxe SQL
    FILE_NOT_FOUND,    // EN: CSV file not found / FR: Fichier CSV introuvable
    COLUMN_NOT_FOUND,  // EN: Referenced column doesn't exist / FR: Colonne référencée n'existe pas
    TYPE_MISMATCH,     // EN: Data type mismatch / FR: Incompatibilité de type de données
    INDEX_ERROR,       // EN: Index creation or usage error / FR: Erreur de création ou d'utilisation d'index
    MEMORY_ERROR,      // EN: Insufficient memory / FR: Mémoire insuffisante
    IO_ERROR,          // EN: File I/O error / FR: Erreur d'E/S de fichier
    EXECUTION_ERROR    // EN: General execution error / FR: Erreur d'exécution générale
};

// EN: Value types supported by the query engine
// FR: Types de valeurs supportés par le moteur de requêtes
using QueryValue = std::variant<std::string, int64_t, double, bool, std::nullptr_t>;

// EN: SQL WHERE condition representation
// FR: Représentation des conditions WHERE SQL
struct WhereCondition {
    std::string column;                    // EN: Column name / FR: Nom de colonne
    SqlOperator operator_;                 // EN: Comparison operator / FR: Opérateur de comparaison
    QueryValue value;                      // EN: Comparison value / FR: Valeur de comparaison
    std::vector<QueryValue> in_values;     // EN: Values for IN operator / FR: Valeurs pour opérateur IN
    QueryValue range_start, range_end;     // EN: Range for BETWEEN / FR: Plage pour BETWEEN
    std::string pattern;                   // EN: Pattern for LIKE/REGEX / FR: Motif pour LIKE/REGEX
    LogicalOperator logical_op = LogicalOperator::AND;  // EN: Logical connector / FR: Connecteur logique
};

// EN: SQL SELECT column specification
// FR: Spécification de colonne SELECT SQL
struct SelectColumn {
    std::string column;                    // EN: Column name (* for all) / FR: Nom de colonne (* pour toutes)
    std::string alias;                     // EN: Column alias / FR: Alias de colonne
    AggregateFunction aggregate = AggregateFunction::NONE;  // EN: Aggregation function / FR: Fonction d'agrégation
    bool distinct = false;                 // EN: DISTINCT modifier / FR: Modificateur DISTINCT
};

// EN: SQL ORDER BY specification
// FR: Spécification ORDER BY SQL
struct OrderByColumn {
    std::string column;                    // EN: Column name / FR: Nom de colonne
    SortDirection direction = SortDirection::ASC;  // EN: Sort direction / FR: Direction de tri
};

// EN: SQL JOIN specification
// FR: Spécification JOIN SQL
struct JoinClause {
    enum Type { INNER, LEFT, RIGHT, FULL } type = INNER;  // EN: Join type / FR: Type de jointure
    std::string table;                     // EN: Table to join / FR: Table à joindre
    std::string on_left;                   // EN: Left join column / FR: Colonne de jointure gauche
    std::string on_right;                  // EN: Right join column / FR: Colonne de jointure droite
};

// EN: Complete SQL query representation
// FR: Représentation complète de requête SQL
struct SqlQuery {
    // EN: Basic SELECT structure
    // FR: Structure SELECT de base
    std::vector<SelectColumn> columns;     // EN: SELECT columns / FR: Colonnes SELECT
    std::string table;                     // EN: FROM table / FR: Table FROM
    std::vector<WhereCondition> where;     // EN: WHERE conditions / FR: Conditions WHERE
    std::vector<std::string> group_by;     // EN: GROUP BY columns / FR: Colonnes GROUP BY
    std::vector<WhereCondition> having;    // EN: HAVING conditions / FR: Conditions HAVING
    std::vector<OrderByColumn> order_by;   // EN: ORDER BY specification / FR: Spécification ORDER BY
    size_t limit = 0;                      // EN: LIMIT count (0 = no limit) / FR: Nombre LIMIT (0 = pas de limite)
    size_t offset = 0;                     // EN: OFFSET count / FR: Nombre OFFSET
    
    // EN: Advanced features
    // FR: Fonctionnalités avancées
    std::vector<JoinClause> joins;         // EN: JOIN clauses / FR: Clauses JOIN
    bool distinct_query = false;           // EN: SELECT DISTINCT / FR: SELECT DISTINCT
    
    // EN: Query metadata
    // FR: Métadonnées de requête
    std::string raw_sql;                   // EN: Original SQL string / FR: Chaîne SQL originale
    std::chrono::system_clock::time_point created_at;  // EN: Query creation time / FR: Heure de création de requête
};

// EN: Index configuration for a column
// FR: Configuration d'index pour une colonne
struct IndexConfig {
    std::string column;                    // EN: Column to index / FR: Colonne à indexer
    IndexType type;                        // EN: Index type / FR: Type d'index
    std::vector<std::string> composite_columns;  // EN: Additional columns for composite index / FR: Colonnes supplémentaires pour index composé
    size_t max_memory_mb = 100;            // EN: Maximum memory usage in MB / FR: Utilisation mémoire maximale en MB
    bool case_sensitive = true;            // EN: Case sensitive indexing / FR: Indexation sensible à la casse
    std::string tokenizer = "standard";    // EN: Tokenizer for full-text index / FR: Tokeniseur pour index texte intégral
};

// EN: Query execution statistics
// FR: Statistiques d'exécution de requête
struct QueryStatistics {
    std::chrono::milliseconds parse_time{0};      // EN: SQL parsing time / FR: Temps d'analyse SQL
    std::chrono::milliseconds execution_time{0};  // EN: Query execution time / FR: Temps d'exécution de requête
    std::chrono::milliseconds index_time{0};      // EN: Index usage time / FR: Temps d'utilisation d'index
    size_t rows_examined = 0;              // EN: Total rows examined / FR: Nombre total de lignes examinées
    size_t rows_returned = 0;              // EN: Rows returned in result / FR: Lignes retournées dans le résultat
    size_t indexes_used = 0;               // EN: Number of indexes used / FR: Nombre d'index utilisés
    size_t memory_used_bytes = 0;          // EN: Memory used during execution / FR: Mémoire utilisée pendant l'exécution
    bool query_cached = false;             // EN: Whether result was cached / FR: Si le résultat était en cache
    std::vector<std::string> index_hits;   // EN: Indexes that were used / FR: Index qui ont été utilisés
    std::string execution_plan;            // EN: Query execution plan / FR: Plan d'exécution de requête
};

// EN: Query result container
// FR: Conteneur de résultat de requête
class QueryResult {
public:
    // EN: Constructor
    // FR: Constructeur
    QueryResult() = default;
    explicit QueryResult(std::vector<std::string> headers);
    
    // EN: Result data access
    // FR: Accès aux données de résultat
    void addRow(const std::vector<std::string>& row);
    void addRow(std::vector<std::string>&& row);
    
    const std::vector<std::string>& getHeaders() const { return headers_; }
    const std::vector<std::vector<std::string>>& getRows() const { return rows_; }
    size_t getRowCount() const { return rows_.size(); }
    size_t getColumnCount() const { return headers_.size(); }
    
    // EN: Data access by index
    // FR: Accès aux données par index
    const std::vector<std::string>& getRow(size_t index) const;
    const std::string& getCell(size_t row, size_t col) const;
    const std::string& getCell(size_t row, const std::string& column) const;
    
    // EN: Column utilities
    // FR: Utilitaires de colonnes
    int getColumnIndex(const std::string& column) const;
    std::vector<std::string> getColumn(const std::string& column) const;
    std::vector<std::string> getColumn(size_t index) const;
    
    // EN: Result manipulation
    // FR: Manipulation du résultat
    void sortBy(const std::string& column, SortDirection direction = SortDirection::ASC);
    void sortBy(const std::vector<OrderByColumn>& sort_spec);
    QueryResult slice(size_t offset, size_t limit) const;
    
    // EN: Export functions
    // FR: Fonctions d'exportation
    std::string toCSV() const;
    std::string toJSON() const;
    std::string toTable() const;  // EN: ASCII table format / FR: Format de tableau ASCII
    
    // EN: Statistics and metadata
    // FR: Statistiques et métadonnées
    void setStatistics(const QueryStatistics& stats) { statistics_ = stats; }
    const QueryStatistics& getStatistics() const { return statistics_; }
    
    bool isEmpty() const { return rows_.empty(); }
    void clear();
    
private:
    std::vector<std::string> headers_;
    std::vector<std::vector<std::string>> rows_;
    std::unordered_map<std::string, size_t> column_index_map_;
    QueryStatistics statistics_;
    
    void buildColumnIndexMap();
};

// EN: SQL query parser
// FR: Analyseur de requêtes SQL
class QueryParser {
public:
    // EN: Constructor
    // FR: Constructeur
    QueryParser() = default;
    
    // EN: Main parsing function
    // FR: Fonction d'analyse principale
    QueryError parse(const std::string& sql, SqlQuery& query);
    
    // EN: Validation functions
    // FR: Fonctions de validation
    static bool isValidColumnName(const std::string& name);
    static bool isValidTableName(const std::string& name);
    static std::string normalizeString(const std::string& str);
    
    // EN: Utility functions
    // FR: Fonctions utilitaires
    std::string getLastError() const { return last_error_; }
    size_t getErrorPosition() const { return error_position_; }
    
private:
    std::string last_error_;
    size_t error_position_ = 0;
    
    // EN: Parsing helper functions
    // FR: Fonctions d'aide à l'analyse
    QueryError parseSelect(const std::string& sql, size_t& pos, SqlQuery& query);
    QueryError parseFrom(const std::string& sql, size_t& pos, SqlQuery& query);
    QueryError parseWhere(const std::string& sql, size_t& pos, SqlQuery& query);
    QueryError parseGroupBy(const std::string& sql, size_t& pos, SqlQuery& query);
    QueryError parseHaving(const std::string& sql, size_t& pos, SqlQuery& query);
    QueryError parseOrderBy(const std::string& sql, size_t& pos, SqlQuery& query);
    QueryError parseLimit(const std::string& sql, size_t& pos, SqlQuery& query);
    QueryError parseJoin(const std::string& sql, size_t& pos, SqlQuery& query);
    
    // EN: Token parsing utilities
    // FR: Utilitaires d'analyse de tokens
    std::string parseIdentifier(const std::string& sql, size_t& pos);
    QueryValue parseValue(const std::string& sql, size_t& pos);
    SqlOperator parseOperator(const std::string& sql, size_t& pos);
    AggregateFunction parseAggregate(const std::string& token);
    
    // EN: String utilities
    // FR: Utilitaires de chaînes
    void skipWhitespace(const std::string& sql, size_t& pos);
    bool matchKeyword(const std::string& sql, size_t& pos, const std::string& keyword);
    std::string extractQuotedString(const std::string& sql, size_t& pos);
    
    void setError(const std::string& message, size_t position);
};

// EN: Index management for fast queries
// FR: Gestion d'index pour requêtes rapides
class IndexManager {
public:
    // EN: Constructor
    // FR: Constructeur
    IndexManager() = default;
    ~IndexManager() = default;
    
    // EN: Index creation and management
    // FR: Création et gestion d'index
    QueryError createIndex(const std::string& table, const IndexConfig& config);
    QueryError dropIndex(const std::string& table, const std::string& column);
    bool hasIndex(const std::string& table, const std::string& column) const;
    
    // EN: Index usage for queries
    // FR: Utilisation d'index pour les requêtes
    std::vector<size_t> findRowsByIndex(const std::string& table, const std::string& column, 
                                       const QueryValue& value) const;
    std::vector<size_t> findRowsByRange(const std::string& table, const std::string& column,
                                       const QueryValue& min_val, const QueryValue& max_val) const;
    std::vector<size_t> findRowsByPattern(const std::string& table, const std::string& column,
                                         const std::string& pattern, bool regex = false) const;
    
    // EN: Index optimization
    // FR: Optimisation d'index
    void optimizeIndexes(const std::string& table);
    void rebuildIndex(const std::string& table, const std::string& column);
    
    // EN: Statistics and info
    // FR: Statistiques et informations
    size_t getIndexCount(const std::string& table) const;
    size_t getIndexMemoryUsage(const std::string& table) const;
    std::vector<std::string> getIndexedColumns(const std::string& table) const;
    
    // EN: Data loading for indexing
    // FR: Chargement de données pour indexation
    QueryError loadTableData(const std::string& table, const std::vector<std::string>& headers,
                            const std::vector<std::vector<std::string>>& data);
    void clearTableData(const std::string& table);
    
private:
    // EN: Internal index structures
    // FR: Structures d'index internes
    struct HashIndex {
        std::unordered_map<std::string, std::vector<size_t>> value_to_rows;
        size_t memory_usage = 0;
    };
    
    struct BTreeIndex {
        std::map<std::string, std::vector<size_t>> value_to_rows;
        size_t memory_usage = 0;
    };
    
    struct FullTextIndex {
        std::unordered_map<std::string, std::vector<size_t>> token_to_rows;
        std::string tokenizer;
        bool case_sensitive;
        size_t memory_usage = 0;
    };
    
    // EN: Table data and indexes
    // FR: Données de table et index
    std::unordered_map<std::string, std::vector<std::string>> table_headers_;
    std::unordered_map<std::string, std::vector<std::vector<std::string>>> table_data_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<HashIndex>>> hash_indexes_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<BTreeIndex>>> btree_indexes_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unique_ptr<FullTextIndex>>> fulltext_indexes_;
    std::unordered_map<std::string, std::unordered_map<std::string, IndexConfig>> index_configs_;
    
    mutable std::mutex index_mutex_;
    
    // EN: Helper functions
    // FR: Fonctions d'aide
    QueryError buildHashIndex(const std::string& table, const std::string& column);
    QueryError buildBTreeIndex(const std::string& table, const std::string& column);
    QueryError buildFullTextIndex(const std::string& table, const std::string& column);
    
    std::vector<std::string> tokenizeText(const std::string& text, const std::string& tokenizer, bool case_sensitive) const;
    size_t calculateIndexMemory(const std::string& table, const std::string& column) const;
};

// EN: Main query execution engine
// FR: Moteur d'exécution de requêtes principal
class QueryEngine {
public:
    // EN: Constructor with configuration
    // FR: Constructeur avec configuration
    struct Config {
        size_t max_memory_mb = 500;        // EN: Maximum memory usage / FR: Utilisation mémoire maximale
        size_t max_result_rows = 1000000;  // EN: Maximum rows in result / FR: Nombre maximum de lignes dans le résultat
        size_t index_cache_size = 10;      // EN: Number of indexes to cache / FR: Nombre d'index à mettre en cache
        bool enable_query_cache = true;    // EN: Enable result caching / FR: Activer la mise en cache des résultats
        size_t query_cache_size = 100;     // EN: Number of queries to cache / FR: Nombre de requêtes à mettre en cache
        bool auto_index = true;            // EN: Automatically create indexes / FR: Créer automatiquement des index
        std::chrono::seconds query_timeout{300};  // EN: Query execution timeout / FR: Délai d'expiration d'exécution de requête
    };
    
    explicit QueryEngine(const Config& config);
    ~QueryEngine() = default;
    
    // EN: Table management
    // FR: Gestion des tables
    QueryError loadTable(const std::string& table_name, const std::string& csv_file);
    QueryError registerTable(const std::string& table_name, const std::vector<std::string>& headers,
                            const std::vector<std::vector<std::string>>& data);
    void unloadTable(const std::string& table_name);
    std::vector<std::string> getTableNames() const;
    
    // EN: Query execution
    // FR: Exécution de requêtes
    QueryResult execute(const std::string& sql);
    QueryResult execute(const SqlQuery& query);
    
    // EN: Index management
    // FR: Gestion d'index
    QueryError createIndex(const std::string& table, const IndexConfig& config);
    QueryError dropIndex(const std::string& table, const std::string& column);
    std::vector<std::string> getIndexedColumns(const std::string& table) const;
    
    // EN: Query optimization
    // FR: Optimisation de requêtes
    std::string explainQuery(const std::string& sql);
    std::string explainQuery(const SqlQuery& query);
    void optimizeTable(const std::string& table);
    
    // EN: Cache management
    // FR: Gestion du cache
    void clearQueryCache();
    void clearIndexCache();
    size_t getQueryCacheSize() const;
    
    // EN: Statistics and monitoring
    // FR: Statistiques et surveillance
    struct EngineStatistics {
        size_t total_queries_executed = 0;
        size_t cache_hits = 0;
        size_t cache_misses = 0;
        std::chrono::milliseconds total_execution_time{0};
        size_t total_rows_processed = 0;
        size_t memory_usage_bytes = 0;
        size_t active_indexes = 0;
    };
    
    EngineStatistics getStatistics() const;
    void resetStatistics();
    
    // EN: Configuration
    // FR: Configuration
    const Config& getConfig() const { return config_; }
    void updateConfig(const Config& config);
    
private:
    Config config_;
    QueryParser parser_;
    IndexManager index_manager_;
    
    // EN: Table storage
    // FR: Stockage des tables
    std::unordered_map<std::string, std::vector<std::string>> table_headers_;
    std::unordered_map<std::string, std::vector<std::vector<std::string>>> table_data_;
    
    // EN: Query cache
    // FR: Cache de requêtes
    mutable std::unordered_map<std::string, QueryResult> query_cache_;
    mutable std::unordered_map<std::string, std::chrono::system_clock::time_point> cache_timestamps_;
    
    // EN: Statistics
    // FR: Statistiques
    mutable EngineStatistics statistics_;
    mutable std::mutex stats_mutex_;
    mutable std::mutex cache_mutex_;
    mutable std::mutex table_mutex_;
    
    // EN: Query execution helpers
    // FR: Aides à l'exécution de requêtes
    QueryResult executeInternal(const SqlQuery& query);
    QueryResult executeSelect(const SqlQuery& query);
    
    // EN: Condition evaluation
    // FR: Évaluation des conditions
    bool evaluateWhere(const std::vector<std::string>& row, const std::vector<std::string>& headers,
                      const std::vector<WhereCondition>& conditions) const;
    bool evaluateCondition(const std::string& value, const WhereCondition& condition) const;
    
    // EN: Aggregation functions
    // FR: Fonctions d'agrégation
    QueryResult applyAggregation(const QueryResult& intermediate_result, const SqlQuery& query);
    std::string calculateAggregate(const std::vector<std::string>& values, AggregateFunction func) const;
    
    // EN: Sorting and limiting
    // FR: Tri et limitation
    void applySorting(QueryResult& result, const std::vector<OrderByColumn>& order_by) const;
    QueryResult applyLimitOffset(const QueryResult& result, size_t limit, size_t offset) const;
    
    // EN: Utility functions
    // FR: Fonctions utilitaires
    std::string generateCacheKey(const SqlQuery& query) const;
    bool shouldUseIndex(const std::string& table, const std::string& column) const;
    std::vector<size_t> optimizedRowSelection(const std::string& table, const std::vector<WhereCondition>& conditions);
    
    // EN: Memory management
    // FR: Gestion de la mémoire
    size_t estimateMemoryUsage() const;
    void cleanupCache();
    
    void updateStatistics(const QueryResult& result, std::chrono::milliseconds execution_time) const;
};

// EN: Query utility functions
// FR: Fonctions utilitaires de requête
namespace QueryUtils {
    // EN: Value conversion utilities
    // FR: Utilitaires de conversion de valeurs
    QueryValue stringToQueryValue(const std::string& str);
    std::string queryValueToString(const QueryValue& value);
    bool compareValues(const QueryValue& a, const QueryValue& b, SqlOperator op);
    
    // EN: SQL escaping and formatting
    // FR: Échappement et formatage SQL
    std::string escapeString(const std::string& str);
    std::string formatSql(const std::string& sql);
    bool isNumeric(const std::string& str);
    
    // EN: Performance utilities
    // FR: Utilitaires de performance
    std::string formatDuration(std::chrono::milliseconds duration);
    std::string formatMemorySize(size_t bytes);
    std::string formatNumber(size_t number);
    
    // EN: CSV utilities
    // FR: Utilitaires CSV
    QueryError loadCsvFile(const std::string& filename, std::vector<std::string>& headers,
                          std::vector<std::vector<std::string>>& data);
    QueryError saveCsvFile(const std::string& filename, const QueryResult& result);
    
    // EN: Validation utilities
    // FR: Utilitaires de validation
    bool isValidSql(const std::string& sql);
    std::vector<std::string> extractTableNames(const std::string& sql);
    std::vector<std::string> extractColumnNames(const std::string& sql);
}

} // namespace CSV
} // namespace BBP