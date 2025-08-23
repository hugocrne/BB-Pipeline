# 🛰️ BB-Pipeline - Checklist de développement

> Utilise cette liste comme suivi de dev. Coche ✅ au fur et à mesure.

## 📁 Structure du projet

### Racine
- [x] `CMakeLists.txt` — configuration CMake
- [x] `README.md` — doc générale  
- [x] `.gitignore` — ignores build/cache/out
- [x] `CLAUDE.md` — guide pour Claude Code
- [x] `CHECKLIST.md` — cette checklist
- [ ] `LICENSE` — licence du projet

### Configs
- [ ] `configs/defaults.yaml` — timeouts, threads, RPS global, UA, cache, soft‑404
- [ ] `configs/scoring.yaml` — pondérations des signaux + seuils de confiance
- [ ] `configs/h_headers.txt` — headers HTTP custom (optionnel)
- [ ] `configs/h_auth.txt` — auth templates (Bearer, Cookie…) (optionnel)

### Données & sorties
- [ ] `data/scope.csv` — entrée (scope autorisé)
- [x] `out/.gitkeep` — placeholder (CSV résultats)

### Scripts wrappers (outils Kali/GitHub)
- [ ] `scripts/enum.sh` — subfinder/amass merge
- [ ] `scripts/download_js.sh` — fetch JS en parallèle
- [ ] `scripts/gau_wayback.sh` — URLs historiques (gau, waybackurls)

### Wordlists
- [ ] `wordlists/subs.txt` — sous‑domaines usuels
- [ ] `wordlists/paths.txt` — chemins sensibles
- [ ] `wordlists/extensions.txt` — extensions
- [ ] `wordlists/secrets.patterns` — regex clés/secrets

### Schémas CSV (contrats E/S)
- [ ] `schemas/01_subdomains.schema.csv`
- [ ] `schemas/02_probe.schema.csv`
- [ ] `schemas/03_headless.schema.csv`
- [ ] `schemas/04_discovery.schema.csv`
- [ ] `schemas/05_jsintel.schema.csv`
- [ ] `schemas/06_api_catalog.schema.csv`
- [ ] `schemas/07_api_findings.schema.csv`
- [ ] `schemas/08_mobile_intel.schema.csv`
- [ ] `schemas/09_changes.schema.csv`
- [ ] `schemas/99_final_ranked.schema.csv`

## 🔧 Code C++ - Headers (include/bbp/)

### Core headers
- [ ] `include/bbp/version.hpp` — version & constantes
- [ ] `include/bbp/config.hpp` — accès YAML
- [ ] `include/bbp/types.hpp` — types internes
- [ ] `include/bbp/util.hpp` — helpers
- [ ] `include/bbp/csv.hpp` — I/O CSV
- [ ] `include/bbp/logger.hpp` — log minimal
- [ ] `include/bbp/rate_limit.hpp` — token bucket
- [ ] `include/bbp/scope.hpp` — parsing scope.csv
- [ ] `include/bbp/http.hpp` — HTTP client
- [ ] `include/bbp/tls.hpp` — parsing cert
- [ ] `include/bbp/cache.hpp` — ETag/Last‑Modified
- [ ] `include/bbp/fingerprints.hpp` — signatures
- [ ] `include/bbp/soft404.hpp` — heuristiques
- [ ] `include/bbp/pipeline.hpp` — interfaces orchestrateur

### Module interfaces
- [ ] `include/bbp/modules/subhunter.hpp` — interface subdomain enum
- [ ] `include/bbp/modules/httpxpp.hpp` — interface HTTP probe
- [ ] `include/bbp/modules/headless.hpp` — interface SPA analysis
- [ ] `include/bbp/modules/dirbff.hpp` — interface discovery
- [ ] `include/bbp/modules/jsintel.hpp` — interface JS intel
- [ ] `include/bbp/modules/apiparser.hpp` — interface API catalog
- [ ] `include/bbp/modules/apitester.hpp` — interface API testing
- [ ] `include/bbp/modules/mobile.hpp` — interface mobile intel
- [ ] `include/bbp/modules/changes.hpp` — interface change monitor
- [ ] `include/bbp/modules/aggregator.hpp` — interface final aggregation

## 🔧 Code C++ - Implémentations (src/)

### Core implementations
- [x] `src/core/config.cpp` — gestion config YAML (placeholder)
- [x] `src/core/csv.cpp` — parser/writer CSV (placeholder)
- [x] `src/core/logger.cpp` — système logs NDJSON (placeholder)
- [x] `src/core/rate_limit.cpp` — token bucket (placeholder)
- [x] `src/core/scope.cpp` — validation scope (placeholder)
- [x] `src/core/http.cpp` — client HTTP/cURL (placeholder)
- [x] `src/core/tls.cpp` — analyse certificats (placeholder)
- [x] `src/core/cache.cpp` — système cache (placeholder)
- [x] `src/core/fingerprints.cpp` — détection techno (placeholder)
- [x] `src/core/soft404.cpp` — heuristiques (placeholder)
- [x] `src/core/util.cpp` — utilitaires généraux (placeholder)

### Modules & binaires
- [x] `src/bbpctl/main.cpp` — orchestrateur `bbpctl` (hello world)
- [ ] `src/subhunter/main.cpp` — CLI pour `bbp-subhunter`
- [ ] `src/subhunter/subhunter.cpp` — logique subdomain enum
- [ ] `src/httpxpp/main.cpp` — CLI pour `bbp-httpxpp`
- [ ] `src/httpxpp/httpxpp.cpp` — logique HTTP probe
- [ ] `src/headless/main.cpp` — CLI pour `bbp-headless`
- [ ] `src/headless/headless.cpp` — logique SPA analysis
- [ ] `src/dirbff/main.cpp` — CLI pour `bbp-dirbff`
- [ ] `src/dirbff/dirbff.cpp` — logique discovery/bruteforce
- [ ] `src/jsintel/main.cpp` — CLI pour `bbp-jsintel`
- [ ] `src/jsintel/jsintel.cpp` — logique JS intelligence
- [ ] `src/apiparser/main.cpp` — CLI pour `bbp-apiparser`
- [ ] `src/apiparser/apiparser.cpp` — logique API catalog
- [ ] `src/apitester/main.cpp` — CLI pour `bbp-apitester`
- [ ] `src/apitester/apitester.cpp` — logique API testing
- [ ] `src/mobile/main.cpp` — CLI pour `bbp-mobile`
- [ ] `src/mobile/mobile.cpp` — logique mobile intel
- [ ] `src/changes/main.cpp` — CLI pour `bbp-changes`
- [ ] `src/changes/changes.cpp` — logique change monitoring
- [ ] `src/aggregator/main.cpp` — CLI pour `bbp-aggregator`
- [ ] `src/aggregator/aggregator.cpp` — logique final ranking

## 🧪 Tests & QA

### Environment de test
- [ ] `tests/docker-compose.yml` — environnement d'intégration synthétique
- [ ] `tests/web/Dockerfile` — serveur web de test
- [ ] `tests/web/nginx.conf` — config nginx test
- [ ] `tests/api/Dockerfile` — API de test
- [ ] `tests/api/mock.json` — données mock API

### Tests unitaires
- [ ] Tests CSV parser/validation
- [ ] Tests soft-404 detection
- [ ] Tests TLS certificate parsing
- [ ] Tests fingerprint matching
- [ ] Tests rate limiting
- [ ] Tests scope validation

### Tests d'intégration
- [ ] Pipeline complet end-to-end
- [ ] Tests de performance (100-300 req/s)
- [ ] Golden CSV comparisons
- [ ] Tests de robustesse (crash recovery)

## 🚀 Build & Deployment

### Build system
- [x] Configuration CMake de base
- [x] Support Ninja generator
- [x] Gestion des dépendances
- [ ] Configuration des dépendances complètes (Boost, JSON, etc.)
- [ ] Support multi-platform (Linux/macOS)
- [ ] Scripts de build automatisés
- [ ] Packaging/installation

### CI/CD
- [ ] GitHub Actions setup
- [ ] Tests automatisés sur push
- [ ] Build multi-platform
- [ ] Release automation
- [ ] Docker images

## 📊 Monitoring & Observabilité

- [ ] Métriques de performance intégrées
- [ ] Dashboard de monitoring
- [ ] Alertes sur échecs
- [ ] Logs structurés avec correlation IDs
- [ ] Health checks des modules

## 🔐 Sécurité & Conformité

- [ ] Validation stricte du scope
- [ ] Rate limiting par programme
- [ ] Pas de stockage PII
- [ ] Kill-switch SIGINT
- [ ] Audit trail complet
- [ ] Documentation conformité bug bounty

## 📚 Documentation

- [ ] Documentation API des modules
- [ ] Guides d'utilisation avancés
- [ ] Exemples de configuration
- [ ] Troubleshooting guide
- [ ] Architecture decision records (ADR)

---

## 🎯 Milestones de développement

### Phase 1: Core Foundation ⏳
- [ ] Core libraries implémentées
- [ ] CSV parser/writer fonctionnel
- [ ] Logger NDJSON
- [ ] Rate limiting
- [ ] Scope validation

### Phase 2: Modules de base 🔜
- [ ] Subhunter (énumération DNS)
- [ ] HTTPxpp (probe HTTP)
- [ ] Orchestrateur bbpctl fonctionnel

### Phase 3: Discovery & Intelligence 🔜
- [ ] Dirbff (directory bruteforcing)
- [ ] JSIntel (analyse JavaScript)
- [ ] Fingerprinting avancé

### Phase 4: API & Advanced 🔜
- [ ] API Parser (OpenAPI/GraphQL)
- [ ] API Tester (tests de sécurité)
- [ ] Mobile Intel (analyse APK)

### Phase 5: Monitoring & Final 🔜
- [ ] Change monitoring
- [ ] Aggregator final
- [ ] Scoring sophistiqué
- [ ] Tests & QA complets

---

**Status global**: 🟡 **Setup initial** (CMake ✅, Hello World ✅)

**Prochaine étape**: Implémenter les modules core (CSV, config, logger)