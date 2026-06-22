#include "nodeValidation.h"
#include "nodeLog.h"
#include "nodeCSV.h"
#include "error.h"
#include <string.h>   // strcmp


#include <pthread.h>

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

int process_block(NodeContext *ctx, Block *new_block) {
    if (new_block == NULL) return INVALID_BLOCK;

    char hash[HASH_HEX_SIZE + 1];
    if (blockGetHash(new_block, hash) != 0) {
        log_msg(ctx, "ERROR: blockGetHash fallito");
        return -1;
    }

    if (validate_merkle(ctx, new_block) != 0) {
        log_msg(ctx, "ERROR: INVALID_MERKLE hash=%.16s...", hash);
        return INVALID_MERKLE;
    }

    pthread_mutex_lock(&ctx->chain_mutex);

    int rc = commit_block_to_shared_csv(ctx, new_block);

    if (rc != 0) {
        pthread_mutex_unlock(&ctx->chain_mutex);
        return rc;
    }

    uint64_t len = ctx->chain_length;

    pthread_mutex_unlock(&ctx->chain_mutex);

    nSSetLastBlock(ctx->status, new_block);
    nSSetChainLength(ctx->status, len);
    nSSetState(ctx->status, NODE_IDLE);

    log_msg(ctx, "Blocco accettato: index=%llu hash=%.16s...",
            (unsigned long long)(len - 1), hash);

    return 0;
}