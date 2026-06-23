//
// Created by andrea on 25/05/26.
//
#include "client.h"
#include "childProcess.h"
#include <time.h>
#include "message.h"
#include "protocolSocket.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdarg.h>

/* variabile :  static = definita e valida unicamente su questo file
                volatile = dice al compilatore che il vaore potrebbe cambiare anche fuori il normale flusso di proramma
                sig_atomic_t = tipo di variabile rende atomiche letture o scritte che non richiedono più operazioni
                es. running = 0 o running = 1 | NON es. running++ -> richiede di leggere, aggiungere 1 , poi scrivere
*/

static volatile sig_atomic_t running = 1;

/**
 * Questa funzione viene chiamata quando arriva un segnale registrato
 * @param sig segnale in ingresso
 */
static void handle_signal(int sig) {
    /*Quando il processo figlio riceve un segnale allora
     *Se è SIGTERM o SIGINT si smette di eseguire il ciclo while
     */
    if (sig == SIGTERM || sig == SIGINT) {
        running = 0;
    }
}

static int   client_id = -1;
static FILE *log_file  = NULL;

// Log con timestamp su file client-<pid>.log (stesso formato del node)
static void log_msg(const char *fmt, ...) {
    if (log_file == NULL) return;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *tm_info = localtime(&ts.tv_sec);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(log_file, "[%s.%03ld] [CLIENT-%d|PID-%d] ",
            time_buf, ts.tv_nsec / 1000000, client_id, (int)getpid());

    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file);
}


int main(int argc, char* argv[]) {
    /*Argomenti da passare :
     *    1. frequenza di transazioni
     *    2. id del processo client
     */
    if (argc < 3) {
        fprintf(stderr, "Utilizzo : %s <transaction_frequency> <client_id> \n",argv[0]);
        return 1;

    }
    const int transaction_frequency = atoi(argv[1]);
    const int id = atoi(argv[2]);
    if (transaction_frequency <= 0 ) {
        fprintf(stderr, "transaction_frequency non valida\n");
        return 1;
    }
    if ( id < 0 ) {
        fprintf(stderr, "id non valido \n");
        return 1;
    }

    // Apertura del file di log: client-<pid>.log
    client_id = id;
    char log_path[64];
    snprintf(log_path, sizeof(log_path), "client-%d.log", (int)getpid());
    log_file = fopen(log_path, "a");
    if (log_file == NULL) {
        fprintf(stderr, "CLIENT %d : impossibile aprire il log %s\n", id, log_path);
        return 1;
    }
    log_msg("Avvio client (frequenza=%d)", transaction_frequency);

    srand ((unsigned int)(time(NULL) ^ getpid()));

    //installazione dell'handler del segnale all'interno del processo
    struct sigaction sa ;
    sa.sa_handler = handle_signal;//dice al processo cosa chiamare quando arriva un segnale esterno
    sigemptyset(&sa.sa_mask);//inizializza l'insieme di segnali che il processo deve bloccare
    //nel mentre sta eseguendo sa_handler
    sa.sa_flags = 0;

    //dico al processo che consfigurazione utilizzare quando arriva un certo segnale
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        return 1;
    }

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        return 1;
    }

    //creo il client
    Client* client = createClient();
    //controllo che sia stato creato correttamente
    if (client == NULL) {
        fprintf(stderr,"Client Allocation Error");
        return 1;
    }
    //inizializzo il client e controllo
    if ( clientInit(client,transaction_frequency) != 0) {
        fprintf(stderr,"Client Init Error");
        free(client);
        return 1;
    }
    //creo il childProcess
    ChildProcess* childProcess = childProcessCreate();
    //controllo
    if (childProcess == NULL) {
        fprintf(stderr,"Process information Allocation Error");
        free(client);
        return 1;
    }
    //inizializzo e controllo
    if (childProcessInit(childProcess,getpid(),id,CLIENT)!= 0) {
        fprintf(stderr,"Process information Init Error");
        free(client);
        free(childProcess);
        return 1;
    }


    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path,
        MINERS_SOCKET,
          sizeof(addr.sun_path)-1);
    
    while (running) {

        sleep(transaction_frequency);
        if (!running) break;

        if (clientGenerateTransaction(client) == 0) {

            Message message;
            messageInit(&message);
            messageSetType(&message, MSG_NEW_TX);
            messageSetSender(&message, childProcess);

            char transaction[MAX_TR_LENGHT+1];
            if (clientGetTransaction(client, sizeof(transaction), transaction) != 0) {
                log_msg("Errore nel recupero della transazione generata");
                continue;
            }

            log_msg("Transazione generata: %s", transaction);

            messageSetPayload(&message, transaction, strlen(transaction));

            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0) {
                log_msg("Errore creazione socket");
                continue;
            }

            if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                log_msg("Errore connessione al socket dei miner");
                close(fd);
                continue;
            }

            if (sendMessage(fd, &message) == 0)
                log_msg("Transazione inviata ai miner");
            else
                log_msg("Errore nell'invio della transazione");

            close(fd);

        } else {
            log_msg("Errore nella generazione della transazione");
        }
    }

    log_msg("Terminazione client");
    if (log_file != NULL) fclose(log_file);

    free(client);
    free(childProcess);
    exit(0);

}