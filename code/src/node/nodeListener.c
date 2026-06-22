#include "nodeListener.h"
#include "nodeLog.h"
#include "nodeValidation.h"
#include "nodeFIFO.h"
#include "error.h"
#include "message.h"
#include "block.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>

void *listener_thread(void *arg) {
    NodeContext *ctx = (NodeContext *)arg;

    log_msg(ctx, "Listener thread avviato, ascolto su %d miner", ctx->num_miners);

    while (ctx->running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;

        for (int i = 0; i < ctx->num_miners; i++) {
            if (ctx->from_miner[i] < 0) continue;
            FD_SET(ctx->from_miner[i], &rfds);
            if (ctx->from_miner[i] > maxfd) maxfd = ctx->from_miner[i];
        }

        if (maxfd < 0) {
            log_msg(ctx, "Tutte le pipe dei miner chiuse, listener termina");
            break;
        }

        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        if (ready < 0) {
            if (errno == EINTR) continue;
            log_msg(ctx, "ERROR: select fallita: %s", strerror(errno));
            break;
        }

        if (ready == 0) continue;

        for (int i = 0; i < ctx->num_miners; i++) {
            if (ctx->from_miner[i] < 0)               continue;
            if (!FD_ISSET(ctx->from_miner[i], &rfds)) continue;

            Message msg;
            messageInit(&msg);

            int r = receiveMessage(ctx->from_miner[i], &msg);
            if (r == SOCKET_CLOSED) {
                log_msg(ctx, "Pipe del miner %d chiusa", i);
                close(ctx->from_miner[i]);
                ctx->from_miner[i] = -1;
                continue;
            }
            if (r != 0) {
                log_msg(ctx, "ERROR: receiveMessage dal miner %d fallita: %d", i, r);
                continue;
            }

            MessageType type;
            messageGetType(&msg, &type);

            if (type != MSG_BLOCK_MINED) {
                log_msg(ctx, "WARN: tipo messaggio inatteso dal miner %d: %d", i, type);
                continue;
            }

            log_msg(ctx, "Ricevuto MSG_BLOCK_MINED dal miner %d", i);

            char csv_line[BLOCK_CSV_LINE_SIZE];
            if (messageGetPayload(&msg, csv_line, sizeof(csv_line)) != 0) {
                log_msg(ctx, "ERROR: messageGetPayload fallita (miner %d)", i);
                continue;
            }

            Block *new_block = blockCreate();
            if (new_block == NULL) {
                log_msg(ctx, "ERROR: blockCreate fallita");
                continue;
            }

            if (blockFromCsv(new_block, csv_line) != 0) {
                log_msg(ctx, "ERROR: blockFromCsv fallita (miner %d)", i);
                blockDestroy(new_block);
                continue;
            }

            uint64_t block_index;
            blockGetIndex(new_block, &block_index);

            int res = process_block(ctx, new_block);

            if (res == 0) {
                log_msg(ctx, "Blocco dal miner %d accettato", i);
                notify_all_miners(ctx, block_index, BLOCK_VALID);
            } else if (res == BLOCK_ALREADY_PRESENT) {
                log_msg(ctx, "Blocco dal miner %d gia' presente", i);
                blockDestroy(new_block);
            } else {
                log_msg(ctx, "Blocco dal miner %d rifiutato (codice %d)", i, res);
                blockDestroy(new_block);
                notify_miner(ctx, i, block_index, BLOCK_INVALID);
            }
        }
    }

    log_msg(ctx, "Listener thread terminato");
    return NULL;
}