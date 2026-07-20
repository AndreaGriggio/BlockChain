#ifndef COMMUNICATION_PROTOCOL_H
#define COMMUNICATION_PROTOCOL_H
 
#include <stdint.h>
#include "../constants.h"
 
typedef enum {BLOCK_VALID, BLOCK_INVALID} BlockValidationResult;
 
typedef struct {
    uint64_t                block_index;
    int                     miner_id;
    BlockValidationResult   result;
    char                    block_hash[HASH_HEX_SIZE + 1];
} BlockResponse;
 
#endif // COMMUNICATION_PROTOCOL_H
