#ifndef COMMUNICATION_PROTOCOL_H
#define COMMUNICATION_PROTOCOL_H
 
#include <stdint.h>
 
typedef enum {BLOCK_VALID, BLOCK_INVALID} BlockValidationResult;
 
typedef struct {
    uint64_t            block_index;
    int                 miner_id;
    BlockValidationResult result;
} BlockResponse;
 
#endif // COMMUNICATION_PROTOCOL_H
