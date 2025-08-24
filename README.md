# 🛰️ BB‑Pipeline

## 📖 Présentation

**BB‑Pipeline** est un framework **local**, modulaire et extensible pour l’automatisation de la recon et des tests “safe” en bug bounty.

**Philosophie :**

* **Entrée unique** : `data/scope.csv`
* **Étapes modulaires** (binaires C++ indépendants)
* **Sorties normalisées** : CSV versionnés par module
* **Orchestrateur** : `bbpctl`
* **Safe‑by‑default** : scope strict, respect des rate‑limits, pas d’actions destructrices
* **Zéro angle mort** : DNS/TLS, HTTP, SPA/JS, discovery, API (OpenAPI/GraphQL), mobile APK, change monitoring, agrégation & scoring

---

## 🎯 Objectifs

* Pipeline complet **CSV‑in → CSV‑out**
* Modules remplaçables/évolutifs, intégration facile d’outils Kali/GitHub
* **Contrats E/S** stables par **schémas CSV**
* Orchestrateur avec **rate‑limit**, **resume**, **dry‑run**, **kill‑switch**

---

## 📦 Architecture

### Arborescence du projet

``` sh
bb-pipeline/
├─ CMakeLists.txt
├─ README.md
├─ .gitignore
│
├─ configs/
│  ├─ defaults.yaml
│  ├─ scoring.yaml
│  ├─ h_headers.txt
│  └─ h_auth.txt
│
├─ data/
│  └─ scope.csv
│
├─ out/
│  └─ .gitkeep
│
├─ scripts/
│  ├─ enum.sh
│  ├─ download_js.sh
│  └─ gau_wayback.sh
│
├─ wordlists/
│  ├─ subs.txt
│  ├─ paths.txt
│  ├─ extensions.txt
│  └─ secrets.patterns
│
├─ schemas/
│  ├─ 01_subdomains.schema.csv
│  ├─ 02_probe.schema.csv
│  ├─ 03_headless.schema.csv
│  ├─ 04_discovery.schema.csv
│  ├─ 05_jsintel.schema.csv
│  ├─ 06_api_catalog.schema.csv
│  ├─ 07_api_findings.schema.csv
│  ├─ 09_changes.schema.csv
│  └─ 99_final_ranked.schema.csv
│
├─ include/bbp/...
├─ src/
│  ├─ core/...
│  ├─ subhunter/...
│  ├─ httpxpp/...
│  ├─ headless/...
│  ├─ dirbff/...
│  ├─ jsintel/...
│  ├─ apiparser/...
│  ├─ apitester/...
│  ├─ mobile/...
│  ├─ changes/...
│  ├─ aggregator/...
│  └─ bbpctl/...
│
└─ tests/
   ├─ docker-compose.yml
   ├─ web/...
   └─ api/...
```

---

## ✅ Checklist des fichiers

> Utilise cette liste comme suivi de dev.

### Racine

* [ ] `CMakeLists.txt` — configuration CMake
* [ ] `README.md` — doc générale
* [ ] `.gitignore` — ignores build/cache/out

### Configs

* [ ] `configs/defaults.yaml` — timeouts, threads, RPS global, UA, cache, soft‑404
* [ ] `configs/scoring.yaml` — pondérations des signaux + seuils de confiance
* [ ] `configs/h_headers.txt` — headers HTTP custom (optionnel)
* [ ] `configs/h_auth.txt` — auth templates (Bearer, Cookie…) (optionnel)

### Données & sorties

* [ ] `data/scope.csv` — entrée (scope autorisé)
* [ ] `out/.gitkeep` — placeholder (CSV résultats)

### Scripts wrappers (outils Kali/GitHub)

* [ ] `scripts/enum.sh` — subfinder/amass merge
* [ ] `scripts/download_js.sh` — fetch JS en parallèle
* [ ] `scripts/gau_wayback.sh` — URLs historiques (gau, waybackurls)

### Wordlists

* [ ] `wordlists/subs.txt` — sous‑domaines usuels
* [ ] `wordlists/paths.txt` — chemins sensibles
* [ ] `wordlists/extensions.txt` — extensions
* [ ] `wordlists/secrets.patterns` — regex clés/secrets

### Schémas CSV (contrats E/S)

* [ ] `schemas/01_subdomains.schema.csv`
* [ ] `schemas/02_probe.schema.csv`
* [ ] `schemas/03_headless.schema.csv`
* [ ] `schemas/04_discovery.schema.csv`
* [ ] `schemas/05_jsintel.schema.csv`
* [ ] `schemas/06_api_catalog.schema.csv`
* [ ] `schemas/07_api_findings.schema.csv`
* [ ] `schemas/09_changes.schema.csv`
* [ ] `schemas/99_final_ranked.schema.csv`

### Include (API interne)

* [ ] `include/bbp/version.hpp` — version & constantes
* [ ] `include/bbp/config.hpp` — accès YAML
* [ ] `include/bbp/types.hpp` — types internes
* [ ] `include/bbp/util.hpp` — helpers
* [ ] `include/bbp/csv.hpp` — I/O CSV
* [ ] `include/bbp/logger.hpp` — log minimal
* [ ] `include/bbp/rate_limit.hpp` — token bucket
* [ ] `include/bbp/scope.hpp` — parsing scope.csv
* [ ] `include/bbp/http.hpp` — HTTP client
* [ ] `include/bbp/tls.hpp` — parsing cert
* [ ] `include/bbp/cache.hpp` — ETag/Last‑Modified
* [ ] `include/bbp/fingerprints.hpp` — signatures
* [ ] `include/bbp/soft404.hpp` — heuristiques
* [ ] `include/bbp/pipeline.hpp` — interfaces orchestrateur
* [ ] `include/bbp/modules/*.hpp` — interfaces de chaque module

### Src (implémentations & binaires)

* [ ] `src/core/*` — utilitaires partagés
* [ ] `src/subhunter/{subhunter.cpp,main.cpp}` — `bbp-subhunter`
* [ ] `src/httpxpp/{httpxpp.cpp,main.cpp}`   — `bbp-httpxpp`
* [ ] `src/headless/{headless.cpp,main.cpp}` — `bbp-headless`
* [ ] `src/dirbff/{dirbff.cpp,main.cpp}`     — `bbp-dirbff`
* [ ] `src/jsintel/{jsintel.cpp,main.cpp}`   — `bbp-jsintel`
* [ ] `src/apiparser/{apiparser.cpp,main.cpp}` — `bbp-apiparser`
* [ ] `src/apitester/{apitester.cpp,main.cpp}` — `bbp-apitester`
* [ ] `src/mobile/{mobile.cpp,main.cpp}`     — `bbp-mobile`
* [ ] `src/changes/{changes.cpp,main.cpp}`   — `bbp-changes`
* [ ] `src/aggregator/{aggregator.cpp,main.cpp}` — `bbp-aggregator`
* [ ] `src/bbpctl/main.cpp` — orchestrateur `bbpctl`

### Tests / Sandbox

* [ ] `tests/docker-compose.yml` — environnement d’intégration synthétique
* [ ] `tests/web/{Dockerfile,nginx.conf}`
* [ ] `tests/api/{Dockerfile,mock.json}`

---

## ⚙️ Modules & sorties CSV

| Module            | Binaire          | Rôle                                      | Sortie                |
| ----------------- | ---------------- | ----------------------------------------- | --------------------- |
| M1 Subdomain Enum | `bbp-subhunter`  | subfinder/amass + DNS/TLS + hints         | `01_subdomains.csv`   |
| M2 HTTP Probe     | `bbp-httpxpp`    | liveness + fingerprint + WAF/CDN hints    | `02_probe.csv`        |
| M3 Headless       | `bbp-headless`   | SPA routes + XHR/fetch + SW/manifest      | `03_headless.csv`     |
| M4 Discovery      | `bbp-dirbff`     | brute paths + soft‑404 + tags “juicy”     | `04_discovery.csv`    |
| M5 JS Intel       | `bbp-jsintel`    | endpoints/keys/params depuis JS           | `05_jsintel.csv`      |
| M6 API Parser     | `bbp-apiparser`  | OpenAPI/Swagger + introspection GQL       | `06_api_catalog.csv`  |
| M7 API Tester     | `bbp-apitester`  | tests safe (IDOR, rate‑limit, verb, mass) | `07_api_findings.csv` |
| M8 Mobile Intel   | `bbp-mobile`     | APK (jadx, apktool) → endpoints/keys      | `08_mobile_intel.csv` |
| M9 Change Monitor | `bbp-changes`    | diff (title, cert, favicon, headers…)     | `09_changes.csv`      |
| M10 Aggregator    | `bbp-aggregator` | merge + scoring + ranking final           | `99_final_ranked.csv` |
| Orchestrateur     | `bbpctl`         | exécute le pipeline finement              | (coordonne tout)      |

---

## 🧾 Schémas CSV (contrats E/S)

Chaque CSV est **versionné** (`schema_ver`), et documenté dans `/schemas/`.

Exemple — `schemas/02_probe.schema.csv` :

``` csv
schema_ver,program,host,url,scheme,status,content_type,content_length,title,server,cdn,waf_hint,robots_len,favicon_hash,len_bucket,signature,score_hint
```

Tous les CSV incluent `schema_ver=1` (actuel).

---

## 🛠️ Dépendances & Build

### Dépendances (Kali/Debian)

```bash
sudo apt update && sudo apt install -y \
  cmake build-essential ninja-build pkg-config \
  libboost-all-dev libssl-dev nlohmann-json3-dev \
  libspdlog-dev libgumbo-dev jq curl \
  subfinder amass httpx nmap nuclei ffuf apktool default-jdk-headless
```

### Build (CMake)

```bash
cmake -S . -B build -G Ninja
cmake --build build -j
# binaires sous ./build/
```

---

## 🚀 Commandes d’exécution

### 1) Orchestrateur — pipeline complet

```bash
./build/bbpctl run \
  --scope data/scope.csv \
  --out out \
  --threads 200 \
  --respect-rate-limits
```

Options utiles :

* `--dry-run` : affiche le plan sans exécuter
* `--resume` : reprend un run interrompu
* `--stage {subhunter|probe|headless|...}` : exécute une étape précise
* `--config configs/defaults.yaml` : surcharge config
* `--kill-after SECONDS` : stoppe après N secondes

### 2) Module isolé

```bash
./build/bbp-subhunter --scope data/scope.csv --out out/01_subdomains.csv
./build/bbp-httpxpp   --in out/01_subdomains.csv --out out/02_probe.csv
./build/bbp-dirbff    --in out/02_probe.csv --out out/04_discovery.csv
./build/bbp-jsintel   --in out/02_probe.csv --out out/05_jsintel.csv
./build/bbp-apiparser --disc out/04_discovery.csv --js out/05_jsintel.csv --out out/06_api_catalog.csv
./build/bbp-apitester --in out/06_api_catalog.csv --auth-file configs/h_auth.txt --out out/07_api_findings.csv
./build/bbp-aggregator --probe out/02_probe.csv --disc out/04_discovery.csv --js out/05_jsintel.csv --api out/07_api_findings.csv --changes out/09_changes.csv --out out/99_final_ranked.csv
```

### 3) Paramétrage headers/auth par programme

Dans `data/scope.csv` :

``` csv
schema_ver,program,domain,notes,allow_subdomains,allow_api,allow_mobile,rate_limit_rps,headers_file,auth_file
1,Demo,example.com,main,1,1,1,2,configs/h_headers.txt,configs/h_auth.txt
```

Fichiers associés :

* `configs/h_headers.txt`
* `configs/h_auth.txt`

---

## 🔐 Principes de conformité

* Scope‑first : jamais hors domaines autorisés
* Safe‑by‑default : GET/HEAD, POST no‑op optionnel
* Logs NDJSON + `X‑BBP‑Correlation‑Id`
* Pas de PII stockée, uniquement signaux/deltas
* Kill‑switch SIGINT → arrêt propre + flush CSV
* Dry‑run disponible

---

## 🧪 Tests & Sandbox

* Unitaires : CSV parser, soft‑404, TLS parse, signatures
* Intégration Docker : web, API IDOR lite, GraphQL introspectable, openapi.json, WAF simulé, upload mock
* Golden CSVs : comparaison auto
* Perf : probe 100–300 req/s (sous RPS scope)

---

## 🗺️ Roadmap

1. Core : orchestrateur, CSV, logger, rate‑limit, cache
2. Subhunter + Probe
3. Discovery + JS Intel
4. API Parser + Tester
5. Headless + Mobile + Changes
6. Aggregator final
7. QA/Perf + sandbox docker

---

## 📌 TL;DR commandes utiles

```bash
# Build
cmake -S . -B build -G Ninja && cmake --build build -j

# Pipeline complet
./build/bbpctl run --scope data/scope.csv --out out --threads 200 --respect-rate-limits

# Dry run
./build/bbpctl run --scope data/scope.csv --out out --dry-run

# Reprise après crash
./build/bbpctl run --scope data/scope.csv --out out --resume

# Étape spécifique
./build/bbpctl run --scope data/scope.csv --out out --stage discovery
```

---

## ✅ Conclusion

BB‑Pipeline = **framework d’automatisation bug bounty local, modulaire et robuste**.

* Architecture claire, modulaire
* Zéro angle mort (DNS, HTTP, SPA, API, Mobile, Changes)
* CSV exploitables
* Conformité stricte aux règles bug bounty

👉 Développe **module par module**, teste indépendamment, puis orchestre via `bbpctl`.
