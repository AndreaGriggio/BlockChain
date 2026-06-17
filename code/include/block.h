#ifndef BLOCK_H
#define BLOCK_H
#include <stddef.h> // per size_t
#include <stdint.h> // per uint64_t
#include "constants.h"
#define BLOCK_CSV_LINE_SIZE \
    (MAX_BLOCK_TXS_BUF + HASH_HEX_SIZE * 2 + UINT64_TO_CHAR_SIZE*3+128)//128 per il meme tenerci larghi
typedef struct Block Block;

typedef struct {
    size_t count;
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
 * @param index indice del blocco
 * @param timestamp timestamp del blocco
 * @param prev blocco precedente
 * @param nonce nonce del blocco 
 * @param txs lista di transazioni
 * @return stato dell'operazione
 */
int blockInit(
    Block *block_ptr
    ,uint64_t index,
    uint64_t timestamp,
    const Block *prev,
    uint64_t nonce,
    const TxList *txs);

int blockGetmerkle(const Block *block_ptr,char output_merkle[MERKLE_ROOT_HEX_SIZE + 1]);
/**
 *La funzione prende in input 0 < n < 30 Hashcodes e ritorna il merkle root degli hashcode
 * @param hashes Hashcodes delle transazioni in base 16 convertiti con sha256_of_string()
 * @param count Numero d Hashcodes
 * @param output_merkle Puntatore all'output merkle dove verrà inserito il risulatato finale
 * @return 0 se tutto è andato a buon fine
 */
int calcMerkle(char hashes[][MERKLE_ROOT_HEX_SIZE+1],size_t count,char output_merkle[MERKLE_ROOT_HEX_SIZE+1]);

/**
 *
 * @param block_ptr Blocco su cui calcolare hash
 * @param out_hash  Buffer dove va inserito l'hashcode
 * @return 0 se tutto è andato a buon fine
 */
int blockGetHash(const Block *block_ptr, char out_hash[HASH_HEX_SIZE + 1]);
/**
 *
 * @param block_ptr Blocco da validare
 * @param prev Blocco valido che precede block_ptr
 * @return 0 se tutto è andato a buon fine
 */
int blockValidate(const Block *block_ptr, const Block *prev);
/**
 *
 * @param block_ptr Blocco da convertire in stringa
 * @param buffer Buffer dove mettere il risultato
 * @param size Dimensione del buffer
 * @return 0 se tutto è andato a buon fine
 */
int blockToCsv(const Block *block_ptr, char *buffer, size_t size);
/**
 *
 * @param block_ptr Blocco creato e inizializzato
 * @param line Linea letta fa file CSV
 * @return 0 se tutto è andato a buon fine
 */
int blockFromCsv(Block *block_ptr, const char *line);
int blockDestroy(Block* block_ptr);

/**
 *
 * @param block_ptr Blocco a cui aggiungere transazioni
 * @param transaction transazione da aggiungere
 * @return 0 se tutto è andato a buon fine
 */
int blockAddTransaction(Block *block_ptr,const char transaction[MAX_TX_SIZE + 1]);

/**
 * Uccide le transazioni che un bloco contiene
 * @param block_ptr Blocco al quale azzerare le transazioni
 * @return 0 se tutto è andato a buon fine
 */
int blockKillTransactions(Block *block_ptr);

int blockGetMerkleRoot(const Block *block_ptr, char output[MERKLE_ROOT_HEX_SIZE + 1]);
int blockGetIndex(const Block *block_ptr, uint64_t *index);

int pack_transactions(Block *block_ptr, const TxList *list);
int unpack_transactions(const Block *block_ptr, TxList *list);


#endif //BLOCK_H
