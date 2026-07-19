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
#include "broker.h"
 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/select.h>


static NodeContext *g_ctx = NULL;


static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        if (g_ctx != NULL) g_ctx->running = 0;
    }
}

static void handle_sigusr1(int sig) {
    (void)sig;
    if (g_ctx != NULL) g_ctx->pending_broker = 1;
}

static int read_broker_response(NodeContext *ctx, BrokerResponse *resp) {
    if (ctx == NULL || resp == NULL) return -1;

    memset(resp, 0, sizeof(*resp));

    ssize_t rd = 0;
    char *buf = (char *)resp;
    size_t total = sizeof(*resp);

    while (rd < (ssize_t)total) {
        ssize_t n = read(ctx->fd_from_broker, buf + rd, total - rd);

        if (n == 0) {
            log_msg(ctx, "FIFO broker chiusa, uscita");
            ctx->running = 0;
            return -1;
        }

        if (n < 0) {
            if (errno == EINTR) continue;
            log_msg(ctx, "ERROR: read broker fallita: %s", strerror(errno));
            return -1;
        }

        rd += n;
    }

    return 0;
}

static void process_broker_response(NodeContext *ctx, const BrokerResponse *resp) {
    if (ctx == NULL || resp == NULL) return;

    if (resp->csv_line[0] == '\0') {
        log_msg(ctx, "WARN: BrokerResponse vuota, ignorata");
        return;
    }

    Block *broker_block = blockCreate();
    if (broker_block == NULL) {
        log_msg(ctx, "ERROR: blockCreate fallita");
        return;
    }

    if (blockFromCsv(broker_block, resp->csv_line) != 0) {
        log_msg(ctx, "ERROR: blockFromCsv fallita su BrokerResponse: '%.160s'",
                resp->csv_line);
        blockDestroy(broker_block);
        return;
    }

    uint64_t block_index = 0;
    char block_hash[HASH_HEX_SIZE + 1];

    blockGetIndex(broker_block, &block_index);
    blockGetHash(broker_block, block_hash);

    int rc = commit_block_to_local_csv(ctx, broker_block);
    blockDestroy(broker_block);

    if (rc == 0) {
        log_msg(ctx, "Blocco index=%llu accettato, notifico miner",
                (unsigned long long)block_index);

        notify_all_miners(ctx, block_index, block_hash, BLOCK_VALID);

    } else if (rc == BLOCK_ALREADY_PRESENT) {
        log_msg(ctx, "Blocco index=%llu ridondante, recupero transazioni per miner %d",
                (unsigned long long)block_index, resp->miner_id);

        if (resp->miner_id >= 0
            && resp->miner_id < ctx->num_miners
            && ctx->to_miner[resp->miner_id] >= 0) {

            
            log_msg(ctx, "ERROR: notify BLOCK_RECOVER_TXS al miner %d fallita",
                    resp->miner_id);
            
        }

    } else {
        log_msg(ctx, "Blocco index=%llu non accettato (rc=%d)",
                (unsigned long long)block_index, rc);
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
    ctx->fd_to_broker   = -1;
    ctx->fd_from_broker = -1;
    ctx->pending_broker = 0;
 
    g_ctx = ctx;

    /*Singal handler : se arriva sigterm o sigint, 
    handle_signal mette runnin g = 0
    In modo che il processo esca in modo sicuro
    */ 
    
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if(sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "NODE %d: sigaction fallita: %s\n",
            node_id,
            strerror(errno));
        fclose(log_file);
        return -1;
    }
    
    struct sigaction sa_usr1;
    sa_usr1.sa_handler = handle_sigusr1;
    sigemptyset(&sa_usr1.sa_mask);
    sa_usr1.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa_usr1, NULL) == -1) {
        fprintf(stderr, "NODE %d: sigaction SIGUSR1 fallita: %s\n",
                node_id, strerror(errno));
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

    if (openBrokerFifos(ctx) != 0) {
        log_msg(ctx, "ERROR: openBrokerFifos fallita");
        destroyNodeFifos(ctx, num_miners);
        nodeDestroyStatus(ctx->status);
        if (ctx->last_block != NULL) blockDestroy(ctx->last_block);
        fclose(log_file);
        ctx->log_file = NULL;
        nodeContextDestroy(ctx);
        g_ctx = NULL;
        return FIFO_ERROR;
    }

    /* registrazione PID al broker: il broker deve conoscere il PID
    * di tutti i nodi prima di ricevere il primo blocco, altrimenti
    * non può mandare SIGUSR1 ai nodi che non hanno ancora scritto */
    BrokerMessage reg;
    memset(&reg, 0, sizeof(reg));
    reg.msg_type   = BROKER_MSG_BLOCK;
    reg.node_id    = ctx->node_id;
    reg.sender_pid = getpid();
    reg.csv_line[0] = '\0';   /* stringa vuota: solo registrazione, nessun blocco */

    ssize_t wr = write(ctx->fd_to_broker, &reg, sizeof(BrokerMessage));
    if (wr != (ssize_t)sizeof(BrokerMessage)) {
        log_msg(ctx, "ERROR: registrazione PID al broker fallita");
    } else {
        log_msg(ctx, "PID registrato al broker");
    }

    /*
    Il thread di comunicazione resta in ascolto sul canale dei miner
    */

    pthread_t listener;
    if(pthread_create(&listener, NULL, listener_thread, ctx) != 0){
        log_msg(ctx, "ERROR: pthread_create listener fallita");
        closeBrokerFifos(ctx);
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
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(ctx->fd_from_broker, &rfds);

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ready = select(ctx->fd_from_broker + 1, &rfds, NULL, NULL, &tv);

        if (ready < 0) {
            if (errno == EINTR) continue;
            log_msg(ctx, "ERROR: select broker fallita: %s", strerror(errno));
            break;
        }

        if (ready == 0) {
            continue;
        }

        if (!FD_ISSET(ctx->fd_from_broker, &rfds)) {
            continue;
        }

        BrokerResponse resp;
        if (read_broker_response(ctx, &resp) != 0) {
            continue;
        }

        log_msg(ctx, "BrokerResponse ricevuta: miner=%d csv='%.120s'",
                resp.miner_id, resp.csv_line);

        process_broker_response(ctx, &resp);
    }


    log_msg(ctx, "Terminazione richiesta");

    /*
    Appena il thred di comunicazione termina , rilasciamo le risorse condivise
    */

    pthread_join(listener, NULL);

    closeBrokerFifos(ctx);       
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