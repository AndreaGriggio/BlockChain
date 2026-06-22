#define _GNU_SOURCE

#include "nodeContext.h"
#include "nodeLog.h"
#include "nodeCSV.h"
#include "nodeFIFO.h"
#include "nodeListener.h"
#include "nodeValidation.h"
#include "nodeStatus.h"
#include "childProcess.h"
#include "error.h"
#include "constants.h"
 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>


static NodeContext *g_ctx = NULL;


static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        if (g_ctx != NULL) g_ctx->running = 0;
    }
}


int main (int argc, char* argv[]){

    // Il processo node viene avviato dal bootstrap/launcher

    if (argc < 4 ) {
        fprintf(stderr, "Utilizzo: %s <node_id> <num_nodes> <num_miners>\n",argv[0]);
    return INVALID_PARAMS;
    }


    int node_id= atoi(argv[1]); // identifica il nodo
    int num_nodes = atoi(argv[2]); // totale del numero di nodi nel sistema
    int num_miners = atoi(argv[3]); // numero totale di miners (serve per aprire le fifo verso i miner)


    if (node_id < 0 || 
        num_nodes <= 0 || 
        num_miners <= 0 || 
        node_id >= num_nodes){
            fprintf(stderr, "NODE: argomenti non validi\n");
            return INVALID_PARAMS;
    }

    char log_path[64];  // ogni processo deve avere il suo file di log
    snprintf(log_path, 
            sizeof(log_path),
            "node-%d.log",
            (int) getpid()); // usiamo il pid reale
    FILE *log_file = fopen(log_path, "a");
    if (log_file == NULL) {
        fprintf(stderr, "NODE %d: impossibile aprire il log%s:%s\n", 
            node_id, 
            log_path, 
            strerror(errno));
        return -1;
    }


    /* creo e inizializzo il contesto */
    NodeContext *ctx = nodeContextCreate();
    if (ctx == NULL) {
        fprintf(stderr, "NODE %d: nodeContextCreate fallita\n", node_id);
        fclose(log_file);
        return -1;
    }
    ctx->node_id    = node_id;
    ctx->num_nodes  = num_nodes;
    ctx->num_miners = num_miners;
    ctx->log_file   = log_file;
 
    g_ctx = ctx;

    /*Singal handler : se arriva sigterm o sigint, 
    handle_signal mette runnin g = 0
    In modo che il processo esca in modo sicuro
    */ 
    
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if(sigaction(SIGTERM, &sa, NULL) == -1 ||
       sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "NODE %d: sigaction fallita: %s\n",
            node_id,
            strerror(errno));
        fclose(log_file);
        return -1;
       }

    log_msg(ctx, "Avvio node id=%d num_nodes=%d num_miners=%d",
            node_id, num_nodes, num_miners);

    /*
    NodeStatus contiene lo stato logico del node
    ovvero non possedendo i blocchi: conserva solo riferimenti e metadati
    protetti da un mutex
    */

    ctx->status = nodeCreateStatus();
    if(ctx->status == NULL) {
        log_msg(ctx, "ERROR: nodeCreateStatus fallita ");
        fclose(log_file);
        ctx->log_file = NULL;
        nodeContextDestroy(ctx);
        g_ctx = NULL;
        return -1;
    }
    
    /*
    ChildProcess descrive questo processo dal punto di vista logico
    ovvero pid reale ma id logico e ruolo NODE
    */

    ChildProcess * cp = childProcessCreate();
    if (cp == NULL) {
        log_msg(ctx,"ERROR: childProcessCreate fallita");
        nodeDestroyStatus(ctx->status);
        fclose(log_file);
        ctx->log_file = NULL;
        nodeContextDestroy(ctx);
        g_ctx = NULL;
        return -1;
    }

    if(childProcessInit(cp,getpid(),node_id,NODE) != 0){
        log_msg(ctx,"ERROR: childProcessInit fallita");
        childProcessDestroy(cp);
        nodeDestroyStatus(ctx->status);
        fclose(log_file);
        ctx->log_file = NULL;
        nodeContextDestroy(ctx);
        g_ctx = NULL;
        return INVALID_PARAMS;
    }

    /*
    Copio le informazioni del ChildProcess dentro NodeStatus
    In modo che NodeStatus abbia una copia , e quindi possiamo distruggere cp
    */

    if (nodeInitStatus(ctx->status, cp, NODE_IDLE, 0) != 0) {
        log_msg(ctx, "ERROR: nodeInitStatus fallita");
        childProcessDestroy(cp);
        nodeDestroyStatus(ctx->status);
        fclose(log_file);
        ctx->log_file = NULL;
        nodeContextDestroy(ctx);
        g_ctx = NULL;
        return INVALID_PARAMS;
    }

    childProcessDestroy(cp);

    /* 
    Carico la blockchain iniziale dal CVS condiviso
    Il bootstrap crea questo file prima di lanciare i node 
    */

     if (load_initial_state(ctx, CSV_FILE_NAME) != 0) {
        log_msg(ctx, "ERROR: load_initial_state fallita");
        nodeDestroyStatus(ctx->status);
        fclose(log_file);
        ctx->log_file = NULL;
        nodeContextDestroy(ctx);
        g_ctx = NULL;
        return CSV_ERROR;
    }

    /*
    Sincronizzazione di NodeStatus con lo stato caricato da CSV
    l'ultimo blocco rimane comunque a node.c mentre
    NodeStatus possiede il puntatore const
    */

    pthread_mutex_lock(&ctx->chain_mutex);
    const Block *loaded_last_block = ctx->last_block;
    uint64_t loaded_chain_length = ctx->chain_length;
    pthread_mutex_unlock(&ctx->chain_mutex);

    if(loaded_last_block != NULL) {
        nSSetLastBlock(ctx->status, loaded_last_block);
    }
    nSSetChainLength(ctx->status,loaded_chain_length);

    /*
    Creazione del canale di comunicazione node -> Miners    
    */

    if(createNodeFifos(ctx,num_miners) != 0){
        log_msg(ctx, "ERROR: createNodeFifos fallita");
        nodeDestroyStatus(ctx->status);
        if (ctx->last_block != NULL) blockDestroy(ctx->last_block);
        fclose(log_file);
        ctx->log_file = NULL;
        nodeContextDestroy(ctx);
        g_ctx = NULL;
        return FIFO_ERROR;
    }

    /*
    Il thread di comunicazione resta in ascolto sul canale dei miner
    */

    pthread_t listener;
    if(pthread_create(&listener, NULL, listener_thread, ctx) != 0){
        log_msg(ctx, "ERROR: pthread_create listener fallita");
        destroyNodeFifos(ctx,num_miners);
        nodeDestroyStatus(ctx->status);
        if (ctx->last_block != NULL) blockDestroy(ctx->last_block);
        fclose(log_file);
        ctx->log_file = NULL;
        nodeContextDestroy(ctx);
        g_ctx = NULL;
        return -1;
    }

    log_msg(ctx, "Node avviato correttamente");

    /*
    Il processo si ferma solo in caso di SIGTERM o SIGINT
    */

    while (ctx->running) {
        pause();
    }
    log_msg(ctx, "Terminazione del blocco richiesto");

    /*
    Appena il thred di comunicazione termina , rilasciamo le risorse condivise
    */

    pthread_join(listener, NULL);

    destroyNodeFifos(ctx,num_miners);
    nodeDestroyStatus(ctx->status);

    if (ctx->last_block != NULL) {
        blockDestroy(ctx->last_block);
        ctx->last_block = NULL;
    }

    log_msg(ctx, "Node terminato");
 
    fclose(log_file);
    ctx->log_file = NULL;
 
    nodeContextDestroy(ctx);
    g_ctx = NULL;
 
    return 0;
}