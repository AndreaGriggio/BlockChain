//
// Created by andrea on 17/06/26.
//


//system constants
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "childProcess.h"
#include "error.h"
#include "miner.h"
#include "minerStatus.h"
#include "minerCommunicationProtocol.h"
#include "transactionPool.h"

static MinerStatus* status = NULL;
static Miner* miner = NULL;

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t suspend = 1;


static int difficulty;
static int id;
static int num_nodes;
static int *to_node   = NULL;
static int *from_node = NULL;
static Block* previous_block = NULL;
//static BlockList* pending_blocks = NULL;
static int miner_id = -1;

static int fd_socket = -1;
static pthread_t mining_thread;
/*TODO: SI potrebbero spostare in un file separato passando:
*   1. Il numero di nodes, lo status per accedere all'id in alternativa direttamente l'id
*   2. i puntatore per i file descriptor
*/
static int createNodeFifos(const int num_nodes) {
    to_node   = (int*) malloc(sizeof(int) * num_nodes);
    from_node = (int*) malloc(sizeof(int) * num_nodes);

    if ( to_node == NULL || from_node == NULL ) {
        fprintf(stderr,"MINER : Allocare fifo comunicazione Miner->Nodi è troppo complicato");
        return -1;
    }

    ChildProcess* cp = childProcessCreate();

    if (cp == NULL){return -1;}

    mSGetCPChildProcess(status,cp);


    getCpId(cp,&miner_id);

    if (miner_id < 0){return -1;}

    for (int i = 0; i < num_nodes; i++) {
        char path_to[64];

        snprintf(path_to  , sizeof(path_to)   ,"%s%d%d"  ,MINER_NODE_FIFO ,miner_id ,i);

        mkfifo(path_to,0666);

        do {
            to_node[i]   = open(path_to, O_WRONLY | O_NONBLOCK);
            if (to_node[i] < 0 && errno != ENXIO) {
                fprintf(stderr,"open path_to");
                return -1;
            }
            if (to_node[i] < 0) usleep(10000);
        }while (to_node[i] < 0 );

        if (to_node[i] < 0 ) {
            fprintf(stderr,"MINER : errore creazione node fifo comunicazione");
            return -1;
        }
        if (fcntl(to_node[i], F_SETPIPE_SZ,  PIPE_BUF) < 0 )  return -1;

    }
    for (int i = 0; i < num_nodes; i++) {
        char  path_from [64];

        snprintf(path_from, sizeof(path_from) ,"%s%d%d"  ,NODE_MINER_FIFO ,i  ,miner_id);
        do {
            from_node[i] = open(path_from, O_RDONLY);
            if (from_node[i] < 0 && errno != ENXIO && errno != ENOENT) {
                perror("open path_to");
                return -1; // errore fatale, non retry
            }
            if (from_node[i] < 0) usleep(10000);
        }while (from_node[i] < 0);

        if (fcntl(from_node[i], F_SETPIPE_SZ,PIPE_BUF) < 0) return -1;

    }
    return 0;

}

static void closeAndDestroyFifos(const int num_nodes) {
    for (int i = 0; i < num_nodes;i ++) {
        if (to_node != NULL) {
            if (to_node[i] >= 0) close(to_node[i]);

            if (miner_id >= 0) {
                char path_to[64];
                snprintf(path_to, sizeof(path_to), "%s%d%d", MINER_NODE_FIFO, miner_id, i);
                unlink(path_to);
            }
        }
        if (from_node != NULL) {
            if (from_node[i] >= 0) close(from_node[i]);
        }
    }
    free(to_node);   to_node   = NULL;
    free(from_node); from_node = NULL;
}


typedef struct MiningThreadArgs {
    Miner*           miner;
    MinerStatus*     status;

} MiningThreadArgs;
/**
 *
 *
 * @param sig segnale in ingresso
 *
 */
static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        running = 0;
    }
}


//Chiamate di funzioni che servono per arrestare o fare partire il thread di mining
/**
 *Inizia il processo di mining
 * @param arg miner che inizia il processo di calcolo del blocco
 * @return 0 se tutto è andato bene
 */
static void* minerMining_Thread_Entry_point(void* arg) {
    MiningThreadArgs* args = (MiningThreadArgs*)arg;
    minerMiningLoop(args->miner,
                    args->status);
    return NULL;
}

static void startMining(pthread_t* mining_thread,void* args) {
    pthread_create( mining_thread,
            NULL,
            minerMining_Thread_Entry_point,
            args);
}


/**
 *Ferma il processo di mining
 * @param mining_thread thread da aspettare per fermare il mining
 * @return 0 se tutto è andato bene
 */
static int stopMining(const pthread_t* mining_thread) {
    msSignal(status,MINER_STOPPED);
    pthread_join(*mining_thread, NULL);
    return 0;
}

static int pauseMining(void) {

    msSignal(status,MINER_IDLE);
    return 0;
}
static int restartMining(void) {

    msSignal(status,MINER_RESTART);
    return 0;

}
static int init(void) {
    struct sigaction sa ;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT,  &sa, NULL) == -1) { perror("sigaction SIGINT"); return 1; }
    if (sigaction(SIGTERM, &sa, NULL) == -1) { perror("sigaction SIGTERM");        return 1; }

    ChildProcess* childProcess = childProcessCreate();

    if (childProcess == NULL) {
        fprintf(stderr,"MINER %d : Error creating child process\n",id);
        return 1;
    }

    if (childProcessInit(childProcess, getpid(),id,MINER) != 0) {
        fprintf(stderr,"MINER %d : Error initializing child process\n",id);
        free(childProcess);
        return 1;
    }
    status = minerCreateStatus();

    minerInitStatus(status,childProcess,MINER_IDLE,0,0);

    /* Le FIFO leggono miner_id da status->cp: vanno create DOPO minerInitStatus. */
    if (createNodeFifos(num_nodes) != 0) {
        fprintf(stderr,"MINER %d : errore creazione FIFO nodi\n",id);
        return 1;
    }

    //creo la struttura dati per accogliere il blocco
    miner = minerCreate();
    if (miner == NULL) {
        fprintf(stderr,"MINER %d : Error allocation miner",id);
        return 1;
    }

    if (minerInit(miner, difficulty) != 0) {
        fprintf(stderr,"MINER %d : Init error",id);
        free(miner);
        free(childProcess);
        return 1;
    };

    /* static: il thread di mining usa &args dopo che init() è ritornata. */
    static MiningThreadArgs args;
    args.miner  = miner;
    args.status = status;

    startMining(&mining_thread,&args);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path,
        MINERS_SOCKET,
          sizeof(addr.sun_path)-1);


    int tries = 0;
    int res = 0;

    do {
        fd_socket = socket(AF_UNIX,SOCK_STREAM,0);
        tries ++;
        usleep(10000);
        if (tries > MAX_CONNECTION_TRIES) return SOCKET_ERROR;
    }while (fd_socket < 0 );

    tries = 0;

    do {
        res = connect (fd_socket,(struct sockaddr *) &addr,sizeof(addr));
        tries ++;
        usleep(10000);
        if (tries > MAX_CONNECTION_TRIES) return SOCKET_ERROR;
    }while (res < 0);

    return 0;
}

static int sendBlockToNodes(void) {

    for (int i = 0; i < num_nodes; i++) {

        int tries = 0;
        int res = 0;

        do {
             res = sendBlockToNode(previous_block,status,to_node[i]);
            if ( res == INVALID_PARAMS) break;
            tries ++;
        }while (res != 0 && tries < MAX_CONNECTION_TRIES);
    }
}
int main(int argc, char ** argv) {

    if (argc < 4) {
        fprintf(stderr, "Utilizzo : %s <difficulty> <id>  \n",argv[0]);
        return 1;
    }

    difficulty = atoi(argv[1]);
    id = atoi(argv[2]);
    num_nodes = atoi(argv[3]);

    if (id < 0) {
        fprintf(stderr,"ID non valida ex. id > 0\n");
        return 1;
    }

    if (difficulty <= 0 ) {
        fprintf(stderr,"MINER %d : Difficulty non valida ex. difficulty > 0\n",id);
        return 1;
    }

    if ( num_nodes < 0 ) {
        fprintf(stderr,"MINER %d : fd_count non valido ex. nodi_count > 0\n",id);
        return 1;
    }

    if (init() != 0) {
        fprintf(stderr,"MINER %d : init fallita\n",id);
        return 1;
    }


    //Strutture dati per gestire un blocco

    TransactionPool* trxs= createTransactionPool();
    MinerState current_state = MINER_IDLE;


    while (1) {
        if (suspend == 0) break;

            while (running) {


                /*quello che deve fare il miner è :
                *1. Ascoltare periodicamente sulla socket se è stato segnalato con un segnale comune quando vengono caricate informazioni
                *2. Minare come un dannato e provare a risolvere il blocco -> thread separato su cui questo while ha completo controllo
                */


                receiveTransactionFromClient(fd_socket,trxs);

                mSSetTransactionCount(status, trxs->count);

                mSGetState(status,&current_state);

                if ( trxs->count != 0 && current_state == MINER_BLOCK_FOUND) {
                    for (int i = 0; i<MAX_TX_PER_BLOCK; i ++ ) {
                        minerGetBlock(miner,previous_block);

                        blockAddTransaction(previous_block,poolRemoveLast(trxs));
                    }
                }


                sendBlockToNodes();


            }

        if ( suspend == 0) break;
    }


    close(fd_socket);
    stopMining(&mining_thread);
    closeAndDestroyFifos(num_nodes);
    return 0;
}

int miner_communication_main(int argc,char * argv[]) {
    return main(argc,argv);
}


