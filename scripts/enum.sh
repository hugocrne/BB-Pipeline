#!/bin/bash
# BB-Pipeline - Script wrapper pour énumération de sous-domaines
# Merge subfinder/amass et autres outils

set -euo pipefail

DOMAIN="$1"
OUTPUT_FILE="$2"
WORDLIST="${3:-}"

echo "[INFO] Énumération de sous-domaines pour: $DOMAIN"

# Créer fichier temporaire pour merger les résultats
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

# Subfinder
echo "[INFO] Exécution de subfinder..."
if command -v subfinder &> /dev/null; then
    subfinder -d "$DOMAIN" -silent -o "$TEMP_DIR/subfinder.txt" || echo "[WARN] Subfinder failed"
else
    echo "[WARN] Subfinder non trouvé"
    touch "$TEMP_DIR/subfinder.txt"
fi

# Amass (mode passif pour éviter d'être trop agressif)
echo "[INFO] Exécution d'amass (passif)..."
if command -v amass &> /dev/null; then
    timeout 300 amass enum -passive -d "$DOMAIN" -o "$TEMP_DIR/amass.txt" || echo "[WARN] Amass timeout ou failed"
else
    echo "[WARN] Amass non trouvé"
    touch "$TEMP_DIR/amass.txt"
fi

# Bruteforce avec wordlist si fournie
if [[ -n "$WORDLIST" && -f "$WORDLIST" ]]; then
    echo "[INFO] Bruteforce DNS avec wordlist: $WORDLIST"
    if command -v puredns &> /dev/null; then
        puredns bruteforce "$WORDLIST" "$DOMAIN" -r /etc/resolv.conf --write "$TEMP_DIR/bruteforce.txt" || echo "[WARN] Puredns failed"
    elif command -v massdns &> /dev/null; then
        # Fallback vers massdns si puredns pas disponible
        sed "s/$/.${DOMAIN}/" "$WORDLIST" | massdns -r /etc/resolv.conf -t A -o S | grep -E "^[^;]" | cut -d' ' -f1 | sed 's/\.$//g' > "$TEMP_DIR/bruteforce.txt" || echo "[WARN] Massdns failed"
    else
        echo "[WARN] Ni puredns ni massdns trouvés pour le bruteforce"
        touch "$TEMP_DIR/bruteforce.txt"
    fi
else
    echo "[INFO] Pas de wordlist fournie, skip du bruteforce"
    touch "$TEMP_DIR/bruteforce.txt"
fi

# Certificate Transparency (crt.sh via curl)
echo "[INFO] Recherche Certificate Transparency..."
curl -s "https://crt.sh/?q=%25.$DOMAIN&output=json" | \
    jq -r '.[].name_value' 2>/dev/null | \
    sed 's/\*\.//g' | \
    grep -E "^[a-zA-Z0-9.-]+\.$DOMAIN$" | \
    sort -u > "$TEMP_DIR/crtsh.txt" || touch "$TEMP_DIR/crtsh.txt"

# Merger tous les résultats et dédupliquer
echo "[INFO] Fusion des résultats..."
cat "$TEMP_DIR"/*.txt | \
    grep -E "^[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$" | \
    grep "$DOMAIN" | \
    sort -u > "$OUTPUT_FILE"

# Stats
TOTAL=$(wc -l < "$OUTPUT_FILE")
echo "[INFO] Total sous-domaines trouvés: $TOTAL"
echo "[INFO] Résultats sauvés dans: $OUTPUT_FILE"

# Validation DNS rapide (optionnelle)
if command -v dig &> /dev/null && [[ $TOTAL -lt 1000 ]]; then
    echo "[INFO] Validation DNS des sous-domaines..."
    VALIDATED_FILE="${OUTPUT_FILE}.validated"
    while read -r subdomain; do
        if dig +short "$subdomain" A | grep -E '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$' &> /dev/null; then
            echo "$subdomain" >> "$VALIDATED_FILE"
        fi
    done < "$OUTPUT_FILE"
    
    if [[ -f "$VALIDATED_FILE" ]]; then
        VALIDATED_COUNT=$(wc -l < "$VALIDATED_FILE")
        echo "[INFO] Sous-domaines validés DNS: $VALIDATED_COUNT"
        mv "$VALIDATED_FILE" "$OUTPUT_FILE"
    fi
fi

echo "[INFO] Énumération terminée pour $DOMAIN"