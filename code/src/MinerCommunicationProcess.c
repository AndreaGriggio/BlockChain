#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "childProcess.h"
#include "miner.h"

static volatile sig_atomic_t running;

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

    Miner* miner = minerCreate();
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
    while (running ) {
        /*quello che deve fare il miner è :
        *1. Ascoltare periodicamente sulla socket se è stato segnalato con un segnale comune quando vengono caricate informazioni
        *2. Minare come un dannato e provare a risolvere il blocco -> thread separato su cui questo while ha completo controllo
        */
        int fd =
    }



    return 0;
}

//
// Created by andrea on 17/06/26.
//
int miner_communication_main(int argc,char * argv[]) {
    return main(argc,argv);
}


