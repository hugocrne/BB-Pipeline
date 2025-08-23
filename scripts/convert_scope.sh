#!/bin/bash
# BB-Pipeline - Convertisseur de scope HackerOne/Bugcrowd vers format BB-Pipeline
# Convertit depuis les formats standards des plateformes bug bounty

set -euo pipefail

INPUT_FILE="$1"
OUTPUT_FILE="$2"
PROGRAM_NAME="${3:-Bug Bounty Program}"

echo "[INFO] Conversion du scope depuis: $INPUT_FILE"
echo "[INFO] Format de sortie: $OUTPUT_FILE"
echo "[INFO] Nom du programme: $PROGRAM_NAME"

# Vérifier que le fichier d'entrée existe
if [[ ! -f "$INPUT_FILE" ]]; then
    echo "[ERROR] Fichier d'entrée non trouvé: $INPUT_FILE"
    exit 1
fi

# Détecter le format d'entrée en regardant l'en-tête
HEADER=$(head -1 "$INPUT_FILE")
FORMAT="unknown"

if [[ "$HEADER" == *"identifier,asset_type"* ]]; then
    FORMAT="hackerone"
    echo "[INFO] Format détecté: HackerOne/Bugcrowd"
elif [[ "$HEADER" == *"domain,subdomain"* ]]; then
    FORMAT="basic"
    echo "[INFO] Format détecté: Liste basique de domaines"
else
    echo "[WARN] Format non reconnu, tentative de conversion générique..."
fi

# Créer l'en-tête du fichier de sortie BB-Pipeline
echo "schema_ver,program,domain,notes,allow_subdomains,allow_api,allow_mobile,rate_limit_rps,headers_file,auth_file" > "$OUTPUT_FILE"

# Convertir selon le format
case "$FORMAT" in
    "hackerone")
        echo "[INFO] Conversion depuis le format HackerOne..."
        
        # Traiter chaque ligne (skip header)
        tail -n +2 "$INPUT_FILE" | while IFS=',' read -r identifier asset_type instruction eligible_bounty eligible_submission availability_req confidentiality_req integrity_req max_severity system_tags created_at updated_at; do
            # Nettoyer l'identifier (enlever les quotes si présentes)
            identifier=$(echo "$identifier" | sed 's/^"//;s/"$//')
            
            # Skip si pas une URL ou un domaine
            if [[ "$asset_type" != "URL" ]]; then
                continue
            fi
            
            # Extraire le domaine depuis l'URL/identifier
            domain=""
            if [[ "$identifier" =~ ^https?://([^/]+) ]]; then
                # URL complète
                domain="${BASH_REMATCH[1]}"
            elif [[ "$identifier" =~ ^([a-zA-Z0-9.-]+\.[a-zA-Z]{2,})$ ]]; then
                # Domaine direct
                domain="$identifier"
            else
                # Skip les formats non supportés (apps mobiles, etc.)
                continue
            fi
            
            # Déterminer les permissions basées sur le domaine
            allow_subdomains=1
            allow_api=1 
            allow_mobile=0
            rate_limit_rps=10
            
            # Ajuster selon le type de subdomain
            if [[ "$domain" == api.* ]] || [[ "$domain" == *api* ]]; then
                allow_api=1
                rate_limit_rps=5  # Plus conservatif pour les APIs
                notes="API endpoint"
            elif [[ "$domain" == admin.* ]] || [[ "$domain" == *admin* ]]; then
                allow_subdomains=0  # Pas de brute force sur admin
                rate_limit_rps=3
                notes="Admin interface"
            elif [[ "$domain" == auth.* ]] || [[ "$domain" == *auth* ]] || [[ "$domain" == login.* ]]; then
                allow_subdomains=0
                rate_limit_rps=2
                notes="Authentication system"
            elif [[ "$domain" == dev.* ]] || [[ "$domain" == *dev* ]] || [[ "$domain" == staging.* ]] || [[ "$domain" == *staging* ]]; then
                allow_subdomains=1
                rate_limit_rps=15  # Dev/staging souvent moins protégés
                notes="Development/Staging environment"
            elif [[ "$domain" == shop.* ]] || [[ "$domain" == *shop* ]] || [[ "$domain" == store.* ]]; then
                allow_api=1
                allow_mobile=1  # E-commerce souvent mobile
                rate_limit_rps=8
                notes="E-commerce platform"
            else
                notes="Main domain"
            fi
            
            # Headers et auth files selon le contexte
            headers_file=""
            auth_file=""
            
            if [[ "$allow_api" == "1" ]]; then
                headers_file="configs/h_headers.txt"
                auth_file="configs/h_auth.txt"
            fi
            
            # Écrire la ligne convertie
            echo "1,\"$PROGRAM_NAME\",\"$domain\",\"$notes\",$allow_subdomains,$allow_api,$allow_mobile,$rate_limit_rps,\"$headers_file\",\"$auth_file\"" >> "$OUTPUT_FILE"
        done
        ;;
        
    "basic")
        echo "[INFO] Conversion depuis liste basique..."
        tail -n +2 "$INPUT_FILE" | while IFS=',' read -r domain rest; do
            domain=$(echo "$domain" | sed 's/^"//;s/"$//')
            echo "1,\"$PROGRAM_NAME\",\"$domain\",\"Basic target\",1,1,0,10,\"configs/h_headers.txt\",\"configs/h_auth.txt\"" >> "$OUTPUT_FILE"
        done
        ;;
        
    *)
        echo "[INFO] Conversion générique..."
        # Essayer de détecter les domaines ligne par ligne
        while read -r line; do
            if [[ "$line" =~ ([a-zA-Z0-9.-]+\.[a-zA-Z]{2,}) ]]; then
                domain="${BASH_REMATCH[1]}"
                echo "1,\"$PROGRAM_NAME\",\"$domain\",\"Auto-detected\",1,1,0,10,\"configs/h_headers.txt\",\"configs/h_auth.txt\"" >> "$OUTPUT_FILE"
            fi
        done < "$INPUT_FILE"
        ;;
esac

# Déduplication et nettoyage
echo "[INFO] Déduplication des domaines..."
TEMP_FILE=$(mktemp)
head -1 "$OUTPUT_FILE" > "$TEMP_FILE"  # Garder l'en-tête
tail -n +2 "$OUTPUT_FILE" | sort -t',' -k3,3 -u >> "$TEMP_FILE"  # Dédupliquer par domaine
mv "$TEMP_FILE" "$OUTPUT_FILE"

# Statistiques
TOTAL_DOMAINS=$(tail -n +2 "$OUTPUT_FILE" | wc -l)
API_DOMAINS=$(tail -n +2 "$OUTPUT_FILE" | grep -c ',1,' || true)  # allow_api = 1
MOBILE_DOMAINS=$(tail -n +2 "$OUTPUT_FILE" | awk -F',' '$7==1' | wc -l)  # allow_mobile = 1

echo "[INFO] Conversion terminée!"
echo "[INFO] Total domaines: $TOTAL_DOMAINS"
echo "[INFO] Domaines avec API: $API_DOMAINS"
echo "[INFO] Domaines avec mobile: $MOBILE_DOMAINS"
echo "[INFO] Fichier généré: $OUTPUT_FILE"

# Créer un rapport de conversion
REPORT_FILE="${OUTPUT_FILE%.csv}_report.json"
cat > "$REPORT_FILE" << EOF
{
  "conversion_timestamp": "$(date -Iseconds)",
  "source_file": "$INPUT_FILE",
  "output_file": "$OUTPUT_FILE",
  "program_name": "$PROGRAM_NAME",
  "format_detected": "$FORMAT",
  "total_domains": $TOTAL_DOMAINS,
  "api_enabled_domains": $API_DOMAINS,
  "mobile_enabled_domains": $MOBILE_DOMAINS,
  "rate_limiting_stats": {
    "conservative_2rps": $(tail -n +2 "$OUTPUT_FILE" | awk -F',' '$8==2' | wc -l),
    "normal_10rps": $(tail -n +2 "$OUTPUT_FILE" | awk -F',' '$8==10' | wc -l),
    "aggressive_15rps": $(tail -n +2 "$OUTPUT_FILE" | awk -F',' '$8==15' | wc -l)
  }
}
EOF

echo "[INFO] Rapport de conversion: $REPORT_FILE"