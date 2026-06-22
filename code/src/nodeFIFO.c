#define _GNU_SOURCE

#include "nodeFIFO.h"
#include "nodeLog.h"
#include "error.h"
#include "constants.h"
#include "childProcess.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

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

    ChildProcess *cp = childProcessCreate();
    if (cp == NULL) return -1;

    if (ctx->status == NULL) {
        fprintf(stderr, "NODE: status non inizializzato\n");
        childProcessDestroy(cp);
        return -1;
    }

    nSGetCPChildProcess(ctx->status, cp);

    int id;
    if (getCpId(cp, &id) != 0 || id < 0) {
        childProcessDestroy(cp);
        return -1;
    }
    childProcessDestroy(cp);

    for (int i = 0; i < num_miners; i++) {
        char path_to[64];
        snprintf(path_to, sizeof(path_to), "%s%d%d", NODE_MINER_FIFO, id, i);

        if (mkfifo(path_to, 0666) < 0 && errno != EEXIST) {
            fprintf(stderr, "NODE %d: mkfifo %s fallita: %s\n",
                    id, path_to, strerror(errno));
            return -1;
        }

        do {
            ctx->to_miner[i] = open(path_to, O_WRONLY | O_NONBLOCK);
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

        fprintf(stderr, "NODE %d: to_miner[%d] aperto (fd=%d path=%s)\n",
                id, i, ctx->to_miner[i], path_to);
    }

    for (int i = 0; i < num_miners; i++) {
        char path_from[64];
        snprintf(path_from, sizeof(path_from), "%s%d%d", MINER_NODE_FIFO, i, id);

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

        fprintf(stderr, "NODE %d: from_miner[%d] aperto (fd=%d path=%s)\n",
                id, i, ctx->from_miner[i], path_from);
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
                 uint64_t block_index, BlockValidationResult result) {
    if (ctx->to_miner[miner_idx] < 0) return -1;

    BlockResponse resp;
    resp.block_index = block_index;
    resp.miner_id    = miner_idx;
    resp.result      = result;

    ssize_t written = write(ctx->to_miner[miner_idx],
                            &resp, sizeof(BlockResponse));
    if (written != sizeof(BlockResponse)) {
        log_msg(ctx, "ERROR: notify_miner %d fallita", miner_idx);
        return -1;
    }

    log_msg(ctx, "Notificato miner %d: block_index=%llu result=%s",
            miner_idx,
            (unsigned long long)block_index,
            result == BLOCK_VALID ? "VALID" : "INVALID");
    return 0;
}

void notify_all_miners(NodeContext *ctx,
                       uint64_t block_index, BlockValidationResult result) {
    for (int i = 0; i < ctx->num_miners; i++) {
        notify_miner(ctx, i, block_index, result);
    }
}