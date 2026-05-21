#include <stdio.h>
#include <string.h>
#include "block.h"

#define HASH_HEX_SIZE 65        // 64 caratteri hex + '\0'
#define MAX_BLOCK_TXS_BUF 4096  // dimensione massima buffer transazioni
#define MAX_TX_PER_BLOCK 30     // Numero massimo di transazioni per blocco
#define MAX_TX_SIZE 128         // Dimensione massima di una singola transazione


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

int main(void){
    return 0;
}