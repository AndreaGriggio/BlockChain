//
// Created by andrea on 20/06/26.
//


#include "transactionPool.h"
#include "constants.h"
#include "error.h"

#include <stdlib.h>
#include <string.h>

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

int poolRemoveAt(TransactionPool* pool, size_t index) {
    if (pool == NULL || index >= pool->count) return INVALID_PARAMS;

    free(pool->items[index]);

    // L'ultimo elemento prende il posto del rimosso (rimozione O(1), ordine non garantito)
    pool->items[index] = pool->items[pool->count - 1];
    pool->items[pool->count - 1] = NULL;
    pool->count--;
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

const char* poolGet(const TransactionPool* pool, size_t index) {
    if (pool == NULL || index >= pool->count) return NULL;
    size_t len = strlen(pool->items[index]);

    char* copy = malloc(len + 1);
    if (copy == NULL) return NULL;

    memcpy(copy, pool->items[index], len + 1);
    return copy;
}

const char* poolRemoveLast(const TransactionPool* pool) {
    if (pool == NULL) return NULL;
    if (pool->count == 0) return NULL;
    size_t len = strlen(pool->items[pool->count - 1]);

    char* copy = malloc(len + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, pool->items[pool->count - 1], len + 1);
    pool->count--;
    return copy;
}

int destroyTransactionPool(TransactionPool* pool) {
    if (pool == NULL) return INVALID_PARAMS;

    clearTransactionPool(pool);
    free(pool->items);
    free(pool);
    return 0;
}