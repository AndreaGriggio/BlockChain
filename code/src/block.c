#include "block.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


Block* blockCreate() {
    return malloc(sizeof(Block));
}
int blockInit(Block *block_ptr,const u_int64_t timestamp, const Block *prev, const TxList *txs) {
    //chiamate a tutte le altre funzioni per ottenere i campi necessari
    return 0;
}

int blockGetHash(const Block *block_ptr, char out_hash[65]) {
    if (block_ptr == NULL) return 1;

    const size_t size = UINT64_TO_CHAR_SIZE*3 + MERKLE_ROOT_HEX_SIZE + HASH_HEX_SIZE;//dimensione della stringa di hex

    char input_sha256 [size+1];

    int res = BLOCK_TO_HEX_BE(block_ptr->index, //risultato è la lunghezza della stringa concatenata
                    block_ptr->timestamp,
                    block_ptr->prev_hash,
                    block_ptr->merkle_root,
                    block_ptr->nonce,
                    input_sha256,
                    size);

    if (res<0 || res >= size+1 ){return 1;}

    sha256_of_string(
        (const unsigned char *)input_sha256
        ,size
        ,out_hash);

    return 0;


}

void blockDestroy(Block* block_ptr) {
    free(block_ptr);
}



//funzione per impacchettare le transazioni
int pack_transactions(Block *b, const TxList *list) {
    memset(b->transactions, 0, MAX_BLOCK_TXS_BUF);      //azzero transactions nel blocco

    if (list->count == 0)  return 0;

    //copia la prima transazione
    strncpy(b->transactions, list->strings[0], MAX_BLOCK_TXS_BUF - 1);

    for (int i = 1; i < list->count; i++) {
        //verifico spazio libero nel buffer
        size_t current_len = strlen(b->transactions);
        size_t needed_space = strlen(list->strings[i] + 2);   // +2 per i ::

        if (current_len + needed_space >= MAX_BLOCK_TXS_BUF) {
            return -1;
        }

        strcat(b->transactions, "::");
        strcat(b->transactions, list->strings[i]);
    }
    return 0;
}

//funzione per de-impacchettare le transazioni
int unpack_transactions(Block *b, const TxList *list) {
    list->count = 0;
    memset(list->strings, 0, sizeof(list->strings));

    if (strlen(b->transactions) == 0) return 0;

    char token_buf[MAX_BLOCK_TXS_BUF];
    strncpy(token_buf, b->transactions, MAX_BLOCK_TXS_BUF);

    char *saveptr;
    char *token = strtok_r(token_buf, ":", &saveptr);
    while (token != NULL) {
        if (strlen(token) > 0) { // Salta i token vuoti generati dal doppio dei punti ":"
            if (list->count >= MAX_TX_PER_BLOCK) return -1; // Limite raggiunto

            strncpy(list->strings[(int)list->count], token, MAX_TX_SIZE - 1);
            list->count++;
        }
        token = strtok_r(NULL, ":", &saveptr);
    }
}
