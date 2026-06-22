//
// Created by andrea on 20/06/26.
//

#ifndef TRANSACTIONPOOL_H
#define TRANSACTIONPOOL_H
#include <stddef.h>

/**
 * Pool dinamico di transazioni: array di puntatori a stringa che cresce
 * automaticamente (raddoppio della capacità) cosi' da non avere un limite
 * fisso sul numero di transazioni memorizzabili.
 *  - items    : array di puntatori, ogni transazione allocata su misura
 *  - count    : numero di transazioni attualmente nel pool
 *  - capacity : numero di slot allocati nell'array items
 */
typedef struct {
    char ** items;
    size_t count;
    size_t capacity;
}TransactionPool;

/**
 * Alloca e inizializza un nuovo pool vuoto (count = capacity = 0).
 * @return puntatore al pool, oppure NULL se la malloc fallisce
 */
TransactionPool* createTransactionPool(void);

/**
 * Inizializza un pool gia' allocato portandolo allo stato vuoto.
 * NON libera eventuale memoria precedente: usare su un pool nuovo.
 * @param pool pool da inizializzare
 * @return 0 se tutto e' andato bene, INVALID_PARAMS se pool == NULL
 */
int initTransactionPool(TransactionPool* pool);

/**
 * Aggiunge una transazione al pool facendone una COPIA interna.
 * Se il pool e' pieno la capacita' viene raddoppiata (realloc).
 * Il chiamante mantiene la ownership di tx (puo' essere stack o m->payload).
 * @param pool pool a cui aggiungere
 * @param tx   stringa della transazione (terminata da '\0')
 * @return 0 se ok, INVALID_PARAMS se argomenti nulli, MEMORY_ERROR se alloc fallisce
 */
int poolPush(TransactionPool* pool, const char* tx);

/**
 * Rimuove la transazione in posizione index. L'ordine NON viene preservato
 * (l'ultimo elemento prende il posto del rimosso): rimozione in O(1).
 * @param pool  pool da cui rimuovere
 * @param index indice della transazione da rimuovere
 * @return 0 se ok, INVALID_PARAMS se pool nullo o index fuori range
 */
int poolRemoveAt(TransactionPool* pool, size_t index);

/**
 * Libera tutte le transazioni e riporta count a 0, mantenendo pero' la
 * capacita' gia' allocata (utile dopo aver minato un blocco).
 * @param pool pool da svuotare
 * @return 0 se ok, INVALID_PARAMS se pool == NULL
 */
int clearTransactionPool(TransactionPool* pool);

/**
 * Numero di transazioni attualmente nel pool.
 * @param pool pool da interrogare
 * @return count, oppure 0 se pool == NULL
 */
size_t poolCount(const TransactionPool* pool);

/**
 * Copia la transazione in posizione index nel buffer fornito dal chiamante.
 * Il pool resta UNICO proprietario della propria memoria: il puntatore interno
 * non viene mai esposto, cosi' nessuno puo' liberarlo per sbaglio.
 * @param pool     pool da interrogare
 * @param index    indice richiesto
 * @return 0 se ok, INVALID_PARAMS se argomenti nulli o index fuori range,
 *         BUFFER_TOO_SMALL se out non e' abbastanza grande
 */
const char* poolGet(const TransactionPool* pool, size_t index);

/**
 * Libera tutte le transazioni, l'array interno e la struttura stessa.
 * Dopo la chiamata il puntatore pool non e' piu' valido.
 * @param pool pool da distruggere
 * @return 0 se ok, INVALID_PARAMS se pool == NULL
 */
int destroyTransactionPool(TransactionPool* pool);
const char* poolRemoveLast(TransactionPool* pool);
#endif //TRANSACTIONPOOL_H