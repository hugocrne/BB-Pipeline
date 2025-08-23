#!/bin/bash
# BB-Pipeline - Runner principal pour tous les tests
# Lance tous les tests du projet de manière séquentielle et parallèle

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
TESTS_DIR="$SCRIPT_DIR"
LOG_DIR="$TESTS_DIR/logs"
PARALLEL_JOBS="${PARALLEL_JOBS:-4}"

# Couleurs pour les logs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Fonctions utilitaires
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

# Créer le répertoire de logs
mkdir -p "$LOG_DIR"

echo "🛰️  BB-Pipeline Test Suite"
echo "=========================="
echo "Projet: $PROJECT_ROOT"
echo "Tests: $TESTS_DIR"
echo "Logs: $LOG_DIR"
echo "Jobs parallèles: $PARALLEL_JOBS"
echo ""

# Variables pour le rapport final
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Fonction pour exécuter un test
run_test() {
    local test_file="$1"
    local test_name=$(basename "$test_file" .sh)
    local log_file="$LOG_DIR/${test_name}.log"
    
    log_info "Exécution: $test_name"
    
    if [[ -x "$test_file" ]]; then
        if timeout 300 "$test_file" > "$log_file" 2>&1; then
            log_success "$test_name - PASSED"
            return 0
        else
            log_error "$test_name - FAILED (voir $log_file)"
            return 1
        fi
    else
        log_warning "$test_name - SKIPPED (pas exécutable)"
        return 2
    fi
}

# Exporter la fonction pour xargs
export -f run_test log_info log_success log_error log_warning
export LOG_DIR RED GREEN YELLOW BLUE NC

# 1. Tests unitaires (rapides)
echo "📋 Phase 1: Tests unitaires"
echo "----------------------------"

UNIT_TESTS=(
    "$TESTS_DIR/test_scripts.sh"
    "$TESTS_DIR/test_config_validation.sh"
    "$TESTS_DIR/test_csv_schemas.sh"
)

for test in "${UNIT_TESTS[@]}"; do
    if [[ -f "$test" ]]; then
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        if run_test "$test"; then
            PASSED_TESTS=$((PASSED_TESTS + 1))
        elif [[ $? -eq 2 ]]; then
            SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        else
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
    fi
done

echo ""

# 2. Tests de build
echo "🔨 Phase 2: Tests de build"
echo "---------------------------"

log_info "Test du build CMake..."
BUILD_LOG="$LOG_DIR/build.log"
TOTAL_TESTS=$((TOTAL_TESTS + 1))

cd "$PROJECT_ROOT"
if timeout 600 bash -c "cmake -S . -B build -G Ninja && cmake --build build -j" > "$BUILD_LOG" 2>&1; then
    log_success "Build - PASSED"
    PASSED_TESTS=$((PASSED_TESTS + 1))
    
    # Test d'exécution du binaire
    log_info "Test d'exécution bbpctl..."
    EXEC_LOG="$LOG_DIR/bbpctl_exec.log"
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    if timeout 10 ./build/bbpctl > "$EXEC_LOG" 2>&1; then
        log_success "bbpctl execution - PASSED"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        log_error "bbpctl execution - FAILED (voir $EXEC_LOG)"
        FAILED_TESTS=$((FAILED_TESTS + 1))
    fi
else
    log_error "Build - FAILED (voir $BUILD_LOG)"
    FAILED_TESTS=$((FAILED_TESTS + 1))
fi

echo ""

# 3. Tests d'intégration (lents)
echo "🔗 Phase 3: Tests d'intégration"
echo "--------------------------------"

INTEGRATION_TESTS=(
    "$TESTS_DIR/test_scope_conversion.sh"
    "$TESTS_DIR/test_wordlists_validation.sh"
    "$TESTS_DIR/test_end_to_end.sh"
)

for test in "${INTEGRATION_TESTS[@]}"; do
    if [[ -f "$test" ]]; then
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        if run_test "$test"; then
            PASSED_TESTS=$((PASSED_TESTS + 1))
        elif [[ $? -eq 2 ]]; then
            SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        else
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
    fi
done

echo ""

# 4. Tests de performance (optionnels)
if [[ "${RUN_PERF_TESTS:-false}" == "true" ]]; then
    echo "⚡ Phase 4: Tests de performance"
    echo "--------------------------------"
    
    PERF_TESTS=(
        "$TESTS_DIR/test_performance.sh"
        "$TESTS_DIR/test_memory_usage.sh"
    )
    
    for test in "${PERF_TESTS[@]}"; do
        if [[ -f "$test" ]]; then
            TOTAL_TESTS=$((TOTAL_TESTS + 1))
            if run_test "$test"; then
                PASSED_TESTS=$((PASSED_TESTS + 1))
            elif [[ $? -eq 2 ]]; then
                SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
            else
                FAILED_TESTS=$((FAILED_TESTS + 1))
            fi
        fi
    done
    echo ""
fi

# Rapport final
echo "📊 RAPPORT FINAL"
echo "================"
echo "Total tests:    $TOTAL_TESTS"
echo "✅ Passés:       $PASSED_TESTS"
echo "❌ Échoués:      $FAILED_TESTS"
echo "⏭️  Sautés:       $SKIPPED_TESTS"
echo ""

# Calculer le pourcentage de réussite
if [[ $TOTAL_TESTS -gt 0 ]]; then
    SUCCESS_RATE=$(( (PASSED_TESTS * 100) / TOTAL_TESTS ))
    echo "Taux de réussite: ${SUCCESS_RATE}%"
else
    echo "Aucun test exécuté"
fi

# Créer un rapport JSON
REPORT_FILE="$LOG_DIR/test_report.json"
cat > "$REPORT_FILE" << EOF
{
  "timestamp": "$(date -Iseconds)",
  "project_root": "$PROJECT_ROOT",
  "total_tests": $TOTAL_TESTS,
  "passed": $PASSED_TESTS,
  "failed": $FAILED_TESTS,
  "skipped": $SKIPPED_TESTS,
  "success_rate": $(( TOTAL_TESTS > 0 ? (PASSED_TESTS * 100) / TOTAL_TESTS : 0 )),
  "duration_seconds": $SECONDS,
  "logs_directory": "$LOG_DIR"
}
EOF

log_info "Rapport JSON: $REPORT_FILE"

# Code de sortie
if [[ $FAILED_TESTS -gt 0 ]]; then
    log_error "Des tests ont échoué!"
    exit 1
elif [[ $TOTAL_TESTS -eq 0 ]]; then
    log_warning "Aucun test trouvé!"
    exit 2
else
    log_success "Tous les tests sont passés!"
    exit 0
fi