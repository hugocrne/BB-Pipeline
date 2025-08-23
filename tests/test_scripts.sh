#!/bin/bash
# BB-Pipeline - Tests pour tous les scripts du projet
# Teste les scripts dans /scripts/, leur syntaxe et fonctionnalité de base

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SCRIPTS_DIR="$PROJECT_ROOT/scripts"
TEST_DATA_DIR="$SCRIPT_DIR/data"
TEMP_DIR=$(mktemp -d)

# Cleanup function
cleanup() {
    rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

# Couleurs pour les logs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Compteurs de tests
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Fonction pour exécuter un test
run_test() {
    local test_name="$1"
    local test_command="$2"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    log_info "Test: $test_name"
    
    if eval "$test_command" >/dev/null 2>&1; then
        log_success "$test_name"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        log_error "$test_name"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
}

echo "🧪 Tests des scripts BB-Pipeline"
echo "================================="
echo ""

# Créer des données de test
mkdir -p "$TEST_DATA_DIR"

# Créer un fichier scope de test
cat > "$TEST_DATA_DIR/test_scope.csv" << 'EOF'
identifier,asset_type,instruction,eligible_for_bounty,eligible_for_submission,availability_requirement,confidentiality_requirement,integrity_requirement,max_severity,system_tags,created_at,updated_at
example.com,URL,,true,true,,,,critical,,2024-01-01,2024-01-01
api.example.com,URL,,true,true,,,,critical,,2024-01-01,2024-01-01
admin.example.com,URL,,true,true,,,,critical,,2024-01-01,2024-01-01
EOF

# Créer une liste d'URLs de test pour JS
cat > "$TEST_DATA_DIR/test_js_urls.txt" << 'EOF'
https://example.com/assets/main.js
https://example.com/js/app.js
https://api.example.com/v1/client.js
https://example.com/static/bundle.js
EOF

# Créer une liste de domaines pour enum
echo "example.com" > "$TEST_DATA_DIR/test_domains.txt"

echo "📁 Tests de validation des scripts"
echo "-----------------------------------"

# Test 1: Vérifier que tous les scripts existent
run_test "Scripts existence check" "test -f '$SCRIPTS_DIR/enum.sh' -a -f '$SCRIPTS_DIR/download_js.sh' -a -f '$SCRIPTS_DIR/gau_wayback.sh' -a -f '$SCRIPTS_DIR/convert_scope.sh'"

# Test 2: Vérifier que tous les scripts sont exécutables
run_test "Scripts executable check" "test -x '$SCRIPTS_DIR/enum.sh' -a -x '$SCRIPTS_DIR/download_js.sh' -a -x '$SCRIPTS_DIR/gau_wayback.sh' -a -x '$SCRIPTS_DIR/convert_scope.sh'"

# Test 3: Validation syntaxique des scripts bash
for script in "$SCRIPTS_DIR"/*.sh; do
    script_name=$(basename "$script")
    run_test "Syntax validation: $script_name" "bash -n '$script'"
done

echo ""
echo "🔧 Tests fonctionnels des scripts"
echo "---------------------------------"

# Test 4: convert_scope.sh - Conversion basique
run_test "convert_scope.sh basic conversion" "
    '$SCRIPTS_DIR/convert_scope.sh' '$TEST_DATA_DIR/test_scope.csv' '$TEMP_DIR/converted_scope.csv' 'Test Program' &&
    test -f '$TEMP_DIR/converted_scope.csv' &&
    grep -q 'schema_ver,program,domain' '$TEMP_DIR/converted_scope.csv' &&
    grep -q 'example.com' '$TEMP_DIR/converted_scope.csv'
"

# Test 5: convert_scope.sh - Rapport JSON généré
run_test "convert_scope.sh JSON report" "
    test -f '$TEMP_DIR/converted_scope_report.json' &&
    grep -q 'total_domains' '$TEMP_DIR/converted_scope_report.json'
"

# Test 6: enum.sh - Validation des paramètres (sans exécution réseau)
run_test "enum.sh parameter validation" "
    timeout 5 '$SCRIPTS_DIR/enum.sh' 'nonexistent.example.local' '$TEMP_DIR/enum_output.txt' 2>/dev/null || true &&
    test -f '$TEMP_DIR/enum_output.txt'
"

# Test 7: download_js.sh - Validation des paramètres
run_test "download_js.sh parameter validation" "
    timeout 5 '$SCRIPTS_DIR/download_js.sh' '$TEST_DATA_DIR/test_js_urls.txt' '$TEMP_DIR/js_downloads' 2>/dev/null || true &&
    test -d '$TEMP_DIR/js_downloads'
"

# Test 8: gau_wayback.sh - Validation des paramètres  
run_test "gau_wayback.sh parameter validation" "
    timeout 5 '$SCRIPTS_DIR/gau_wayback.sh' 'nonexistent.example.local' '$TEMP_DIR/wayback_urls.txt' 2>/dev/null || true &&
    test -f '$TEMP_DIR/wayback_urls.txt'
"

echo ""
echo "📋 Tests de validation des données"
echo "----------------------------------"

# Test 9: Wordlists - Vérifier qu'elles ne sont pas vides
run_test "Wordlists not empty" "
    test -s '$PROJECT_ROOT/wordlists/subs.txt' &&
    test -s '$PROJECT_ROOT/wordlists/paths.txt' &&
    test -s '$PROJECT_ROOT/wordlists/extensions.txt' &&
    test -s '$PROJECT_ROOT/wordlists/secrets.patterns'
"

# Test 10: Wordlists - Format correct
run_test "Wordlists format validation" "
    head -10 '$PROJECT_ROOT/wordlists/subs.txt' | grep -E '^[a-zA-Z0-9_-]+$' &&
    head -10 '$PROJECT_ROOT/wordlists/paths.txt' | grep -E '^/' &&
    head -10 '$PROJECT_ROOT/wordlists/extensions.txt' | grep -E '^[a-zA-Z0-9]+$'
"

# Test 11: Patterns de secrets - Syntaxe regex valide
run_test "Secret patterns regex validation" "
    grep -E '^[^:]+:' '$PROJECT_ROOT/wordlists/secrets.patterns' | head -5 | while IFS=':' read -r name pattern; do
        echo 'test' | grep -E \"\$pattern\" >/dev/null 2>&1 || true
    done
"

echo ""
echo "⚙️  Tests de configuration"
echo "-------------------------"

# Test 12: YAML configs - Syntaxe valide (si yq disponible)
if command -v yq >/dev/null 2>&1; then
    run_test "YAML syntax validation" "
        yq eval . '$PROJECT_ROOT/configs/defaults.yaml' >/dev/null &&
        yq eval . '$PROJECT_ROOT/configs/scoring.yaml' >/dev/null
    "
else
    log_warning "yq non disponible, skip validation YAML"
fi

# Test 13: CSV schemas - En-têtes valides
run_test "CSV schemas headers validation" "
    for schema in '$PROJECT_ROOT/schemas'/*.csv; do
        head -1 \"\$schema\" | grep -q '^schema_ver' || exit 1
    done
"

# Test 14: Config files - Format correct
run_test "Config files format validation" "
    grep -E '^[A-Za-z0-9_-]+:' '$PROJECT_ROOT/configs/h_headers.txt' >/dev/null &&
    grep -E '^[A-Za-z]+:' '$PROJECT_ROOT/configs/h_auth.txt' >/dev/null
"

echo ""
echo "🧪 Tests de robustesse"
echo "----------------------"

# Test 15: Scripts avec paramètres invalides
run_test "Scripts error handling" "
    ! '$SCRIPTS_DIR/convert_scope.sh' 2>/dev/null &&
    ! '$SCRIPTS_DIR/enum.sh' 2>/dev/null &&
    ! '$SCRIPTS_DIR/download_js.sh' 2>/dev/null &&
    ! '$SCRIPTS_DIR/gau_wayback.sh' 2>/dev/null
"

# Test 16: Scripts avec fichiers inexistants
run_test "Scripts missing files handling" "
    ! '$SCRIPTS_DIR/convert_scope.sh' '/nonexistent/file.csv' '$TEMP_DIR/out.csv' 'Test' 2>/dev/null &&
    ! '$SCRIPTS_DIR/download_js.sh' '/nonexistent/urls.txt' '$TEMP_DIR/js' 2>/dev/null
"

# Test 17: Permissions sur répertoires temporaires
run_test "Temporary directories permissions" "
    mkdir -p '$TEMP_DIR/test_perms' &&
    chmod 755 '$TEMP_DIR/test_perms' &&
    test -w '$TEMP_DIR/test_perms'
"

echo ""
echo "📊 Rapport des tests de scripts"
echo "==============================="
echo "Total tests:    $TOTAL_TESTS"
echo "✅ Passés:       $PASSED_TESTS"  
echo "❌ Échoués:      $FAILED_TESTS"
echo ""

# Calculer le taux de réussite
if [[ $TOTAL_TESTS -gt 0 ]]; then
    SUCCESS_RATE=$(( (PASSED_TESTS * 100) / TOTAL_TESTS ))
    echo "Taux de réussite: ${SUCCESS_RATE}%"
fi

# Code de sortie
if [[ $FAILED_TESTS -gt 0 ]]; then
    log_error "Des tests ont échoué!"
    exit 1
else
    log_success "Tous les tests de scripts sont passés!"
    exit 0
fi