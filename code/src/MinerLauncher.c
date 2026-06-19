#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h> /* per pid_t */

static volatile sig_atomic_t running = 1;

static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        running = 0;
    }
}

/**
 * Crea un nuovo processo miner tramite fork() + exec()
 * @param id identificativo univoco del miner
 * @param difficulty difficoltà di mining da passare al miner
 * @param num_nodes numero di node (serve al miner per le sue fifo verso i node)
 * @param out_pid puntatore dove scrivere il pid del processo creato
 * @return 0 se la creazione del processo è avvenuta con successo, -1 altrimenti
 */

static int spawnMiner(int id, int difficulty, int num_nodes, pid_t *out_pid) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        char difficulty_str[12];
        char id_str[12];
        char num_nodes_str[12];

        snprintf(difficulty_str, sizeof(difficulty_str), "%d", difficulty);
        snprintf(id_str, sizeof(id_str), "%d", id);
        snprintf(num_nodes_str, sizeof(num_nodes_str), "%d", num_nodes);

        execl("./code/bin/miner", "./code/bin/miner", difficulty_str, id_str, num_nodes_str, NULL);

        perror("execl");
        _exit(1);
    }

    *out_pid = pid;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Utilizzo : %s <difficulty> <num_miners> <num_nodes>\n", argv[0]);
        return 1;
    }

    const int difficulty = atoi(argv[1]);
    const int num_miners = atoi(argv[2]);
    const int num_nodes = atoi(argv[3]);

    if (num_miners <= 0) {
        fprintf(stderr, "num_miners non valido\n");
        return 1;
    }
    if (difficulty <= 0) {
        fprintf(stderr, "difficulty non valida\n");
        return 1;
    }
    if (num_nodes < 0) {
        fprintf(stderr, "num_nodes non valido\n");
        return 1;
    }

    pid_t *pids = malloc((size_t)num_miners * sizeof(pid_t));
    if (pids == NULL) {
        fprintf(stderr, "Errore di allocazione della memoria per i pid\n");
        return 1;
    }

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGTERM, &sa, NULL) == -1 || sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        free(pids);
        return 1;
    }

    int spawned = 0;

    for (int i = 0; i < num_miners; i++) {
        if (spawnMiner(i, difficulty, num_nodes, &pids[i]) != 0) {
            fprintf(stderr, "Errore nella creazione del processo miner %d\n", i);
            break;
        }
        spawned++;
    }

    if (spawned == 0) {
        fprintf(stderr, "Nessun processo miner è stato creato con successo\n");
        free(pids);
        return 1;
    }

    while (running) {
        pause();
    }

    for (int i = 0; i < spawned; i++) {
        kill(pids[i], SIGTERM);
    }
    for (int i = 0; i < spawned; i++) {
        waitpid(pids[i], NULL, 0);
    }

    free(pids);
    return 0;
}
