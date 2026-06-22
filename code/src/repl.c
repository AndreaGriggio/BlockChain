#include "repl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "constants.h"     // MINERS_SOCKET, CSV_FILE_NAME, CSV_SEM_NAME
#include "error.h"
#include "block.h"
#include "message.h"       // Message, MSG_NEW_TX, sendMessage
#include "childProcess.h"
#include "utils.h"         // validateTransaction

// Invia una transazione ai miner su MINERS_SOCKET (come fa il client) 
static int submit_transaction(const char *tx) {
    if (tx == NULL || strlen(tx) > MAX_TX_SIZE) return INVALID_TRANSACTION;

    char buf[MAX_TX_SIZE + 1];
    strncpy(buf, tx, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';

    if (validateTransaction(buf) != 0) return INVALID_TRANSACTION;

    ChildProcess *self = childProcessCreate();
    if (self == NULL) return MEMORY_ERROR;
    if (childProcessInit(self, getpid(), 0, CLIENT) != 0) {
        childProcessDestroy(self);
        return INVALID_PARAMS;
    }

    Message message;
    messageInit(&message);
    messageSetType(&message, MSG_NEW_TX);
    messageSetSender(&message, self);
    messageSetPayload(&message, buf, (uint32_t)strlen(buf));

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MINERS_SOCKET, sizeof addr.sun_path - 1);
    addr.sun_path[sizeof addr.sun_path - 1] = '\0';

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { childProcessDestroy(self); return SOCKET_ERROR; }
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        close(fd); childProcessDestroy(self); return SOCKET_ERROR;
    }

    int rc = sendMessage(fd, &message);
    close(fd);
    childProcessDestroy(self);
    return rc;
}

// Legge il CSV condiviso (sotto semaforo) e stampa i blocchi richiesti 
static int request_blocks(const char *what, const char *flag, const char *val) {
    int single   = (strcmp(what, "block") == 0);
    int by_index = (flag != NULL && strcmp(flag, "--index") == 0);
    int by_hash  = (flag != NULL && strcmp(flag, "--hash")  == 0);

    if (single && flag == NULL)                  return INVALID_PARAMS;
    if (flag != NULL && !by_index && !by_hash)   return INVALID_PARAMS;
    if ((by_index || by_hash) && val == NULL)    return INVALID_PARAMS;

    uint64_t want_index = by_index ? strtoull(val, NULL, 10) : 0;

    sem_t *sem = sem_open(CSV_SEM_NAME, 0);
    if (sem == SEM_FAILED) return SEM_ERROR;
    sem_wait(sem);

    FILE *f = fopen(CSV_FILE_NAME, "r");
    if (f == NULL) { sem_post(sem); sem_close(sem); return CSV_ERROR; }

    char line[BLOCK_CSV_LINE_SIZE];
    int header = 1, started = 0, found = 0;

    while (fgets(line, sizeof line, f) != NULL) {
        if (header) { header = 0; continue; }

        Block *b = blockCreate();
        if (b == NULL) break;
        if (blockFromCsv(b, line) != 0) { blockDestroy(b); continue; }

        uint64_t idx = 0;
        char hash[HASH_HEX_SIZE + 1];
        blockGetIndex(b, &idx);
        blockGetHash(b, hash);
        blockDestroy(b);

        int match = 0;
        if (flag == NULL) {
            match = 1;
        } else if (by_index) {
            match = single ? (idx == want_index) : (idx >= want_index);
        } else {
            if (single) match = (strcmp(hash, val) == 0);
            else { if (!started && strcmp(hash, val) == 0) started = 1; match = started; }
        }

        if (match) { fputs(line, stdout); found = 1; if (single) break; }
    }

    fclose(f);
    sem_post(sem);
    sem_close(sem);
    return found ? 0 : BLOCK_NOT_FOUND;
}

// Copia il CSV condiviso nel file richiesto, sotto semaforo (save)
static int save_blockchain(const char *dst) {
    sem_t *sem = sem_open(CSV_SEM_NAME, 0);
    if (sem == SEM_FAILED) return SEM_ERROR;
    sem_wait(sem);

    int rc = 0;
    FILE *src = fopen(CSV_FILE_NAME, "r");
    FILE *out = (src != NULL) ? fopen(dst, "w") : NULL;
    if (src == NULL || out == NULL) {
        rc = CSV_ERROR;
    } else {
        char buffer[4096];
        size_t n;
        while ((n = fread(buffer, 1, sizeof buffer, src)) > 0) {
            if (fwrite(buffer, 1, n, out) != n) { rc = CSV_ERROR; break; }
        }
        if (ferror(src)) rc = CSV_ERROR;
    }
    if (src) fclose(src);
    if (out) fclose(out);

    sem_post(sem);
    sem_close(sem);
    return rc;
}

void repl_run(pid_t child_pgid, volatile sig_atomic_t *running) {
    char line[512];
    printf("> ");
    fflush(stdout);

    while (*running && fgets(line, sizeof line, stdin) != NULL) {

        line[strcspn(line, "\n")] = '\0';

        char *cmd = strtok(line, " ");
        if (cmd == NULL) { printf("> "); fflush(stdout); continue; }

        if (strcmp(cmd, "pause") == 0) {
            killpg(child_pgid, SIGSTOP);
            printf("Sistema in pausa\n");

        } else if (strcmp(cmd, "resume") == 0) {
            killpg(child_pgid, SIGCONT);
            printf("Sistema ripreso\n");

        } else if (strcmp(cmd, "submit") == 0) {
            char *arg = strtok(NULL, "");
            if (arg == NULL) {
                printf("Uso: submit \"Mittente pays Destinatario N coins\"\n");
            } else {
                if (*arg == '"') arg++;
                size_t alen = strlen(arg);
                if (alen > 0 && arg[alen - 1] == '"') arg[alen - 1] = '\0';
                if (submit_transaction(arg) == 0)
                    printf("Transazione inviata: %s\n", arg);
                else
                    printf("Transazione rifiutata (malformata o invio fallito)\n");
            }

        } else if (strcmp(cmd, "save") == 0) {
            char *what  = strtok(NULL, " ");
            char *fname = strtok(NULL, " ");
            if (what == NULL || strcmp(what, "blockchain") != 0 || fname == NULL) {
                printf("Uso: save blockchain <filename>\n");
            } else {
                if (save_blockchain(fname) == 0)
                    printf("Blockchain salvata in %s\n", fname);
                else
                    printf("Salvataggio fallito\n");
            }

        } else if (strcmp(cmd, "request") == 0) {
            char *what = strtok(NULL, " ");
            char *flag = strtok(NULL, " ");
            char *val  = strtok(NULL, " ");
            if (what == NULL ||
                (strcmp(what, "blockchain") != 0 && strcmp(what, "block") != 0)) {
                printf("Uso: request blockchain [--index <i> | --hash <h>]  |  request block --index <i> | --hash <h>\n");
            } else {
                int r = request_blocks(what, flag, val);
                if (r == INVALID_PARAMS)
                    printf("Uso: request blockchain [--index <i> | --hash <h>]  |  request block --index <i> | --hash <h>\n");
                else if (r == BLOCK_NOT_FOUND)
                    printf("Nessun blocco trovato\n");
                else if (r != 0)
                    printf("Errore nella request (codice %d)\n", r);
            }

        } else if (strcmp(cmd, "stop") == 0) {
            *running = 0;
            break;

        } else {
            printf("Comando sconosciuto: %s\n", cmd);
        }

        printf("> ");
        fflush(stdout);
    }
}
