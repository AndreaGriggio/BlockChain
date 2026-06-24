//
// Created by andrea on 20/06/26.
//


#include "transactionPool.h"
#include "constants.h"
#include "error.h"

#include <stdlib.h>
#include <string.h>

#include "blocksPool.h"

TransactionPool* createTransactionPool(void) {
    TransactionPool* pool = malloc(sizeof(TransactionPool));
    if (pool == NULL) return NULL;

    initTransactionPool(pool);
    return pool;
}

int initTransactionPool(TransactionPool* pool) {
    if (pool == NULL) return INVALID_PARAMS;


    pool->items = NULL;
    pool->count = 0;
    pool->capacity = 0;
    return 0;
}


int poolPush(TransactionPool* pool, const char* tx) {
    if (pool == NULL || tx == NULL) return INVALID_PARAMS;

    size_t len = strlen(tx);

    if ( len > MAX_TX_SIZE) return INVALID_PARAMS;

    // Se il pool e' pieno cresco la capacita': realloc copia/sposta per me
    if (pool->count == pool->capacity) {
        size_t new_cap = (pool->capacity == 0)
                             ? POOL_INITIAL_CAPACITY
                             : pool->capacity * POOL_GROWTH_FACTOR;

        char** tmp = realloc(pool->items, new_cap * sizeof(char*));
        if (tmp == NULL) return MEMORY_ERROR; // pool->items resta valido

        pool->items = tmp;
        pool->capacity = new_cap;
    }

    // Copia interna della transazione (ownership del pool)

    char* copy = malloc(len + 1);
    if (copy == NULL) return MEMORY_ERROR;

    memcpy(copy, tx, len + 1); // include il terminatore '\0'

    pool->items[pool->count] = copy;
    pool->count++;
    return 0;
}



int clearTransactionPool(TransactionPool* pool) {
    if (pool == NULL) return INVALID_PARAMS;

    for (size_t i = 0; i < pool->count; i++) {
        free(pool->items[i]);
        pool->items[i] = NULL;
    }
    pool->count = 0; // capacita' conservata per riuso
    return 0;
}

size_t poolCount(const TransactionPool* pool) {
    if (pool == NULL) return 0;
    return pool->count;
}



char* poolRemoveLast(TransactionPool* pool) {
    if (pool == NULL) return NULL;
    if (pool->count == 0) return NULL;
    
    pool->count--;

    char * tx = pool->items[pool->count];

    pool->items[pool->count] = NULL; // rimuovo il puntatore dal pool

    return tx;
}

int destroyTransactionPool(TransactionPool* pool) {
    if (pool == NULL) return INVALID_PARAMS;

    clearTransactionPool(pool);
    free(pool->items);
    free(pool);
    return 0;
}

TxList* poolTrxCreateList(TransactionPool* pool) {
    if (pool == NULL) return NULL;
    TxList* list = malloc(sizeof(TxList));
    if (list == NULL) return NULL;

    list->count = 0;

    while (list->count < MAX_TX_PER_BLOCK && pool->count > 0) {
        char* tx = poolRemoveLast(pool);   // copia malloc'ata,

        if (tx == NULL) break;//qualcosa è andato storto con malloc

        strncpy(list->strings[list->count], tx, MAX_TX_SIZE - 1);
        list->strings[list->count][MAX_TX_SIZE - 1] = '\0';
        list->count ++;
        free(tx);                          // libera la copia
    }


    return list;
}
