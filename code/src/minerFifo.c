//
// Created by andrea on 21/06/26.
//

#define _GNU_SOURCE
#include "minerFifo.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "constants.h"
#include "error.h"

int nodeChannelsOpen(NodeChannels *ch, const int num_nodes, const int miner_id) {
    if (ch == NULL || num_nodes < 0 || miner_id < 0) return INVALID_PARAMS;

    ch->to_node   = malloc(sizeof(int) * num_nodes);
    ch->from_node = malloc(sizeof(int) * num_nodes);
    ch->num_nodes = num_nodes;
    ch->miner_id  = miner_id;

    if (ch->to_node == NULL || ch->from_node == NULL) {
        fprintf(stderr,"MINER : Allocare fifo comunicazione Miner->Nodi è troppo complicato");
        free(ch->to_node);   ch->to_node   = NULL;
        free(ch->from_node); ch->from_node = NULL;
        return MEMORY_ERROR;
    }

    /* Inizializzo i descrittori a -1 così la close è sicura anche su apertura parziale. */
    for (int i = 0; i < num_nodes; i++) {
        ch->to_node[i]   = -1;
        ch->from_node[i] = -1;
    }

    for (int i = 0; i < num_nodes; i++) {
        char path_to[64];

        snprintf(path_to  , sizeof(path_to)   ,"%s%d%d"  ,MINER_NODE_FIFO ,miner_id ,i);

        mkfifo(path_to,0666);

        do {
            ch->to_node[i]   = open(path_to, O_WRONLY | O_NONBLOCK);
            if (ch->to_node[i] < 0 && errno != ENXIO) {
                fprintf(stderr,"open path_to");
                return FIFO_ERROR;
            }
            if (ch->to_node[i] < 0) usleep(10000);
        }while (ch->to_node[i] < 0 );

        if (fcntl(ch->to_node[i], F_SETPIPE_SZ,  PIPE_BUF) < 0 )  return FIFO_ERROR;

    }
    for (int i = 0; i < num_nodes; i++) {
        char  path_from [64];

        snprintf(path_from, sizeof(path_from) ,"%s%d%d"  ,NODE_MINER_FIFO ,i  ,miner_id);
        do {
            ch->from_node[i] = open(path_from, O_RDONLY);
            if (ch->from_node[i] < 0 && errno != ENXIO && errno != ENOENT) {
                perror("open path_from");
                return FIFO_ERROR; // errore fatale, non retry
            }
            if (ch->from_node[i] < 0) usleep(10000);
        }while (ch->from_node[i] < 0);

        if (fcntl(ch->from_node[i], F_SETPIPE_SZ,PIPE_BUF) < 0) return FIFO_ERROR;

    }
    return 0;
}

void nodeChannelsClose(NodeChannels *ch) {
    if (ch == NULL) return;

    for (int i = 0; i < ch->num_nodes; i ++) {
        if (ch->to_node != NULL) {
            if (ch->to_node[i] >= 0) close(ch->to_node[i]);

            if (ch->miner_id >= 0) {
                char path_to[64];
                snprintf(path_to, sizeof(path_to), "%s%d%d", MINER_NODE_FIFO, ch->miner_id, i);
                unlink(path_to);
            }
        }
        if (ch->from_node != NULL) {
            if (ch->from_node[i] >= 0) close(ch->from_node[i]);
        }
    }
    free(ch->to_node);   ch->to_node   = NULL;
    free(ch->from_node); ch->from_node = NULL;
    ch->num_nodes = 0;
}
