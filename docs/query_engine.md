# Query Engine - Documentation

## Vue d'ensemble / Overview

Le Query Engine de BB-Pipeline fournit des capacités de requête SQL-like pour les données CSV avec indexation rapide et optimisations de performance. Il permet d'exécuter des requêtes complexes sur des datasets volumineux de manière efficace, avec support complet pour les opérations SQL standard.

The Query Engine of BB-Pipeline provides SQL-like querying capabilities for CSV data with fast indexing and performance optimizations. It enables efficient execution of complex queries on large datasets with full support for standard SQL operations.

## Fonctionnalités / Features

### 🔍 **Syntaxe SQL Complète / Complete SQL Syntax**

- **SELECT** : Projection de colonnes avec support pour `*` et alias
- **FROM** : Spécification de table source
- **WHERE** : Conditions de filtrage avec opérateurs multiples
- **ORDER BY** : Tri multi-colonnes avec ASC/DESC
- **LIMIT/OFFSET** : Pagination des résultats
- **DISTINCT** : Élimination des doublons
- **Fonctions d'agrégation** : COUNT, SUM, AVG, MIN, MAX

### 🚀 **Indexation Avancée / Advanced Indexing**

- **Hash Index** : Recherche d'égalité ultra-rapide
- **B-Tree Index** : Requêtes de plage optimisées
- **Full-Text Index** : Recherche textuelle avec tokenization
- **Composite Index** : Index multi-colonnes
- **Auto-indexing** : Création automatique d'index

### ⚡ **Optimisations Performance / Performance Optimizations**

- **Query Caching** : Mise en cache intelligente des résultats
- **Index-based Optimization** : Sélection automatique d'index
- **Memory Management** : Gestion mémoire optimisée
- **Parallel Processing** : Traitement parallèle quand possible
- **Query Planning** : Analyse et optimisation des plans d'exécution

## Architecture

### Classes Principales / Main Classes

#### `QueryEngine`
Moteur principal de requête avec gestion de tables et d'index.

```cpp
class QueryEngine {
public:
    struct Config {
        size_t max_memory_mb = 500;
        size_t max_result_rows = 1000000;
        bool enable_query_cache = true;
        bool auto_index = true;
        std::chrono::seconds query_timeout{300};
    };
    
    explicit QueryEngine(const Config& config = Config{});
    
    // Gestion des tables
    QueryError loadTable(const std::string& table_name, const std::string& csv_file);
    QueryError registerTable(const std::string& table_name, 
                            const std::vector<std::string>& headers,
                            const std::vector<std::vector<std::string>>& data);
    
    // Exécution de requêtes
    QueryResult execute(const std::string& sql);
    
    // Gestion d'index
    QueryError createIndex(const std::string& table, const IndexConfig& config);
    
    // Analyse de performance
    std::string explainQuery(const std::string& sql);
};
```

#### `QueryResult`
Conteneur de résultats avec fonctions d'accès et de formatage.

```cpp
class QueryResult {
public:
    // Accès aux données
    const std::vector<std::string>& getHeaders() const;
    size_t getRowCount() const;
    const std::string& getCell(size_t row, size_t col) const;
    std::vector<std::string> getColumn(const std::string& column) const;
    
    // Manipulation
    void sortBy(const std::string& column, SortDirection direction);
    QueryResult slice(size_t offset, size_t limit) const;
    
    // Export
    std::string toCSV() const;
    std::string toJSON() const;
    std::string toTable() const;
};
```

#### `QueryParser`
Analyseur SQL avec gestion complète de la syntaxe.

```cpp
class QueryParser {
public:
    QueryError parse(const std::string& sql, SqlQuery& query);
    std::string getLastError() const;
    
    static bool isValidColumnName(const std::string& name);
    static std::string normalizeString(const std::string& str);
};
```

#### `IndexManager`
Gestionnaire d'index avec support multi-types.

```cpp
class IndexManager {
public:
    QueryError createIndex(const std::string& table, const IndexConfig& config);
    bool hasIndex(const std::string& table, const std::string& column) const;
    
    std::vector<size_t> findRowsByIndex(const std::string& table, 
                                       const std::string& column, 
                                       const QueryValue& value) const;
};
```

## Utilisation / Usage

### Configuration de Base / Basic Setup

```cpp
#include "csv/query_engine.hpp"

using namespace BBP::CSV;

// Configuration du moteur
QueryEngine::Config config;
config.max_memory_mb = 200;
config.enable_query_cache = true;
config.auto_index = true;

QueryEngine engine(config);
```

### Chargement de Données / Data Loading

```cpp
// Depuis fichier CSV
QueryError error = engine.loadTable("employees", "data/employees.csv");
if (error != QueryError::SUCCESS) {
    std::cerr << "Erreur chargement table" << std::endl;
}

// Depuis données en mémoire
std::vector<std::string> headers = {"id", "name", "department", "salary"};
std::vector<std::vector<std::string>> data = {
    {"1", "Alice", "Engineering", "75000"},
    {"2", "Bob", "Marketing", "65000"},
    {"3", "Charlie", "Engineering", "80000"}
};

engine.registerTable("employees", headers, data);
```

### Requêtes Basiques / Basic Queries

```cpp
// Sélection simple
auto result = engine.execute("SELECT * FROM employees");
std::cout << "Nombre d'employés: " << result.getRowCount() << std::endl;

// Sélection avec colonnes spécifiques
result = engine.execute("SELECT name, department FROM employees");

// Avec conditions WHERE
result = engine.execute("SELECT * FROM employees WHERE department = 'Engineering'");

// Avec tri
result = engine.execute("SELECT * FROM employees ORDER BY salary DESC");

// Avec pagination
result = engine.execute("SELECT * FROM employees LIMIT 10 OFFSET 5");
```

### Requêtes Avancées / Advanced Queries

```cpp
// Fonctions d'agrégation
auto result = engine.execute("SELECT COUNT(*) FROM employees");
std::cout << "Total employés: " << result.getCell(0, 0) << std::endl;

result = engine.execute("SELECT AVG(salary) FROM employees WHERE department = 'Engineering'");
std::cout << "Salaire moyen Engineering: " << result.getCell(0, 0) << std::endl;

// Requêtes complexes multi-conditions
result = engine.execute(R"(
    SELECT department, COUNT(*) as emp_count, AVG(salary) as avg_salary 
    FROM employees 
    WHERE salary > 60000 
    GROUP BY department 
    ORDER BY avg_salary DESC
)");

// DISTINCT pour éliminer doublons
result = engine.execute("SELECT DISTINCT department FROM employees");

// Recherche par motifs
result = engine.execute("SELECT * FROM employees WHERE name LIKE 'A%'");

// Plages de valeurs
result = engine.execute("SELECT * FROM employees WHERE salary BETWEEN 60000 AND 80000");

// Listes de valeurs
result = engine.execute("SELECT * FROM employees WHERE department IN ('Engineering', 'Marketing')");
```

### Gestion d'Index / Index Management

```cpp
// Créer index hash pour recherches d'égalité
IndexConfig hash_config;
hash_config.column = "department";
hash_config.type = IndexType::HASH;
hash_config.max_memory_mb = 50;

engine.createIndex("employees", hash_config);

// Créer index B-tree pour recherches de plage
IndexConfig btree_config;
btree_config.column = "salary";
btree_config.type = IndexType::BTREE;

engine.createIndex("employees", btree_config);

// Créer index texte intégral
IndexConfig fulltext_config;
fulltext_config.column = "name";
fulltext_config.type = IndexType::FULL_TEXT;
fulltext_config.case_sensitive = false;
fulltext_config.tokenizer = "standard";

engine.createIndex("employees", fulltext_config);

// Vérifier utilisation d'index
std::string plan = engine.explainQuery("SELECT * FROM employees WHERE department = 'Engineering'");
std::cout << plan << std::endl;
```

### Analyse de Performance / Performance Analysis

```cpp
// Plan d'exécution
std::string plan = engine.explainQuery(R"(
    SELECT * FROM employees 
    WHERE department = 'Engineering' 
    AND salary > 70000 
    ORDER BY name
)");

std::cout << "Plan d'exécution:\n" << plan << std::endl;

// Statistiques du moteur
auto stats = engine.getStatistics();
std::cout << "Requêtes exécutées: " << stats.total_queries_executed << std::endl;
std::cout << "Cache hits: " << stats.cache_hits << std::endl;
std::cout << "Temps total: " << stats.total_execution_time.count() << "ms" << std::endl;
```

### Formatage des Résultats / Result Formatting

```cpp
auto result = engine.execute("SELECT name, department, salary FROM employees LIMIT 5");

// Format tableau ASCII
std::cout << "Format Tableau:\n" << result.toTable() << std::endl;

// Format CSV
std::cout << "Format CSV:\n" << result.toCSV() << std::endl;

// Format JSON
std::cout << "Format JSON:\n" << result.toJSON() << std::endl;

// Export vers fichier
QueryUtils::saveCsvFile("output.csv", result);
```

## Syntaxe SQL Supportée / Supported SQL Syntax

### SELECT Statement

```sql
SELECT [DISTINCT] column1 [AS alias1], column2 [AS alias2], ...
FROM table_name
[WHERE conditions]
[GROUP BY columns]
[HAVING conditions]
[ORDER BY columns [ASC|DESC]]
[LIMIT count [OFFSET offset]]
```

### Opérateurs WHERE / WHERE Operators

```sql
-- Comparaison
column = value
column != value
column <> value
column < value
column <= value
column > value
column >= value

-- Motifs
column LIKE 'pattern%'
column NOT LIKE 'pattern%'

-- Listes
column IN (value1, value2, value3)
column NOT IN (value1, value2)

-- Plages
column BETWEEN value1 AND value2

-- Valeurs NULL
column IS NULL
column IS NOT NULL

-- Opérateurs logiques
condition1 AND condition2
condition1 OR condition2
NOT condition
```

### Fonctions d'Agrégation / Aggregate Functions

```sql
COUNT(column)       -- Compte les lignes
COUNT(*)           -- Compte toutes les lignes
SUM(column)        -- Somme des valeurs
AVG(column)        -- Moyenne des valeurs
MIN(column)        -- Valeur minimale
MAX(column)        -- Valeur maximale
DISTINCT(column)   -- Valeurs uniques
GROUP_CONCAT(column) -- Concaténation avec virgules
```

### Exemples Pratiques / Practical Examples

```sql
-- Analyse des départements
SELECT department, COUNT(*) as employee_count, AVG(salary) as avg_salary
FROM employees
GROUP BY department
ORDER BY avg_salary DESC;

-- Top 5 des salaires
SELECT name, salary
FROM employees
ORDER BY salary DESC
LIMIT 5;

-- Employés avec salaire au-dessus de la moyenne
SELECT name, salary
FROM employees
WHERE salary > (SELECT AVG(salary) FROM employees);

-- Recherche par nom
SELECT *
FROM employees
WHERE name LIKE '%son%'
ORDER BY name;

-- Analyse par tranche d'âge
SELECT 
    CASE 
        WHEN age < 30 THEN 'Jeunes'
        WHEN age < 40 THEN 'Moyens'
        ELSE 'Seniors'
    END as age_group,
    COUNT(*) as count
FROM employees
GROUP BY age_group;
```

## Configuration Avancée / Advanced Configuration

### Paramètres du Moteur / Engine Parameters

```cpp
QueryEngine::Config config;

// Mémoire et performance
config.max_memory_mb = 1000;              // Mémoire maximale (MB)
config.max_result_rows = 5000000;         // Lignes max par résultat
config.query_timeout = std::chrono::seconds(600); // Timeout des requêtes

// Cache
config.enable_query_cache = true;         // Activer cache requêtes
config.query_cache_size = 500;           // Taille du cache

// Index
config.auto_index = true;                // Création auto d'index
config.index_cache_size = 50;            // Cache d'index

QueryEngine engine(config);
```

### Configuration d'Index / Index Configuration

```cpp
IndexConfig config;

// Index hash - optimal pour égalité
config.type = IndexType::HASH;
config.column = "id";
config.max_memory_mb = 100;

// Index B-tree - optimal pour plages
config.type = IndexType::BTREE;
config.column = "salary";

// Index texte intégral
config.type = IndexType::FULL_TEXT;
config.column = "description";
config.case_sensitive = false;
config.tokenizer = "standard";  // ou "whitespace"

// Index composé (multi-colonnes)
config.type = IndexType::COMPOSITE;
config.composite_columns = {"department", "level"};
```

## Optimisations de Performance / Performance Optimizations

### Meilleures Pratiques / Best Practices

1. **Indexation Stratégique**
   - Indexer les colonnes fréquemment utilisées dans WHERE
   - Utiliser index hash pour égalités exactes
   - Utiliser index B-tree pour recherches de plage
   - Éviter trop d'index (coût mémoire)

2. **Conception de Requêtes**
   - Limiter les résultats avec LIMIT quand possible
   - Utiliser WHERE pour filtrer avant ORDER BY
   - Préférer COUNT(*) à COUNT(column) si approprié
   - Éviter SELECT * quand colonnes spécifiques suffisent

3. **Gestion Mémoire**
   - Configurer max_memory_mb selon ressources
   - Utiliser pagination pour gros résultats
   - Surveiller utilisation mémoire via statistiques
   - Vider cache périodiquement si nécessaire

### Exemple d'Optimisation / Optimization Example

```cpp
// Configuration optimisée pour gros datasets
QueryEngine::Config config;
config.max_memory_mb = 2000;
config.enable_query_cache = true;
config.auto_index = true;

QueryEngine engine(config);

// Charger données
engine.loadTable("large_dataset", "huge_file.csv");

// Créer index stratégiques
IndexConfig primary_index;
primary_index.column = "id";
primary_index.type = IndexType::HASH;
engine.createIndex("large_dataset", primary_index);

IndexConfig range_index;
range_index.column = "timestamp";
range_index.type = IndexType::BTREE;
engine.createIndex("large_dataset", range_index);

// Requête optimisée
auto result = engine.execute(R"(
    SELECT id, name, value
    FROM large_dataset
    WHERE id = '12345'
    AND timestamp BETWEEN '2024-01-01' AND '2024-12-31'
    ORDER BY timestamp DESC
    LIMIT 100
)");

// Analyser performance
std::string plan = engine.explainQuery(/* same query */);
auto stats = engine.getStatistics();
```

## Gestion d'Erreurs / Error Handling

### Types d'Erreurs / Error Types

```cpp
enum class QueryError {
    SUCCESS = 0,
    SYNTAX_ERROR,      // Erreur syntaxe SQL
    FILE_NOT_FOUND,    // Fichier CSV introuvable
    COLUMN_NOT_FOUND,  // Colonne inexistante
    TYPE_MISMATCH,     // Type incompatible
    INDEX_ERROR,       // Erreur d'index
    MEMORY_ERROR,      // Mémoire insuffisante
    IO_ERROR,          // Erreur I/O
    EXECUTION_ERROR    // Erreur d'exécution
};
```

### Gestion Robuste / Robust Handling

```cpp
try {
    auto result = engine.execute("SELECT * FROM employees WHERE invalid_syntax");
    
    if (result.isEmpty()) {
        std::cerr << "Requête n'a retourné aucun résultat" << std::endl;
    }
    
    // Vérifier statistiques pour erreurs
    const auto& stats = result.getStatistics();
    if (stats.execution_time > std::chrono::seconds(10)) {
        std::cerr << "Requête très lente: " << stats.execution_time.count() << "ms" << std::endl;
    }
    
} catch (const std::exception& e) {
    std::cerr << "Erreur d'exécution: " << e.what() << std::endl;
}

// Validation préalable
if (!QueryUtils::isValidSql("SELECT * FROM table")) {
    std::cerr << "SQL invalide détecté avant exécution" << std::endl;
}
```

## Intégration avec BB-Pipeline / Integration with BB-Pipeline

### Analyse de Données de Reconnaissance / Reconnaissance Data Analysis

```cpp
// Charger résultats de reconnaissance
engine.loadTable("subdomains", "out/01_subdomains.csv");
engine.loadTable("probe_results", "out/02_probe.csv");
engine.loadTable("vulnerabilities", "out/07_api_findings.csv");

// Créer index pour optimisation
IndexConfig domain_index;
domain_index.column = "domain";
domain_index.type = IndexType::HASH;
engine.createIndex("subdomains", domain_index);

// Analyses complexes
auto result = engine.execute(R"(
    SELECT domain, COUNT(*) as subdomain_count
    FROM subdomains
    WHERE status = 'active'
    GROUP BY domain
    ORDER BY subdomain_count DESC
    LIMIT 10
)");

std::cout << "Top 10 domaines par nombre de sous-domaines:\n";
std::cout << result.toTable() << std::endl;

// Corrélation entre modules
result = engine.execute(R"(
    SELECT COUNT(*) as total_endpoints
    FROM probe_results
    WHERE http_status = '200'
    AND content_length > 1000
)");

// Export pour autres outils
QueryUtils::saveCsvFile("analysis_results.csv", result);
```

### Dashboard et Monitoring / Dashboard and Monitoring

```cpp
class ReconDashboard {
private:
    QueryEngine engine;
    
public:
    void generateReport() {
        // Statistiques générales
        auto total_domains = engine.execute("SELECT COUNT(DISTINCT domain) FROM subdomains");
        auto active_endpoints = engine.execute("SELECT COUNT(*) FROM probe_results WHERE status = 'up'");
        auto vulnerabilities = engine.execute("SELECT COUNT(*) FROM vulnerabilities WHERE severity = 'high'");
        
        std::cout << "=== Rapport de Reconnaissance ===\n";
        std::cout << "Domaines découverts: " << total_domains.getCell(0, 0) << "\n";
        std::cout << "Endpoints actifs: " << active_endpoints.getCell(0, 0) << "\n";
        std::cout << "Vulnérabilités critiques: " << vulnerabilities.getCell(0, 0) << "\n";
        
        // Top technologies détectées
        auto tech_stats = engine.execute(R"(
            SELECT technology, COUNT(*) as count
            FROM probe_results
            WHERE technology IS NOT NULL
            GROUP BY technology
            ORDER BY count DESC
            LIMIT 5
        )");
        
        std::cout << "\nTop Technologies:\n" << tech_stats.toTable() << std::endl;
    }
};
```

## Extensibilité / Extensibility

### Nouvelles Fonctions / Custom Functions

Le moteur peut être étendu pour supporter de nouvelles fonctions :

```cpp
class CustomQueryEngine : public QueryEngine {
public:
    // Ajouter support pour fonctions personnalisées
    QueryResult execute(const std::string& sql) override {
        // Préprocessing pour fonctions custom
        std::string processed_sql = preprocessCustomFunctions(sql);
        return QueryEngine::execute(processed_sql);
    }

private:
    std::string preprocessCustomFunctions(const std::string& sql) {
        // Transformer fonctions personnalisées en SQL standard
        return sql;
    }
};
```

### Nouveaux Types d'Index / Custom Index Types

```cpp
class CustomIndexManager : public IndexManager {
public:
    QueryError createGeoIndex(const std::string& table, const std::string& lat_col, const std::string& lng_col) {
        // Implémentation d'index géospatial
        return QueryError::SUCCESS;
    }
    
    std::vector<size_t> findNearbyPoints(const std::string& table, double lat, double lng, double radius) {
        // Recherche par proximité géographique
        return {};
    }
};
```

## Limitations et Considérations / Limitations and Considerations

### Limitations Actuelles / Current Limitations

1. **Pas de JOIN** : Les jointures entre tables ne sont pas encore supportées
2. **Sous-requêtes limitées** : Support limité pour les sous-requêtes complexes
3. **Transactions** : Pas de support transactionnel (read-only)
4. **Types de données** : Tout est traité comme chaîne de caractères
5. **Concurrence** : Accès concurrent limité (lectures multiples, pas d'écritures)

### Considérations de Performance / Performance Considerations

1. **Mémoire** : Garde toutes les données en mémoire
2. **Index** : Les index consomment de la mémoire supplémentaire
3. **Cache** : Le cache de requêtes peut devenir volumineux
4. **CPU** : Les requêtes complexes peuvent être CPU-intensives

### Cas d'Usage Recommandés / Recommended Use Cases

✅ **Adapté pour :**
- Analyse de données de reconnaissance
- Génération de rapports
- Filtrage et agrégation de logs CSV
- Tableaux de bord temps réel
- Data mining sur CSV moyens/gros

❌ **Non adapté pour :**
- Applications transactionnelles
- Données relationnelles complexes avec besoins JOIN
- Très gros datasets (>10GB)
- Applications nécessitant persistance

## Roadmap / Roadmap

### Prochaines Versions / Upcoming Versions

#### v2.0 - Fonctionnalités Relationnelles
- [ ] Support JOIN (INNER, LEFT, RIGHT, FULL)
- [ ] Sous-requêtes corrélées
- [ ] Vues matérialisées
- [ ] Index composés avancés

#### v2.1 - Performance et Scalabilité
- [ ] Streaming pour gros datasets
- [ ] Partitioning automatique
- [ ] Parallel query execution
- [ ] Compression des données en mémoire

#### v2.2 - Fonctionnalités Avancées
- [ ] Window functions (ROW_NUMBER, RANK, etc.)
- [ ] Fonctions de date/temps
- [ ] Expressions régulières avancées
- [ ] Support JSON/XML dans colonnes

#### v3.0 - Distribution et Persistance
- [ ] Query engine distribué
- [ ] Persistance sur disque
- [ ] Réplication des données
- [ ] API REST pour requêtes distantes

---

*Documentation générée pour BB-Pipeline Query Engine Module v1.0*