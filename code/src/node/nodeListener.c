#include "nodeListener.h"
#include "nodeLog.h"
#include "nodeValidation.h"
#include "nodeFIFO.h"
#include "error.h"
#include "message.h"
#include "block.h"
#include "broker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <semaphore.h>
#include <fcntl.h>

void *listener_thread(void *arg)
{
    NodeContext *ctx = (NodeContext *)arg;

    log_msg(ctx, "Listener thread avviato, ascolto su %d miner", ctx->num_miners);

    while (ctx->running)
    {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;

        for (int i = 0; i < ctx->num_miners; i++)
        {
            if (ctx->from_miner[i] < 0)
                continue;
            FD_SET(ctx->from_miner[i], &rfds);
            if (ctx->from_miner[i] > maxfd)
                maxfd = ctx->from_miner[i];
        }

        if (maxfd < 0)
        {
            log_msg(ctx, "Tutte le pipe dei miner chiuse, listener termina");
            break;
        }

        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
        int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            log_msg(ctx, "ERROR: select fallita: %s", strerror(errno));
            break;
        }

        if (ready == 0)
            continue;

        for (int i = 0; i < ctx->num_miners; i++)
        {
            if (ctx->from_miner[i] < 0)
                continue;
            if (!FD_ISSET(ctx->from_miner[i], &rfds))
                continue;

            Message msg;
            messageInit(&msg);

            int r = receiveMessage(ctx->from_miner[i], &msg);
            if (r == SOCKET_CLOSED)
            {
                log_msg(ctx, "Pipe del miner %d chiusa", i);
                close(ctx->from_miner[i]);
                ctx->from_miner[i] = -1;
                continue;
            }
            if (r != 0)
            {
                log_msg(ctx, "ERROR: receiveMessage dal miner %d fallita: %d", i, r);
                close(ctx->from_miner[i]);
                ctx->from_miner[i] = -1;
                continue;
            }

            MessageType type;
            messageGetType(&msg, &type);

            if (type != MSG_BLOCK_MINED)
            {
                log_msg(ctx, "WARN: tipo messaggio inatteso dal miner %d: %d", i, type);
                continue;
            }

            log_msg(ctx, "Ricevuto MSG_BLOCK_MINED dal miner %d", i);

            char csv_line[BLOCK_CSV_LINE_SIZE];
            if (messageGetPayload(&msg, csv_line, sizeof(csv_line)) != 0)
            {
                log_msg(ctx, "ERROR: messageGetPayload fallita (miner %d)", i);
                continue;
            }

            Block *new_block = blockCreate();
            if (new_block == NULL)
            {
                log_msg(ctx, "ERROR: blockCreate fallita");
                continue;
            }

            if (blockFromCsv(new_block, csv_line) != 0)
            {
                log_msg(ctx, "ERROR: blockFromCsv fallita (miner %d)", i);
                blockDestroy(new_block);
                continue;
            }

            if (validate_merkle(ctx, new_block) != 0)
            {
                log_msg(ctx, "Blocco miner %d: Merkle non valido, scarto", i);
                blockDestroy(new_block);
                continue;
            }

            int chain_ok;
            pthread_mutex_lock(&ctx->chain_mutex);

            if (ctx->last_block == NULL)
            {
                chain_ok = 1;
            }
            else
            {
                int validation_result = blockValidate(new_block, ctx->last_block);
                chain_ok = (validation_result == 0);
            }

            pthread_mutex_unlock(&ctx->chain_mutex);

            if (!chain_ok)
            {
                log_msg(ctx, "Blocco miner %d non collegabile alla catena, scarto", i);
                blockDestroy(new_block);
                continue;
            }

            blockDestroy(new_block);

            sem_t *sem = sem_open(BROKER_SEM_NAME, 0);
            if (sem == SEM_FAILED)
            {
                log_msg(ctx, "ERROR: sem_open(%s) fallita: %s",
                        BROKER_SEM_NAME, strerror(errno));
                continue;
            }

            int wait_result;
            do
            {
                wait_result = sem_wait(sem);
            } while (wait_result == -1 && errno == EINTR);

            if (wait_result == -1)
            {
                log_msg(ctx, "ERROR: sem_wait broker fallita: %s", strerror(errno));
                sem_close(sem);
                continue;
            }

            BrokerMessage bmsg;
            memset(&bmsg, 0, sizeof(bmsg));
            bmsg.msg_type = BROKER_MSG_BLOCK;
            bmsg.node_id = ctx->node_id;
            bmsg.miner_id = i;
            strncpy(bmsg.csv_line, csv_line, BLOCK_CSV_LINE_SIZE - 1);
            bmsg.csv_line[BLOCK_CSV_LINE_SIZE - 1] = '\0';

            ssize_t wr = write(ctx->fd_to_broker, &bmsg, sizeof(BrokerMessage));
            if (wr != (ssize_t)sizeof(BrokerMessage))
            {
                log_msg(ctx, "ERROR: write al broker fallita (%zd/%zu bytes)",
                        wr, sizeof(BrokerMessage));
            }
            else
            {
                log_msg(ctx, "Blocco inviato al broker (miner %d)", i);
            }

            sem_post(sem);
            sem_close(sem);
        }
    }

    log_msg(ctx, "Listener thread terminato");
    return NULL;
}
