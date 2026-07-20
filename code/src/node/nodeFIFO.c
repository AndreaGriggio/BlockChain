#define _GNU_SOURCE

#include "nodeFIFO.h"
#include "nodeLog.h"
#include "error.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#include <string.h>   

int createNodeFifos(NodeContext *ctx, int num_miners) {
    ctx->to_miner   = (int *)malloc(sizeof(int) * num_miners);
    ctx->from_miner = (int *)malloc(sizeof(int) * num_miners);

    if (ctx->to_miner == NULL || ctx->from_miner == NULL) {
        fprintf(stderr, "NODE: malloc fd arrays fallita\n");
        free(ctx->to_miner);
        free(ctx->from_miner);
        ctx->to_miner   = NULL;
        ctx->from_miner = NULL;
        return -1;
    }

    for (int i = 0; i < num_miners; i++) {
        ctx->to_miner[i]   = -1;
        ctx->from_miner[i] = -1;
    }

    int id = ctx->node_id;

    if (id < 0) {
        return INVALID_PARAMS;
    }

    for (int i = 0; i < num_miners; i++) {
        char path_to[64];
        snprintf(path_to, sizeof(path_to), "%s%d_%d", NODE_MINER_FIFO, id, i);

        if (mkfifo(path_to, 0666) < 0 && errno != EEXIST) {
            fprintf(stderr, "NODE %d: mkfifo %s fallita: %s\n",
                    id, path_to, strerror(errno));
            return -1;
        }

        do {
            ctx->to_miner[i] = open(path_to, O_RDWR);
            if (ctx->to_miner[i] < 0 && errno != ENXIO) {
                fprintf(stderr, "NODE %d: open %s fallita: %s\n",
                        id, path_to, strerror(errno));
                return -1;
            }
            if (ctx->to_miner[i] < 0) usleep(10000);
        } while (ctx->to_miner[i] < 0);

        
        if (fcntl(ctx->to_miner[i], F_SETPIPE_SZ, PIPE_BUF) < 0) {
            fprintf(stderr, "NODE %d: fcntl to_miner[%d] fallita: %s\n",
                    id, i, strerror(errno));
            return -1;
        }
        
    }

    for (int i = 0; i < num_miners; i++) {
        char path_from[64];
        snprintf(path_from, sizeof(path_from), "%s%d_%d", MINER_NODE_FIFO, i, id);

        do {
            ctx->from_miner[i] = open(path_from, O_RDONLY);
            if (ctx->from_miner[i] < 0 && errno != ENXIO && errno != ENOENT) {
                fprintf(stderr, "NODE %d: open %s fallita: %s\n",
                        id, path_from, strerror(errno));
                return -1;
            }
            if (ctx->from_miner[i] < 0) usleep(10000);
        } while (ctx->from_miner[i] < 0);

        
        if (fcntl(ctx->from_miner[i], F_SETPIPE_SZ, PIPE_BUF) < 0) {
            fprintf(stderr, "NODE %d: fcntl from_miner[%d] fallita: %s\n",
                    id, i, strerror(errno));
            return -1;
        }
        
    }

    return 0;
}


void destroyNodeFifos(NodeContext *ctx, int num_miners) {
    if (ctx->to_miner != NULL) {
        for (int i = 0; i < num_miners; i++) {
            if (ctx->to_miner[i] >= 0) close(ctx->to_miner[i]);
        }
        free(ctx->to_miner);
        ctx->to_miner = NULL;
    }

    if (ctx->from_miner != NULL) {
        for (int i = 0; i < num_miners; i++) {
            if (ctx->from_miner[i] >= 0) close(ctx->from_miner[i]);
        }
        free(ctx->from_miner);
        ctx->from_miner = NULL;
    }
}

int notify_miner(NodeContext *ctx, int miner_idx,
                 uint64_t block_index,
                 const char *block_hash,
                 BlockValidationResult result) {
    if (ctx == NULL || ctx->to_miner == NULL) return -1;
    if (miner_idx < 0 || miner_idx >= ctx->num_miners) return -1;
    if (ctx->to_miner[miner_idx] < 0) return -1;

    BlockResponse resp;
    memset(&resp, 0, sizeof(resp));

    resp.block_index = block_index;
    resp.miner_id    = miner_idx;
    resp.result      = result;

    if (block_hash != NULL) {
        strncpy(resp.block_hash, block_hash, HASH_HEX_SIZE);
        resp.block_hash[HASH_HEX_SIZE] = '\0';
    }

    ssize_t written = write(ctx->to_miner[miner_idx],
                            &resp, sizeof(BlockResponse));

    if (written != sizeof(BlockResponse)) {
        log_msg(ctx, "ERROR: notify_miner %d fallita", miner_idx);
        return -1;
    }

    log_msg(ctx, "Notificato miner %d: block_index=%llu hash=%s result=%s",
            miner_idx,
            (unsigned long long)block_index,
            resp.block_hash,
            result == BLOCK_VALID ? "VALID" : "INVALID");

    return 0;
}

void notify_all_miners(NodeContext *ctx,
                       uint64_t block_index,
                       const char *block_hash,
                       BlockValidationResult result) {
    for (int i = 0; i < ctx->num_miners; i++) {
        notify_miner(ctx, i, block_index, block_hash, result);
    }
}



int openBrokerFifos(NodeContext *ctx) {
    if (ctx == NULL) return INVALID_PARAMS;

    int id = ctx->node_id;
    char path_from_broker[64];
    snprintf(path_from_broker, sizeof(path_from_broker),
             "%s%d", BROKER_NODE_FIFO, id);

    do {
        ctx->fd_from_broker = open(path_from_broker, O_RDWR);
        if (ctx->fd_from_broker < 0 && errno != ENXIO && errno != ENOENT) {
            fprintf(stderr, "NODE %d: open %s fallita: %s\n",
                    id, path_from_broker, strerror(errno));
            return FIFO_ERROR;
        }
        if (ctx->fd_from_broker < 0) usleep(10000);
    } while (ctx->fd_from_broker < 0);

    log_msg(ctx, "Aperta FIFO broker->node: %s", path_from_broker);

    char path_to_broker[64];
    snprintf(path_to_broker, sizeof(path_to_broker),
             "%s%d", NODE_BROKER_FIFO, id);

    do {
        ctx->fd_to_broker = open(path_to_broker, O_RDWR);
        if (ctx->fd_to_broker < 0 && errno != ENXIO && errno != ENOENT) {
            fprintf(stderr, "NODE %d: open %s fallita: %s\n",
                    id, path_to_broker, strerror(errno));
            close(ctx->fd_from_broker);
            ctx->fd_from_broker = -1;
            return FIFO_ERROR;
        }
        if (ctx->fd_to_broker < 0) usleep(10000);
    } while (ctx->fd_to_broker < 0);

    log_msg(ctx, "Aperta FIFO node->broker: %s", path_to_broker);
    return 0;
}

void closeBrokerFifos(NodeContext *ctx) {
    if (ctx == NULL) return;

    if (ctx->fd_to_broker >= 0) {
        close(ctx->fd_to_broker);
        ctx->fd_to_broker = -1;
    }
    if (ctx->fd_from_broker >= 0) {
        close(ctx->fd_from_broker);
        ctx->fd_from_broker = -1;
    }
}