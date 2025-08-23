#!/bin/bash
# BB-Pipeline - Script de setup de l'environnement de développement
# Configure automatiquement toutes les dépendances et outils nécessaires

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Couleurs
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

echo "🛰️  BB-Pipeline - Setup environnement de développement"
echo "====================================================="
echo ""

# Détecter l'OS
OS="unknown"
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    if command -v apt-get >/dev/null 2>&1; then
        OS="ubuntu"
    elif command -v yum >/dev/null 2>&1; then
        OS="centos"
    fi
fi

log_info "OS détecté: $OS"
echo ""

# === Phase 1: Vérification des prérequis ===
echo "📋 Phase 1: Vérification des prérequis"
echo "--------------------------------------"

check_command() {
    local cmd="$1"
    local name="${2:-$cmd}"
    
    if command -v "$cmd" >/dev/null 2>&1; then
        local version=$($cmd --version 2>/dev/null | head -1 || echo "version inconnue")
        log_success "$name installé: $version"
        return 0
    else
        log_warning "$name non trouvé"
        return 1
    fi
}

# Vérifier les outils de base
MISSING_TOOLS=()

check_command "git" "Git" || MISSING_TOOLS+=("git")
check_command "cmake" "CMake" || MISSING_TOOLS+=("cmake")
check_command "ninja" "Ninja" || MISSING_TOOLS+=("ninja")

# Vérifier les compilateurs
if check_command "clang++" "Clang++"; then
    COMPILER="clang++"
elif check_command "g++" "G++"; then
    COMPILER="g++"
else
    MISSING_TOOLS+=("compiler")
    log_error "Aucun compilateur C++ trouvé!"
fi

echo ""

# === Phase 2: Installation des dépendances ===
echo "📦 Phase 2: Installation des dépendances"
echo "----------------------------------------"

install_deps_macos() {
    log_info "Installation des dépendances macOS via Homebrew..."
    
    if ! command -v brew >/dev/null 2>&1; then
        log_info "Installation de Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    
    # Dépendances principales
    brew install cmake ninja boost openssl nlohmann-json spdlog gumbo-parser yaml-cpp curl
    
    # Outils de développement
    brew install shellcheck clang-format
    
    # Outils optionnels
    brew install fswatch # pour make test-watch
    
    log_success "Dépendances macOS installées!"
}

install_deps_ubuntu() {
    log_info "Installation des dépendances Ubuntu/Debian..."
    
    sudo apt update
    sudo apt install -y \
        cmake build-essential ninja-build pkg-config git \
        libboost-all-dev libssl-dev nlohmann-json3-dev \
        libspdlog-dev libgumbo-dev jq curl \
        shellcheck clang-format \
        inotify-tools # pour make test-watch
    
    log_success "Dépendances Ubuntu installées!"
}

install_deps_centos() {
    log_info "Installation des dépendances CentOS/RHEL..."
    
    sudo yum groupinstall -y "Development Tools"
    sudo yum install -y cmake ninja-build boost-devel openssl-devel \
                        jsoncpp-devel spdlog-devel gumbo-parser-devel \
                        yaml-cpp-devel libcurl-devel
    
    log_warning "Certaines dépendances peuvent nécessiter EPEL repository"
    log_success "Dépendances CentOS installées!"
}

case "$OS" in
    "macos")
        install_deps_macos
        ;;
    "ubuntu") 
        install_deps_ubuntu
        ;;
    "centos")
        install_deps_centos
        ;;
    *)
        log_warning "OS non supporté automatiquement. Installation manuelle requise."
        log_info "Consultez le README.md pour les instructions d'installation."
        ;;
esac

echo ""

# === Phase 3: Configuration du projet ===
echo "⚙️  Phase 3: Configuration du projet"
echo "------------------------------------"

cd "$PROJECT_ROOT"

log_info "Configuration CMake..."
if cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug; then
    log_success "Configuration CMake réussie!"
else
    log_error "Échec de la configuration CMake"
    exit 1
fi

log_info "Build de test..."
if cmake --build build -j; then
    log_success "Build de test réussi!"
else
    log_error "Échec du build de test"
    exit 1
fi

echo ""

# === Phase 4: Validation de l'environnement ===
echo "✅ Phase 4: Validation de l'environnement"
echo "-----------------------------------------"

log_info "Test d'exécution de bbpctl..."
if ./build/bbpctl --version >/dev/null 2>&1; then
    log_success "bbpctl fonctionne correctement!"
else
    log_error "Problème avec l'exécution de bbpctl"
fi

log_info "Test de la suite de tests..."
if ./tests/run_all_tests.sh >/dev/null 2>&1; then
    log_success "Suite de tests fonctionnelle!"
else
    log_warning "Quelques tests ont échoué (normal en setup initial)"
fi

log_info "Vérification des outils de développement..."
check_command "clang-format" "Clang-Format"
check_command "shellcheck" "ShellCheck"

echo ""

# === Phase 5: Configuration Git (optionnel) ===
echo "🔧 Phase 5: Configuration Git"
echo "------------------------------"

setup_git_hooks() {
    log_info "Configuration des Git hooks..."
    
    mkdir -p .git/hooks
    
    # Pre-commit hook
    cat > .git/hooks/pre-commit << 'EOF'
#!/bin/bash
# BB-Pipeline pre-commit hook

echo "🔍 Vérification du formatage..."
make check-format || {
    echo "❌ Formatage incorrect! Exécutez 'make format' pour corriger."
    exit 1
}

echo "🧪 Tests rapides..."
make test-scripts || {
    echo "❌ Tests échoués!"
    exit 1
}

echo "✅ Pre-commit validé!"
EOF
    
    chmod +x .git/hooks/pre-commit
    log_success "Git hooks configurés!"
}

if [[ -d .git ]]; then
    read -p "Configurer les Git hooks? (y/N): " -r
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        setup_git_hooks
    fi
else
    log_info "Repository Git non initialisé, skip des hooks"
fi

echo ""

# === Résumé final ===
echo "🎉 Setup terminé!"
echo "================="
echo ""
log_success "Environnement de développement prêt!"
echo ""
echo "📝 Commandes utiles:"
echo "  make help          - Affiche toutes les commandes"
echo "  make dev           - Setup environnement de développement"
echo "  make build         - Build le projet"
echo "  make test          - Lance les tests"
echo "  make run           - Exécute bbpctl"
echo "  make format        - Formate le code"
echo ""
echo "🚀 Prochaines étapes:"
echo "  1. Ouvrir le projet dans VS Code"
echo "  2. Installer les extensions recommandées"
echo "  3. Commencer le développement avec 'make dev'"
echo ""

# Vérifier VS Code
if command -v code >/dev/null 2>&1; then
    echo "💡 Astuce: Ouvrez le projet avec 'code .' pour VS Code configuré!"
fi

exit 0