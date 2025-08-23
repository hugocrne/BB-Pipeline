#!/bin/bash
# BB-Pipeline - Tests spécifiques pour les schémas CSV
# Valide la cohérence et complétude des schémas

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

RED='\033[0;31m'
GREEN='\033[0;32m' 
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

TOTAL_TESTS=0
PASSED_TESTS=0 
FAILED_TESTS=0

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

echo "📊 Tests des schémas CSV"
echo "========================"
echo ""

# Test que tous les schémas attendus existent
EXPECTED_SCHEMAS=(
    "01_subdomains.schema.csv"
    "02_probe.schema.csv"
    "03_headless.schema.csv" 
    "04_discovery.schema.csv"
    "05_jsintel.schema.csv"
    "06_api_catalog.schema.csv"
    "07_api_findings.schema.csv"
    "08_mobile_intel.schema.csv"
    "09_changes.schema.csv"
    "99_final_ranked.schema.csv"
)

for schema in "${EXPECTED_SCHEMAS[@]}"; do
    run_test "Schema exists: $schema" "test -f '$PROJECT_ROOT/schemas/$schema'"
done

# Test que tous ont les champs obligatoires
run_test "All schemas have required fields" "
    for schema in '$PROJECT_ROOT/schemas'/*.csv; do
        header=\$(head -1 \"\$schema\")
        echo \"\$header\" | grep -q 'schema_ver' || exit 1
        echo \"\$header\" | grep -q 'program' || exit 1
        echo \"\$header\" | grep -q 'timestamp' || exit 1
    done
"

# Test cohérence des noms de champs
run_test "Field naming consistency" "
    grep -h '^schema_ver' '$PROJECT_ROOT/schemas'/*.csv | sort | uniq | wc -l | grep -q '^1$'
"

echo ""
echo "Rapport: $PASSED_TESTS/$TOTAL_TESTS tests passés"

[[ $FAILED_TESTS -eq 0 ]]