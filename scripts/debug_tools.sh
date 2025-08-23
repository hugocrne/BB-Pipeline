#!/bin/bash
# BB-Pipeline - Outils de debugging avancés
# Utilitaires pour le profiling, analyse mémoire, etc.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

show_help() {
    echo "🛰️  BB-Pipeline - Outils de debugging"
    echo "====================================="
    echo ""
    echo "Usage: $0 <commande> [args...]"
    echo ""
    echo "Commandes disponibles:"
    echo "  valgrind <binary> [args]    - Analyse mémoire avec Valgrind"
    echo "  asan <binary> [args]        - Build et run avec AddressSanitizer"
    echo "  profile <binary> [args]     - Profile avec perf (Linux) ou Instruments (macOS)"
    echo "  coverage                    - Génère rapport de couverture de code"
    echo "  benchmark <binary> [args]   - Benchmark de performance"
    echo "  debug-symbols <binary>      - Analyse des symboles de debug"
    echo "  flamegraph <binary> [args]  - Génère flamegraph de performance"
    echo ""
}

# Détecter l'OS
detect_os() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    else
        echo "unknown"
    fi
}

OS=$(detect_os)

# === Valgrind Analysis ===
run_valgrind() {
    local binary="$1"
    shift
    local args=("$@")
    
    if ! command -v valgrind >/dev/null 2>&1; then
        log_error "Valgrind non installé!"
        if [[ "$OS" == "macos" ]]; then
            log_info "Installation: brew install valgrind (peut être instable sur macOS récents)"
        else
            log_info "Installation: sudo apt install valgrind"
        fi
        exit 1
    fi
    
    log_info "Analyse mémoire avec Valgrind..."
    
    local log_file="debug_valgrind_$(date +%Y%m%d_%H%M%S).log"
    
    valgrind \
        --tool=memcheck \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --verbose \
        --log-file="$log_file" \
        "$binary" "${args[@]}"
    
    log_success "Analyse terminée, rapport: $log_file"
    
    # Résumé
    if grep -q "ERROR SUMMARY: 0 errors" "$log_file"; then
        log_success "Aucune erreur mémoire détectée!"
    else
        log_warning "Erreurs mémoire détectées, voir $log_file"
    fi
}

# === AddressSanitizer Build & Run ===
run_asan() {
    local binary_name=$(basename "$1")
    shift
    local args=("$@")
    
    log_info "Build avec AddressSanitizer..."
    
    cd "$PROJECT_ROOT"
    
    # Build avec sanitizers
    cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON -G Ninja
    cmake --build build-asan -j
    
    local asan_binary="build-asan/$binary_name"
    
    if [[ ! -x "$asan_binary" ]]; then
        log_error "Binaire ASan non trouvé: $asan_binary"
        exit 1
    fi
    
    log_info "Exécution avec AddressSanitizer..."
    
    # Variables d'environnement ASan
    export ASAN_OPTIONS="abort_on_error=1:check_initialization_order=1:strict_init_order=1:detect_stack_use_after_return=1"
    export UBSAN_OPTIONS="print_stacktrace=1:abort_on_error=1"
    
    "$asan_binary" "${args[@]}"
    
    log_success "Exécution ASan terminée sans erreur!"
}

# === Performance Profiling ===
run_profile() {
    local binary="$1"
    shift
    local args=("$@")
    
    log_info "Profiling de performance..."
    
    if [[ "$OS" == "linux" ]]; then
        # Linux: utiliser perf
        if ! command -v perf >/dev/null 2>&1; then
            log_error "perf non installé! Installation: sudo apt install linux-tools-generic"
            exit 1
        fi
        
        local perf_data="perf_$(date +%Y%m%d_%H%M%S).data"
        
        log_info "Profiling avec perf..."
        perf record -g -o "$perf_data" "$binary" "${args[@]}"
        
        log_info "Génération du rapport..."
        perf report -i "$perf_data" --stdio > "${perf_data%.data}.report"
        
        log_success "Profiling terminé: $perf_data, rapport: ${perf_data%.data}.report"
        
    elif [[ "$OS" == "macos" ]]; then
        # macOS: utiliser Instruments
        log_info "Profiling avec Instruments (macOS)..."
        
        local trace_file="profile_$(date +%Y%m%d_%H%M%S).trace"
        
        instruments -t "Time Profiler" -D "$trace_file" "$binary" "${args[@]}"
        
        log_success "Profiling terminé: $trace_file"
        log_info "Ouvrez le fichier .trace avec Instruments pour analyser"
        
    else
        log_error "Profiling non supporté sur cet OS"
        exit 1
    fi
}

# === Code Coverage ===
run_coverage() {
    log_info "Génération du rapport de couverture..."
    
    cd "$PROJECT_ROOT"
    
    # Build avec coverage
    cmake -S . -B build-coverage -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON -G Ninja
    cmake --build build-coverage -j
    
    # Exécuter les tests pour générer les données de couverture
    log_info "Exécution des tests..."
    BINARY_DIR="build-coverage" ./tests/run_all_tests.sh
    
    if command -v gcov >/dev/null 2>&1 && command -v lcov >/dev/null 2>&1; then
        # Générer rapport HTML avec lcov
        log_info "Génération du rapport HTML..."
        
        mkdir -p coverage
        lcov --directory build-coverage --capture --output-file coverage/coverage.info
        lcov --remove coverage/coverage.info '/usr/*' --output-file coverage/coverage.info
        lcov --remove coverage/coverage.info '*/build*/*' --output-file coverage/coverage.info
        
        genhtml coverage/coverage.info --output-directory coverage/html
        
        log_success "Rapport de couverture généré: coverage/html/index.html"
        
    else
        log_warning "lcov/gcov non installés, rapport basique uniquement"
        
        # Rapport basique
        find build-coverage -name "*.gcda" -exec gcov {} \; > coverage/coverage.txt 2>&1
        
        log_success "Rapport de couverture basique: coverage/coverage.txt"
    fi
}

# === Debug Symbols Analysis ===
analyze_debug_symbols() {
    local binary="$1"
    
    log_info "Analyse des symboles de debug..."
    
    if command -v objdump >/dev/null 2>&1; then
        echo "=== Sections ===" > debug_symbols.txt
        objdump -h "$binary" >> debug_symbols.txt
        
        echo -e "\n=== Symboles ===" >> debug_symbols.txt
        objdump -t "$binary" | head -50 >> debug_symbols.txt
        
        echo -e "\n=== Informations debug ===" >> debug_symbols.txt
        objdump -g "$binary" | head -20 >> debug_symbols.txt 2>/dev/null || echo "Pas d'infos debug DWARF" >> debug_symbols.txt
        
        log_success "Analyse sauvée: debug_symbols.txt"
        
    elif [[ "$OS" == "macos" ]] && command -v otool >/dev/null 2>&1; then
        echo "=== Sections ===" > debug_symbols.txt
        otool -l "$binary" >> debug_symbols.txt
        
        echo -e "\n=== Symboles ===" >> debug_symbols.txt
        nm "$binary" | head -50 >> debug_symbols.txt
        
        log_success "Analyse sauvée: debug_symbols.txt"
        
    else
        log_error "Outils d'analyse non disponibles (objdump/otool)"
        exit 1
    fi
}

# === Flamegraph Generation ===
generate_flamegraph() {
    local binary="$1"
    shift
    local args=("$@")
    
    log_info "Génération de flamegraph..."
    
    if [[ "$OS" != "linux" ]]; then
        log_error "Flamegraph supporté uniquement sur Linux"
        exit 1
    fi
    
    if ! command -v perf >/dev/null 2>&1; then
        log_error "perf requis pour flamegraph"
        exit 1
    fi
    
    # Télécharger FlameGraph si pas présent
    if [[ ! -d "FlameGraph" ]]; then
        log_info "Téléchargement de FlameGraph..."
        git clone https://github.com/brendangregg/FlameGraph.git
    fi
    
    local perf_data="flamegraph_$(date +%Y%m%d_%H%M%S).data"
    
    # Profile avec perf
    perf record -F 997 -g -o "$perf_data" "$binary" "${args[@]}"
    
    # Générer flamegraph
    perf script -i "$perf_data" | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > "${perf_data%.data}.svg"
    
    log_success "Flamegraph généré: ${perf_data%.data}.svg"
}

# === Benchmark ===
run_benchmark() {
    local binary="$1"
    shift
    local args=("$@")
    
    log_info "Benchmark de performance..."
    
    local results_file="benchmark_$(date +%Y%m%d_%H%M%S).txt"
    
    echo "=== Benchmark BB-Pipeline ===" > "$results_file"
    echo "Date: $(date)" >> "$results_file"
    echo "Binary: $binary" >> "$results_file"
    echo "Args: ${args[*]}" >> "$results_file"
    echo "" >> "$results_file"
    
    # Plusieurs runs pour moyenne
    local total_time=0
    local runs=5
    
    for i in $(seq 1 $runs); do
        log_info "Run $i/$runs..."
        
        local start_time=$(date +%s.%N)
        "$binary" "${args[@]}" >/dev/null 2>&1 || true
        local end_time=$(date +%s.%N)
        
        local run_time=$(echo "$end_time - $start_time" | bc -l)
        total_time=$(echo "$total_time + $run_time" | bc -l)
        
        echo "Run $i: ${run_time}s" >> "$results_file"
    done
    
    local avg_time=$(echo "scale=3; $total_time / $runs" | bc -l)
    
    echo "" >> "$results_file"
    echo "Average time: ${avg_time}s" >> "$results_file"
    
    # Info système
    echo "" >> "$results_file"
    echo "=== System Info ===" >> "$results_file"
    uname -a >> "$results_file"
    
    if command -v lscpu >/dev/null 2>&1; then
        lscpu | head -20 >> "$results_file"
    fi
    
    log_success "Benchmark terminé: $results_file (temps moyen: ${avg_time}s)"
}

# === Main ===
main() {
    if [[ $# -eq 0 ]]; then
        show_help
        exit 1
    fi
    
    local command="$1"
    shift
    
    case "$command" in
        "valgrind")
            run_valgrind "$@"
            ;;
        "asan")
            run_asan "$@"
            ;;
        "profile")
            run_profile "$@"
            ;;
        "coverage")
            run_coverage
            ;;
        "debug-symbols")
            analyze_debug_symbols "$@"
            ;;
        "flamegraph")
            generate_flamegraph "$@"
            ;;
        "benchmark")
            run_benchmark "$@"
            ;;
        "help"|"-h"|"--help")
            show_help
            ;;
        *)
            log_error "Commande inconnue: $command"
            show_help
            exit 1
            ;;
    esac
}

main "$@"