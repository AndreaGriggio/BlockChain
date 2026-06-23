//
// Created by andrea on 21/06/26.
//

#ifndef BLOCKSPOOL_H
#define BLOCKSPOOL_H
#include "block.h"
typedef enum BlockState {
    BLOCK_CONFIRMED = 1,
    BLOCK_WAITING = 2,
    BLOCK_DISCARDED = 3,
    UNUSED_POOL = 4,
    BLOCKS_FOUND_NOT_FILLED = 5,
}BlockState;

typedef struct BlockHandlingHelper {
    BlockState* state;
    Block * block;
}BlockHandlingHelper;

typedef struct BlocksPool {
    Block ** items;

    BlockState poolState;
    size_t count;
    size_t capacity;
}BlocksPool;

BlocksPool* createBlocksPool(void);

int initBlocksPool(BlocksPool* pool);

int poolPushBlock(BlocksPool* pool, Block* block);

int poolBlockRemoveAt(BlocksPool* pool,size_t index);

int clearBlocksPool(BlocksPool* pool);

int poolBlockGetCount(const BlocksPool* pool,size_t* count);

int poolGetState(const BlocksPool* pool,Block* block,BlockState* b_State,size_t index);

int destroyBlocksPool(BlocksPool* pool);

int poolBlockRemoveLast(BlocksPool* pool,Block* block,BlockState* b_State);

int poolBlocksSetState(BlocksPool* pool,BlockState state);

#endif //BLOCKSPOOL_H
