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

#include "childProcess.h"
#include "error.h"
#include "miner.h"
#include "minerStatus.h"
#include "minerFifo.h"
#include "minerThread.h"
#include "minerCommunicationProtocol.h"
#include "transactionPool.h"


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
 * thread di mining e stabilisce la connessione socket con il processo master.
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

                receiveTransactionFromClient(fd_socket,trxs);

                minerThreadPause(status);
                //prendo lo stato del miner
                mSGetState(status,&current_state);

                if ( trxs->count != 0 && current_state == MINER_BLOCK_FOUND) {
                    uint64_t i = 0;
                    mSGetTransactionCount(status,&i);
                    while (trxs->count != 0 &&  i < MAX_TX_PER_BLOCK){
                        minerGetBlock(miner,previous_block);

                        blockAddTransaction(previous_block,poolRemoveLast(trxs));
                        i++;
                    }
                    mSSetTransactionCount(status,i);
                }

                minerThreadMine(status);


                if (sendBlockToNodes() == 0 ) {

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
