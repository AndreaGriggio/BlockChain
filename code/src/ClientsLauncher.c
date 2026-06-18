#include <signal.h>
#include <stdio.h>
#include  <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h> /* per pid_t */

/*  variabile : static = definita e valida unicamente su questo file
                volatile = dice al compilatore che il valore potrebbe cambiare anche fuori il normale flusso del programma
                sig_atomic_t = tipo di variabile, rende atomiche letture o scritte che non richiedono più operazioni
                es. running = 0 o running = 1 | NON es. running++ -> richiede di leggere, aggiungere 1 , poi scrivere
*/

static volatile sig_atomic_t running = 1;


/**
 * Questa funzione viene chiamata quando arriva un segnale registrato
 * imposta running a 0 per terminare il ciclo while che termina i suoi figli a sua volta
 * @param sig segnale in ingresso
 */

static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        running = 0;
    }
}

/**
 * Crea un nuovo processo client tramite fork() + exec()
 * @param id identificativo univoco del client
 * @param frequency frequenza di generazione delle transazioni
 * @param out_pid puntatore dove scrivere il pid del processo creato (se la fork ha avuto successo)
 * @return 0 se la creazione del processo è avvenuta con successo, -1 altrimenti
 */

 static int spawnClient(int id, int frequency, pid_t *out_pid) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0){
        /* processo figlio: restituisce la propria immagine con code/bin/client */
        char freq_str[12];
        char id_str[12];
        
        snprintf(freq_str, sizeof(freq_str), "%d", frequency);
        snprintf(id_str, sizeof(id_str), "%d", id);

        execl("./code/bin/client", "./code/bin/client", freq_str, id_str, NULL);

        /* se arriviamo qui, execl ha fallito: niente  di code è stato caricato */
        perror("execl");
        _exit(1);
    
    }

    /* processo padre: restituisce il pid del processo creato */
    *out_pid = pid;
    return 0;
 }


 int main (int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Utilizzo : %s <frequency> <clients> \n",argv[0]);
        return 1;
    }

    const int frequency = atoi(argv[1]);
    const int clients = atoi(argv[2]);

    if (clients <= 0) {
        fprintf(stderr, "clients non valido\n");
        return 1;
    }

    if (frequency <= 0 ) {
        fprintf(stderr, "frequency non valida\n");
        return 1;
    }

    pid_t *pids = malloc((size_t)clients * sizeof(pid_t));
    if (pids == NULL) {
        fprintf(stderr, "Errore di allocazione della memoria per i pid\n");
        return 1;
    }

    /*installazione dell'handler del segnale all'interno del processo*/
    struct sigaction sa ;

    //dice al processo cosa chiamare quando arriva un segnale esterno
    sa.sa_handler = handle_signal;

    //inizializza l'insieme di segnali che il processo deve bloccare
    sigemptyset(&sa.sa_mask);

    //nel mentre sta eseguendo sa_handler
    sa.sa_flags = 0;

    if (sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        free(pids);
        return 1;
    }

    int spawned = 0;

    for (int i = 0; i < clients; i++) {
        if (spawnClient(i,frequency, &pids[i]) != 0) {
            fprintf(stderr, "Errore nella creazione del processo client %d\n", i);
        break;
        }
        spawned++;
    }

    if (spawned == 0) {
        fprintf(stderr, "Nessun processo client è stato creato con successo\n");
        free(pids);
        return 1;
    }

    /* il padre resta in attesa di un segnale, senza fare polling attivo*/

    while (running) {
        pause();
    }

    /* terminazione ordinata: manda SIGTERM ai processi figli ovvero i client ancora vivi */
    for (int i = 0; i < spawned; i++) {
        kill(pids[i], SIGTERM);
    }

    /* raccoglie tutti i figli per evitare processi zombie*/
    for (int i = 0; i < spawned; i++) {
        waitpid(pids[i], NULL, 0);
    }
    free(pids);
    return 0;
}