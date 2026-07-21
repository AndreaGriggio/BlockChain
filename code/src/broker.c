#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "broker.h"
#include "constants.h"
#include "error.h"

static volatile sig_atomic_t running = 1;
static FILE *broker_log = NULL;

#define SEEN_BLOCK_CACHE_SIZE 256

// Cache circolare per evitare n inoltri dello stesso blocco
typedef struct
{
    char hashes[SEEN_BLOCK_CACHE_SIZE][HASH_HEX_SIZE + 1];
    size_t count;
    size_t next;
} SeenBlockCache;

// recupero hashcode del blocco da csv_line
static int block_hash_from_csv(const char *csv_line,
                               char out_hash[HASH_HEX_SIZE + 1])
{
    Block *block = blockCreate();
    if (block == NULL)
        return MEMORY_ERROR;

    int rc = blockFromCsv(block, csv_line);
    if (rc == 0)
        rc = blockGetHash(block, out_hash);

    blockDestroy(block);
    return rc;
}

static int cache_contains(const SeenBlockCache *cache, const char *hash)
{
    for (size_t i = 0; i < cache->count; i++)
    {
        if (strcmp(cache->hashes[i], hash) == 0)
            return 1;
    }
    return 0;
}

static void cache_add(SeenBlockCache *cache, const char *hash)
{
    strncpy(cache->hashes[cache->next], hash, HASH_HEX_SIZE);
    cache->hashes[cache->next][HASH_HEX_SIZE] = '\0';
    cache->next = (cache->next + 1) % SEEN_BLOCK_CACHE_SIZE;
    if (cache->count < SEEN_BLOCK_CACHE_SIZE)
        cache->count++;
}

// funzione per scrivere messaggi di log sul file broker_log
static void blog(const char *fmt, ...)
{
    if (broker_log == NULL)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(broker_log, fmt, ap);
    va_end(ap);
    fprintf(broker_log, "\n");
    fflush(broker_log);
}

// cleanup finale
static void broker_cleanup(int *fd_from_node, int *fd_to_node, int num_nodes)
{
    if (fd_from_node != NULL)
    {
        for (int i = 0; i < num_nodes; i++)
        {
            if (fd_from_node[i] >= 0)
            {
                close(fd_from_node[i]);
                fd_from_node[i] = -1;
            }
        }
        free(fd_from_node);
    }

    if (fd_to_node != NULL)
    {
        for (int i = 0; i < num_nodes; i++)
        {
            if (fd_to_node[i] >= 0)
            {
                close(fd_to_node[i]);
                fd_to_node[i] = -1;
            }
        }
        free(fd_to_node);
    }

    blog("BROKER: terminato");

    if (broker_log != NULL)
    {
        fclose(broker_log);
        broker_log = NULL;
    }
}

static void handle_signal(int sig)
{
    if (sig == SIGTERM || sig == SIGINT)
        running = 0;
}

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        fprintf(stderr, "Uso: %s <num_nodes>\n", argv[0]);
        return 1;
    }

    int num_nodes = (int)strtol(argv[1], NULL, 10);
    if (num_nodes <= 0)
    {
        fprintf(stderr, "BROKER: num_nodes non valido\n");
        return 1;
    }

    char logname[64];
    snprintf(logname, sizeof logname, "broker-%d.log", getpid());
    broker_log = fopen(logname, "w");

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    int *fd_from_node = malloc(sizeof(int) * num_nodes);
    int *fd_to_node = malloc(sizeof(int) * num_nodes);

    if (fd_from_node == NULL || fd_to_node == NULL)
    {
        fprintf(stderr, "BROKER: malloc fallita\n");
        free(fd_from_node);
        free(fd_to_node);
        return 1;
    }

    for (int i = 0; i < num_nodes; i++)
    {
        fd_from_node[i] = -1;
        fd_to_node[i] = -1;
    }

    for (int i = 0; i < num_nodes; i++)
    {
        char path[64];
        snprintf(path, sizeof(path), "%s%d", BROKER_NODE_FIFO, i);

        do
        {
            fd_to_node[i] = open(path, O_RDWR);
            if (fd_to_node[i] < 0 && errno != ENXIO && errno != ENOENT)
            {
                fprintf(stderr, "BROKER: open %s fallita: %s\n",
                        path, strerror(errno));
                broker_cleanup(fd_from_node, fd_to_node, num_nodes);
                return 1;
            }
            if (fd_to_node[i] < 0)
                usleep(10000);
        } while (fd_to_node[i] < 0);

        blog("BROKER: aperta FIFO broker->node_%d", i);
    }

    for (int i = 0; i < num_nodes; i++)
    {
        char path[64];
        snprintf(path, sizeof(path), "%s%d", NODE_BROKER_FIFO, i);

        do
        {
            fd_from_node[i] = open(path, O_RDONLY);
            if (fd_from_node[i] < 0 && errno != ENXIO && errno != ENOENT)
            {
                fprintf(stderr, "BROKER: open %s fallita: %s\n",
                        path, strerror(errno));
                broker_cleanup(fd_from_node, fd_to_node, num_nodes);
                return 1;
            }
            if (fd_from_node[i] < 0)
                usleep(10000);
        } while (fd_from_node[i] < 0);

        blog("BROKER: aperta FIFO node_%d->broker", i);
    }

    blog("BROKER: pronto, num_nodes=%d", num_nodes);
    blog("BROKER: sizeof(BrokerMessage)=%zu sizeof(BrokerResponse)=%zu PIPE_BUF=%d",
         sizeof(BrokerMessage), sizeof(BrokerResponse), PIPE_BUF);

    SeenBlockCache seen_blocks;
    memset(&seen_blocks, 0, sizeof(seen_blocks));

    while (running)
    {

        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;

        for (int i = 0; i < num_nodes; i++)
        {
            if (fd_from_node[i] < 0)
                continue;
            FD_SET(fd_from_node[i], &rfds);
            if (fd_from_node[i] > maxfd)
                maxfd = fd_from_node[i];
        }

        if (maxfd < 0)
        {
            blog("BROKER: tutte le FIFO chiuse, uscita");
            break;
        }

        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
        int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        if (ready < 0)
        {
            if (errno == EINTR)
                continue;
            blog("BROKER: select fallita: %s", strerror(errno));
            break;
        }
        if (ready == 0)
            continue;

        for (int i = 0; i < num_nodes; i++)
        {
            if (fd_from_node[i] < 0)
                continue;
            if (!FD_ISSET(fd_from_node[i], &rfds))
                continue;

            BrokerMessage msg;
            ssize_t rd = 0;
            char *buf = (char *)&msg;
            size_t total = sizeof(BrokerMessage);

            while (rd < (ssize_t)total)
            {
                ssize_t n = read(fd_from_node[i], buf + rd, total - rd);
                if (n == 0)
                {
                    blog("BROKER: FIFO node_%d chiusa", i);
                    close(fd_from_node[i]);
                    fd_from_node[i] = -1;
                    rd = -1;
                    break;
                }
                if (n < 0)
                {
                    if (errno == EINTR)
                        continue;
                    blog("BROKER: read da node_%d fallita: %s", i, strerror(errno));
                    rd = -1;
                    break;
                }
                rd += n;
            }

            if (fd_from_node[i] < 0)
                continue;

            if (rd != (ssize_t)total)
            {
                blog("BROKER: messaggio incompleto da node_%d (%zd/%zu bytes)",
                     i, rd, sizeof(BrokerMessage));
                continue;
            }
            if (msg.node_id < 0 || msg.node_id >= num_nodes)
            {
                blog("BROKER ERROR: node_id non valido da FIFO node_%d: msg.node_id=%d",
                     i, msg.node_id);
                continue;
            }

            if (msg.node_id != i)
            {
                blog("BROKER WARN: messaggio letto da FIFO node_%d ma contiene node_id=%d",
                     i, msg.node_id);
                continue;
            }

            char block_hash[HASH_HEX_SIZE + 1];
            if (block_hash_from_csv(msg.csv_line, block_hash) != 0)
            {
                blog("BROKER ERROR: blocco malformato da node_%d, scartato", msg.node_id);
                continue;
            }

            if (cache_contains(&seen_blocks, block_hash))
            {
                blog("BROKER: duplicato hash=%.16s... da node_%d, nessun broadcast",
                     block_hash, msg.node_id);
                continue;
            }
            cache_add(&seen_blocks, block_hash);

            blog("BROKER: blocco hash=%.16s... ricevuto da node_%d, broadcast a %d nodi",
                 block_hash, msg.node_id, num_nodes);

            BrokerResponse resp;
            memset(&resp, 0, sizeof(resp));
            resp.miner_id = msg.miner_id;
            strncpy(resp.csv_line, msg.csv_line, BLOCK_CSV_LINE_SIZE - 1);
            resp.csv_line[BLOCK_CSV_LINE_SIZE - 1] = '\0';

            if (resp.csv_line[0] == '\0')
            {
                blog("BROKER ERROR: BrokerResponse vuota, broadcast annullato");
                continue;
            }

            for (int j = 0; j < num_nodes; j++)
            {
                if (fd_to_node[j] < 0)
                    continue;

                blog("BROKER: invio a node_%d miner=%d csv='%.120s'",
                     j, resp.miner_id, resp.csv_line);

                ssize_t wr = 0;
                const char *wbuf = (const char *)&resp;
                size_t wtotal = sizeof(BrokerResponse);

                while (wr < (ssize_t)wtotal)
                {
                    ssize_t n = write(fd_to_node[j], wbuf + wr, wtotal - wr);
                    if (n < 0)
                    {
                        if (errno == EINTR)
                            continue;
                        blog("BROKER: write verso node_%d fallita: %s",
                             j, strerror(errno));
                        break;
                    }
                    wr += n;
                }
            }
        }
    }

    broker_cleanup(fd_from_node, fd_to_node, num_nodes);
    return 0;
}