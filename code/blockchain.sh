#!/usr/bin/env bash
set -euo pipefail

# Codici di errore 
readonly E_CHAIN_MISMATCH=1
readonly E_INVALID_TRANSACTION=2
readonly E_INVALID_BLOCK=4
readonly E_INVALID_MERKLE=10
readonly E_BLOCK_TOO_FAR=11
readonly E_CSV_ERROR=12

# Calcola sha256 del Contenuto Letterale della stringa 
sha256_hex(){
    printf '%s' "$1" | sha256sum | awk '{print $1}'
    }

#Divide $1 sul delimitare lettereale $2 (es. "::") 
#riempie l'array del caller passato per nome $3

split_by_delim() {
    local input="$1"
    local delim="$2"
    local -n result_ref="$3"
    result_ref=()
    local remaining="$input"
    while [[ "$remaining" == *"$delim"* ]]; do
        result_ref+=("${remaining%%"$delim"*}")
        remaining="${remaining#*"$delim"}"
    done
    result_ref+=("$remaining")
}

# Algoritmo Merkle come da specifiche: accoppia gli hash, se dispari pad con
# sha256(""), ripete finché resta un solo hash
#Stesso comportamento del calcMerkle/blockGetmerkle in block.c (anche per il caso count==1)
calc_merkle() {
    local -a hashes=("$@")
    local first_round=1
    while (( ${#hashes[@]} > 1 || first_round == 1 )); do
        first_round=0
        if (( ${#hashes[@]} % 2 != 0 )); then
            hashes+=( "$(sha256_hex "")" )
        fi
        local -a next=()
        for ((i=0; i<${#hashes[@]}; i+=2)); do
            next+=( "$(sha256_hex "${hashes[i]}${hashes[i+1]}")" )
        done
        hashes=("${next[@]}")
    done
    echo "${hashes[0]}"
}


cmd_merkle() {
    local tx_string="$1"
    local -a tx_array
    split_by_delim "$tx_string" "::" tx_array

    local -a tx_hashes=()
    for tx in "${tx_array[@]}"; do
        tx_hashes+=( "$(sha256_hex "$tx")" )
    done

    calc_merkle "${tx_hashes[@]}"
}

# index(16) + timestamp(16) + prev_hash(64) + merkle_root(64) + nonce(16) = 176
# Sono gli stessi campi e lo stesso ordine di BLOCK_TO_HEX_BE in utils.h
# Le transazioni non entrano nell'hash del blocco (solo nel merkle root)
readonly HEX_FIELDS_LEN=176

cmd_hash() {
    local block_hex="$1"

    if [[ ! "$block_hex" =~ ^[0-9a-fA-F]+$ ]]; then
        echo "Errore: input non è una stringa hex valida" >&2
        return 1
    fi

    if (( ${#block_hex} < HEX_FIELDS_LEN )); then
        echo "Errore: input troppo corto per contenere i campi fissi del blocco" >&2
        return 1
    fi

    local fixed_fields="${block_hex:0:HEX_FIELDS_LEN}"
    sha256_hex "$fixed_fields"
}

TX_REGEX='^[A-Za-z0-9]+ pays [A-Za-z0-9]+ [1-9][0-9]* coins$'

cmd_verify() {
    local csv_path="$1"

    if [[ ! -f "$csv_path" || ! -r "$csv_path" ]]; then
        echo "Errore: impossibile leggere il file: $csv_path" >&2
        return "$E_CSV_ERROR"
    fi

    local -a lines=()
    mapfile -t lines < "$csv_path"

    if (( ${#lines[@]} > 0 )) && [[ "${lines[0]}" == "index,timestamp,prev_hash,merkle_root,nonce,transactions" ]]; then
        lines=("${lines[@]:1}")
    fi

    if (( ${#lines[@]} == 0 )); then
        echo "Catena vuota: nessun blocco da verificare"
        return 0
    fi

    local errors=0
    local prev_hash="" prev_index=0
    local block_num=0

    for line in "${lines[@]}"; do
        [[ -z "$line" ]] && continue

        local idx_hex ts_hex ph_hex mr_hex nonce_hex tx_field
        IFS=',' read -r idx_hex ts_hex ph_hex mr_hex nonce_hex tx_field <<< "$line"

        if [[ -z "$idx_hex" || -z "$ts_hex" || -z "$ph_hex" || -z "$mr_hex" || -z "$nonce_hex" ]] \
           || [[ ! "$idx_hex$ts_hex$ph_hex$mr_hex$nonce_hex" =~ ^[0-9a-fA-F]+$ ]]; then
            echo "Blocco #$block_num: INVALID_BLOCK (riga malformata)"
            errors=$((errors+1))
            block_num=$((block_num+1))
            continue
        fi

        local idx=$((16#$idx_hex))

        if (( block_num == 0 )); then
            if (( idx != 0 )); then
                echo "Blocco #$block_num: INVALID_BLOCK (genesis deve avere index 0, trovato $idx)"
                errors=$((errors+1))
            fi
        else
            if (( idx > prev_index + 1 )); then
                echo "Blocco #$block_num: BLOCK_TOO_FAR (index=$idx, atteso=$((prev_index+1)))"
                errors=$((errors+1))
            elif (( idx != prev_index + 1 )) || [[ "$ph_hex" != "$prev_hash" ]]; then
                echo "Blocco #$block_num: CHAIN_MISMATCH (index=$idx atteso=$((prev_index+1)), prev_hash=$ph_hex atteso=$prev_hash)"
                errors=$((errors+1))
            fi
        fi

        local computed_merkle
        computed_merkle=$(cmd_merkle "$tx_field")
        if [[ "$computed_merkle" != "$mr_hex" ]]; then
            echo "Blocco #$block_num: INVALID_MERKLE (calcolato=$computed_merkle, dichiarato=$mr_hex)"
            errors=$((errors+1))
        fi

        if (( block_num > 0 )); then
            local -a tx_array
            split_by_delim "$tx_field" "::" tx_array
            for tx in "${tx_array[@]}"; do
                if [[ ! "$tx" =~ $TX_REGEX ]]; then
                    echo "Blocco #$block_num: INVALID_TRANSACTION ('$tx')"
                    errors=$((errors+1))
                fi
            done
        fi

        prev_hash=$(sha256_hex "${idx_hex}${ts_hex}${ph_hex}${mr_hex}${nonce_hex}")
        prev_index=$idx
        block_num=$((block_num+1))
    done

    if (( errors > 0 )); then
        echo "Verifica fallita: $errors errore/i su $block_num blocchi"
        return "$E_INVALID_BLOCK"
    fi

    echo "Catena valida: $block_num blocchi"
    return 0
}


case "${1:-}" in
    --merkle)
        cmd_merkle "$2"
        ;;
    --hash)
        cmd_hash "$2"
        ;;
        --verify)
        cmd_verify "$2"
        ;;
    *)
        echo "Utilizzo: $0 --verify <state.csv> | --hash <block_hex> | --merkle <transactions>" >&2
        exit 1
        ;;
esac