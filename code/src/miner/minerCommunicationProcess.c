//
// Created by andrea on 17/06/26.
//


//system constants
#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "blocksPool.h"
#include "childProcess.h"
#include "error.h"
#include "miner.h"
#include "minerStatus.h"
#include "minerFifo.h"
#include "minerThread.h"
#include "minerCommunicationProtocol.h"
#include "transactionPool.h"
#include "../utils.h"


static MinerStatus* status = NULL;
static Miner* miner = NULL;

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t suspend = 1;


static int difficulty;
static int id;
static int num_nodes;

static NodeChannels channels = {0};
static Block* previous_block = NULL;

//static BlockList* pending_blocks = NULL;

static int fd_socket = -1;

static pthread_t mining_thread;

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

/**
 * Inizializza il processo miner: installa gli handler dei segnali, crea e
 * inizializza child process, status e miner, apre le FIFO verso i nodi, avvia il
 * thread di mining e valida il descrittore della socket in ascolto ereditato dal padre.
 * @return 0 se tutto è andato a buon fine, valore non nullo (1 o SOCKET_ERROR) in
 *         caso di errore
 */
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

    /* L'id del miner serve a comporre i path delle FIFO verso i nodi. */
    int miner_id = -1;
    getCpId(childProcess, &miner_id);
    if (miner_id < 0) {
        fprintf(stderr,"MINER %d : id child process non valido\n",id);
        return 1;
    }

    if (nodeChannelsOpen(&channels, num_nodes, miner_id) != 0) {
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

    minerThreadStart(&mining_thread,&args);

    /* La socket dei client e' gia' stata creata dal padre (bind+listen) e il suo
     * descrittore ci e' arrivato via argv[4] in fd_socket: qui faremo solo accept().
     * Non creiamo ne' connettiamo alcuna socket. */
    if (fd_socket < 0) {
        fprintf(stderr,"MINER %d : descrittore socket in ascolto non valido\n",id);
        return SOCKET_ERROR;
    }

    return 0;
}

/**
 * Invia il blocco corrente (previous_block) a tutti i nodi, ritentando l'invio
 * fino a MAX_CONNECTION_TRIES volte per ciascun nodo in caso di errore.
 * @return 0 al termine del tentativo di invio verso tutti i nodi
 */
static int sendBlockToNodes(void) {

    for (int i = 0; i < num_nodes; i++) {

        int tries = 0;
        int res = 0;

        do {
             res = sendBlockToNode(previous_block,status,channels.to_node[i]);
            if ( res == INVALID_PARAMS) break;
            tries ++;
        }while (res != 0 && tries < MAX_CONNECTION_TRIES);
    }
    return 0;
}
/**
 * Entry point del processo miner: valida gli argomenti (difficoltà, id, numero di
 * nodi), esegue l'inizializzazione e gira il ciclo principale ricevendo
 * transazioni dal client, gestendo lo stato del mining e inviando i blocchi ai
 * nodi; al termine rilascia socket, thread di mining e FIFO.
 * @param argc Numero di argomenti da riga di comando
 * @param argv Argomenti: [1] difficoltà, [2] id, [3] numero di nodi
 * @return 0 in caso di terminazione corretta, 1 in caso di errore
 */
int main(int argc, char ** argv) {

    if (argc < 5) {
        fprintf(stderr, "Utilizzo : %s <difficulty> <id> <num_nodes> <listen_fd>\n",argv[0]);
        return 1;
    }

    difficulty = atoi(argv[1]);
    id = atoi(argv[2]);
    num_nodes = atoi(argv[3]);
    fd_socket = atoi(argv[4]);

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
    if ( fd_socket < 0 ) {
        fprintf(stderr, "MINER %d : fd_socket non valida ex. fd_socket > 0\n",id);
        return 1;
    }

    if (init() != 0) {
        fprintf(stderr,"MINER %d : init fallita\n",id);
        return 1;
    }


    //Strutture dati per gestire un blocco

    TransactionPool* trxs= createTransactionPool();
    MinerState current_state = MINER_IDLE;
    BlocksPool* readyPool = createBlocksPool();
    readyPool->poolState = BLOCK_WAITING;

    BlocksPool* confirmedPool = createBlocksPool();
    confirmedPool->poolState = BLOCK_CONFIRMED;

    int compiled_block = 0;


    while (1) {
        if (suspend == 0) break;

            while (running) {

                /* Una connessione = una transazione (il client fa connect->send->close).
                 * select con timeout per restare responsivi verso mining e nodi. */
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(fd_socket, &rfds);
                struct timeval tv = { .tv_sec = 0, .tv_usec = 100000 };

                if (select(fd_socket + 1, &rfds, NULL, NULL, &tv) > 0
                    && FD_ISSET(fd_socket, &rfds)) {
                    int conn_fd = accept(fd_socket, NULL, NULL);
                    if (conn_fd >= 0) {
                        receiveTransactionFromClient(conn_fd, trxs);
                        close(conn_fd);
                    }
                }

                minerThreadPause(status);
                //prendo lo stato del miner
                mSGetState(status,&current_state);


                //condizioni necessarie per compilare un blocco prima dell'invio
                if ( trxs->count != 0 && current_state == MINER_BLOCK_FOUND) {
                    uint64_t i = 0;

                    Block* block;//prendiamo un nuovo blocco
                    u_int64_t index;
                    minerGetBlock(miner,block);
                    TxList tx_list;


                    if ( block == NULL ) { break;}
                    blockGetIndex(previous_block,&index);


                    while (trxs->count != 0 &&  i < MAX_TX_PER_BLOCK) {
                        const char* trx = poolRemoveLast(trxs);

                        memcpy(tx_list.strings[i],trx, sizeof(trx));
                        i++;
                    }
                    blockInit(block,index + 1 ,nowUnix(),previous_block,difficulty,&tx_list);
                    compiled_block = 1;
                    mSSetTransactionCount(status,i);



                }

                minerThreadMine(status);

                if (compiled_block == 1) {
                    if (sendBlockToNodes() == 0 ) {
                        compiled_block = 0;
                    }
                }

            }

        if ( suspend == 0) break;
    }

    close(fd_socket);
    minerThreadStop(status,&mining_thread);
    nodeChannelsClose(&channels);
    return 0;
}

/**
 * Wrapper che inoltra l'esecuzione al main del processo miner (utile per avviare
 * il miner da un launcher esterno).
 * @param argc Numero di argomenti da riga di comando
 * @param argv Argomenti da riga di comando
 * @return Valore di ritorno di main()
 */
int miner_communication_main(int argc,char * argv[]) {
    return main(argc,argv);
}
