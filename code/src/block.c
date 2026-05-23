#include "block.h"
#include "error.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
 * @param prev Blocco precedente
 * @param nonce
 * @param txs
 * @return
 */
int blockInit(Block *block_ptr,const u_int64_t index, const u_int64_t timestamp, const Block *prev, const u_int64_t nonce,const TxList *txs){
    //chiamate a tutte le altre funzioni per ottenere i campi necessari
    if (block_ptr == NULL) return INVALID_BLOCK;

    block_ptr -> index = index;
    block_ptr -> timestamp = timestamp;

    if (prev == NULL) block_ptr->prev_hash[0] = '\0'; // se il blocco è minato ma non aggiunto a blockchain
    else {
        const int res = blockGetHash(prev,block_ptr->prev_hash);
        if ( res == INVALID_HASH) return INVALID_BLOCK;
    }

    block_ptr -> nonce = nonce;

    if (txs == NULL){ block_ptr-> transactions[0] = '\0';
                      block_ptr -> merkle_root[0] = '\0';
    }// inzializziamo il discorso ce non sono state passate transazioni da inserire
    else pack_transactions(block_ptr,txs);// mettiamo dentro al blocco le transazioni



    return 0;
}
int blockGetmerkle(const Block* block_ptr,char* output_merkle) {
    if (block_ptr == NULL || block_ptr->transactions[0] == '\0' ) return 1;

    TxList list;

    if ( unpack_transactions(block_ptr,&list)!= 0 ) return 1;

/**TODO
 *Implementare controllo pari o dispari per aggiungere hash 1,2
 *Implementare una funzione ricorsiva per il merkle
 *oppure anche una funzione sequenziale che spacca list in più sottoblocchi
 *prima di calcolare SHA256(transazione)
   */

    return 0;

}
int blockGetHash(const Block *block_ptr, char out_hash[65]) {
    if (block_ptr == NULL) return INVALID_HASH;

    const size_t size = UINT64_TO_CHAR_SIZE*3 + MERKLE_ROOT_HEX_SIZE + HASH_HEX_SIZE;//dimensione della stringa di hex

    char input_sha256 [size+1];

    const int res = BLOCK_TO_HEX_BE(block_ptr->index, //risultato è la lunghezza della stringa concatenata
                    block_ptr->timestamp,
                    block_ptr->prev_hash,
                    block_ptr->merkle_root,
                    block_ptr->nonce,
                    input_sha256,
                    size);

    if (res<0 || res >= size+1 ){return INVALID_HASH;}

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

    if (strlen(b->transactions) == 0) return 0;//ATTENZIONE strlen scorre un array quindi se transactions è lungo 4099
    //ci può mettere un poco

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
    return 0;
}
