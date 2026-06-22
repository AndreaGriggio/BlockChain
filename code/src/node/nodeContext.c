#include "nodeContext.h"
#include <stdlib.h>

NodeContext *nodeContextCreate(void) {
    NodeContext *ctx = malloc(sizeof(NodeContext));
    if (ctx == NULL) return NULL;

    ctx->node_id    = -1;
    ctx->num_nodes  = 0;
    ctx->num_miners = 0;

    ctx->last_block   = NULL;
    ctx->chain_length = 0;
    pthread_mutex_init(&ctx->chain_mutex, NULL);

    ctx->to_miner   = NULL;
    ctx->from_miner = NULL;

    ctx->status   = NULL;
    ctx->log_file = NULL;
    ctx->running  = 1;

    return ctx;
}

void nodeContextDestroy(NodeContext *ctx) {
    if (ctx == NULL) return;
    pthread_mutex_destroy(&ctx->chain_mutex);
    free(ctx);
}