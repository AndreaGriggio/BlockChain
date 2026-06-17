#include "node.h"
#include "block.h"
#include "message.h"
#include "childProcess.h"
#include "error.h"
#include "protocolSocket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

static volatile sig_atomic_t running = 1;

static Block   *last_block   = NULL;
static uint64_t chain_length = 0;

static pthread_mutex_t chain_mutex = PTHREAD_MUTEX_INITIALIZER;

static int    node_id    = -1;
static int    num_nodes  = 0;
static char **node_fifos = NULL;
static char   my_fifo[256];

static FILE *log_file = NULL;

#define CSV_SEM_NAME  "/blockchain_csv"
#define CSV_FILE_NAME "./blockchain.csv"

typedef struct {
    int server_fd;
} ListenerArgs;

static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        running = 0;
    }
}

static void log_msg(const char *fmt, ...) {
    if (log_file == NULL) return;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm *tm_info = localtime(&ts.tv_sec);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(log_file, "[%s.%03ld] [NODE-%d|PID-%d] ",
            time_buf, ts.tv_nsec / 1000000, node_id, (int)getpid());

    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file);
}


static int append_block_to_csv(const Block *block_ptr) {
    sem_t *sem = sem_open(CSV_SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        log_msg("ERROR: sem_open fallito: %s", strerror(errno));
        return SEM_ERROR;
    }

    sem_wait(sem);

    FILE *f = fopen(CSV_FILE_NAME, "a");
    if (f == NULL) {
        log_msg("ERROR: apertura CSV fallita: %s", strerror(errno));
        sem_post(sem);
        sem_close(sem);
        return CSV_ERROR;
    }

    char line[BLOCK_CSV_LINE_SIZE];
    if (blockToCsv(block_ptr, line, sizeof(line)) != 0) {
        log_msg("ERROR: blockToCsv fallito");
        fclose(f);
        sem_post(sem);
        sem_close(sem);
        return CSV_ERROR;
    }

    fprintf(f, "%s\n", line);
    fclose(f);

    sem_post(sem);
    sem_close(sem);
    return 0;
}

static int load_initial_state(const char *csv_path) {
    FILE *f = fopen(csv_path, "r");
    if (f == NULL) {
        log_msg("ERROR: impossibile aprire %s: %s", csv_path, strerror(errno));
        return -1;
    }

    char line[BLOCK_CSV_LINE_SIZE];
    Block *prev = NULL;

    long first_pos = ftell(f);
    if (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, "index,", 6) != 0) {
            fseek(f, first_pos, SEEK_SET);
        }
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        Block *b = blockCreate();
        if (b == NULL) {
            log_msg("ERROR: blockCreate fallito");
            fclose(f);
            if (prev) blockDestroy(prev);
            return -1;
        }

        if (blockFromCsv(b, line) != 0) {
            log_msg("ERROR: blockFromCsv fallito su: %s", line);
            blockDestroy(b);
            fclose(f);
            if (prev) blockDestroy(prev);
            return -1;
        }

        if (prev) blockDestroy(prev);
        prev = b;
        chain_length++;
    }

    fclose(f);

    pthread_mutex_lock(&chain_mutex);
    if (last_block) blockDestroy(last_block);
    last_block = prev;
    pthread_mutex_unlock(&chain_mutex);

    log_msg("Stato iniziale caricato: %llu blocchi",
            (unsigned long long)chain_length);
    return 0;
}

static int validate_merkle(const Block *block_ptr) {
    char computed[MERKLE_ROOT_HEX_SIZE + 1];
    char stored[MERKLE_ROOT_HEX_SIZE + 1];

    if (blockGetmerkle(block_ptr, computed) != 0) {
        log_msg("ERROR: blockGetmerkle fallito");
        return -1;
    }

    if (blockGetMerkleRoot(block_ptr, stored) != 0) {
        log_msg("ERROR: blockGetMerkleRoot fallito");
        return -1;
    }

    if (strcmp(computed, stored) != 0) {
        log_msg("ERROR: Merkle root non valido");
        return INVALID_MERKLE;
    }

    return 0;
}


/*
 * process_block - Valida un blocco ricevuto e lo aggiunge alla chain locale.
 *
 * PARAMETRI:
 *    new_block  - blocco da validare e aggiungere
 *    from_miner - 1 se arriva da un miner, 0 se da un altro node.
 *                 Per ora non usato, servirà per la propagazione tra node.
 *
 * CONCORRENZA:
 *    Acquisisce chain_mutex all'inizio per garantire che un solo thread
 *    alla volta modifichi last_block e chain_length. Due thread che
 *    ricevono blocchi contemporaneamente vengono serializzati qui:
 *    il primo acquisisce il mutex e vince, il secondo aspetta e poi
 *    trova last_block già aggiornato e fallisce la validazione.
 *
 * FLUSSO:
 *
 * 1. Acquisisce chain_mutex
 *
 * 2. Calcola l'hash del blocco
 *
 * 3. Decide il caso:
 *
 *    CASO GENESIS (last_block == NULL && chain_length == 0)
 *    → Chain vuota, è il primo blocco della blockchain.
 *    → Viene accettato senza validare prev_hash perché non esiste
 *      un blocco precedente da confrontare.
 *
 *    CASO NORMALE (last_block != NULL)
 *    → blockValidate: controlla che index == last_block.index + 1
 *                     controlla che prev_hash == hash(last_block)
 *                     Se fallisce ritorna CHAIN_MISMATCH.
 *    → validate_merkle: ricalcola il merkle root dalle transazioni
 *                       e lo confronta con quello dichiarato nel blocco.
 *                       Se fallisce ritorna INVALID_BLOCK.
 *
 *    CASO INCONSISTENTE (last_block == NULL && chain_length > 0)
 *    → Stato interno corrotto — non dovrebbe mai accadere.
 *    → Ritorna -1.
 *
 * 4. Blocco valido — aggiorna la chain
 *    → Salva last_block in old
 *    → last_block = new_block
 *    → chain_length++
 *
 * 5. Rilascia chain_mutex
 *    Il mutex viene rilasciato prima di scrivere sul CSV per non
 *    tenerlo bloccato durante operazioni lente su disco.
 *
 * 6. Libera la memoria del blocco precedente ora che non serve più.
 *
 * 7. Scrive il blocco sul CSV
 *    append_block_to_csv usa il semaforo POSIX per garantire
 *    accesso esclusivo al file condiviso tra tutti i node.
 *
 * RITORNA:
 *    0             se il blocco è stato accettato
 *    CHAIN_MISMATCH se index o prev_hash non validi
 *    INVALID_BLOCK  se merkle root non valido
 *    -1             in caso di errore interno
 */

static int process_block(Block *new_block, int from_miner) {
    (void)from_miner; 
    pthread_mutex_lock(&chain_mutex);

    char hash[HASH_HEX_SIZE + 1];
    blockGetHash(new_block, hash);

    if (last_block == NULL && chain_length == 0) {
        log_msg("Blocco genesis ricevuto: hash=%.16s...", hash);

    } else if (last_block != NULL) {

            uint64_t new_index;
            uint64_t last_index;
            blockGetIndex(new_block, &new_index);
            blockGetIndex(last_block, &last_index);

            if (new_index > last_index + 1) {
                log_msg("ERROR: blocco troppo avanti, index=%llu atteso=%llu",
                        (unsigned long long)new_index,
                        (unsigned long long)(last_index + 1));
                pthread_mutex_unlock(&chain_mutex);
                return BLOCK_TOO_FAR;
            }

        if (blockValidate(new_block, last_block) != 0) {
            log_msg("ERROR: CHAIN_MISMATCH, blocco scartato");
            pthread_mutex_unlock(&chain_mutex);
            return CHAIN_MISMATCH;
        }

        if (validate_merkle(new_block) != 0) {
            log_msg("ERROR: Merkle root non valido, blocco scartato");
            pthread_mutex_unlock(&chain_mutex);
            return INVALID_MERKLE; 
        }

    } else {
        log_msg("ERROR: stato interno inconsistente");
        pthread_mutex_unlock(&chain_mutex);
        return -1;
    }

    Block *old = last_block;
    last_block = new_block;
    chain_length++;

    pthread_mutex_unlock(&chain_mutex);

    if (old) blockDestroy(old);

    if (append_block_to_csv(new_block) != 0) {
        log_msg("ERROR: scrittura CSV fallita");
        return -1;
    }

    log_msg("Blocco accettato: index=%llu hash=%.16s...",
            (unsigned long long)(chain_length - 1), hash);

    return 0;
}


static void *listener_thread(void *arg) {
    ListenerArgs *args = (ListenerArgs *)arg;
    int server_fd = args->server_fd;

    log_msg("Listener socket avviato");

    while (running) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (!running) break;
            log_msg("ERROR: accept fallita: %s", strerror(errno));
            continue;
        }

        Message msg;
        messageInit(&msg);

        int r = receiveMessage(client_fd, &msg);
        if (r != 0) {
            log_msg("ERROR: receiveMessage fallita: %d", r);
            close(client_fd);
            continue;
        }

        close(client_fd);

        MessageType type;
        messageGetType(&msg, &type);

        if (type == MSG_BLOCK_MINED) {
            log_msg("Ricevuto MSG_BLOCK_MINED da miner");

            char csv_line[BLOCK_CSV_LINE_SIZE];
            if (messageGetPayload(&msg, csv_line, sizeof(csv_line)) != 0) {
                log_msg("ERROR: messageGetPayload fallita");
                continue;
            }

            Block *new_block = blockCreate();
            if (new_block == NULL) {
                log_msg("ERROR: blockCreate fallita");
                continue;
            }

            if (blockFromCsv(new_block, csv_line) != 0) {
                log_msg("ERROR: blockFromCsv fallita");
                blockDestroy(new_block);
                continue;
            }

            /* processo il blocco */
            if (process_block(new_block, 1) != 0) {
                blockDestroy(new_block);
            }

        } else {
            log_msg("WARN: tipo messaggio inatteso: %d", type);
        }
    }

    log_msg("Listener socket terminato");
    return NULL;
}




int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr,
            "Utilizzo: %s <id> <num_nodes> <server_fd> <initial_csv> <fifo0> [fifo1 ...]\n",
            argv[0]);
        return 1;
    }

    node_id         = atoi(argv[1]);
    num_nodes       = atoi(argv[2]);
    int server_fd   = atoi(argv[3]);
    const char *initial_csv = argv[4];

    if (node_id < 0 || num_nodes <= 0 || server_fd < 0) {
        fprintf(stderr, "Argomenti non validi\n");
        return 1;
    }

    node_fifos = &argv[5];
    snprintf(my_fifo, sizeof(my_fifo), "%s", node_fifos[node_id]);

    char log_path[64];
    snprintf(log_path, sizeof(log_path), "node-%d.log", (int)getpid());
    log_file = fopen(log_path, "w");
    if (log_file == NULL) {
        fprintf(stderr, "Impossibile aprire log file\n");
        return 1;
    }

    log_msg("Node avviato: id=%d num_nodes=%d", node_id, num_nodes);

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);

    if (strlen(initial_csv) > 0) {
        if (load_initial_state(initial_csv) != 0) {
            log_msg("ERROR: caricamento stato iniziale fallito");
            fclose(log_file);
            return 1;
        }
    }

    pthread_t socket_tid;
    ListenerArgs largs = { server_fd };
    if (pthread_create(&socket_tid, NULL, listener_thread, &largs) != 0) {
        log_msg("ERROR: pthread_create fallita");
        fclose(log_file);
        return 1;
    }

    log_msg("Thread listener avviato, in attesa di blocchi...");

    pthread_join(socket_tid, NULL);

    pthread_mutex_lock(&chain_mutex);
    if (last_block) blockDestroy(last_block);
    pthread_mutex_unlock(&chain_mutex);

    fclose(log_file);
    log_msg("Node terminato");
    return 0;
}