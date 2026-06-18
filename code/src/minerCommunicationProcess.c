
//
// Created by andrea on 17/06/26.
//
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include "childProcess.h"
#include "miner.h"
#include "minerStatus.h"

static MinerStatus* status = NULL;
static Miner* miner = NULL;

static pthread_mutex_t miner_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;
static volatile sig_atomic_t running = 1;


typedef struct MiningThreadArgs {
    Miner*           miner;
    MinerStatus*     status;
    pthread_mutex_t* miner_mutex;
    pthread_mutex_t* status_mutex;
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


//Getter e setter per lo stato del MinerStatus
void updateState(const MinerState new_state) {
    pthread_mutex_lock(&status_mutex);
    status->state = new_state;
    pthread_mutex_unlock(&status_mutex);
}


MinerState readState() {
    pthread_mutex_lock(&status_mutex);
    const MinerState s = status->state;
    pthread_mutex_unlock(&status_mutex);
    return s;
}


//Chiamate di funzioni che servono per arrestare o fare partire il thread di mining
/**
 *Inizia il processo di mining
 * @param miner miner che inizia il processo di calcolo del blocco
 * @return 0 se tutto è andato bene
 */
static void* minerMining_Thread_Entry_point(void* arg) {
    MiningThreadArgs* args = (MiningThreadArgs*)arg;
    minerMiningLoop(args->miner,
                    args->status,
                    args->miner_mutex,
                    args->status_mutex);
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
 * @param miner miner a cui fermare il processo
 * @return 0 se tutto è andato bene
 */
int stopMining(pthread_t* mining_thread) {
    pthread_join(*mining_thread, NULL);
}

int pauseMining(Miner* miner);


int main(int argc, char ** argv) {

    if (argc < 2) {
        fprintf(stderr, "Utilizzo : %s <difficulty> <id>  \n",argv[0]);
        return 1;
    }

    const int difficulty = atoi(argv[1]);
    const int id = atoi(argv[2]);

    if (difficulty <= 0 ) {
        fprintf(stderr,"Difficulty non valida ex. difficulty > 0");
        return 1;
    }
    if (id < 0) {
        fprintf(stderr,"ID non valida ex. id > 0");
        return 1;
    }
    struct sigaction sa ;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT,&sa,NULL)== -1) {
        perror("sigaction SIGINT");
        return 1;
    }

    ChildProcess* childProcess = childProcessCreate();

    if (childProcess == NULL) {
        fprintf(stderr,"Error creating child process");
        return 1;
    }

    if (childProcessInit(childProcess, getpid(),id,MINER) != 0) {
        fprintf(stderr,"Error initializing child process");
        free(childProcess);
        return 1;
    }
    status = minerCreateStatus();
    mSSetCP(status,childProcess); //inizializzo lo status
    mSSetState(status,MINER_IDLE);
    mSSetAttempts(status, 0);
    mSSetTransactionCount(status, 0);

    //creo la struttura dati per accogliere il blocco
    miner = minerCreate();
    if (miner == NULL) {
        fprintf(stderr,"Error allocation miner");
        return 1;
    }
    if (minerInit(miner, difficulty)== 0) {
        fprintf(stderr,"Miner Init error");
        free(miner);
        free(childProcess);
        return 1;
    };

    pthread_t mining_thread;

    MiningThreadArgs args = {
        .miner        = miner,
        .status       = status,
        .miner_mutex  = &miner_mutex,
        .status_mutex = &status_mutex
    };

    startMining(&mining_thread,&args);

    while (running ) {
        /*quello che deve fare il miner è :
        *1. Ascoltare periodicamente sulla socket se è stato segnalato con un segnale comune quando vengono caricate informazioni
        *2. Minare come un dannato e provare a risolvere il blocco -> thread separato su cui questo while ha completo controllo
        */

        int fd =
    }


    stopMining(&mining_thread);
    return 0;
}

int miner_communication_main(int argc,char * argv[]) {
    return main(argc,argv);
}


