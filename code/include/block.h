#ifndef BLOCCO_H
#define BLOCCO_H
#include <stdio.h>
#include <sys/types.h>

#define HASH_HEX_SIZE 65        //64 caratteri hex + '\0'
#define MAX_BLOCK_TXS_BUF 4096
#define MAX_TX_PER_BLOCK 30    // Numero massimo di transazioni per blocco
#define MAX_TX_SIZE 128         // Dimensione massima di una singola transazione

typedef struct {
    u_int64_t index;
    u_int64_t timestamp;
    char prev_hash[HASH_HEX_SIZE];
    char merkle_root[HASH_HEX_SIZE];
    u_int64_t nonce;
    char transactions[MAX_BLOCK_TXS_BUF];
}Block;

typedef struct {
    char count;
    char strings[MAX_TX_PER_BLOCK][MAX_TX_SIZE];
} TxList;

int pack_transactions(Block *b, const TxList *list);
int unpack_transactions(Block *b, const TxList *list);


#endif //BLOCCO_H
