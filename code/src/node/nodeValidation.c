#include "nodeValidation.h"
#include "nodeLog.h"
#include "error.h"
#include <string.h>   // strcmp

int validate_merkle(NodeContext *ctx, const Block *block_ptr) {
    char computed[MERKLE_ROOT_HEX_SIZE + 1];
    char stored[MERKLE_ROOT_HEX_SIZE + 1];

    if (blockGetmerkle(block_ptr, computed) != 0) {
        log_msg(ctx, "ERROR: blockGetmerkle fallito");
        return -1;
    }

    if (blockGetMerkleRoot(block_ptr, stored) != 0) {
        log_msg(ctx, "ERROR: blockGetMerkleRoot fallito");
        return -1;
    }

    if (strcmp(computed, stored) != 0) {
        log_msg(ctx, "ERROR: Merkle root non valido");
        return INVALID_MERKLE;
    }

    return 0;
}
