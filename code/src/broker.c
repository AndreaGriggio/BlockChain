#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "broker.h"
#include "constants.h"
#include "error.h"

static volatile sig_atomic_t running = 1;

static void broker_cleanup(int *fd_from_node, int *fd_to_node,
                            pid_t *node_pids, int num_nodes) {
    if (fd_from_node != NULL) {
        for (int i = 0; i < num_nodes; i++) {
            if (fd_from_node[i] >= 0) {
                close(fd_from_node[i]);
                fd_from_node[i] = -1;
            }
        }
        free(fd_from_node);
    }

    if (fd_to_node != NULL) {
        for (int i = 0; i < num_nodes; i++) {
            if (fd_to_node[i] >= 0) {
                close(fd_to_node[i]);
                fd_to_node[i] = -1;
            }
        }
        free(fd_to_node);
    }

    if (node_pids != NULL) {
        free(node_pids);
    }

    fprintf(stderr, "BROKER: terminato\n");
}

static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) running = 0;
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <num_nodes>\n", argv[0]);
        return 1;
    }

    int num_nodes = (int)strtol(argv[1], NULL, 10);
    if (num_nodes <= 0) {
        fprintf(stderr, "BROKER: num_nodes non valido\n");
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);

    int   *fd_from_node = malloc(sizeof(int)   * num_nodes);
    int   *fd_to_node   = malloc(sizeof(int)   * num_nodes);
    pid_t *node_pids    = malloc(sizeof(pid_t) * num_nodes);

    if (fd_from_node == NULL || fd_to_node == NULL || node_pids == NULL) {
        fprintf(stderr, "BROKER: malloc fallita\n");
        free(fd_from_node);
        free(fd_to_node);
        free(node_pids);
        return 1;
    }

    for (int i = 0; i < num_nodes; i++) {
        fd_from_node[i] = -1;
        fd_to_node[i]   = -1;
        node_pids[i]    = -1;
    }

    for (int i = 0; i < num_nodes; i++) {
        char path[64];
        snprintf(path, sizeof(path), "%s%d", BROKER_NODE_FIFO, i);

        do {
            fd_to_node[i] = open(path, O_RDWR);
            if (fd_to_node[i] < 0 && errno != ENXIO && errno != ENOENT) {
                fprintf(stderr, "BROKER: open %s fallita: %s\n",
                        path, strerror(errno));
                broker_cleanup(fd_from_node, fd_to_node, node_pids, num_nodes);
                return 1;
            }
            if (fd_to_node[i] < 0) usleep(10000);
        } while (fd_to_node[i] < 0);

        fprintf(stderr, "BROKER: aperta FIFO broker->node_%d\n", i);
    }

    for (int i = 0; i < num_nodes; i++) {
        char path[64];
        snprintf(path, sizeof(path), "%s%d", NODE_BROKER_FIFO, i);

        do {
            fd_from_node[i] = open(path, O_RDONLY);
            if (fd_from_node[i] < 0 && errno != ENXIO && errno != ENOENT) {
                fprintf(stderr, "BROKER: open %s fallita: %s\n",
                        path, strerror(errno));
                broker_cleanup(fd_from_node, fd_to_node, node_pids, num_nodes);
                return 1;
            }
            if (fd_from_node[i] < 0) usleep(10000);
        } while (fd_from_node[i] < 0);

        fprintf(stderr, "BROKER: aperta FIFO node_%d->broker\n", i);
    }

    fprintf(stderr, "BROKER: pronto, num_nodes=%d\n", num_nodes);
    fprintf(stderr,
        "BROKER: sizeof(BrokerMessage)=%zu sizeof(BrokerResponse)=%zu\n",
        sizeof(BrokerMessage), sizeof(BrokerResponse));

    


    while (running) {

        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;

        for (int i = 0; i < num_nodes; i++) {
            if (fd_from_node[i] < 0) continue;
            FD_SET(fd_from_node[i], &rfds);
            if (fd_from_node[i] > maxfd) maxfd = fd_from_node[i];
        }

        if (maxfd < 0) {
            fprintf(stderr, "BROKER: tutte le FIFO chiuse, uscita\n");
            break;
        }

        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);

        if (ready < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "BROKER: select fallita: %s\n", strerror(errno));
            break;
        }
        if (ready == 0) continue;

        for (int i = 0; i < num_nodes; i++) {
            if (fd_from_node[i] < 0)               continue;
            if (!FD_ISSET(fd_from_node[i], &rfds)) continue;

            BrokerMessage msg;
            ssize_t rd = 0;
            char *buf = (char *)&msg;
            size_t total = sizeof(BrokerMessage);

            while (rd < (ssize_t)total) {
                ssize_t n = read(fd_from_node[i], buf + rd, total - rd);
                if (n == 0) {
                    fprintf(stderr, "BROKER: FIFO node_%d chiusa\n", i);
                    close(fd_from_node[i]);
                    fd_from_node[i] = -1;
                    rd = -1;
                    break;
                }
                if (n < 0) {
                    if (errno == EINTR) continue;
                    fprintf(stderr, "BROKER: read da node_%d fallita: %s\n",
                            i, strerror(errno));
                    rd = -1;
                    break;
                }
                rd += n;
            }

            if (fd_from_node[i] < 0) continue;  /* FIFO chiusa */

            if (rd != (ssize_t)total) {
                fprintf(stderr, "BROKER: messaggio incompleto da node_%d "
                        "(%zd/%zu bytes)\n", i, rd, sizeof(BrokerMessage));
                continue;
            }
            if (msg.node_id < 0 || msg.node_id >= num_nodes) {
                fprintf(stderr,
                        "BROKER ERROR: node_id non valido da FIFO node_%d: msg.node_id=%d\n",
                        i, msg.node_id);
                continue;
            }

            if (msg.node_id != i) {
                fprintf(stderr,
                        "BROKER WARN: messaggio letto da FIFO node_%d ma contiene node_id=%d\n",
                        i, msg.node_id);
                continue;
            }

            if (node_pids[msg.node_id] == -1 && msg.sender_pid > 0) {
                node_pids[msg.node_id] = msg.sender_pid;
                fprintf(stderr, "BROKER: registrato PID node_%d = %d\n",
                        msg.node_id, (int)msg.sender_pid);
            }

            /* se csv_line è vuota è solo una registrazione, nessun broadcast */
            if (msg.csv_line[0] == '\0') {
                fprintf(stderr, "BROKER: registrazione da node_%d, nessun broadcast\n",
                        msg.node_id);
                continue;
            }

            fprintf(stderr, "BROKER: blocco ricevuto da node_%d, "
                    "broadcast a %d nodi\n", msg.node_id, num_nodes);

            BrokerResponse resp;
            memset(&resp, 0, sizeof(resp));
            resp.miner_id = msg.miner_id;
            strncpy(resp.csv_line, msg.csv_line, BLOCK_CSV_LINE_SIZE - 1);
            resp.csv_line[BLOCK_CSV_LINE_SIZE - 1] = '\0';

            if (resp.csv_line[0] == '\0') {
                fprintf(stderr, "BROKER ERROR: BrokerResponse vuota, broadcast annullato\n");
                continue;
            }

            for (int j = 0; j < num_nodes; j++) {
                if (fd_to_node[j] < 0) continue;

                fprintf(stderr,
                    "BROKER: invio a node_%d miner=%d csv='%.120s'\n",
                    j, resp.miner_id, resp.csv_line);

                ssize_t wr = 0;
                const char *wbuf = (const char *)&resp;
                size_t wtotal = sizeof(BrokerResponse);

                while (wr < (ssize_t)wtotal) {
                    ssize_t n = write(fd_to_node[j], wbuf + wr, wtotal - wr);
                    if (n < 0) {
                        if (errno == EINTR) continue;
                        fprintf(stderr, "BROKER: write verso node_%d fallita: %s\n",
                                j, strerror(errno));
                        break;
                    }
                    wr += n;
                }

                if (node_pids[j] > 0) {
                    if (kill(node_pids[j], SIGUSR1) < 0) {
                        fprintf(stderr,
                                "BROKER: kill(node_%d, SIGUSR1) fallita: %s\n",
                                j, strerror(errno));
                    }
                }
            }
        }
    }

    broker_cleanup(fd_from_node, fd_to_node, node_pids, num_nodes);
    return 0;
}