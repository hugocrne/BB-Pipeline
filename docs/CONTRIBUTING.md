# 🤝 Guide de contribution - BB-Pipeline

Merci de votre intérêt pour contribuer à BB-Pipeline ! Ce guide vous aidera à contribuer efficacement.

## 🚀 Démarrage rapide

### Setup environnement

```bash
# Cloner le projet
git clone https://github.com/your-org/bb-pipeline
cd bb-pipeline

# Setup automatique des dépendances
./scripts/setup_dev.sh

# Setup des hooks pre-commit
pip install pre-commit
pre-commit install

# Build et test
make dev
make test
```

### Workflow de développement

1. **Fork** le repository
2. **Créer une branche** pour votre feature : `git checkout -b feature/amazing-feature`
3. **Développer** avec les outils fournis
4. **Tester** : `make test`
5. **Commit** : messages clairs et descriptifs
6. **Push** : `git push origin feature/amazing-feature`
7. **Pull Request** avec description détaillée

## 📝 Standards de développement

### Style de code C++

Le projet utilise **C++20** avec les standards suivants :

```cpp
// ✅ Bon
class HttpProber {
public:
    ProbeResult probe(const std::string& url);
    
private:
    std::unique_ptr<HttpClient> client_;
    Configuration config_;
};

// ❌ Éviter
class httpProber {
    public:
    probeResult probe(string url);
    
    private:
    HttpClient* client;
    Configuration config;
};
```

**Conventions :**
- **Classes** : `PascalCase`
- **Fonctions/variables** : `snake_case`  
- **Constantes** : `UPPER_CASE`
- **Membres privés** : suffix `_`
- **Headers** : `#pragma once`
- **Includes** : ordre système → tiers → projet

### Formatage automatique

```bash
# Formate automatiquement tout le code
make format

# Vérifie le formatage (CI)
make check-format
```

### Tests

**Tests obligatoires** pour :
- Nouvelles fonctions publiques
- Correction de bugs
- Modules complets

```cpp
// Exemple test
TEST(HttpProberTest, ProbeValidUrl) {
    HttpProber prober;
    auto result = prober.probe("https://example.com");
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.status_code, 200);
}
```

**Lancer les tests :**
```bash
make test              # Tous les tests
make test-scripts      # Tests des scripts seulement
make debug-sanitizers  # Build debug avec sanitizers
```

## 🏗️ Architecture des contributions

### Ajout d'un nouveau module

1. **Créer la structure**
```bash
mkdir -p src/mymodule
touch src/mymodule/main.cpp
touch src/mymodule/mymodule.cpp
touch include/bbp/modules/mymodule.hpp
```

2. **Implémenter l'interface**
```cpp
class MyModule : public PipelineModule {
public:
    std::string name() const override { return "mymodule"; }
    std::vector<std::string> dependencies() const override;
    ExecutionResult execute(const ModuleInput& input) override;
};
```

3. **Ajouter au CMake**
```cmake
# Dans CMakeLists.txt, ajouter à MODULES
set(MODULES
    subhunter
    httpxpp
    mymodule  # <-- nouveau module
    # ...
)
```

4. **Créer le schéma CSV**
```bash
# schemas/XX_mymodule.schema.csv
schema_ver,program,custom_field1,custom_field2,timestamp
```

5. **Tests dédiés**
```bash
# tests/test_mymodule.sh
#!/bin/bash
# Tests spécifiques au module
```

### Modification des core libraries

Les modifications des libraries core (`src/core/*`) nécessitent :

1. **Compatibilité ascendante** préservée
2. **Tests exhaustifs** de régression
3. **Documentation** mise à jour
4. **Review approfondie** (2+ reviewers)

### Ajout de dépendances

1. **Justification** : pourquoi cette dépendance ?
2. **Alternatives** évaluées
3. **Impact** sur la taille du binaire
4. **License** compatible
5. **Disponibilité** sur toutes les plateformes

## 🧪 Tests et qualité

### Types de tests

1. **Tests unitaires** : logique métier isolée
2. **Tests d'intégration** : modules avec vraies données
3. **Tests end-to-end** : pipeline complet
4. **Tests de performance** : benchmarks
5. **Tests de sécurité** : sanitizers, valgrind

### Coverage

```bash
# Générer rapport de couverture
./scripts/debug_tools.sh coverage

# Ouvrir le rapport
open coverage/html/index.html  # macOS
xdg-open coverage/html/index.html  # Linux
```

**Objectif** : > 80% de couverture sur le code nouveau

### Debugging

```bash
# AddressSanitizer pour détection mémoire
./scripts/debug_tools.sh asan ./build/bbpctl

# Valgrind pour analyse complète  
./scripts/debug_tools.sh valgrind ./build/bbpctl

# Profiling performance
./scripts/debug_tools.sh profile ./build/bbpctl

# Flamegraph (Linux)
./scripts/debug_tools.sh flamegraph ./build/bbpctl
```

## 📋 Checklist Pull Request

### Avant de soumettre

- [ ] Code formaté (`make format`)
- [ ] Tests passent (`make test`)
- [ ] Pas de warning compilation
- [ ] Documentation mise à jour
- [ ] CHANGELOG.md mis à jour
- [ ] Commit messages descriptifs

### Description PR

**Template obligatoire :**

```markdown
## 📝 Description
Brève description des changements.

## 🎯 Type de changement
- [ ] Bug fix
- [ ] Nouvelle feature
- [ ] Breaking change
- [ ] Documentation
- [ ] Performance
- [ ] Refactoring

## 🧪 Tests
- [ ] Tests unitaires ajoutés/mis à jour
- [ ] Tests d'intégration validés
- [ ] Testé manuellement sur [OS]

## 📚 Documentation
- [ ] Code documenté
- [ ] README.md mis à jour
- [ ] Architecture documentée

## 🔍 Review checklist
- [ ] Code review self-effectué
- [ ] Pas de secrets/credentials
- [ ] Performance acceptable
- [ ] Compatibilité préservée
```

### Critères d'acceptation

1. **Fonctionnel** : feature fonctionne comme spécifié
2. **Testé** : couverture appropriée
3. **Performant** : pas de régression
4. **Sécurisé** : pas de vulnérabilités
5. **Maintenable** : code lisible et documenté
6. **Compatible** : pas de breaking changes

## 🐛 Reporting de bugs

### Issues template

```markdown
## 🐛 Bug Description
Description claire et concise.

## 🔄 Reproduction
Étapes pour reproduire :
1. Exécuter `./build/bbpctl ...`
2. Observer `...`

## ✅ Comportement attendu
Ce qui devrait se passer.

## 🖥️ Environnement
- OS: [Linux/macOS/Windows]
- Version: [sortie de `./build/bbpctl --version`]
- Build: [Debug/Release]

## 📎 Logs/Screenshots
Joindre logs pertinents.
```

### Bug triage

**Priorités :**
- **P0-Critical** : crash, sécurité, perte de données
- **P1-High** : fonctionnalité majeure cassée  
- **P2-Medium** : bug gênant mais workaround possible
- **P3-Low** : améliorations, edge cases

## 🎨 Guidelines UI/UX

### Messages utilisateur

```cpp
// ✅ Bon : clair et actionnable
log_error("Failed to parse scope.csv:3 - Invalid domain format. Expected: domain.com");

// ❌ Éviter : vague et inutile
log_error("Parse error");
```

### Codes de sortie

```cpp
constexpr int EXIT_SUCCESS = 0;
constexpr int EXIT_CONFIG_ERROR = 1;
constexpr int EXIT_SCOPE_ERROR = 2;
constexpr int EXIT_NETWORK_ERROR = 3;
constexpr int EXIT_RATE_LIMITED = 4;
```

### Progress indicators

```bash
# Pour les opérations longues
[INFO] Scanning 1247 domains...
[████████████████████████████████] 100% (1247/1247) ETA: 0s
```

## 🎯 Roadmap et priorités

### Version 1.0 (Foundation)
- [ ] Core libraries complètes
- [ ] Modules subhunter, httpxpp, dirbff
- [ ] Orchestrateur fonctionnel
- [ ] Tests complets

### Version 1.1 (Intelligence)  
- [ ] Modules jsintel, apiparser, apitester
- [ ] Scoring sophistiqué
- [ ] Performance optimisée

### Version 1.2 (Advanced)
- [ ] Modules headless, mobile, changes  
- [ ] API REST
- [ ] Plugin system

### Version 2.0 (Scale)
- [ ] Distribution/clustering
- [ ] UI web
- [ ] Machine learning

## 💬 Communication

### Channels

- **GitHub Issues** : bugs, features requests
- **GitHub Discussions** : questions générales, idées
- **Pull Requests** : code review, implémentation

### Conventions

- **Issues** : templates obligatoires
- **Commits** : [conventional commits](https://www.conventionalcommits.org/)
- **Branches** : `feature/`, `bugfix/`, `hotfix/`
- **Tags** : semantic versioning (v1.2.3)

## 🏆 Reconnaissance

Les contributeurs sont listés dans :
- `CONTRIBUTORS.md`
- Release notes
- `git log` avec `--format=fuller`

**Types de contributions reconnues :**
- Code (features, bugfixes)
- Documentation
- Tests
- Reporting de bugs
- Review de code
- Design/UX
- Performance
- Sécurité

---

Merci de contribuer à BB-Pipeline ! 🛰️✨