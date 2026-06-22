// Contiene la struttura NodeContext che raccoglie tutto lo stato
// condiviso tra i moduli del node, eliminando le variabili globali.

#ifndef NODECONTEXT_H
#define NODECONTEXT_H

#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>

#include "../block.h"
#include "nodeStatus.h"

typedef struct NodeContext {
    int node_id;                    /* id logico del node (0-based)              */
    int num_nodes;                  /* numero totale di node nel sistema         */
    int num_miners;                 /* numero totale di miner nel sistema        */

    Block          *last_block;     /* puntatore all'ultimo blocco accettato   */
    uint64_t        chain_length;   /* numero di blocchi nella chain locale    */
    pthread_mutex_t chain_mutex;    /* protegge last_block e chain_length      */

    int *to_miner;                  /* fd di scrittura node→miner (uno per miner) */
    int *from_miner;                /* fd di lettura  miner→node  (uno per miner) */

    NodeStatus *status;             /* stato condiviso con NodeStatus              */

    FILE *log_file;                 /* file di log del processo                    */

    volatile sig_atomic_t running;  /* messo a 0 da SIGTERM/SIGINT             */

} NodeContext;

NodeContext *nodeContextCreate(void);

void nodeContextDestroy(NodeContext *ctx);

#endif 

