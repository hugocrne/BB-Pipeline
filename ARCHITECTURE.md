# 🏗️ Architecture BB-Pipeline

## 📁 Structure des Dossiers

```
BB-Pipeline/
├── src/                              # Code source
│   ├── infrastructure/               # Composants core réutilisables
│   │   ├── logging/                  # Système de logging NDJSON avec correlation IDs
│   │   │   └── logger.cpp
│   │   ├── networking/               # Composants réseau et cache
│   │   │   ├── rate_limiter.cpp      # Token bucket avec backoff adaptatif
│   │   │   └── cache_system.cpp      # Cache HTTP ETag/Last-Modified
│   │   ├── threading/                # Gestion des threads et parallélisme
│   │   │   └── thread_pool.cpp       # Pool threads avec queue prioritaire
│   │   ├── config/                   # Configuration et parsing
│   │   │   └── config_manager.cpp    # Parser YAML avec validation
│   │   └── system/                   # Signal handler, error recovery (à venir)
│   ├── pipeline/                     # Logique pipeline spécifique
│   │   ├── orchestrator/             # Logique bbpctl (à venir)
│   │   ├── csv/                      # Moteur CSV (à venir)
│   │   └── modules/                  # Modules pipeline (subhunter, etc)
│   ├── types/                        # Types métier (Scope, ScopeEntry)
│   ├── utils/                        # Utilities génériques (à venir)
│   └── main.cpp                      # Point d'entrée
├── include/                          # Headers publics
│   ├── infrastructure/               # Headers infrastructure
│   │   ├── logging/
│   │   │   └── logger.hpp
│   │   ├── networking/
│   │   │   ├── rate_limiter.hpp
│   │   │   └── cache_system.hpp
│   │   ├── threading/
│   │   │   └── thread_pool.hpp
│   │   ├── config/
│   │   │   └── config_manager.hpp
│   │   └── system/
│   ├── pipeline/
│   ├── types/
│   └── utils/
├── tests/                            # Tests unitaires
├── configs/                          # Configurations YAML
├── data/                             # Fichiers scope CSV
├── out/                              # Sortie pipeline
├── wordlists/                        # Wordlists pour énumération
└── build/                            # Build artifacts
```

## 🎯 Principes Architecturaux

### 1. **Séparation Infrastructure / Pipeline**
- `infrastructure/` : Composants génériques réutilisables
- `pipeline/` : Logique spécifique au pipeline de reconnaissance

### 2. **Organisation Modulaire par Domaine**
- `logging/` : Tout ce qui concerne les logs
- `networking/` : Rate limiting, cache, HTTP clients
- `threading/` : Thread pools, memory managers
- `config/` : Configuration et validation
- `system/` : Signal handlers, error recovery

### 3. **Headers Publics Séparés**
- `include/` : Interface publique des modules
- Headers privés restent dans `src/` si nécessaires

### 4. **Tests par Module**
- Un test par composant infrastructure
- Tests d'intégration pour le pipeline complet

## 📦 Composants Infrastructure Terminés

### ✅ Logging (`infrastructure/logging/`)
- Logger singleton thread-safe
- Format NDJSON structuré
- Correlation IDs automatiques
- Niveaux debug/info/warn/error

### ✅ Networking (`infrastructure/networking/`)
- **Rate Limiter** : Token bucket par domaine avec backoff adaptatif
- **Cache System** : Cache HTTP avec validation ETag/Last-Modified

### ✅ Threading (`infrastructure/threading/`)
- **Thread Pool** : Pool haute performance avec queue prioritaire
- Auto-scaling basé sur la charge
- Support timeout et annulation

### ✅ Config (`infrastructure/config/`)
- **Config Manager** : Parser YAML avec validation
- Templates et expansion de variables d'environnement
- Rechargement à chaud

## 🔄 Utilisation des Composants

### Includes
```cpp
// Infrastructure logging
#include "infrastructure/logging/logger.hpp"

// Infrastructure networking 
#include "infrastructure/networking/rate_limiter.hpp"
#include "infrastructure/networking/cache_system.hpp"

// Infrastructure threading
#include "infrastructure/threading/thread_pool.hpp"

// Infrastructure config
#include "infrastructure/config/config_manager.hpp"

// Types métier
#include "types/scope.hpp"
```

### Compilation
```bash
# Build complet
cmake --build build -j

# Tests individuels
g++ -std=c++17 -I./include -pthread tests/test_cache_system.cpp \
    src/infrastructure/networking/cache_system.cpp \
    src/infrastructure/logging/logger.cpp -lz -o tests/test_cache_system
```

## 🚀 Prochaines Étapes

1. **System** (`infrastructure/system/`)
   - Signal Handler avec flush CSV garanti
   - Error Recovery avec exponential backoff
   - Memory Manager avec pool allocator

2. **Pipeline** (`pipeline/`)
   - CSV Engine (parser, writer, merger)
   - Orchestrateur bbpctl
   - Modules reconnaissance (subhunter, httpxpp, etc.)

3. **Utils** (`utils/`)
   - Helpers génériques
   - Utilitaires de validation
   - Parsers communs

Cette architecture modulaire facilite :
- **Navigation** : Chaque développeur trouve rapidement ce qu'il cherche
- **Maintenance** : Modifications isolées par domaine
- **Réutilisabilité** : Composants infrastructure utilisables ailleurs
- **Testing** : Tests unitaires focalisés par module
- **Extensibilité** : Ajout facile de nouveaux modules