
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

static volatile sig_atomic_t running = 1;


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
 * @param miner miner a cui fermare il processo
 * @return 0 se tutto è andato bene
 */
static int stopMining(pthread_t* mining_thread) {
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


int main(int argc, char ** argv) {

    if (argc < 3) {
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

    if (sigaction(SIGINT,  &sa, NULL) == -1) { perror("sigaction SIGINT"); return 1; }
    if (sigaction(SIGTERM, &sa, NULL) == -1) { perror("sigaction SIGTERM");        return 1; }

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

    minerInitStatus(status,childProcess,MINER_IDLE,0,0);


    //creo la struttura dati per accogliere il blocco
    miner = minerCreate();
    if (miner == NULL) {
        fprintf(stderr,"Error allocation miner");
        return 1;
    }

    if (minerInit(miner, difficulty) != 0) {
        fprintf(stderr,"Miner Init error");
        free(miner);
        free(childProcess);
        return 1;
    };

    pthread_t mining_thread;

    MiningThreadArgs args = {
        .miner        = miner,
        .status       = status,
    };

    startMining(&mining_thread,&args);

    while (running) {
        /*quello che deve fare il miner è :
        *1. Ascoltare periodicamente sulla socket se è stato segnalato con un segnale comune quando vengono caricate informazioni
        *2. Minare come un dannato e provare a risolvere il blocco -> thread separato su cui questo while ha completo controllo
        */


    }


    stopMining(&mining_thread);
    return 0;
}

int miner_communication_main(int argc,char * argv[]) {
    return main(argc,argv);
}


