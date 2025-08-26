#include "csv/query_engine.hpp"
#include "infrastructure/logging/logger.hpp"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <thread>
#include <future>
#include <iomanip>
#include <cmath>
#include <cctype>
#include <functional>
#include <set>

namespace BBP {
namespace CSV {

// EN: QueryResult implementation
// FR: Implémentation de QueryResult

QueryResult::QueryResult(std::vector<std::string> headers) 
    : headers_(std::move(headers)) {
    buildColumnIndexMap();
}

void QueryResult::addRow(const std::vector<std::string>& row) {
    rows_.push_back(row);
}

void QueryResult::addRow(std::vector<std::string>&& row) {
    rows_.emplace_back(std::move(row));
}

const std::vector<std::string>& QueryResult::getRow(size_t index) const {
    if (index >= rows_.size()) {
        throw std::out_of_range("Row index out of range");
    }
    return rows_[index];
}

const std::string& QueryResult::getCell(size_t row, size_t col) const {
    if (row >= rows_.size()) {
        throw std::out_of_range("Row index out of range");
    }
    if (col >= headers_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    return rows_[row][col];
}

const std::string& QueryResult::getCell(size_t row, const std::string& column) const {
    int col_idx = getColumnIndex(column);
    if (col_idx < 0) {
        throw std::invalid_argument("Column not found: " + column);
    }
    return getCell(row, static_cast<size_t>(col_idx));
}

int QueryResult::getColumnIndex(const std::string& column) const {
    auto it = column_index_map_.find(column);
    return (it != column_index_map_.end()) ? static_cast<int>(it->second) : -1;
}

std::vector<std::string> QueryResult::getColumn(const std::string& column) const {
    int col_idx = getColumnIndex(column);
    if (col_idx < 0) {
        throw std::invalid_argument("Column not found: " + column);
    }
    return getColumn(static_cast<size_t>(col_idx));
}

std::vector<std::string> QueryResult::getColumn(size_t index) const {
    if (index >= headers_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    
    std::vector<std::string> column_data;
    column_data.reserve(rows_.size());
    for (const auto& row : rows_) {
        column_data.push_back(index < row.size() ? row[index] : "");
    }
    return column_data;
}

void QueryResult::sortBy(const std::string& column, SortDirection direction) {
    int col_idx = getColumnIndex(column);
    if (col_idx < 0) {
        throw std::invalid_argument("Column not found for sorting: " + column);
    }
    
    std::sort(rows_.begin(), rows_.end(), 
        [col_idx, direction](const std::vector<std::string>& a, const std::vector<std::string>& b) {
            const std::string& val_a = static_cast<size_t>(col_idx) < a.size() ? a[col_idx] : "";
            const std::string& val_b = static_cast<size_t>(col_idx) < b.size() ? b[col_idx] : "";
            
            // EN: Try numeric comparison first
            // FR: Essayer d'abord la comparaison numérique
            if (QueryUtils::isNumeric(val_a) && QueryUtils::isNumeric(val_b)) {
                double num_a = std::stod(val_a);
                double num_b = std::stod(val_b);
                return direction == SortDirection::ASC ? (num_a < num_b) : (num_a > num_b);
            }
            
            // EN: Fall back to string comparison
            // FR: Se rabattre sur la comparaison de chaînes
            return direction == SortDirection::ASC ? (val_a < val_b) : (val_a > val_b);
        });
}

void QueryResult::sortBy(const std::vector<OrderByColumn>& sort_spec) {
    if (sort_spec.empty()) return;
    
    // EN: Build column indices for sorting
    // FR: Construire les indices de colonnes pour le tri
    std::vector<int> col_indices;
    for (const auto& spec : sort_spec) {
        int idx = getColumnIndex(spec.column);
        if (idx < 0) {
            throw std::invalid_argument("Column not found for sorting: " + spec.column);
        }
        col_indices.push_back(idx);
    }
    
    std::sort(rows_.begin(), rows_.end(),
        [&col_indices, &sort_spec](const std::vector<std::string>& a, const std::vector<std::string>& b) {
            for (size_t i = 0; i < col_indices.size(); ++i) {
                int col_idx = col_indices[i];
                const std::string& val_a = static_cast<size_t>(col_idx) < a.size() ? a[col_idx] : "";
                const std::string& val_b = static_cast<size_t>(col_idx) < b.size() ? b[col_idx] : "";
                
                if (val_a == val_b) continue;
                
                // EN: Try numeric comparison first
                // FR: Essayer d'abord la comparaison numérique
                if (QueryUtils::isNumeric(val_a) && QueryUtils::isNumeric(val_b)) {
                    double num_a = std::stod(val_a);
                    double num_b = std::stod(val_b);
                    bool result = sort_spec[i].direction == SortDirection::ASC ? (num_a < num_b) : (num_a > num_b);
                    return result;
                }
                
                // EN: Fall back to string comparison
                // FR: Se rabattre sur la comparaison de chaînes
                bool result = sort_spec[i].direction == SortDirection::ASC ? (val_a < val_b) : (val_a > val_b);
                return result;
            }
            return false; // Equal
        });
}

QueryResult QueryResult::slice(size_t offset, size_t limit) const {
    QueryResult result(headers_);
    result.statistics_ = statistics_;
    
    size_t start = std::min(offset, rows_.size());
    size_t end = std::min(start + limit, rows_.size());
    
    for (size_t i = start; i < end; ++i) {
        result.addRow(rows_[i]);
    }
    
    return result;
}

std::string QueryResult::toCSV() const {
    std::ostringstream oss;
    
    // EN: Write headers
    // FR: Écrire les en-têtes
    for (size_t i = 0; i < headers_.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << headers_[i] << "\"";
    }
    oss << "\n";
    
    // EN: Write data rows
    // FR: Écrire les lignes de données
    for (const auto& row : rows_) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) oss << ",";
            oss << "\"" << row[i] << "\"";
        }
        // EN: Fill missing columns with empty values
        // FR: Remplir les colonnes manquantes avec des valeurs vides
        for (size_t i = row.size(); i < headers_.size(); ++i) {
            oss << ",\"\"";
        }
        oss << "\n";
    }
    
    return oss.str();
}

std::string QueryResult::toJSON() const {
    std::ostringstream oss;
    oss << "{\"data\":[";
    
    for (size_t row_idx = 0; row_idx < rows_.size(); ++row_idx) {
        if (row_idx > 0) oss << ",";
        oss << "{";
        
        for (size_t col_idx = 0; col_idx < headers_.size(); ++col_idx) {
            if (col_idx > 0) oss << ",";
            oss << "\"" << headers_[col_idx] << "\":\"";
            if (col_idx < rows_[row_idx].size()) {
                oss << rows_[row_idx][col_idx];
            }
            oss << "\"";
        }
        
        oss << "}";
    }
    
    oss << "],\"count\":" << rows_.size() << "}";
    return oss.str();
}

std::string QueryResult::toTable() const {
    if (rows_.empty()) {
        return "Empty result set\n";
    }
    
    // EN: Calculate column widths
    // FR: Calculer les largeurs de colonnes
    std::vector<size_t> widths(headers_.size());
    for (size_t i = 0; i < headers_.size(); ++i) {
        widths[i] = headers_[i].length();
        for (const auto& row : rows_) {
            if (i < row.size()) {
                widths[i] = std::max(widths[i], row[i].length());
            }
        }
        widths[i] = std::min(widths[i], size_t(50)); // EN: Max width / FR: Largeur max
    }
    
    std::ostringstream oss;
    
    // EN: Top border
    // FR: Bordure supérieure
    oss << "+";
    for (size_t width : widths) {
        oss << std::string(width + 2, '-') << "+";
    }
    oss << "\n";
    
    // EN: Headers
    // FR: En-têtes
    oss << "|";
    for (size_t i = 0; i < headers_.size(); ++i) {
        oss << " " << std::left << std::setw(widths[i]) << headers_[i] << " |";
    }
    oss << "\n";
    
    // EN: Header separator
    // FR: Séparateur d'en-têtes
    oss << "+";
    for (size_t width : widths) {
        oss << std::string(width + 2, '=') << "+";
    }
    oss << "\n";
    
    // EN: Data rows
    // FR: Lignes de données
    for (const auto& row : rows_) {
        oss << "|";
        for (size_t i = 0; i < headers_.size(); ++i) {
            std::string value = (i < row.size()) ? row[i] : "";
            if (value.length() > widths[i]) {
                value = value.substr(0, widths[i] - 3) + "...";
            }
            oss << " " << std::left << std::setw(widths[i]) << value << " |";
        }
        oss << "\n";
    }
    
    // EN: Bottom border
    // FR: Bordure inférieure
    oss << "+";
    for (size_t width : widths) {
        oss << std::string(width + 2, '-') << "+";
    }
    oss << "\n";
    
    oss << "(" << rows_.size() << " row" << (rows_.size() != 1 ? "s" : "") << ")\n";
    
    return oss.str();
}

void QueryResult::clear() {
    rows_.clear();
    statistics_ = QueryStatistics{};
}

void QueryResult::buildColumnIndexMap() {
    column_index_map_.clear();
    for (size_t i = 0; i < headers_.size(); ++i) {
        column_index_map_[headers_[i]] = i;
    }
}

// EN: QueryParser implementation
// FR: Implémentation de QueryParser

QueryError QueryParser::parse(const std::string& sql, SqlQuery& query) {
    // EN: Reset state
    // FR: Réinitialiser l'état
    last_error_.clear();
    error_position_ = 0;
    
    // EN: Store original SQL
    // FR: Stocker le SQL original
    query.raw_sql = sql;
    query.created_at = std::chrono::system_clock::now();
    
    // EN: Parse SQL statement
    // FR: Analyser la déclaration SQL
    size_t pos = 0;
    skipWhitespace(sql, pos);
    
    if (pos >= sql.length()) {
        setError("Empty SQL query", pos);
        return QueryError::SYNTAX_ERROR;
    }
    
    // EN: Check for SELECT keyword
    // FR: Vérifier le mot-clé SELECT
    if (!matchKeyword(sql, pos, "SELECT")) {
        setError("Expected SELECT keyword", pos);
        return QueryError::SYNTAX_ERROR;
    }
    
    // EN: Parse different clauses in order
    // FR: Analyser les différentes clauses dans l'ordre
    QueryError error = parseSelect(sql, pos, query);
    if (error != QueryError::SUCCESS) return error;
    
    error = parseFrom(sql, pos, query);
    if (error != QueryError::SUCCESS) return error;
    
    error = parseWhere(sql, pos, query);
    if (error != QueryError::SUCCESS) return error;
    
    error = parseGroupBy(sql, pos, query);
    if (error != QueryError::SUCCESS) return error;
    
    error = parseHaving(sql, pos, query);
    if (error != QueryError::SUCCESS) return error;
    
    error = parseOrderBy(sql, pos, query);
    if (error != QueryError::SUCCESS) return error;
    
    error = parseLimit(sql, pos, query);
    if (error != QueryError::SUCCESS) return error;
    
    return QueryError::SUCCESS;
}

QueryError QueryParser::parseSelect(const std::string& sql, size_t& pos, SqlQuery& query) {
    skipWhitespace(sql, pos);
    
    // EN: Check for DISTINCT
    // FR: Vérifier DISTINCT
    if (matchKeyword(sql, pos, "DISTINCT")) {
        query.distinct_query = true;
        skipWhitespace(sql, pos);
    }
    
    // EN: Parse column specifications
    // FR: Analyser les spécifications de colonnes
    while (pos < sql.length()) {
        SelectColumn column;
        
        // EN: Check for aggregate functions
        // FR: Vérifier les fonctions d'agrégation
        std::string token = parseIdentifier(sql, pos);
        if (token.empty()) {
            setError("Expected column specification", pos);
            return QueryError::SYNTAX_ERROR;
        }
        
        // EN: Check if it's an aggregate function
        // FR: Vérifier si c'est une fonction d'agrégation
        AggregateFunction agg = parseAggregate(token);
        if (agg != AggregateFunction::NONE) {
            column.aggregate = agg;
            skipWhitespace(sql, pos);
            if (pos >= sql.length() || sql[pos] != '(') {
                setError("Expected '(' after aggregate function", pos);
                return QueryError::SYNTAX_ERROR;
            }
            pos++; // Skip '('
            skipWhitespace(sql, pos);
            column.column = parseIdentifier(sql, pos);
            skipWhitespace(sql, pos);
            if (pos >= sql.length() || sql[pos] != ')') {
                setError("Expected ')' after aggregate function", pos);
                return QueryError::SYNTAX_ERROR;
            }
            pos++; // Skip ')'
        } else {
            column.column = token;
        }
        
        skipWhitespace(sql, pos);
        
        // EN: Check for alias (AS keyword)
        // FR: Vérifier l'alias (mot-clé AS)
        if (matchKeyword(sql, pos, "AS")) {
            skipWhitespace(sql, pos);
            column.alias = parseIdentifier(sql, pos);
            skipWhitespace(sql, pos);
        }
        
        query.columns.push_back(column);
        
        // EN: Check for comma or end of SELECT clause
        // FR: Vérifier la virgule ou la fin de la clause SELECT
        if (pos >= sql.length()) break;
        
        if (sql[pos] == ',') {
            pos++;
            skipWhitespace(sql, pos);
            continue;
        }
        
        // EN: Check if we've reached FROM or other clause
        // FR: Vérifier si nous avons atteint FROM ou autre clause
        size_t temp_pos = pos;
        std::string next_token = parseIdentifier(sql, temp_pos);
        if (next_token == "FROM" || next_token == "WHERE" || next_token == "GROUP" || 
            next_token == "HAVING" || next_token == "ORDER" || next_token == "LIMIT") {
            break;
        }
        
        setError("Expected ',' between column specifications", pos);
        return QueryError::SYNTAX_ERROR;
    }
    
    return QueryError::SUCCESS;
}

QueryError QueryParser::parseFrom(const std::string& sql, size_t& pos, SqlQuery& query) {
    skipWhitespace(sql, pos);
    
    if (!matchKeyword(sql, pos, "FROM")) {
        setError("Expected FROM clause", pos);
        return QueryError::SYNTAX_ERROR;
    }
    
    skipWhitespace(sql, pos);
    query.table = parseIdentifier(sql, pos);
    
    if (query.table.empty()) {
        setError("Expected table name after FROM", pos);
        return QueryError::SYNTAX_ERROR;
    }
    
    return QueryError::SUCCESS;
}

QueryError QueryParser::parseWhere(const std::string& sql, size_t& pos, SqlQuery& query) {
    skipWhitespace(sql, pos);
    
    if (!matchKeyword(sql, pos, "WHERE")) {
        return QueryError::SUCCESS; // EN: WHERE is optional / FR: WHERE est optionnel
    }
    
    // EN: Parse WHERE conditions
    // FR: Analyser les conditions WHERE
    while (pos < sql.length()) {
        WhereCondition condition;
        
        skipWhitespace(sql, pos);
        condition.column = parseIdentifier(sql, pos);
        if (condition.column.empty()) {
            setError("Expected column name in WHERE clause", pos);
            return QueryError::SYNTAX_ERROR;
        }
        
        skipWhitespace(sql, pos);
        condition.operator_ = parseOperator(sql, pos);
        
        skipWhitespace(sql, pos);
        if (condition.operator_ == SqlOperator::BETWEEN) {
            condition.range_start = parseValue(sql, pos);
            skipWhitespace(sql, pos);
            if (!matchKeyword(sql, pos, "AND")) {
                setError("Expected AND in BETWEEN clause", pos);
                return QueryError::SYNTAX_ERROR;
            }
            skipWhitespace(sql, pos);
            condition.range_end = parseValue(sql, pos);
        } else if (condition.operator_ == SqlOperator::IN || condition.operator_ == SqlOperator::NOT_IN) {
            skipWhitespace(sql, pos);
            if (pos >= sql.length() || sql[pos] != '(') {
                setError("Expected '(' after IN operator", pos);
                return QueryError::SYNTAX_ERROR;
            }
            pos++; // Skip '('
            
            // EN: Parse IN values
            // FR: Analyser les valeurs IN
            while (pos < sql.length()) {
                skipWhitespace(sql, pos);
                condition.in_values.push_back(parseValue(sql, pos));
                skipWhitespace(sql, pos);
                
                if (pos >= sql.length()) {
                    setError("Expected ')' to close IN clause", pos);
                    return QueryError::SYNTAX_ERROR;
                }
                
                if (sql[pos] == ')') {
                    pos++;
                    break;
                } else if (sql[pos] == ',') {
                    pos++;
                    continue;
                } else {
                    setError("Expected ',' or ')' in IN clause", pos);
                    return QueryError::SYNTAX_ERROR;
                }
            }
        } else if (condition.operator_ == SqlOperator::LIKE || condition.operator_ == SqlOperator::NOT_LIKE) {
            QueryValue pattern_value = parseValue(sql, pos);
            condition.pattern = std::get<std::string>(pattern_value);
        } else if (condition.operator_ != SqlOperator::IS_NULL && condition.operator_ != SqlOperator::IS_NOT_NULL) {
            condition.value = parseValue(sql, pos);
        }
        
        query.where.push_back(condition);
        
        skipWhitespace(sql, pos);
        
        // EN: Check for AND/OR
        // FR: Vérifier AND/OR
        if (matchKeyword(sql, pos, "AND")) {
            query.where.back().logical_op = LogicalOperator::AND;
            continue;
        } else if (matchKeyword(sql, pos, "OR")) {
            query.where.back().logical_op = LogicalOperator::OR;
            continue;
        } else {
            break;
        }
    }
    
    return QueryError::SUCCESS;
}

QueryError QueryParser::parseGroupBy(const std::string& sql, size_t& pos, SqlQuery& query) {
    skipWhitespace(sql, pos);
    
    if (!matchKeyword(sql, pos, "GROUP")) {
        return QueryError::SUCCESS; // EN: GROUP BY is optional / FR: GROUP BY est optionnel
    }
    
    skipWhitespace(sql, pos);
    if (!matchKeyword(sql, pos, "BY")) {
        setError("Expected BY after GROUP", pos);
        return QueryError::SYNTAX_ERROR;
    }
    
    // EN: Parse GROUP BY columns
    // FR: Analyser les colonnes GROUP BY
    while (pos < sql.length()) {
        skipWhitespace(sql, pos);
        std::string column = parseIdentifier(sql, pos);
        if (column.empty()) {
            setError("Expected column name in GROUP BY", pos);
            return QueryError::SYNTAX_ERROR;
        }
        
        query.group_by.push_back(column);
        
        skipWhitespace(sql, pos);
        if (pos < sql.length() && sql[pos] == ',') {
            pos++;
            continue;
        } else {
            break;
        }
    }
    
    return QueryError::SUCCESS;
}

QueryError QueryParser::parseHaving(const std::string& sql, size_t& pos, SqlQuery& query) {
    skipWhitespace(sql, pos);
    
    if (!matchKeyword(sql, pos, "HAVING")) {
        return QueryError::SUCCESS; // EN: HAVING is optional / FR: HAVING est optionnel
    }
    
    // EN: HAVING has same syntax as WHERE
    // FR: HAVING a la même syntaxe que WHERE
    return parseWhere(sql, pos, query); // EN: Parse conditions same way / FR: Analyser les conditions de la même façon
}

QueryError QueryParser::parseOrderBy(const std::string& sql, size_t& pos, SqlQuery& query) {
    skipWhitespace(sql, pos);
    
    if (!matchKeyword(sql, pos, "ORDER")) {
        return QueryError::SUCCESS; // EN: ORDER BY is optional / FR: ORDER BY est optionnel
    }
    
    skipWhitespace(sql, pos);
    if (!matchKeyword(sql, pos, "BY")) {
        setError("Expected BY after ORDER", pos);
        return QueryError::SYNTAX_ERROR;
    }
    
    // EN: Parse ORDER BY columns
    // FR: Analyser les colonnes ORDER BY
    while (pos < sql.length()) {
        skipWhitespace(sql, pos);
        OrderByColumn order_col;
        order_col.column = parseIdentifier(sql, pos);
        if (order_col.column.empty()) {
            setError("Expected column name in ORDER BY", pos);
            return QueryError::SYNTAX_ERROR;
        }
        
        skipWhitespace(sql, pos);
        if (matchKeyword(sql, pos, "DESC")) {
            order_col.direction = SortDirection::DESC;
        } else if (matchKeyword(sql, pos, "ASC")) {
            order_col.direction = SortDirection::ASC;
        }
        // EN: Default is ASC / FR: Par défaut c'est ASC
        
        query.order_by.push_back(order_col);
        
        skipWhitespace(sql, pos);
        if (pos < sql.length() && sql[pos] == ',') {
            pos++;
            continue;
        } else {
            break;
        }
    }
    
    return QueryError::SUCCESS;
}

QueryError QueryParser::parseLimit(const std::string& sql, size_t& pos, SqlQuery& query) {
    skipWhitespace(sql, pos);
    
    if (!matchKeyword(sql, pos, "LIMIT")) {
        return QueryError::SUCCESS; // EN: LIMIT is optional / FR: LIMIT est optionnel
    }
    
    skipWhitespace(sql, pos);
    QueryValue limit_value = parseValue(sql, pos);
    
    if (std::holds_alternative<int64_t>(limit_value)) {
        query.limit = static_cast<size_t>(std::get<int64_t>(limit_value));
    } else {
        setError("Expected numeric value for LIMIT", pos);
        return QueryError::SYNTAX_ERROR;
    }
    
    skipWhitespace(sql, pos);
    
    // EN: Check for OFFSET
    // FR: Vérifier OFFSET
    if (matchKeyword(sql, pos, "OFFSET")) {
        skipWhitespace(sql, pos);
        QueryValue offset_value = parseValue(sql, pos);
        
        if (std::holds_alternative<int64_t>(offset_value)) {
            query.offset = static_cast<size_t>(std::get<int64_t>(offset_value));
        } else {
            setError("Expected numeric value for OFFSET", pos);
            return QueryError::SYNTAX_ERROR;
        }
    }
    
    return QueryError::SUCCESS;
}

QueryError QueryParser::parseJoin(const std::string& /* sql */, size_t& /* pos */, SqlQuery& /* query */) {
    // EN: JOIN parsing implementation would go here
    // FR: L'implémentation de l'analyse JOIN irait ici
    // EN: For now, return success (JOINs are advanced feature)
    // FR: Pour l'instant, retourner succès (JOINs sont une fonctionnalité avancée)
    return QueryError::SUCCESS;
}

std::string QueryParser::parseIdentifier(const std::string& sql, size_t& pos) {
    skipWhitespace(sql, pos);
    
    if (pos >= sql.length()) return "";
    
    std::string identifier;
    
    // EN: Handle quoted identifiers
    // FR: Gérer les identifiants entre guillemets
    if (sql[pos] == '"' || sql[pos] == '`') {
        return extractQuotedString(sql, pos);
    }
    
    // EN: Handle regular identifiers and keywords
    // FR: Gérer les identifiants normaux et les mots-clés
    while (pos < sql.length() && 
           (std::isalnum(sql[pos]) || sql[pos] == '_' || sql[pos] == '*')) {
        identifier += sql[pos];
        pos++;
    }
    
    return identifier;
}

QueryValue QueryParser::parseValue(const std::string& sql, size_t& pos) {
    skipWhitespace(sql, pos);
    
    if (pos >= sql.length()) {
        return std::string(""); // EN: Empty string / FR: Chaîne vide
    }
    
    // EN: Handle quoted strings
    // FR: Gérer les chaînes entre guillemets
    if (sql[pos] == '\'' || sql[pos] == '"') {
        return extractQuotedString(sql, pos);
    }
    
    // EN: Handle NULL
    // FR: Gérer NULL
    if (sql.substr(pos, 4) == "NULL") {
        pos += 4;
        return nullptr;
    }
    
    // EN: Try to parse as number
    // FR: Essayer d'analyser comme nombre
    std::string value_str;
    bool has_dot = false;
    bool is_negative = false;
    (void)is_negative;  // Suppress warning
    
    if (sql[pos] == '-') {
        is_negative = true;
        value_str += sql[pos];
        pos++;
    }
    
    while (pos < sql.length() && 
           (std::isdigit(sql[pos]) || (sql[pos] == '.' && !has_dot))) {
        if (sql[pos] == '.') has_dot = true;
        value_str += sql[pos];
        pos++;
    }
    
    if (!value_str.empty() && (value_str != "-")) {
        try {
            if (has_dot) {
                return std::stod(value_str);
            } else {
                return static_cast<int64_t>(std::stoll(value_str));
            }
        } catch (...) {
            // EN: Fall through to string parsing / FR: Continuer vers l'analyse de chaîne
        }
    }
    
    // EN: Fall back to identifier parsing
    // FR: Se rabattre sur l'analyse d'identifiant
    pos -= value_str.length(); // EN: Reset position / FR: Réinitialiser la position
    return parseIdentifier(sql, pos);
}

SqlOperator QueryParser::parseOperator(const std::string& sql, size_t& pos) {
    skipWhitespace(sql, pos);
    
    if (pos >= sql.length()) return SqlOperator::EQUALS;
    
    // EN: Check for multi-character operators first
    // FR: Vérifier d'abord les opérateurs multi-caractères
    if (pos + 1 < sql.length()) {
        std::string two_char = sql.substr(pos, 2);
        if (two_char == ">=") { pos += 2; return SqlOperator::GREATER_EQUAL; }
        if (two_char == "<=") { pos += 2; return SqlOperator::LESS_EQUAL; }
        if (two_char == "!=") { pos += 2; return SqlOperator::NOT_EQUALS; }
        if (two_char == "<>") { pos += 2; return SqlOperator::NOT_EQUALS; }
    }
    
    // EN: Check for keywords
    // FR: Vérifier les mots-clés
    if (matchKeyword(sql, pos, "LIKE")) return SqlOperator::LIKE;
    if (matchKeyword(sql, pos, "NOT")) {
        skipWhitespace(sql, pos);
        if (matchKeyword(sql, pos, "LIKE")) return SqlOperator::NOT_LIKE;
        if (matchKeyword(sql, pos, "IN")) return SqlOperator::NOT_IN;
    }
    if (matchKeyword(sql, pos, "IN")) return SqlOperator::IN;
    if (matchKeyword(sql, pos, "IS")) {
        skipWhitespace(sql, pos);
        if (matchKeyword(sql, pos, "NOT")) {
            skipWhitespace(sql, pos);
            if (matchKeyword(sql, pos, "NULL")) return SqlOperator::IS_NOT_NULL;
        } else if (matchKeyword(sql, pos, "NULL")) {
            return SqlOperator::IS_NULL;
        }
    }
    if (matchKeyword(sql, pos, "BETWEEN")) return SqlOperator::BETWEEN;
    
    // EN: Single character operators
    // FR: Opérateurs à un caractère
    char op = sql[pos];
    pos++;
    switch (op) {
        case '=': return SqlOperator::EQUALS;
        case '<': return SqlOperator::LESS_THAN;
        case '>': return SqlOperator::GREATER_THAN;
        default: return SqlOperator::EQUALS;
    }
}

AggregateFunction QueryParser::parseAggregate(const std::string& token) {
    std::string upper_token = token;
    std::transform(upper_token.begin(), upper_token.end(), upper_token.begin(), ::toupper);
    
    if (upper_token == "COUNT") return AggregateFunction::COUNT;
    if (upper_token == "SUM") return AggregateFunction::SUM;
    if (upper_token == "AVG") return AggregateFunction::AVG;
    if (upper_token == "MIN") return AggregateFunction::MIN;
    if (upper_token == "MAX") return AggregateFunction::MAX;
    if (upper_token == "DISTINCT") return AggregateFunction::DISTINCT;
    if (upper_token == "GROUP_CONCAT") return AggregateFunction::GROUP_CONCAT;
    
    return AggregateFunction::NONE;
}

void QueryParser::skipWhitespace(const std::string& sql, size_t& pos) {
    while (pos < sql.length() && std::isspace(sql[pos])) {
        pos++;
    }
}

bool QueryParser::matchKeyword(const std::string& sql, size_t& pos, const std::string& keyword) {
    size_t start_pos = pos;
    skipWhitespace(sql, pos);
    
    if (pos + keyword.length() > sql.length()) {
        pos = start_pos;
        return false;
    }
    
    std::string token = sql.substr(pos, keyword.length());
    std::transform(token.begin(), token.end(), token.begin(), ::toupper);
    
    if (token == keyword) {
        pos += keyword.length();
        // EN: Ensure it's a complete word / FR: S'assurer que c'est un mot complet
        if (pos < sql.length() && (std::isalnum(sql[pos]) || sql[pos] == '_')) {
            pos = start_pos;
            return false;
        }
        return true;
    }
    
    pos = start_pos;
    return false;
}

std::string QueryParser::extractQuotedString(const std::string& sql, size_t& pos) {
    if (pos >= sql.length()) return "";
    
    char quote_char = sql[pos];
    pos++; // EN: Skip opening quote / FR: Ignorer le guillemet d'ouverture
    
    std::string result;
    while (pos < sql.length() && sql[pos] != quote_char) {
        if (sql[pos] == '\\' && pos + 1 < sql.length()) {
            // EN: Handle escape sequences / FR: Gérer les séquences d'échappement
            pos++;
            switch (sql[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '\'': result += '\''; break;
                case '"': result += '"'; break;
                default: result += sql[pos]; break;
            }
        } else {
            result += sql[pos];
        }
        pos++;
    }
    
    if (pos < sql.length() && sql[pos] == quote_char) {
        pos++; // EN: Skip closing quote / FR: Ignorer le guillemet de fermeture
    }
    
    return result;
}

void QueryParser::setError(const std::string& message, size_t position) {
    last_error_ = message;
    error_position_ = position;
}

bool QueryParser::isValidColumnName(const std::string& name) {
    if (name.empty() || name == "*") return true;
    
    // EN: Check for valid identifier characters
    // FR: Vérifier les caractères d'identifiant valides
    if (!std::isalpha(name[0]) && name[0] != '_') return false;
    
    for (char c : name) {
        if (!std::isalnum(c) && c != '_') return false;
    }
    
    return true;
}

bool QueryParser::isValidTableName(const std::string& name) {
    return isValidColumnName(name); // EN: Same rules / FR: Mêmes règles
}

std::string QueryParser::normalizeString(const std::string& str) {
    std::string normalized = str;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    
    // EN: Trim whitespace / FR: Supprimer les espaces
    size_t start = normalized.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    
    size_t end = normalized.find_last_not_of(" \t\n\r");
    return normalized.substr(start, end - start + 1);
}

// EN: QueryUtils implementation
// FR: Implémentation de QueryUtils

namespace QueryUtils {

QueryValue stringToQueryValue(const std::string& str) {
    // EN: Try to parse as boolean first
    // FR: Essayer d'abord d'analyser comme booléen
    if (str == "true" || str == "TRUE") return true;
    if (str == "false" || str == "FALSE") return false;
    
    // EN: Try to parse as NULL
    // FR: Essayer d'analyser comme NULL
    if (str == "NULL" || str == "null") return nullptr;
    
    // EN: Try to parse as number
    // FR: Essayer d'analyser comme nombre
    try {
        if (str.find('.') != std::string::npos) {
            return std::stod(str);
        } else {
            return static_cast<int64_t>(std::stoll(str));
        }
    } catch (...) {
        // EN: Fall back to string / FR: Se rabattre sur chaîne
        return str;
    }
}

std::string queryValueToString(const QueryValue& value) {
    return std::visit([](const auto& v) -> std::string {
        if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, int64_t>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, double>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, bool>) {
            return v ? "true" : "false";
        } else { // nullptr_t
            return "NULL";
        }
    }, value);
}

bool compareValues(const QueryValue& a, const QueryValue& b, SqlOperator op) {
    // EN: Handle NULL values
    // FR: Gérer les valeurs NULL
    bool a_is_null = std::holds_alternative<std::nullptr_t>(a);
    bool b_is_null = std::holds_alternative<std::nullptr_t>(b);
    
    if (op == SqlOperator::IS_NULL) return a_is_null;
    if (op == SqlOperator::IS_NOT_NULL) return !a_is_null;
    
    if (a_is_null || b_is_null) {
        return false; // EN: NULL comparisons are false except IS NULL/IS NOT NULL / FR: Les comparaisons NULL sont fausses sauf IS NULL/IS NOT NULL
    }
    
    // EN: Convert both values to strings for comparison
    // FR: Convertir les deux valeurs en chaînes pour comparaison
    std::string str_a = queryValueToString(a);
    std::string str_b = queryValueToString(b);
    
    switch (op) {
        case SqlOperator::EQUALS:
            return str_a == str_b;
        case SqlOperator::NOT_EQUALS:
            return str_a != str_b;
        case SqlOperator::LESS_THAN:
            if (isNumeric(str_a) && isNumeric(str_b)) {
                return std::stod(str_a) < std::stod(str_b);
            }
            return str_a < str_b;
        case SqlOperator::LESS_EQUAL:
            if (isNumeric(str_a) && isNumeric(str_b)) {
                return std::stod(str_a) <= std::stod(str_b);
            }
            return str_a <= str_b;
        case SqlOperator::GREATER_THAN:
            if (isNumeric(str_a) && isNumeric(str_b)) {
                return std::stod(str_a) > std::stod(str_b);
            }
            return str_a > str_b;
        case SqlOperator::GREATER_EQUAL:
            if (isNumeric(str_a) && isNumeric(str_b)) {
                return std::stod(str_a) >= std::stod(str_b);
            }
            return str_a >= str_b;
        case SqlOperator::LIKE: {
            // EN: Simple LIKE implementation with % and _ wildcards
            // FR: Implémentation simple de LIKE avec caractères génériques % et _
            std::regex pattern(str_b);
            std::string regex_pattern = str_b;
            // EN: Replace SQL wildcards with regex equivalents
            // FR: Remplacer les caractères génériques SQL par leurs équivalents regex
            size_t pos = 0;
            while ((pos = regex_pattern.find('%', pos)) != std::string::npos) {
                regex_pattern.replace(pos, 1, ".*");
                pos += 2;
            }
            pos = 0;
            while ((pos = regex_pattern.find('_', pos)) != std::string::npos) {
                regex_pattern.replace(pos, 1, ".");
                pos += 1;
            }
            try {
                std::regex regex_obj(regex_pattern);
                return std::regex_match(str_a, regex_obj);
            } catch (...) {
                return str_a.find(str_b) != std::string::npos;
            }
        }
        case SqlOperator::NOT_LIKE:
            return !compareValues(a, b, SqlOperator::LIKE);
        default:
            return false;
    }
}

std::string escapeString(const std::string& str) {
    std::string escaped;
    escaped.reserve(str.length() * 2);
    
    for (char c : str) {
        switch (c) {
            case '\'': escaped += "\\'"; break;
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\t': escaped += "\\t"; break;
            case '\r': escaped += "\\r"; break;
            default: escaped += c; break;
        }
    }
    
    return escaped;
}

std::string formatSql(const std::string& sql) {
    // EN: Basic SQL formatting - add newlines after major keywords
    // FR: Formatage SQL de base - ajouter des retours à la ligne après les mots-clés principaux
    std::string formatted = sql;
    
    std::vector<std::string> keywords = {"SELECT", "FROM", "WHERE", "GROUP BY", "HAVING", "ORDER BY", "LIMIT"};
    
    for (const auto& keyword : keywords) {
        size_t pos = 0;
        while ((pos = formatted.find(keyword, pos)) != std::string::npos) {
            // EN: Add newline before keyword if not at start
            // FR: Ajouter retour à la ligne avant mot-clé si pas au début
            if (pos > 0 && formatted[pos - 1] != '\n') {
                formatted.insert(pos, "\n");
                pos += keyword.length() + 1;
            } else {
                pos += keyword.length();
            }
        }
    }
    
    return formatted;
}

bool isNumeric(const std::string& str) {
    if (str.empty()) return false;
    
    size_t pos = 0;
    if (str[0] == '-') pos = 1;
    
    bool has_dot = false;
    for (size_t i = pos; i < str.length(); ++i) {
        if (str[i] == '.' && !has_dot) {
            has_dot = true;
        } else if (!std::isdigit(str[i])) {
            return false;
        }
    }
    
    return pos < str.length(); // EN: Ensure we have at least one digit / FR: S'assurer d'avoir au moins un chiffre
}

std::string formatDuration(std::chrono::milliseconds duration) {
    auto ms = duration.count();
    if (ms < 1000) {
        return std::to_string(ms) + "ms";
    } else if (ms < 60000) {
        return std::to_string(ms / 1000.0) + "s";
    } else {
        return std::to_string(ms / 60000.0) + "min";
    }
}

std::string formatMemorySize(size_t bytes) {
    if (bytes < 1024) {
        return std::to_string(bytes) + "B";
    } else if (bytes < 1024 * 1024) {
        return std::to_string(bytes / 1024) + "KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        return std::to_string(bytes / (1024 * 1024)) + "MB";
    } else {
        return std::to_string(bytes / (1024 * 1024 * 1024)) + "GB";
    }
}

std::string formatNumber(size_t number) {
    std::string str = std::to_string(number);
    std::string formatted;
    
    int count = 0;
    for (auto it = str.rbegin(); it != str.rend(); ++it) {
        if (count > 0 && count % 3 == 0) {
            formatted = ',' + formatted;
        }
        formatted = *it + formatted;
        count++;
    }
    
    return formatted;
}

QueryError loadCsvFile(const std::string& filename, std::vector<std::string>& headers,
                      std::vector<std::vector<std::string>>& data) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return QueryError::FILE_NOT_FOUND;
    }
    
    std::string line;
    bool first_line = true;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::vector<std::string> row;
        std::string field;
        bool in_quotes = false;
        
        for (size_t i = 0; i < line.length(); ++i) {
            char c = line[i];
            
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (c == ',' && !in_quotes) {
                row.push_back(field);
                field.clear();
            } else {
                field += c;
            }
        }
        
        // EN: Add the last field / FR: Ajouter le dernier champ
        row.push_back(field);
        
        if (first_line) {
            headers = row;
            first_line = false;
        } else {
            data.push_back(row);
        }
    }
    
    return QueryError::SUCCESS;
}

QueryError saveCsvFile(const std::string& filename, const QueryResult& result) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return QueryError::IO_ERROR;
    }
    
    file << result.toCSV();
    return QueryError::SUCCESS;
}

bool isValidSql(const std::string& sql) {
    QueryParser parser;
    SqlQuery query;
    return parser.parse(sql, query) == QueryError::SUCCESS;
}

std::vector<std::string> extractTableNames(const std::string& sql) {
    // EN: Simple implementation - look for FROM keyword
    // FR: Implémentation simple - chercher le mot-clé FROM
    std::vector<std::string> tables;
    
    size_t pos = sql.find("FROM");
    if (pos == std::string::npos) {
        pos = sql.find("from");
    }
    
    if (pos != std::string::npos) {
        pos += 4; // EN: Skip "FROM" / FR: Ignorer "FROM"
        while (pos < sql.length() && std::isspace(sql[pos])) pos++;
        
        std::string table;
        while (pos < sql.length() && !std::isspace(sql[pos]) && sql[pos] != ';') {
            table += sql[pos];
            pos++;
        }
        
        if (!table.empty()) {
            tables.push_back(table);
        }
    }
    
    return tables;
}

std::vector<std::string> extractColumnNames(const std::string& sql) {
    // EN: Simple implementation - extract column names from SELECT
    // FR: Implémentation simple - extraire les noms de colonnes de SELECT
    std::vector<std::string> columns;
    
    size_t select_pos = sql.find("SELECT");
    if (select_pos == std::string::npos) {
        select_pos = sql.find("select");
    }
    
    if (select_pos != std::string::npos) {
        size_t from_pos = sql.find("FROM", select_pos);
        if (from_pos == std::string::npos) {
            from_pos = sql.find("from", select_pos);
        }
        
        if (from_pos != std::string::npos) {
            std::string columns_part = sql.substr(select_pos + 6, from_pos - select_pos - 6);
            
            // EN: Simple parsing - split by comma
            // FR: Analyse simple - diviser par virgule
            std::istringstream ss(columns_part);
            std::string column;
            while (std::getline(ss, column, ',')) {
                // EN: Trim whitespace / FR: Supprimer les espaces
                column.erase(0, column.find_first_not_of(" \t"));
                column.erase(column.find_last_not_of(" \t") + 1);
                if (!column.empty()) {
                    columns.push_back(column);
                }
            }
        }
    }
    
    return columns;
}

} // namespace QueryUtils

// EN: IndexManager implementation
// FR: Implémentation d'IndexManager

QueryError IndexManager::createIndex(const std::string& table, const IndexConfig& config) {
    std::lock_guard<std::mutex> lock(index_mutex_);
    
    // EN: Check if table exists
    // FR: Vérifier si la table existe
    if (table_data_.find(table) == table_data_.end()) {
        return QueryError::FILE_NOT_FOUND;
    }
    
    // EN: Store index configuration
    // FR: Stocker la configuration d'index
    index_configs_[table][config.column] = config;
    
    // EN: Build the appropriate index type
    // FR: Construire le type d'index approprié
    switch (config.type) {
        case IndexType::HASH:
            return buildHashIndex(table, config.column);
        case IndexType::BTREE:
            return buildBTreeIndex(table, config.column);
        case IndexType::FULL_TEXT:
            return buildFullTextIndex(table, config.column);
        default:
            return QueryError::INDEX_ERROR;
    }
}

QueryError IndexManager::buildHashIndex(const std::string& table, const std::string& column) {
    const auto& headers = table_headers_[table];
    const auto& data = table_data_[table];
    
    // EN: Find column index
    // FR: Trouver l'index de colonne
    int col_idx = -1;
    for (size_t i = 0; i < headers.size(); ++i) {
        if (headers[i] == column) {
            col_idx = static_cast<int>(i);
            break;
        }
    }
    
    if (col_idx < 0) {
        return QueryError::COLUMN_NOT_FOUND;
    }
    
    // EN: Create hash index
    // FR: Créer l'index hash
    auto index = std::make_unique<HashIndex>();
    
    for (size_t row_idx = 0; row_idx < data.size(); ++row_idx) {
        const auto& row = data[row_idx];
        if (static_cast<size_t>(col_idx) < row.size()) {
            const std::string& value = row[col_idx];
            index->value_to_rows[value].push_back(row_idx);
        }
    }
    
    // EN: Calculate memory usage
    // FR: Calculer l'utilisation mémoire
    index->memory_usage = calculateIndexMemory(table, column);
    
    hash_indexes_[table][column] = std::move(index);
    return QueryError::SUCCESS;
}

QueryError IndexManager::buildBTreeIndex(const std::string& table, const std::string& column) {
    const auto& headers = table_headers_[table];
    const auto& data = table_data_[table];
    
    // EN: Find column index
    // FR: Trouver l'index de colonne
    int col_idx = -1;
    for (size_t i = 0; i < headers.size(); ++i) {
        if (headers[i] == column) {
            col_idx = static_cast<int>(i);
            break;
        }
    }
    
    if (col_idx < 0) {
        return QueryError::COLUMN_NOT_FOUND;
    }
    
    // EN: Create B-tree index
    // FR: Créer l'index B-tree
    auto index = std::make_unique<BTreeIndex>();
    
    for (size_t row_idx = 0; row_idx < data.size(); ++row_idx) {
        const auto& row = data[row_idx];
        if (static_cast<size_t>(col_idx) < row.size()) {
            const std::string& value = row[col_idx];
            index->value_to_rows[value].push_back(row_idx);
        }
    }
    
    // EN: Calculate memory usage
    // FR: Calculer l'utilisation mémoire
    index->memory_usage = calculateIndexMemory(table, column);
    
    btree_indexes_[table][column] = std::move(index);
    return QueryError::SUCCESS;
}

QueryError IndexManager::buildFullTextIndex(const std::string& table, const std::string& column) {
    const auto& headers = table_headers_[table];
    const auto& data = table_data_[table];
    const auto& config = index_configs_[table][column];
    
    // EN: Find column index
    // FR: Trouver l'index de colonne
    int col_idx = -1;
    for (size_t i = 0; i < headers.size(); ++i) {
        if (headers[i] == column) {
            col_idx = static_cast<int>(i);
            break;
        }
    }
    
    if (col_idx < 0) {
        return QueryError::COLUMN_NOT_FOUND;
    }
    
    // EN: Create full-text index
    // FR: Créer l'index texte intégral
    auto index = std::make_unique<FullTextIndex>();
    index->tokenizer = config.tokenizer;
    index->case_sensitive = config.case_sensitive;
    
    for (size_t row_idx = 0; row_idx < data.size(); ++row_idx) {
        const auto& row = data[row_idx];
        if (static_cast<size_t>(col_idx) < row.size()) {
            const std::string& text = row[col_idx];
            auto tokens = tokenizeText(text, config.tokenizer, config.case_sensitive);
            
            for (const auto& token : tokens) {
                index->token_to_rows[token].push_back(row_idx);
            }
        }
    }
    
    // EN: Calculate memory usage
    // FR: Calculer l'utilisation mémoire
    index->memory_usage = calculateIndexMemory(table, column);
    
    fulltext_indexes_[table][column] = std::move(index);
    return QueryError::SUCCESS;
}

std::vector<std::string> IndexManager::tokenizeText(const std::string& text, const std::string& tokenizer, bool case_sensitive) const {
    std::vector<std::string> tokens;
    
    if (tokenizer == "standard") {
        // EN: Standard tokenizer - split on whitespace and punctuation
        // FR: Tokeniseur standard - diviser sur espaces et ponctuation
        std::string current_token;
        
        for (char c : text) {
            if (std::isalnum(c) || c == '_') {
                current_token += case_sensitive ? c : std::tolower(c);
            } else {
                if (!current_token.empty()) {
                    tokens.push_back(current_token);
                    current_token.clear();
                }
            }
        }
        
        if (!current_token.empty()) {
            tokens.push_back(current_token);
        }
    } else if (tokenizer == "whitespace") {
        // EN: Whitespace tokenizer - split only on whitespace
        // FR: Tokeniseur espace - diviser seulement sur les espaces
        std::istringstream iss(text);
        std::string token;
        
        while (iss >> token) {
            if (!case_sensitive) {
                std::transform(token.begin(), token.end(), token.begin(), ::tolower);
            }
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

QueryError IndexManager::loadTableData(const std::string& table, const std::vector<std::string>& headers,
                                      const std::vector<std::vector<std::string>>& data) {
    std::lock_guard<std::mutex> lock(index_mutex_);
    
    table_headers_[table] = headers;
    table_data_[table] = data;
    
    return QueryError::SUCCESS;
}

bool IndexManager::hasIndex(const std::string& table, const std::string& column) const {
    std::lock_guard<std::mutex> lock(index_mutex_);
    
    auto table_it = index_configs_.find(table);
    if (table_it == index_configs_.end()) return false;
    
    return table_it->second.find(column) != table_it->second.end();
}

std::vector<size_t> IndexManager::findRowsByIndex(const std::string& table, const std::string& column, 
                                                  const QueryValue& value) const {
    std::lock_guard<std::mutex> lock(index_mutex_);
    
    std::string str_value = QueryUtils::queryValueToString(value);
    
    // EN: Try hash index first
    // FR: Essayer d'abord l'index hash
    auto hash_table_it = hash_indexes_.find(table);
    if (hash_table_it != hash_indexes_.end()) {
        auto hash_col_it = hash_table_it->second.find(column);
        if (hash_col_it != hash_table_it->second.end()) {
            const auto& index = hash_col_it->second;
            auto value_it = index->value_to_rows.find(str_value);
            if (value_it != index->value_to_rows.end()) {
                return value_it->second;
            }
        }
    }
    
    // EN: Try B-tree index
    // FR: Essayer l'index B-tree
    auto btree_table_it = btree_indexes_.find(table);
    if (btree_table_it != btree_indexes_.end()) {
        auto btree_col_it = btree_table_it->second.find(column);
        if (btree_col_it != btree_table_it->second.end()) {
            const auto& index = btree_col_it->second;
            auto value_it = index->value_to_rows.find(str_value);
            if (value_it != index->value_to_rows.end()) {
                return value_it->second;
            }
        }
    }
    
    return {}; // EN: No matching rows found / FR: Aucune ligne correspondante trouvée
}

size_t IndexManager::calculateIndexMemory(const std::string& table_name, const std::string& /* column */) const {
    // EN: Rough estimate of index memory usage
    // FR: Estimation approximative de l'utilisation mémoire de l'index
    size_t memory = 0;
    
    const auto& data = table_data_.at(table_name);
    for (const auto& row : data) {
        memory += sizeof(size_t); // EN: Row index / FR: Index de ligne
        for (const auto& field : row) {
            memory += field.size();
        }
    }
    
    return memory;
}

// EN: QueryEngine implementation
// FR: Implémentation de QueryEngine

QueryEngine::QueryEngine(const Config& config) : config_(config) {
    // EN: Initialize statistics
    // FR: Initialiser les statistiques
    statistics_ = EngineStatistics{};
}

QueryError QueryEngine::loadTable(const std::string& table_name, const std::string& csv_file) {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> data;
    
    QueryError error = QueryUtils::loadCsvFile(csv_file, headers, data);
    if (error != QueryError::SUCCESS) {
        return error;
    }
    
    return registerTable(table_name, headers, data);
}

QueryError QueryEngine::registerTable(const std::string& table_name, const std::vector<std::string>& headers,
                                     const std::vector<std::vector<std::string>>& data) {
    std::lock_guard<std::mutex> lock(table_mutex_);
    
    table_headers_[table_name] = headers;
    table_data_[table_name] = data;
    
    // EN: Load data into index manager
    // FR: Charger les données dans le gestionnaire d'index
    index_manager_.loadTableData(table_name, headers, data);
    
    // EN: Auto-create indexes if enabled
    // FR: Créer automatiquement des index si activé
    if (config_.auto_index && !headers.empty()) {
        IndexConfig index_config;
        index_config.column = headers[0]; // EN: Index first column by default / FR: Indexer la première colonne par défaut
        index_config.type = IndexType::HASH;
        createIndex(table_name, index_config);
    }
    
    return QueryError::SUCCESS;
}

std::vector<std::string> QueryEngine::getTableNames() const {
    std::lock_guard<std::mutex> lock(table_mutex_);
    
    std::vector<std::string> names;
    for (const auto& pair : table_headers_) {
        names.push_back(pair.first);
    }
    
    return names;
}

QueryResult QueryEngine::execute(const std::string& sql) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // EN: Check query cache first
    // FR: Vérifier d'abord le cache de requêtes
    if (config_.enable_query_cache) {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        auto it = query_cache_.find(sql);
        if (it != query_cache_.end()) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            statistics_.cache_hits++;
            statistics_.total_queries_executed++;
            statistics_.total_execution_time += duration;
            
            QueryResult result = it->second;
            QueryStatistics stats;
            stats.execution_time = duration;
            stats.query_cached = true;
            result.setStatistics(stats);
            
            return result;
        }
    }
    
    // EN: Parse SQL query
    // FR: Analyser la requête SQL
    SqlQuery query;
    QueryError parse_error = parser_.parse(sql, query);
    
    if (parse_error != QueryError::SUCCESS) {
        QueryResult error_result;
        QueryStatistics stats;
        stats.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start);
        error_result.setStatistics(stats);
        return error_result;
    }
    
    // EN: Execute the parsed query
    // FR: Exécuter la requête analysée
    QueryResult result = executeInternal(query);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // EN: Update statistics
    // FR: Mettre à jour les statistiques
    updateStatistics(result, duration);
    
    // EN: Cache the result if caching is enabled
    // FR: Mettre en cache le résultat si la mise en cache est activée
    if (config_.enable_query_cache && result.getRowCount() > 0) {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        query_cache_[sql] = result;
        cache_timestamps_[sql] = std::chrono::system_clock::now();
        
        // EN: Clean up cache if too large
        // FR: Nettoyer le cache s'il est trop gros
        if (query_cache_.size() > config_.query_cache_size) {
            cleanupCache();
        }
    }
    
    return result;
}

QueryResult QueryEngine::executeInternal(const SqlQuery& query) {
    std::lock_guard<std::mutex> lock(table_mutex_);
    
    // EN: Check if table exists
    // FR: Vérifier si la table existe
    auto table_it = table_data_.find(query.table);
    if (table_it == table_data_.end()) {
        return QueryResult{}; // EN: Empty result / FR: Résultat vide
    }
    
    const auto& headers = table_headers_[query.table];
    const auto& data = table_data_[query.table];
    
    // EN: Build result headers
    // FR: Construire les en-têtes de résultat
    std::vector<std::string> result_headers;
    if (query.columns.size() == 1 && query.columns[0].column == "*") {
        result_headers = headers;
    } else {
        for (const auto& col : query.columns) {
            result_headers.push_back(col.alias.empty() ? col.column : col.alias);
        }
    }
    
    QueryResult result(result_headers);
    
    // EN: Apply WHERE conditions
    // FR: Appliquer les conditions WHERE
    std::vector<size_t> matching_rows;
    if (query.where.empty()) {
        // EN: No WHERE clause - include all rows
        // FR: Pas de clause WHERE - inclure toutes les lignes
        matching_rows.resize(data.size());
        std::iota(matching_rows.begin(), matching_rows.end(), 0);
    } else {
        matching_rows = optimizedRowSelection(query.table, query.where);
        
        // EN: If no index optimization was possible, scan all rows
        // FR: Si aucune optimisation d'index n'était possible, parcourir toutes les lignes
        if (matching_rows.empty()) {
            for (size_t i = 0; i < data.size(); ++i) {
                if (evaluateWhere(data[i], headers, query.where)) {
                    matching_rows.push_back(i);
                }
            }
        }
    }
    
    // EN: Project columns and add rows to result
    // FR: Projeter les colonnes et ajouter les lignes au résultat
    for (size_t row_idx : matching_rows) {
        if (row_idx >= data.size()) continue;
        
        const auto& row = data[row_idx];
        std::vector<std::string> result_row;
        
        if (query.columns.size() == 1 && query.columns[0].column == "*") {
            result_row = row;
        } else {
            for (const auto& col : query.columns) {
                // EN: Find column index
                // FR: Trouver l'index de colonne
                int col_idx = -1;
                for (size_t i = 0; i < headers.size(); ++i) {
                    if (headers[i] == col.column) {
                        col_idx = static_cast<int>(i);
                        break;
                    }
                }
                
                if (col_idx >= 0 && static_cast<size_t>(col_idx) < row.size()) {
                    result_row.push_back(row[col_idx]);
                } else {
                    result_row.push_back(""); // EN: Missing column / FR: Colonne manquante
                }
            }
        }
        
        result.addRow(std::move(result_row));
    }
    
    // EN: Apply aggregation if needed
    // FR: Appliquer l'agrégation si nécessaire
    bool has_aggregates = false;
    for (const auto& col : query.columns) {
        if (col.aggregate != AggregateFunction::NONE) {
            has_aggregates = true;
            break;
        }
    }
    
    if (has_aggregates || !query.group_by.empty()) {
        result = applyAggregation(result, query);
    }
    
    // EN: Apply DISTINCT if needed
    // FR: Appliquer DISTINCT si nécessaire
    if (query.distinct_query) {
        std::vector<std::vector<std::string>> unique_rows;
        std::set<std::vector<std::string>> seen;
        
        for (const auto& row : result.getRows()) {
            if (seen.find(row) == seen.end()) {
                seen.insert(row);
                unique_rows.push_back(row);
            }
        }
        
        result.clear();
        for (const auto& row : unique_rows) {
            result.addRow(row);
        }
    }
    
    // EN: Apply ORDER BY
    // FR: Appliquer ORDER BY
    if (!query.order_by.empty()) {
        applySorting(result, query.order_by);
    }
    
    // EN: Apply LIMIT and OFFSET
    // FR: Appliquer LIMIT et OFFSET
    if (query.limit > 0) {
        result = applyLimitOffset(result, query.limit, query.offset);
    }
    
    return result;
}

bool QueryEngine::evaluateWhere(const std::vector<std::string>& row, const std::vector<std::string>& headers,
                                const std::vector<WhereCondition>& conditions) const {
    if (conditions.empty()) return true;
    
    bool result = true;
    bool first = true;
    
    for (const auto& condition : conditions) {
        // EN: Find column index
        // FR: Trouver l'index de colonne
        int col_idx = -1;
        for (size_t i = 0; i < headers.size(); ++i) {
            if (headers[i] == condition.column) {
                col_idx = static_cast<int>(i);
                break;
            }
        }
        
        if (col_idx < 0 || static_cast<size_t>(col_idx) >= row.size()) {
            continue; // EN: Column not found / FR: Colonne non trouvée
        }
        
        const std::string& value = row[col_idx];
        bool condition_result = evaluateCondition(value, condition);
        
        if (first) {
            result = condition_result;
            first = false;
        } else {
            switch (condition.logical_op) {
                case LogicalOperator::AND:
                    result = result && condition_result;
                    break;
                case LogicalOperator::OR:
                    result = result || condition_result;
                    break;
                case LogicalOperator::NOT:
                    result = result && !condition_result;
                    break;
            }
        }
    }
    
    return result;
}

bool QueryEngine::evaluateCondition(const std::string& value, const WhereCondition& condition) const {
    QueryValue query_value = QueryUtils::stringToQueryValue(value);
    
    switch (condition.operator_) {
        case SqlOperator::IN: {
            for (const auto& in_value : condition.in_values) {
                if (QueryUtils::compareValues(query_value, in_value, SqlOperator::EQUALS)) {
                    return true;
                }
            }
            return false;
        }
        case SqlOperator::NOT_IN: {
            for (const auto& in_value : condition.in_values) {
                if (QueryUtils::compareValues(query_value, in_value, SqlOperator::EQUALS)) {
                    return false;
                }
            }
            return true;
        }
        case SqlOperator::BETWEEN: {
            return QueryUtils::compareValues(query_value, condition.range_start, SqlOperator::GREATER_EQUAL) &&
                   QueryUtils::compareValues(query_value, condition.range_end, SqlOperator::LESS_EQUAL);
        }
        default:
            return QueryUtils::compareValues(query_value, condition.value, condition.operator_);
    }
}

QueryResult QueryEngine::applyAggregation(const QueryResult& intermediate_result, const SqlQuery& query) {
    // EN: Simple aggregation implementation
    // FR: Implémentation d'agrégation simple
    std::vector<std::string> agg_headers;
    std::vector<std::string> agg_row;
    
    for (const auto& col : query.columns) {
        agg_headers.push_back(col.alias.empty() ? col.column : col.alias);
        
        if (col.aggregate != AggregateFunction::NONE) {
            auto column_data = intermediate_result.getColumn(col.column);
            std::string agg_result = calculateAggregate(column_data, col.aggregate);
            agg_row.push_back(agg_result);
        } else {
            // EN: Non-aggregate column - take first value
            // FR: Colonne non-agrégée - prendre la première valeur
            auto column_data = intermediate_result.getColumn(col.column);
            agg_row.push_back(column_data.empty() ? "" : column_data[0]);
        }
    }
    
    QueryResult result(agg_headers);
    result.addRow(agg_row);
    
    return result;
}

std::string QueryEngine::calculateAggregate(const std::vector<std::string>& values, AggregateFunction func) const {
    if (values.empty()) return "";
    
    switch (func) {
        case AggregateFunction::COUNT:
            return std::to_string(values.size());
            
        case AggregateFunction::SUM: {
            double sum = 0.0;
            for (const auto& value : values) {
                if (QueryUtils::isNumeric(value)) {
                    sum += std::stod(value);
                }
            }
            return std::to_string(sum);
        }
        
        case AggregateFunction::AVG: {
            double sum = 0.0;
            size_t count = 0;
            for (const auto& value : values) {
                if (QueryUtils::isNumeric(value)) {
                    sum += std::stod(value);
                    count++;
                }
            }
            return count > 0 ? std::to_string(sum / count) : "0";
        }
        
        case AggregateFunction::MIN: {
            std::string min_val = values[0];
            for (const auto& value : values) {
                if (QueryUtils::isNumeric(value) && QueryUtils::isNumeric(min_val)) {
                    if (std::stod(value) < std::stod(min_val)) min_val = value;
                } else {
                    if (value < min_val) min_val = value;
                }
            }
            return min_val;
        }
        
        case AggregateFunction::MAX: {
            std::string max_val = values[0];
            for (const auto& value : values) {
                if (QueryUtils::isNumeric(value) && QueryUtils::isNumeric(max_val)) {
                    if (std::stod(value) > std::stod(max_val)) max_val = value;
                } else {
                    if (value > max_val) max_val = value;
                }
            }
            return max_val;
        }
        
        case AggregateFunction::DISTINCT: {
            std::set<std::string> unique_values(values.begin(), values.end());
            return std::to_string(unique_values.size());
        }
        
        case AggregateFunction::GROUP_CONCAT: {
            std::ostringstream oss;
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0) oss << ",";
                oss << values[i];
            }
            return oss.str();
        }
        
        default:
            return "";
    }
}

void QueryEngine::applySorting(QueryResult& result, const std::vector<OrderByColumn>& order_by) const {
    result.sortBy(order_by);
}

QueryResult QueryEngine::applyLimitOffset(const QueryResult& result, size_t limit, size_t offset) const {
    return result.slice(offset, limit);
}

std::vector<size_t> QueryEngine::optimizedRowSelection(const std::string& table, const std::vector<WhereCondition>& conditions) {
    std::vector<size_t> result;
    
    // EN: Try to use indexes for optimization
    // FR: Essayer d'utiliser les index pour l'optimisation
    for (const auto& condition : conditions) {
        if (index_manager_.hasIndex(table, condition.column)) {
            if (condition.operator_ == SqlOperator::EQUALS) {
                result = index_manager_.findRowsByIndex(table, condition.column, condition.value);
                break; // EN: Use first indexed condition / FR: Utiliser la première condition indexée
            }
        }
    }
    
    return result;
}

void QueryEngine::updateStatistics(const QueryResult& result, std::chrono::milliseconds execution_time) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    statistics_.total_queries_executed++;
    statistics_.total_execution_time += execution_time;
    statistics_.total_rows_processed += result.getRowCount();
    
    if (!result.getStatistics().query_cached) {
        statistics_.cache_misses++;
    }
}

void QueryEngine::cleanupCache() {
    // EN: Remove oldest entries from cache
    // FR: Supprimer les entrées les plus anciennes du cache
    auto oldest_time = std::chrono::system_clock::now();
    std::string oldest_key;
    
    for (const auto& pair : cache_timestamps_) {
        if (pair.second < oldest_time) {
            oldest_time = pair.second;
            oldest_key = pair.first;
        }
    }
    
    if (!oldest_key.empty()) {
        query_cache_.erase(oldest_key);
        cache_timestamps_.erase(oldest_key);
    }
}

QueryEngine::EngineStatistics QueryEngine::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return statistics_;
}

std::string QueryEngine::explainQuery(const std::string& sql) {
    SqlQuery query;
    QueryError parse_error = parser_.parse(sql, query);
    
    if (parse_error != QueryError::SUCCESS) {
        return "Parse Error: " + parser_.getLastError();
    }
    
    return explainQuery(query);
}

std::string QueryEngine::explainQuery(const SqlQuery& query) {
    std::ostringstream oss;
    
    oss << "Query Execution Plan:\n";
    oss << "====================\n";
    oss << "Table: " << query.table << "\n";
    oss << "Columns: ";
    for (size_t i = 0; i < query.columns.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << query.columns[i].column;
    }
    oss << "\n";
    
    if (!query.where.empty()) {
        oss << "WHERE conditions: " << query.where.size() << "\n";
        for (const auto& condition : query.where) {
            oss << "  - " << condition.column << " (";
            if (index_manager_.hasIndex(query.table, condition.column)) {
                oss << "INDEXED";
            } else {
                oss << "SCAN";
            }
            oss << ")\n";
        }
    }
    
    if (!query.order_by.empty()) {
        oss << "ORDER BY: " << query.order_by.size() << " column(s)\n";
    }
    
    if (query.limit > 0) {
        oss << "LIMIT: " << query.limit;
        if (query.offset > 0) {
            oss << " OFFSET: " << query.offset;
        }
        oss << "\n";
    }
    
    return oss.str();
}

QueryError QueryEngine::createIndex(const std::string& table, const IndexConfig& config) {
    return index_manager_.createIndex(table, config);
}

} // namespace CSV
} // namespace BBP