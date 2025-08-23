#!/bin/bash
# BB-Pipeline - Script pour télécharger JavaScript files en parallèle
# Fetch JS files depuis des URLs découvertes

set -euo pipefail

INPUT_FILE="$1"
OUTPUT_DIR="$2"
MAX_PARALLEL="${3:-10}"

echo "[INFO] Téléchargement des fichiers JS depuis: $INPUT_FILE"
echo "[INFO] Répertoire de sortie: $OUTPUT_DIR"
echo "[INFO] Parallélisme max: $MAX_PARALLEL"

# Créer le répertoire de sortie
mkdir -p "$OUTPUT_DIR"

# Vérifier que le fichier d'entrée existe
if [[ ! -f "$INPUT_FILE" ]]; then
    echo "[ERROR] Fichier d'entrée non trouvé: $INPUT_FILE"
    exit 1
fi

# Compter le nombre total d'URLs JS
TOTAL_URLS=$(wc -l < "$INPUT_FILE")
echo "[INFO] Total URLs JS à télécharger: $TOTAL_URLS"

# Fonction pour télécharger un fichier JS
download_js() {
    local url="$1"
    local output_dir="$2"
    
    # Extraire le nom de fichier depuis l'URL
    local filename
    filename=$(basename "$url" | sed 's/[^a-zA-Z0-9._-]/_/g')
    
    # Si pas d'extension .js, l'ajouter
    if [[ ! "$filename" =~ \.js$ ]]; then
        filename="${filename}.js"
    fi
    
    # Éviter les doublons en ajoutant un hash de l'URL
    local url_hash
    url_hash=$(echo -n "$url" | md5sum | cut -d' ' -f1 | cut -c1-8)
    filename="${url_hash}_${filename}"
    
    local output_file="$output_dir/$filename"
    
    # Télécharger avec curl
    if curl -L -s --max-time 30 --max-filesize 10485760 \
        -H "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36" \
        -H "Accept: application/javascript, */*" \
        -o "$output_file" "$url" 2>/dev/null; then
        
        # Vérifier que c'est bien du JavaScript
        if file "$output_file" | grep -qE "(text|ASCII)" && \
           grep -qE "(function|var|const|let|class|\{|\})" "$output_file" 2>/dev/null; then
            echo "[SUCCESS] $url -> $filename"
        else
            # Pas du JS valide, supprimer
            rm -f "$output_file"
            echo "[SKIP] $url (pas du JavaScript valide)"
        fi
    else
        # Échec du téléchargement
        rm -f "$output_file"
        echo "[FAIL] $url (erreur de téléchargement)"
    fi
}

# Exporter la fonction pour xargs
export -f download_js

# Télécharger tous les fichiers JS en parallèle
echo "[INFO] Début des téléchargements..."
cat "$INPUT_FILE" | \
    grep -E "\.js(\?|$)" | \
    head -1000 | \
    xargs -I {} -P "$MAX_PARALLEL" bash -c 'download_js "$@"' _ {} "$OUTPUT_DIR"

# Statistiques finales
DOWNLOADED=$(find "$OUTPUT_DIR" -name "*.js" -type f | wc -l)
TOTAL_SIZE=$(du -sh "$OUTPUT_DIR" 2>/dev/null | cut -f1)

echo "[INFO] Téléchargements terminés"
echo "[INFO] Fichiers JS téléchargés: $DOWNLOADED"
echo "[INFO] Taille totale: $TOTAL_SIZE"

# Créer un index des fichiers téléchargés
INDEX_FILE="$OUTPUT_DIR/index.txt"
find "$OUTPUT_DIR" -name "*.js" -type f -exec basename {} \; | sort > "$INDEX_FILE"
echo "[INFO] Index créé: $INDEX_FILE"

# Détecter les fichiers minifiés vs sources
MINIFIED=$(grep -l -E "^[^\\n]{200,}" "$OUTPUT_DIR"/*.js 2>/dev/null | wc -l)
SOURCE_MAPS=$(find "$OUTPUT_DIR" -name "*.js" -exec grep -l "sourceMappingURL" {} \; 2>/dev/null | wc -l)

echo "[INFO] Fichiers minifiés détectés: $MINIFIED"
echo "[INFO] Fichiers avec source maps: $SOURCE_MAPS"

# Créer un rapport de téléchargement
REPORT_FILE="$OUTPUT_DIR/download_report.json"
cat > "$REPORT_FILE" << EOF
{
  "timestamp": "$(date -Iseconds)",
  "total_urls": $TOTAL_URLS,
  "downloaded_files": $DOWNLOADED,
  "total_size": "$TOTAL_SIZE",
  "minified_files": $MINIFIED,
  "source_maps": $SOURCE_MAPS,
  "output_directory": "$OUTPUT_DIR"
}
EOF

echo "[INFO] Rapport de téléchargement: $REPORT_FILE"