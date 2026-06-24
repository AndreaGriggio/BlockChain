//
// Created by andrea on 21/06/26.
//
#include "blocksPool.h"
#include "error.h"
#include "constants.h"

#include <stdlib.h>
#include <string.h>

BlocksPool* createBlocksPool(void) {

    BlocksPool* pool = malloc (sizeof(BlocksPool));
    if ( pool == NULL ) return NULL;

    initBlocksPool(pool);
    return pool;
}

int initBlocksPool(BlocksPool* pool) {
    if ( pool == NULL ) return INVALID_PARAMS;

    pool->poolState = UNUSED_POOL;
    pool->items = NULL;
    pool->count = 0;
    pool->capacity = 0;
    return 0;
}

int poolPushBlock(BlocksPool* pool, Block* block) {
    if (pool == NULL || block == NULL) return INVALID_PARAMS;

    if ( pool->count == pool->capacity ) {
        const size_t newCapacity = pool->capacity == 0
                                    ? POOL_INITIAL_CAPACITY
                                    : pool->capacity * POOL_GROWTH_FACTOR;
        Block ** tmp = realloc( pool->items,sizeof(Block *) * newCapacity );

        if ( tmp == NULL ) return MEMORY_ERROR;

        pool->items = tmp;//<- è una pratica corretta i comportamenti di realloc differiscono in base alla posizione in memoria degli items
        pool->capacity = newCapacity;


    }

    Block * copy = blockCreate();

    if ( copy == NULL ) return MEMORY_ERROR;

    blockCopy(copy,block);

    pool->items[pool->count] = copy;
    pool->count++;

    return 0;
}
int poolBlockGet(BlocksPool* pool,Block* block,size_t index){
    if ( pool == NULL || block == NULL) return INVALID_PARAMS;
    if ( index >= pool->count ) return INVALID_PARAMS;

    blockCopy(block,pool->items[index]);

    return 0;
}

int poolBlockRemoveAt(BlocksPool* pool,size_t index) {
    if (pool == NULL )         return INVALID_PARAMS;
    if ( index >= pool->count )return INVALID_PARAMS;

    blockDestroy(pool->items[index]);

    pool->count--;                                  //count ora indica l'ultimo elemento valido
    pool->items[index] = pool->items[pool->count];  //l'ultimo elemento sostituisce quello rimosso (self-assign innocuo se rimuovo l'ultimo)
    pool->items[pool->count] = NULL;


    return 0;

}

int clearBlocksPool(BlocksPool* pool) {
    if (pool == NULL ) return INVALID_PARAMS;

    for (size_t i = 0; i < pool->count; i++ ) {

        blockDestroy(pool->items[i]);
        pool->items[i] = NULL;

    }
    pool->poolState = UNUSED_POOL;
    pool->count = 0;
    return 0;
}

int poolBlockGetCount(const BlocksPool* pool,size_t* count) {
    if (pool == NULL ) return INVALID_PARAMS;

    *count = pool->count;

    return 0;
}

int poolGetState(const BlocksPool* pool,Block* block,BlockState* b_State,size_t index) {
    if ( pool == NULL )         return INVALID_PARAMS;
    if ( index >= pool->count ) return INVALID_PARAMS;

    blockCopy(block,pool->items[index]);
    *b_State = pool->poolState;

    if ( block == NULL ) return MEMORY_ERROR;

    return 0;

}

int destroyBlocksPool(BlocksPool* pool) {

    if (pool == NULL ) return INVALID_PARAMS;

    clearBlocksPool(pool);
    free(pool);
    return 0;

}

int poolBlockRemoveLast(BlocksPool* pool,Block* block,BlockState* b_State) {
    if ( pool == NULL || block == NULL  || b_State == NULL) return INVALID_PARAMS;
    if ( pool->count == 0 ) return -1;
    blockCopy(block,pool->items[pool->count - 1]);//copio l'ultimo blocco

    blockDestroy(pool->items[pool->count - 1]);    //rilascio memoria
    pool->items[pool->count - 1] = NULL;//metto a terra il puntatore dell'ultimo elemento

    pool->count --;//decremento il numero corrente di blocchi
    *b_State = pool->poolState;

    return 0;

}

int poolBlocksSetState(BlocksPool* pool,BlockState state) {
    if (pool == NULL ) return INVALID_PARAMS;
    pool->poolState = state;
    return 0;
}

