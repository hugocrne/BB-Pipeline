# Delta Compression - Documentation

## Vue d'ensemble / Overview

Le module Delta Compression de BB-Pipeline fournit des capacités avancées de compression différentielle pour les fichiers CSV. Il est conçu pour optimiser le stockage et la transmission des changements entre différentes versions de données CSV, particulièrement utile pour le monitoring de changements dans les pipelines de reconnaissance.

The Delta Compression module of BB-Pipeline provides advanced differential compression capabilities for CSV files. It is designed to optimize storage and transmission of changes between different versions of CSV data, particularly useful for change monitoring in reconnaissance pipelines.

## Fonctionnalités / Features

### 🔧 Algorithmes de Compression / Compression Algorithms

- **NONE**: Aucune compression, stockage direct
- **RLE** (Run-Length Encoding): Encodage par longueurs de plages
- **DELTA_ENCODING**: Encodage delta pour valeurs numériques
- **DICTIONARY**: Compression par dictionnaire pour données répétitives
- **LZ77**: Compression LZ77 pour données générales
- **HYBRID**: Approche hybride combinant plusieurs algorithmes

### 🎯 Modes de Détection de Changements / Change Detection Modes

- **CONTENT_HASH**: Détection basée sur les hash de contenu
- **FIELD_BY_FIELD**: Comparaison champ par champ
- **KEY_BASED**: Utilise des colonnes clés spécifiques
- **SEMANTIC**: Détection sémantique de changements
- **TIMESTAMP_BASED**: Basée sur les timestamps

### 📊 Types d'Opérations / Operation Types

- **INSERT**: Nouvelle ligne ajoutée
- **DELETE**: Ligne supprimée
- **UPDATE**: Ligne modifiée
- **MOVE**: Position de ligne changée

## Architecture

### Classes Principales / Main Classes

#### `DeltaRecord`
Représente un enregistrement de changement avec métadonnées complètes.
```cpp
struct DeltaRecord {
    DeltaOperation operation;
    size_t row_index;
    std::vector<std::string> old_values;
    std::vector<std::string> new_values;
    std::vector<size_t> changed_columns;
    std::string timestamp;
    std::string change_hash;
    std::map<std::string, std::string> metadata;
};
```

#### `DeltaConfig`
Configuration pour les paramètres de compression et détection.
```cpp
struct DeltaConfig {
    CompressionAlgorithm algorithm = CompressionAlgorithm::HYBRID;
    ChangeDetectionMode detection_mode = ChangeDetectionMode::CONTENT_HASH;
    std::vector<std::string> key_columns;
    bool case_sensitive_keys = true;
    bool enable_parallel_processing = true;
    size_t chunk_size = 10000;
    double similarity_threshold = 0.8;
    // ... autres paramètres
};
```

#### `DeltaCompressor`
Classe principale pour effectuer la compression différentielle.
```cpp
class DeltaCompressor {
public:
    explicit DeltaCompressor(const DeltaConfig& config);
    
    DeltaError compress(const std::string& old_file,
                       const std::string& new_file,
                       const std::string& delta_file);
                       
    const DeltaStatistics& getStatistics() const;
};
```

#### `DeltaDecompressor`
Classe pour appliquer les deltas et reconstruire les données.
```cpp
class DeltaDecompressor {
public:
    explicit DeltaDecompressor(const DeltaConfig& config);
    
    DeltaError decompress(const std::string& delta_file,
                         const std::string& base_file,
                         const std::string& output_file);
};
```

## Utilisation / Usage

### Compression basique / Basic Compression

```cpp
#include "csv/delta_compression.hpp"

using namespace BBP::CSV;

// Configuration
DeltaConfig config;
config.algorithm = CompressionAlgorithm::HYBRID;
config.detection_mode = ChangeDetectionMode::KEY_BASED;
config.key_columns = {"id", "timestamp"};

// Compression
DeltaCompressor compressor(config);
auto result = compressor.compress("data_v1.csv", "data_v2.csv", "delta.bin");

if (result == DeltaError::SUCCESS) {
    const auto& stats = compressor.getStatistics();
    std::cout << "Compression ratio: " << stats.getCompressionRatio() << "x" << std::endl;
    std::cout << "Changes detected: " << stats.getTotalChangesDetected() << std::endl;
}
```

### Décompression / Decompression

```cpp
// Décompression
DeltaDecompressor decompressor(config);
auto result = decompressor.decompress("delta.bin", "data_v1.csv", "reconstructed_v2.csv");

if (result == DeltaError::SUCCESS) {
    std::cout << "Successfully reconstructed data" << std::endl;
}
```

### Configuration avancée / Advanced Configuration

```cpp
DeltaConfig advanced_config;
advanced_config.algorithm = CompressionAlgorithm::HYBRID;
advanced_config.detection_mode = ChangeDetectionMode::FIELD_BY_FIELD;
advanced_config.enable_parallel_processing = true;
advanced_config.chunk_size = 5000;
advanced_config.similarity_threshold = 0.9;
advanced_config.max_memory_usage = 100 * 1024 * 1024; // 100MB
advanced_config.compress_output = true;
advanced_config.validate_integrity = true;
```

## Performance / Performance

### Optimisations / Optimizations

- **Traitement parallèle**: Utilisation de threads multiples pour le traitement des chunks
- **Mise en cache intelligente**: Cache des hash et clés pour éviter les recalculs
- **Gestion mémoire optimisée**: Pool allocator pour réduire la fragmentation
- **Compression adaptative**: Sélection automatique du meilleur algorithme

### Métriques / Metrics

La classe `DeltaStatistics` fournit des métriques détaillées:

```cpp
const auto& stats = compressor.getStatistics();

// Statistiques de base
stats.getTotalRecordsProcessed();
stats.getTotalChangesDetected();
stats.getInsertsDetected();
stats.getUpdatesDetected();
stats.getDeletesDetected();

// Performance
stats.getCompressionRatio();
stats.getProcessingTimeMs();
stats.getMemoryUsageBytes();
stats.getOriginalSize();
stats.getCompressedSize();
```

## Formats de Sortie / Output Formats

### Format Delta Binaire / Binary Delta Format

Le format delta utilise une structure en sections:

1. **Header**: Métadonnées de version, timestamps, algorithmes
2. **Records**: Enregistrements de changements compressés
3. **Index**: Index pour accès rapide (optionnel)
4. **Checksum**: Validation d'intégrité

### Exemple de Header

```
DELTA_HEADER_V1.0
SOURCE_FILE=data_v1.csv
TARGET_FILE=data_v2.csv
CREATION_TIMESTAMP=2024-08-26T12:20:30Z
ALGORITHM=HYBRID
DETECTION_MODE=KEY_BASED
KEY_COLUMNS=id,timestamp
TOTAL_CHANGES=42
COMPRESSION_RATIO=4.2
END_HEADER
```

## Cas d'Usage / Use Cases

### 1. Monitoring de Changements
Surveillance continue des modifications dans les données de reconnaissance:

```cpp
// Monitoring quotidien des subdomaines
DeltaConfig monitoring_config;
monitoring_config.detection_mode = ChangeDetectionMode::KEY_BASED;
monitoring_config.key_columns = {"subdomain", "domain"};

DeltaCompressor compressor(monitoring_config);
compressor.compress("subdomains_yesterday.csv", "subdomains_today.csv", "daily_changes.delta");
```

### 2. Sauvegarde Incrémentale
Sauvegarde efficace des grandes datasets:

```cpp
// Sauvegarde incrémentale avec compression élevée
DeltaConfig backup_config;
backup_config.algorithm = CompressionAlgorithm::LZ77;
backup_config.compress_output = true;
backup_config.min_compression_ratio = 2.0;
```

### 3. Synchronisation de Données
Synchronisation efficace entre systèmes distribués:

```cpp
// Config pour transmission réseau
DeltaConfig sync_config;
sync_config.algorithm = CompressionAlgorithm::HYBRID;
sync_config.binary_format = true;
sync_config.validate_integrity = true;
```

## Gestion d'Erreurs / Error Handling

```cpp
DeltaError result = compressor.compress(old_file, new_file, delta_file);

switch (result) {
    case DeltaError::SUCCESS:
        std::cout << "Compression successful" << std::endl;
        break;
    case DeltaError::IO_ERROR:
        std::cerr << "File I/O error" << std::endl;
        break;
    case DeltaError::INVALID_CONFIG:
        std::cerr << "Invalid configuration" << std::endl;
        break;
    case DeltaError::COMPRESSION_FAILED:
        std::cerr << "Compression algorithm failed" << std::endl;
        break;
    case DeltaError::MEMORY_ERROR:
        std::cerr << "Insufficient memory" << std::endl;
        break;
    case DeltaError::FORMAT_ERROR:
        std::cerr << "Invalid file format" << std::endl;
        break;
}
```

## Limitations / Limitations

- **Taille maximale de fichier**: Limitée par la mémoire disponible pour les gros fichiers
- **Ordre des colonnes**: Le mode FIELD_BY_FIELD suppose un ordre cohérent des colonnes
- **Types de données**: Optimisé pour données textuelles, moins efficace pour binaire
- **Dépendances**: Utilise des hash simplifiés au lieu d'OpenSSL pour compatibilité

## Extensibilité / Extensibility

Le système est conçu pour être extensible:

### Nouveaux Algorithmes
```cpp
class CustomCompressor : public DeltaCompressor {
    // Implémentation d'algorithme personnalisé
};
```

### Nouvelles Stratégies de Détection
```cpp
class SemanticChangeDetector : public ChangeDetector {
    // Détection sémantique avancée
};
```

## Tests / Testing

Le module inclut des tests complets:

```bash
# Exécution des tests
./tests/test_delta_compression

# Tests spécifiques
./tests/test_delta_compression --gtest_filter="DeltaConfigTest.*"
```

### Coverage des Tests
- Tests unitaires: Classes individuelles
- Tests d'intégration: Workflows complets
- Tests de performance: Datasets volumineux
- Tests de régression: Compatibilité versions

## Intégration avec BB-Pipeline / Integration with BB-Pipeline

Le module Delta Compression s'intègre naturellement dans le pipeline BB:

```cpp
// Dans le module de change monitoring
DeltaConfig config;
config.detection_mode = ChangeDetectionMode::KEY_BASED;
config.key_columns = {"target", "timestamp"};

DeltaCompressor compressor(config);
compressor.compress(
    "out/previous_scan.csv",
    "out/current_scan.csv",
    "out/changes.delta"
);
```

## Roadmap / Roadmap

### Version Future / Future Versions

- [ ] Support streaming pour très gros fichiers
- [ ] Compression inter-fichiers pour datasets liés
- [ ] API REST pour services distribués
- [ ] Support formats Parquet, Avro
- [ ] Machine learning pour optimisation automatique
- [ ] Chiffrement intégré des deltas
- [ ] Interface graphique pour visualisation

---

*Documentation générée pour BB-Pipeline Delta Compression Module v1.0*