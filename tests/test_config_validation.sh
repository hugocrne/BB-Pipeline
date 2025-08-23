#!/bin/bash
# BB-Pipeline - Tests de validation des configurations
# Teste la validité des fichiers de configuration YAML, CSV, TXT

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

echo "⚙️  Tests de validation des configurations"
echo "=========================================="
echo ""

# Tests YAML
echo "📄 Tests YAML"
echo "-------------"

if command -v python3 >/dev/null 2>&1; then
    run_test "YAML syntax - defaults.yaml" "
        python3 -c 'import yaml; yaml.safe_load(open(\"$PROJECT_ROOT/configs/defaults.yaml\"))'
    "
    
    run_test "YAML syntax - scoring.yaml" "
        python3 -c 'import yaml; yaml.safe_load(open(\"$PROJECT_ROOT/configs/scoring.yaml\"))'
    "
else
    log_info "Python3 non disponible, skip tests YAML"
fi

# Tests CSV schemas
echo ""
echo "📊 Tests CSV Schemas"
echo "-------------------"

run_test "CSV schemas - headers present" "
    for schema in '$PROJECT_ROOT/schemas'/*.csv; do
        head -1 \"\$schema\" | grep -q '^schema_ver' || exit 1
    done
"

run_test "CSV schemas - consistent format" "
    for schema in '$PROJECT_ROOT/schemas'/*.csv; do
        head -1 \"\$schema\" | grep -q ',program,' || exit 1
    done
"

# Tests config files
echo ""  
echo "🔧 Tests fichiers config"
echo "------------------------"

run_test "h_headers.txt format" "
    grep -E '^[A-Za-z0-9_-]+:' '$PROJECT_ROOT/configs/h_headers.txt' >/dev/null
"

run_test "h_auth.txt format" "
    grep -E '^[A-Za-z]+:' '$PROJECT_ROOT/configs/h_auth.txt' >/dev/null
"

echo ""
echo "Rapport: $PASSED_TESTS/$TOTAL_TESTS tests passés"

[[ $FAILED_TESTS -eq 0 ]]