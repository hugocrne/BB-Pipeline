# CLAUDE.md

Always answer in french

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

BB-Pipeline is a local, modular, and extensible framework for automating reconnaissance and "safe" testing in bug bounty hunting. The project follows a defensive security approach - it's designed for ethical bug bounty research with strict scope enforcement and safe-by-default operations.

## Architecture

The framework consists of:

- **Single Entry Point**: `data/scope.csv` defines authorized targets
- **Modular Pipeline**: Independent C++ binaries for each phase
- **Standardized Output**: Versioned CSV files with strict schemas
- **Orchestrator**: `bbpctl` manages the entire pipeline
- **Safety Features**: Strict scope validation, rate limiting, non-destructive operations

### Pipeline Modules

1. **Subdomain Enumeration** (`bbp-subhunter`) → `01_subdomains.csv`
2. **HTTP Probing** (`bbp-httpxpp`) → `02_probe.csv`
3. **Headless Analysis** (`bbp-headless`) → `03_headless.csv`
4. **Discovery/Bruteforcing** (`bbp-dirbff`) → `04_discovery.csv`
5. **JavaScript Intelligence** (`bbp-jsintel`) → `05_jsintel.csv`
6. **API Catalog** (`bbp-apiparser`) → `06_api_catalog.csv`
7. **API Testing** (`bbp-apitester`) → `07_api_findings.csv`
8. **Mobile Intelligence** (`bbp-mobile`) → `08_mobile_intel.csv`
9. **Change Monitoring** (`bbp-changes`) → `09_changes.csv`
10. **Final Aggregation** (`bbp-aggregator`) → `99_final_ranked.csv`

## Build System

This project uses CMake with Ninja as the preferred generator:

```bash
# Build the entire project
cmake -S . -B build -G Ninja
cmake --build build -j

# All binaries will be available in ./build/
```

## Development Commands

### Dependencies Installation (Kali/Debian)
```bash
sudo apt update && sudo apt install -y \
  cmake build-essential ninja-build pkg-config \
  libboost-all-dev libssl-dev nlohmann-json3-dev \
  libspdlog-dev libgumbo-dev jq curl \
  subfinder amass httpx nmap nuclei ffuf apktool default-jdk-headless
```

### Running the Pipeline

**Full Pipeline:**
```bash
./build/bbpctl run \
  --scope data/scope.csv \
  --out out \
  --threads 200 \
  --respect-rate-limits
```

**Development Options:**
- `--dry-run`: Show execution plan without running
- `--resume`: Resume interrupted pipeline
- `--stage {module_name}`: Run specific module only
- `--config configs/defaults.yaml`: Use custom configuration
- `--kill-after SECONDS`: Auto-stop after timeout

**Individual Module Testing:**
```bash
./build/bbp-subhunter --scope data/scope.csv --out out/01_subdomains.csv
./build/bbp-httpxpp --in out/01_subdomains.csv --out out/02_probe.csv
./build/bbp-dirbff --in out/02_probe.csv --out out/04_discovery.csv
```

## Project Structure

- **`configs/`**: YAML configurations, HTTP headers, auth templates
- **`data/`**: Input scope definitions
- **`out/`**: Pipeline output CSV files
- **`scripts/`**: Wrapper scripts for external tools (subfinder, amass, etc.)
- **`wordlists/`**: Subdomain, path, extension, and secret pattern lists
- **`schemas/`**: CSV schema definitions for each pipeline stage
- **`include/bbp/`**: C++ header files with core functionality
- **`src/`**: Implementation files organized by module
- **`tests/`**: Docker-based integration test environment

## CSV Schema System

All pipeline stages use versioned CSV schemas with strict contracts:
- Each CSV includes `schema_ver=1` (current version)
- Schema definitions in `/schemas/` directory
- Standardized fields across modules for data flow consistency

## Safety and Compliance Features

- **Scope-first**: Never operates outside authorized domains
- **Safe-by-default**: Uses GET/HEAD requests, optional non-destructive POST
- **Rate limiting**: Respects per-program rate limits
- **Structured logging**: NDJSON format with correlation IDs
- **Kill-switch**: Clean shutdown on SIGINT with CSV flush
- **No PII storage**: Only signals and deltas are recorded

## Testing

The project includes:
- Unit tests for CSV parsing, soft-404 detection, TLS parsing, fingerprinting
- Docker-based integration environment (`tests/docker-compose.yml`)
- Performance benchmarks (targeting 100-300 req/s within scope limits)
- Golden CSV comparisons for regression testing

## Quick Reference

```bash
# Complete rebuild
cmake -S . -B build -G Ninja && cmake --build build -j

# Test pipeline with dry-run
./build/bbpctl run --scope data/scope.csv --out out --dry-run

# Resume interrupted scan
./build/bbpctl run --scope data/scope.csv --out out --resume

# Run specific stage
./build/bbpctl run --scope data/scope.csv --out out --stage discovery
```

Note: This is a security research framework designed for ethical bug bounty programs. All operations must respect program scope and terms of service.