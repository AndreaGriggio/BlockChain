//
// Created by andrea on 25/05/26.
//
#include "client.h"
#include "childProcess.h"
#include "error.h"
#include "message.h"
#include "protocolSocket.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/* variabile :  static = definita unicamente su questo file
                volatile = dice al compilatore che il vaore potrebbe cambiare
                sig_atomic_t = tipo di variabile rende atomiche letture o scritte che non richiedono più operazioni
                es. running = 0 o running = 1 | NON es. running++ -> richiede di leggere, aggiungere 1 , poi scrivere
*/
static volatile sig_atomic_t running = 1;

static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        running = 0;
    }
    if (sig == SIGCONT) {
        running = 1;
    }

}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Utilizzo : %s <transaction_frequency<\n",argv[0]);
        return 1;

    }
    const int transaction_frequency = atoi(argv[1]);
    const int id = atoi(argv[2]);
    if (transaction_frequency <= 0 ) {
        fprintf(stderr, "transaction_frequency non valida\n");
        return 1;
    }

    struct sigaction sa ;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        return 1;
    }

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        return 1;
    }

    Client* client;
    ChildProcess* childProcess;


    createClient(client);
    if (client == NULL) {
        fprintf(stderr,"Client Allocation Error");
        return 1;
    }

    if ( clientInit(client,transaction_frequency) == INVALID_PARAMS) {
        fprintf(strderr,"Client Init Error");
        return 1;
    }

    childProcessCreate(childProcess);
    if (childProcess == NULL) {
        fprintf(stderr,"Process information Allocation Error");
        return 1;
    }
    if (childProcessInit(childProcess,getpid(),id,CLIENT)!= 0) {
        fprintf(stderr,"Process information Init Error");
        return 1;
    }




    while (running) {
        sleep(1);
        if ( clientGenerateTransaction(client) == 0) {
            Message message;
            messageInit(&message);

            messageSetType(&message,MSG_NEW_TX);
            messageSetSender(&message,childProcess);

            
            if (clientGetTransaction(client,MAX_TR_LENGHT,message.payload) == 0){fprintf(stderr, "Getting Transaction it's too difficult");}

            int fd = socket(AF_UNIX,SOCK_STREAM,0);

            if (fd < 0) { fprintf(stderr,"Socket Creation Error"); }

            if ( connect(fd,(struct sockaddr *)&addr,sizeof(addr))< 0){fprintf(stderr, "Socket Connection Error");}

            sendMessage(fd,message);
        };
    }

}