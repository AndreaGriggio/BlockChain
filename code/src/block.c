#include "block.h"
#include "error.h"
#include "inttypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "utils.h"

typedef struct Block{
    u_int64_t index;
    u_int64_t timestamp;
    char prev_hash[HASH_HEX_SIZE+1];
    char merkle_root[MERKLE_ROOT_HEX_SIZE+1];
    u_int64_t nonce;
    char transactions[MAX_BLOCK_TXS_BUF+1];
}Block;
/**
 * Crea zona di memoria per il blocco
 * @return Ritorna puntatore a zona di memoria del nuovo blocco
 */
Block* blockCreate() {
    return malloc(sizeof(Block));
}

/**
 *Inizializza un blocco in caso di errore ritorna INVALID_BLOCK
 * @param block_ptr Puntatore alla zona di memoria del blocco
 * @param index Indice del blocco presunto corretto
 * @param timestamp Tempo al quale è stato minato il blocco
 * @param prev_hash hashcode blocco precedente
 * @param nonce
 * @param txs
 * @return
 */
int blockInit(Block *block_ptr,const u_int64_t index, const u_int64_t timestamp, const char prev_hash[HASH_HEX_SIZE+1], const u_int64_t nonce,const TxList *txs){
    //chiamate a tutte le altre funzioni per ottenere i campi necessari
    if (block_ptr == NULL) return INVALID_BLOCK;

    block_ptr -> index = index;
    block_ptr -> timestamp = timestamp;
    block_ptr -> nonce = nonce;

    // Genesis: prev_hash NULL => tutti zeri (nessun blocco precedente)
    if (prev_hash == NULL) {
        memset(block_ptr->prev_hash, '0', HASH_HEX_SIZE);
        block_ptr->prev_hash[HASH_HEX_SIZE] = '\0';
    } else if (strlen(prev_hash) != HASH_HEX_SIZE) {
        return INVALID_BLOCK;
    } else {
        strcpy(block_ptr->prev_hash, prev_hash);
    }

    if (pack_transactions(block_ptr, txs) != 0)                 return INVALID_BLOCK;
    if (blockGetmerkle(block_ptr, block_ptr->merkle_root) != 0) return INVALID_BLOCK;

    return 0;
}
int blockGetmerkle(
    const Block* block_ptr,
    char output_merkle[MERKLE_ROOT_HEX_SIZE+1]
) {
    if (block_ptr == NULL || 
        block_ptr->transactions[0] == '\0' )  {
        return INVALID_PARAMS;
    }
        
    TxList list;

    int result = unpack_transactions(block_ptr, &list);
    if(result != 0 ){
        return result;
    }   

    /*
    evita di creare un VLA di dimensione zero nel caso in cui
    il parsing non abbia trovato alcuna transazione valida
    */
    if (list.count == 0){
        return INVALID_PARAMS;
    }

    /*
    Abbiamo bisogno che un numero dispari di hash (qui sempre 1)
    venga completato con sha256("") prima di essre combinato:
    calcMerkle si aspetta sempre una lista già pronta da accoppiare,
    quindi procediamo con 2 elementi
    */

   if (list.count == 1){
    char pair_hashes [2] [MERKLE_ROOT_HEX_SIZE+1];

        sha256_of_string(
            (const unsigned char*) list.strings[0],
            strlen(list.strings[0]),
            pair_hashes[0]
        );

        sha256_of_string(
            (const unsigned char*) "",
             0, 
             pair_hashes[1]
        );
    
        return calcMerkle(
            pair_hashes,
            2,
            output_merkle
        );
   }

    char transactionHashes[list.count][MERKLE_ROOT_HEX_SIZE+1];


    for (size_t i = 0; i < list.count ; i++) {
        sha256_of_string(
            (const unsigned char*) list.strings[i],
            strlen(list.strings[i]),
            transactionHashes[i]
        );
    }

    return calcMerkle(
        transactionHashes,
        list.count,
        output_merkle);
}

int blockGetMerkleRoot(const Block *block_ptr, char output[MERKLE_ROOT_HEX_SIZE + 1]) {
    if (block_ptr == NULL || output == NULL) return INVALID_PARAMS;
    strncpy(output, block_ptr->merkle_root, MERKLE_ROOT_HEX_SIZE);
    output[MERKLE_ROOT_HEX_SIZE] = '\0';
    return 0;
}

int blockGetIndex(const Block *block_ptr, uint64_t *index) {
    if (block_ptr == NULL || index == NULL) return INVALID_PARAMS;
    *index = block_ptr->index;
    return 0;
}


int calcMerkle(char hashes[][HASH_HEX_SIZE+1],const size_t count,char output_merkle[HASH_HEX_SIZE+1]){

    if (hashes == NULL || output_merkle == NULL || count == 0 || count > MAX_TX_PER_BLOCK) return INVALID_PARAMS;

    //caso base ne rimane solo uno copio dentro all'output l'hash finale
    if (count == 1 ) {
        strncpy(output_merkle,
                hashes[0],
                HASH_HEX_SIZE);
        output_merkle[HASH_HEX_SIZE] = '\0';
        return 0;
    }

    //caso non base
    //Dobbiamo copiare gli hashcodes
    //se count pari il numero di oggetti da copiare è uguale
    //se count dispari il numero di oggetti da copiare è count + 1
    const size_t normalized_count = count + count % 2;

    char normalized[normalized_count][HASH_HEX_SIZE+1];
    //Ciclo per copiare gli Hashcode
    for (size_t i = 0; i < count; i++) {
        strncpy(normalized[i],
                hashes[i],
                HASH_HEX_SIZE);

        normalized[i][HASH_HEX_SIZE] = '\0';
    }
    //Sezione per aggiungere all'array copiato di  di hashcodes
    //L'hashcode di una stringa vuota

    if (count % 2 != 0) {
        sha256_of_string(
            (const unsigned char *)"",
            0,
            normalized[count]
        );
    }
    //calcolo nuovo numero di elementi
    const size_t next_count = normalized_count / 2;
    char next_level[next_count][HASH_HEX_SIZE+1];

    //ciclo per il calcolo delle coppie
    // i serve per prendere due coppie alla volta
    // j serve per contare le iterazioni e decidere dove salvare nel nuovo array degli hashes
    for (size_t i = 0, j = 0; i < normalized_count; i += 2, j++) {
        char pair[HASH_HEX_SIZE  * 2 + 1];
        //le coppie vengono prese e concatenate in pair
        snprintf(pair,
                 sizeof(pair),
                 "%s%s",
                 normalized[i],
                 normalized[i + 1]);

        sha256_of_string(
            (const unsigned char *)pair,
            strlen(pair),
            next_level[j]
        );
    }

    return calcMerkle(next_level, next_count, output_merkle);
}


int blockGetHash(const Block *block_ptr, char out_hash[HASH_HEX_SIZE + 1]) {
    if (block_ptr == NULL || out_hash == NULL) return INVALID_HASH;

    const size_t size = UINT64_TO_CHAR_SIZE*3 + MERKLE_ROOT_HEX_SIZE + HASH_HEX_SIZE+1;//dimensione della stringa di hex

    char input_sha256 [size];

    const int res = BLOCK_TO_HEX_BE(block_ptr->index, //risultato è la lunghezza della stringa concatenata
                    block_ptr->timestamp,
                    block_ptr->prev_hash,
                    block_ptr->merkle_root,
                    block_ptr->nonce,
                    input_sha256,
                    size);

    if (res<0 || (size_t)res >= size ){return INVALID_HASH;}

    sha256_of_string(
        (const unsigned char *)input_sha256
        ,strlen(input_sha256)
        ,out_hash);

    return 0;


}
int blockGetTransaction(const Block * block_ptr, char out[MAX_BLOCK_TXS_BUF+1]) {
    if (block_ptr == NULL || out == NULL)return INVALID_PARAMS;

    memcpy(out,block_ptr->transactions,sizeof(block_ptr->transactions));

    return 0;
}
int blockValidate(const Block *block_ptr, const Block *prev) {
    if (block_ptr == NULL || prev == NULL ) return INVALID_BLOCK;

    char prev_hash[HASH_HEX_SIZE+1];
    if (blockGetHash(prev, prev_hash) != 0) return INVALID_BLOCK;

    if (block_ptr->index == prev->index+1 && strcmp(prev_hash, block_ptr->prev_hash) == 0) {return 0;}
    return INVALID_BLOCK;
}


int blockToCsv(const Block *block_ptr, char *buffer,const size_t size) {
    if (size == 0 || block_ptr == NULL || buffer == NULL ) return INVALID_BLOCK;
    const int written = snprintf(buffer,size,
                            "%016" PRIx64 "," "%016" PRIx64 "," "%s,%s," "%016" PRIx64 ",%s",
                            block_ptr->index,
                            block_ptr->timestamp,
                            block_ptr->prev_hash,
                            block_ptr->merkle_root,
                            block_ptr->nonce,
                            block_ptr->transactions
                            );
    if (written < 0 || (size_t)written >= size) return BUFFER_TOO_SMALL;

    return 0;
}


int blockFromCsv(Block *block_ptr, const char *line) {
    if (block_ptr == NULL || line == NULL) return INVALID_BLOCK;

    char buffer[BLOCK_CSV_LINE_SIZE];

    strncpy(buffer, line, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    // Rimuovo eventuale newline finale
    buffer[strcspn(buffer, "\n")] = '\0';

    char *saveptr = NULL;

    const char *index_str        = strtok_r(buffer, ",", &saveptr);
    const char *timestamp_str    = strtok_r(NULL  , ",", &saveptr);
    const char *prev_hash_str    = strtok_r(NULL  , ",", &saveptr);
    const char *merkle_root_str  = strtok_r(NULL  , ",", &saveptr);
    const char *nonce_str        = strtok_r(NULL  , ",", &saveptr);
    const char *transactions_str = strtok_r(NULL  , "" , &saveptr);

    // Controllo sui campi :
    // Nessun blocco precedentemente caricato con formato CSV DEVE avere tutti  i campi compilati
    // Se uno dei campi è NULL, significa che il CSV era malformato

    if (index_str        == NULL ||
        timestamp_str    == NULL ||
        prev_hash_str    == NULL ||
        merkle_root_str  == NULL ||
        nonce_str        == NULL ||
        transactions_str == NULL   ) {
        return INVALID_BLOCK;
        }

    if (parse_uint64_hex(index_str, &block_ptr->index) != 0) {
        return INVALID_BLOCK;
    }

    if (parse_uint64_hex(timestamp_str, &block_ptr->timestamp) != 0) {
        return INVALID_BLOCK;
    }

    if (parse_uint64_hex(nonce_str, &block_ptr->nonce) != 0) {
        return INVALID_BLOCK;
    }

    strncpy(block_ptr->prev_hash, prev_hash_str, HASH_HEX_SIZE);
    block_ptr->prev_hash[HASH_HEX_SIZE] = '\0';

    strncpy(block_ptr->merkle_root, merkle_root_str, HASH_HEX_SIZE);
    block_ptr->merkle_root[HASH_HEX_SIZE] = '\0';

    strncpy(block_ptr->transactions, transactions_str, MAX_BLOCK_TXS_BUF - 1);
    block_ptr->transactions[MAX_BLOCK_TXS_BUF - 1] = '\0';

    return 0;
}


int blockDestroy(Block* block_ptr) {
    if (block_ptr == NULL)return INVALID_PARAMS;
    free(block_ptr);
    return 0;
}

int blockAddTransaction(Block *block_ptr, const char transaction[MAX_TX_SIZE + 1]) {
    if (block_ptr == NULL || transaction == NULL) return INVALID_PARAMS;

    const char* separator = "::";

    const size_t sepLen = strlen(separator);
    const size_t currentSize = strlen(block_ptr->transactions);
    const size_t neededSpace = strlen(transaction);



    if ( neededSpace + sepLen + currentSize> MAX_BLOCK_TXS_BUF)return BUFFER_TOO_SMALL;
    const int written = snprintf(
                                 block_ptr->transactions+currentSize,
                                 MAX_BLOCK_TXS_BUF-currentSize,
                                 "%s%s",
                                 separator,
                                 transaction);
    if (written < 0 || (size_t)written >= MAX_BLOCK_TXS_BUF-currentSize) return BUFFER_TOO_SMALL;
    return 0;
}

int blockKillTransactions(Block *block_ptr) {
    if (block_ptr == NULL ) return INVALID_PARAMS;
    block_ptr->transactions[0] = '\0';
    block_ptr->merkle_root[0] = '\0';
    return 0;
}

int blockCopy(Block* dst, const Block* src) {
    if ( dst == NULL || src == NULL ) return INVALID_PARAMS;

    dst->index = src->index;
    dst->timestamp = src->timestamp;
    dst->nonce = src->nonce;

    memcpy(dst->merkle_root, src->merkle_root, MERKLE_ROOT_HEX_SIZE + 1);
    memcpy(dst->prev_hash, src->prev_hash, HASH_HEX_SIZE +1 );
    memcpy(dst->transactions, src->transactions, MAX_BLOCK_TXS_BUF +1 );

    return 0;
}


//funzione per impacchettare le transazioni
int pack_transactions(Block *b, const TxList *list) {
    memset(b->transactions, 0, MAX_BLOCK_TXS_BUF);      //azzero transactions nel blocco

    if (list->count == 0)  return 0;

    //copia la prima transazione
    strncpy(b->transactions, list->strings[0], MAX_BLOCK_TXS_BUF - 1);

    for (size_t i = 1; i < list->count; i++) {
        //verifico spazio libero nel buffer
        size_t current_len = strlen(b->transactions);
        size_t needed_space = strlen(list->strings[i]) + 2;   // +2 per i ::

        if (current_len + needed_space >= MAX_BLOCK_TXS_BUF) {
            return -1;
        }

        strcat(b->transactions, "::");
        strcat(b->transactions, list->strings[i]);
    }
    return 0;
}

//funzione per de-impacchettare le transazioni
int unpack_transactions(const Block *b, TxList *list) {
    list->count = 0;
    memset(list->strings, 0, sizeof(list->strings));

    if (b == NULL || strlen(b->transactions) == 0) return 0;

    char token_buf[MAX_BLOCK_TXS_BUF];
    strncpy(token_buf, b->transactions, MAX_BLOCK_TXS_BUF - 1);
    token_buf[MAX_BLOCK_TXS_BUF - 1] = '\0';

    char *ptr = token_buf;
    char *found;

    while ((found = strstr(ptr, "::")) != NULL) {
        size_t len = (size_t)(found - ptr);
        if (len > 0) {
            if (list->count >= MAX_TX_PER_BLOCK) return -1;
            strncpy(list->strings[list->count], ptr, len);
            list->strings[list->count][len] = '\0';
            list->count++;
        }
        ptr = found + 2;
    }

    // ultima transazione dopo l'ultimo ::
    if (strlen(ptr) > 0) {
        if (list->count >= MAX_TX_PER_BLOCK) return -1;
        strncpy(list->strings[list->count], ptr, MAX_TX_SIZE - 1);
        list->strings[list->count][MAX_TX_SIZE - 1] = '\0';
        list->count++;
    }

    return 0;
}
