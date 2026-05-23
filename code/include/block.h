#ifndef BLOCK_H
#define BLOCK_H
#include <stdio.h>
#include <sys/types.h>

#define HASH_HEX_SIZE 64        //64 caratteri hex + '\0'
#define MERKLE_ROOT_HEX_SIZE 64
#define MAX_BLOCK_TXS_BUF 4098  // dimensione massima buffer transazioni
#define MAX_TX_PER_BLOCK 30    // Numero massimo di transazioni per blocco
#define MAX_TX_SIZE 128         // Dimensione massima di una singola transazione
#define UINT64_TO_CHAR_SIZE 16

typedef struct {
    u_int64_t index;
    u_int64_t timestamp;
    char prev_hash[HASH_HEX_SIZE+1];
    char merkle_root[MERKLE_ROOT_HEX_SIZE+1];
    u_int64_t nonce;
    char transactions[MAX_BLOCK_TXS_BUF+1];
}Block;

typedef struct {
    char count;
    char strings[MAX_TX_PER_BLOCK][MAX_TX_SIZE];
} TxList;

/**
 * Alloca memoria per un blocco
 * @return block_ptr
 */
Block* blockCreate();
/**
 *
 * @param block_ptr puntatore al blocco
 * @param prev blocco precedente
 * @param txs lista di transazioni
 * @return stato dell'operazione
 */
int blockInit(Block *block_ptr, const u_int64_t timestamp, const Block *prev, const TxList *txs);
int blockGetmerkle(Block *block_ptr);
int blockGetHash(const Block *block_ptr, char out_hash[HASH_HEX_SIZE+1]);
int blockValidate(const Block *block_ptr, const Block *prev);
int blockToCsv(const Block *block_ptr, char *buffer, size_t size);
int blockFromCsv(Block *block_ptr, const char *line);
void blockDestroy(Block* block_ptr);


int pack_transactions(Block *block_ptr, const TxList *list);
int unpack_transactions(Block *block_ptr, const TxList *list);


#endif //BLOCK_H
