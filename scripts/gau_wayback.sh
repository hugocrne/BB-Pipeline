#!/bin/bash
# BB-Pipeline - Script pour récupérer URLs historiques
# Utilise gau, waybackurls et autres sources d'URLs

set -euo pipefail

DOMAIN="$1"
OUTPUT_FILE="$2"
MAX_URLS="${3:-10000}"

echo "[INFO] Récupération des URLs historiques pour: $DOMAIN"
echo "[INFO] Limite d'URLs: $MAX_URLS"

# Créer fichier temporaire pour merger les résultats
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# gau (Get All URLs)
echo "[INFO] Exécution de gau..."
if command -v gau &> /dev/null; then
    timeout 300 gau --threads 5 --timeout 30 "$DOMAIN" > "$TEMP_DIR/gau.txt" 2>/dev/null || echo "[WARN] gau timeout ou failed"
else
    echo "[WARN] gau non trouvé"
    touch "$TEMP_DIR/gau.txt"
fi

# waybackurls
echo "[INFO] Exécution de waybackurls..."
if command -v waybackurls &> /dev/null; then
    timeout 300 waybackurls "$DOMAIN" > "$TEMP_DIR/waybackurls.txt" 2>/dev/null || echo "[WARN] waybackurls timeout ou failed"
else
    echo "[WARN] waybackurls non trouvé"
    touch "$TEMP_DIR/waybackurls.txt"
fi

# Fallback: Wayback Machine via curl si outils pas disponibles
if [[ ! -s "$TEMP_DIR/gau.txt" && ! -s "$TEMP_DIR/waybackurls.txt" ]]; then
    echo "[INFO] Fallback: Wayback Machine API via curl..."
    curl -s "http://web.archive.org/cdx/search/cdx?url=*.$DOMAIN/*&output=text&fl=original&collapse=urlkey&limit=$MAX_URLS" | \
        sort -u > "$TEMP_DIR/wayback_api.txt" || touch "$TEMP_DIR/wayback_api.txt"
else
    touch "$TEMP_DIR/wayback_api.txt"
fi

# CommonCrawl (optionnel, plus lent)
if command -v curl &> /dev/null; then
    echo "[INFO] Recherche CommonCrawl (échantillon)..."
    curl -s "https://index.commoncrawl.org/CC-MAIN-2023-50-index?url=*.$DOMAIN/*&output=json&limit=1000" 2>/dev/null | \
        jq -r '.url' 2>/dev/null | \
        head -1000 > "$TEMP_DIR/commoncrawl.txt" || touch "$TEMP_DIR/commoncrawl.txt"
else
    touch "$TEMP_DIR/commoncrawl.txt"
fi

# Merger et nettoyer les résultats
echo "[INFO] Fusion et nettoyage des URLs..."
cat "$TEMP_DIR"/*.txt | \
    grep -E "^https?://" | \
    grep -i "$DOMAIN" | \
    grep -v -E "\.(png|jpg|jpeg|gif|ico|css|woff|woff2|ttf|svg|mp4|mp3|avi|zip|tar|gz|pdf|doc|docx)(\?|$)" | \
    sort -u | \
    head -"$MAX_URLS" > "$OUTPUT_FILE.tmp"

# Post-processing: catégoriser les URLs
echo "[INFO] Catégorisation des URLs..."

# APIs potentielles
grep -iE "(api|v[0-9]|rest|graphql|json|xml)" "$OUTPUT_FILE.tmp" > "$OUTPUT_FILE.api" || touch "$OUTPUT_FILE.api"

# Pages d'admin/sensitive
grep -iE "(admin|dashboard|panel|login|auth|config|setup|install|debug|test)" "$OUTPUT_FILE.tmp" > "$OUTPUT_FILE.sensitive" || touch "$OUTPUT_FILE.sensitive"

# JavaScript files
grep -E "\.js(\?|$)" "$OUTPUT_FILE.tmp" > "$OUTPUT_FILE.js" || touch "$OUTPUT_FILE.js"

# Paramètres intéressants
grep -E "\?.*=" "$OUTPUT_FILE.tmp" | \
    grep -iE "(id|user|token|key|secret|password|admin|debug|test|dev)" > "$OUTPUT_FILE.params" || touch "$OUTPUT_FILE.params"

# Endpoints avec extensions potentiellement intéressantes
grep -E "\.(php|asp|aspx|jsp|do|action|cgi|pl)(\?|$)" "$OUTPUT_FILE.tmp" > "$OUTPUT_FILE.dynamic" || touch "$OUTPUT_FILE.dynamic"

# URLs principales (toutes)
mv "$OUTPUT_FILE.tmp" "$OUTPUT_FILE"

# Statistiques
TOTAL=$(wc -l < "$OUTPUT_FILE")
API_COUNT=$(wc -l < "$OUTPUT_FILE.api")
SENSITIVE_COUNT=$(wc -l < "$OUTPUT_FILE.sensitive")
JS_COUNT=$(wc -l < "$OUTPUT_FILE.js")
PARAMS_COUNT=$(wc -l < "$OUTPUT_FILE.params")
DYNAMIC_COUNT=$(wc -l < "$OUTPUT_FILE.dynamic")

echo "[INFO] Récupération terminée pour $DOMAIN"
echo "[INFO] Total URLs: $TOTAL"
echo "[INFO] APIs potentielles: $API_COUNT"
echo "[INFO] Pages sensibles: $SENSITIVE_COUNT"
echo "[INFO] Fichiers JavaScript: $JS_COUNT"
echo "[INFO] URLs avec paramètres: $PARAMS_COUNT"
echo "[INFO] Pages dynamiques: $DYNAMIC_COUNT"

# Créer un rapport JSON
REPORT_FILE="${OUTPUT_FILE%.txt}_report.json"
cat > "$REPORT_FILE" << EOF
{
  "domain": "$DOMAIN",
  "timestamp": "$(date -Iseconds)",
  "total_urls": $TOTAL,
  "categories": {
    "api": $API_COUNT,
    "sensitive": $SENSITIVE_COUNT,
    "javascript": $JS_COUNT,
    "with_params": $PARAMS_COUNT,
    "dynamic": $DYNAMIC_COUNT
  },
  "files": {
    "all_urls": "$OUTPUT_FILE",
    "api_urls": "$OUTPUT_FILE.api",
    "sensitive_urls": "$OUTPUT_FILE.sensitive",
    "js_urls": "$OUTPUT_FILE.js",
    "param_urls": "$OUTPUT_FILE.params",
    "dynamic_urls": "$OUTPUT_FILE.dynamic"
  }
}
EOF

echo "[INFO] Rapport JSON: $REPORT_FILE"