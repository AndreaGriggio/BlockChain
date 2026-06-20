#define _GNU_SOURCE

#include "node.h"
#include "block.h"
#include "message.h"
#include "childProcess.h"
#include "error.h"
#include "protocolSocket.h"
#include "constants.h"
#include "communicationProtocol.h"

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
#include <limits.h>


static volatile sig_atomic_t running = 1;


// Puntatore all'ultimo blocco della catena locale 
static Block   *last_block   = NULL; 
static uint64_t chain_length = 0;

static pthread_mutex_t chain_mutex = PTHREAD_MUTEX_INITIALIZER;


static int    node_id    = -1;
static int    num_nodes  = 0;
static int    num_miners = 0;

/* Array degli fd di lettura (da ogni miner) e scrittura (verso ogni miner) */
static int *to_miner   = NULL;
static int *from_miner = NULL;

static NodeStatus* status = NULL;

static FILE *log_file = NULL;


static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        running = 0;
    }
}


// Funzione di log con timestamp
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


//funzione per aggiungere un blocco al file csv condiviso 
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

//funzione per aggiungere un blocco al file csv locale del nodo 
static int append_block_to_csv_local(const Block *block_ptr) {
    char path[64];
    snprintf(path, sizeof(path), "node_%d_blockchain.csv", node_id);
 
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        log_msg("ERROR: apertura CSV locale fallita: %s", strerror(errno));
        return CSV_ERROR;
    }
 
    char line[BLOCK_CSV_LINE_SIZE];
    if (blockToCsv(block_ptr, line, sizeof(line)) != 0) {
        log_msg("ERROR: blockToCsv fallito");
        fclose(f);
        return CSV_ERROR;
    }
 
    fprintf(f, "%s\n", line);
    fclose(f);
    return 0;
}

//funzione per caricare lo stato iniziale della blockchain da un file csv
static int load_initial_state(const char *csv_path) {
    if (csv_path == NULL || strlen(csv_path) == 0) return 0;
 
    FILE *f = fopen(csv_path, "r");
    if (f == NULL) {
        if (errno == ENOENT) {
            f = fopen(csv_path, "w");
            if (f == NULL) {
                log_msg("ERROR: creazione %s fallita: %s", csv_path, strerror(errno));
                return CSV_ERROR;
            }
            fclose(f);
            log_msg("CSV non esistente, creato nuovo file: %s", csv_path);
            return 0;
        }
        log_msg("ERROR: impossibile aprire %s: %s", csv_path, strerror(errno));
        return CSV_ERROR;
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
            return INVALID_BLOCK;
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

//funzione per validare il merkle root di un blocco
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

static int createNodeFifos(const int num_miners) {
    to_miner   = (int*) malloc(sizeof(int) * num_miners);
    from_miner = (int*) malloc(sizeof(int) * num_miners);
 
    if (to_miner == NULL || from_miner == NULL) {
        fprintf(stderr, "MINER: malloc fd arrays fallita\n");
        free(to_miner);
        free(from_miner);
        return -1;
    }

    for (int i = 0; i < num_miners; i++) {
        to_miner[i]   = -1;
        from_miner[i] = -1;
    }
 
    ChildProcess *cp = childProcessCreate();

    if (cp == NULL) {return -1;}

    if (status == NULL) {
    fprintf(stderr, "NODE: status non inizializzato\n");
    childProcessDestroy(cp);
    return -1;
    }

    nSGetCPChildProcess(status,cp);
 
    int id;
    if (getCpId(cp, &id) != 0 || id < 0) {
        childProcessDestroy(cp);
        return -1;
    }
    
    childProcessDestroy(cp);    
    for (int i = 0; i < num_miners; i++) {
        char path_to[64];
        snprintf(path_to, sizeof(path_to), "%s%d%d", NODE_MINER_FIFO, id, i);
 
        if (mkfifo(path_to, 0666) < 0 && errno != EEXIST) {
            fprintf(stderr, "NODE %d: mkfifo %s fallita: %s\n",
                    id, path_to, strerror(errno));
            return -1;
        }
 
        do {
            to_miner[i] = open(path_to, O_WRONLY | O_NONBLOCK);
            if (to_miner[i] < 0 && errno != ENXIO) {
                fprintf(stderr,"open path_to");
                return -1;
            }
            if (to_miner[i] < 0) usleep(10000);
        } while (to_miner[i] < 0);
 
        if (fcntl(to_miner[i], F_SETPIPE_SZ, PIPE_BUF) < 0) {
            fprintf(stderr, "NODE %d: fcntl to_miner[%d] fallita: %s\n",
                    id, i, strerror(errno));
            return -1;
        }
 
        fprintf(stderr, "NODE %d: to_miner[%d] aperto (fd=%d path=%s)\n",
                id, i, to_miner[i], path_to);
    }
 
    for (int i = 0; i < num_miners; i++) {
        char path_from[64];
        snprintf(path_from, sizeof(path_from), "%s%d%d", MINER_NODE_FIFO, i, id);
 
        do {
            from_miner[i] = open(path_from, O_RDONLY);
            if (from_miner[i] < 0 && errno != ENXIO && errno != ENOENT) {
                fprintf(stderr, "NODE %d: open %s fallita: %s\n",
                        id, path_from, strerror(errno));
                return -1;
            }
            if (from_miner[i] < 0) usleep(10000);
        } while (from_miner[i] < 0);
 
        if (fcntl(from_miner[i], F_SETPIPE_SZ, PIPE_BUF) < 0) {
            fprintf(stderr, "NODE %d: fcntl from_miner[%d] fallita: %s\n",
                    id, i, strerror(errno));
            return -1;
        }
 
        fprintf(stderr, "NODE %d: from_miner[%d] aperto (fd=%d path=%s)\n",
                id, i, from_miner[i], path_from);
    }
 
    return 0;
}

//funzione per chiudere e rimuovere le fifo del nodo
static void destroyNodeFifos(int num_nodes) {
    if (to_miner != NULL) {
        for (int i = 0; i < num_nodes; i++) {
            if (to_miner[i] >= 0) close(to_miner[i]);
        }
        free(to_miner);
        to_miner = NULL;
    }
    if (from_miner != NULL) {
        for (int i = 0; i < num_nodes; i++) {
            if (from_miner[i] >= 0) close(from_miner[i]);
        }
        free(from_miner);
        from_miner = NULL;
    }
}




//manda una BlockResponse a un singolo miner
static int notify_miner(int miner_idx, uint64_t block_index, BlockValidationResult result) {
    if (to_miner[miner_idx] < 0) return -1;
 
    BlockResponse resp;
    resp.block_index = block_index;
    resp.miner_id    = miner_idx;
    resp.result      = result;
 
    ssize_t written = write(to_miner[miner_idx], &resp, sizeof(BlockResponse));
    if (written != sizeof(BlockResponse)) {
        log_msg("ERROR: notify_miner %d fallita", miner_idx);
        return -1;
    }
 
    log_msg("Notificato miner %d: block_index=%llu result=%s",
            miner_idx,
            (unsigned long long)block_index,
            result == BLOCK_VALID ? "VALID" : "INVALID");
    return 0;
}
 
//manda una BlockResponse a tutti i miner
static void notify_all_miners(uint64_t block_index, BlockValidationResult result) {
    for (int i = 0; i < num_miners; i++) {
        notify_miner(i, block_index, result);
    }
}


/*
 * valida un blocco e se valido lo aggiunge alla chain locale.
 *
 * RITORNA:
 *   0              blocco valido e aggiunto
 *   CHAIN_MISMATCH index o prev_hash errati
 *   INVALID_MERKLE merkle root non coincide
 *   BLOCK_TOO_FAR  index troppo avanti
 *   GENESIS_ERROR  errore sul blocco genesis
 *   -1             errore interno
 */
static int process_block(Block *new_block) {
    if (new_block == NULL) return INVALID_BLOCK;
 
    pthread_mutex_lock(&chain_mutex);
 
    char hash[HASH_HEX_SIZE + 1];
    if (blockGetHash(new_block, hash) != 0) {
        log_msg("ERROR: blockGetHash fallito");
        pthread_mutex_unlock(&chain_mutex);
        return -1;
    }
 
    if (last_block == NULL && chain_length == 0) {
        log_msg("Blocco genesis ricevuto: hash=%.16s...", hash);
 
        if (validate_merkle(new_block) != 0) {
            log_msg("ERROR: merkle del blocco genesis non valido");
            pthread_mutex_unlock(&chain_mutex);
            return GENESIS_ERROR;
        }
 
    } else if (last_block != NULL) {
        uint64_t new_index;
        uint64_t last_index;
        blockGetIndex(new_block,  &new_index);
        blockGetIndex(last_block, &last_index);
 
        if (new_index > last_index + 1) {
            log_msg("ERROR: blocco troppo avanti, index=%llu atteso=%llu",
                    (unsigned long long)new_index,
                    (unsigned long long)(last_index + 1));
            pthread_mutex_unlock(&chain_mutex);
            return BLOCK_TOO_FAR;
        }
 
        if (blockValidate(new_block, last_block) != 0) {
            log_msg("ERROR: CHAIN_MISMATCH hash=%.16s...", hash);
            pthread_mutex_unlock(&chain_mutex);
            return CHAIN_MISMATCH;
        }
 
        if (validate_merkle(new_block) != 0) {
            log_msg("ERROR: INVALID_MERKLE hash=%.16s...", hash);
            pthread_mutex_unlock(&chain_mutex);
            return INVALID_MERKLE;
        }
 
    } else {
        log_msg("ERROR: stato interno inconsistente (last_block NULL, chain_length=%llu)",
                (unsigned long long)chain_length);
        pthread_mutex_unlock(&chain_mutex);
        return -1;
    }
 
    Block *old = last_block;
    last_block  = new_block;
    chain_length++;
 
    pthread_mutex_unlock(&chain_mutex);
 
    if (old != NULL) blockDestroy(old);
 
    nSSetLastBlock(status, new_block);
    nSSetChainLength(status, chain_length);
    nSSetState(status, NODE_IDLE);
 
    if (append_block_to_csv_local(new_block) != 0) {
        log_msg("ERROR: scrittura CSV locale fallita");
        return CSV_ERROR;
    }
 
    log_msg("Blocco accettato: index=%llu hash=%.16s...",
            (unsigned long long)(chain_length - 1), hash);
 
    return 0;
}


/*legge blocchi dai miner con select() e li valida.
 *
 * FLUSSO:
 *   1. select() su tutti i from_miner in attesa di blocchi
 *   2. Per ogni pipe pronta legge il messaggio e deserializza il blocco
 *   3. Valida il blocco con process_block
 *   4. Se valido:
 *     Bisogna implementare la propagazione del blocco agli altri nodi (non implementata in questa funzione)
 *   5. Se non valido:
 *      - manda BLOCK_INVALID solo al miner che lo ha inviato
 *      - continua ad ascoltare
 */
static void *listener_thread(void *arg) {
    (void)arg;
 
    log_msg("Listener thread avviato, ascolto su %d miner", num_miners);
 
    int block_accepted = 0;
 
    while (running && !block_accepted) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;
 
        for (int i = 0; i < num_miners; i++) {
            if (from_miner[i] < 0) continue;
            FD_SET(from_miner[i], &rfds);
            if (from_miner[i] > maxfd) maxfd = from_miner[i];
        }
 
        if (maxfd < 0) {
            log_msg("Tutte le pipe dei miner chiuse, listener termina");
            break;
        }

        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        int ready = select(maxfd + 1, &rfds, NULL, NULL, &tv);
 
        if (ready < 0) {
            if (errno == EINTR) continue;
            log_msg("ERROR: select fallita: %s", strerror(errno));
            break;
        }
 
        if (ready == 0) continue;  
 
        for (int i = 0; i < num_miners; i++) {
            if (from_miner[i] < 0)               continue;
            if (!FD_ISSET(from_miner[i], &rfds)) continue;
 
            Message msg;
            messageInit(&msg);
 
            int r = receiveMessage(from_miner[i], &msg);
            if (r == SOCKET_CLOSED) {
                log_msg("Pipe del miner %d chiusa", i);
                close(from_miner[i]);
                from_miner[i] = -1;
                continue;
            }
            if (r != 0) {
                log_msg("ERROR: receiveMessage dal miner %d fallita: %d", i, r);
                continue;
            }
 
            MessageType type;
            messageGetType(&msg, &type);
 
            if (type != MSG_BLOCK_MINED) {
                log_msg("WARN: tipo messaggio inatteso dal miner %d: %d", i, type);
                continue;
            }
 
            log_msg("Ricevuto MSG_BLOCK_MINED dal miner %d", i);
 
            char csv_line[BLOCK_CSV_LINE_SIZE];
            if (messageGetPayload(&msg, csv_line, sizeof(csv_line)) != 0) {
                log_msg("ERROR: messageGetPayload fallita (miner %d)", i);
                continue;
            }
 
            Block *new_block = blockCreate();
            if (new_block == NULL) {
                log_msg("ERROR: blockCreate fallita");
                continue;
            }
 
            if (blockFromCsv(new_block, csv_line) != 0) {
                log_msg("ERROR: blockFromCsv fallita (miner %d)", i);
                blockDestroy(new_block);
                continue;
            }
 
            uint64_t block_index;
            blockGetIndex(new_block, &block_index);
 
            int res = process_block(new_block);
 
            if (res == 0) {
                //blocco valido va implementata la comunicazione con gli altri nodi per propagare il blocco
            
            
            } else {
                log_msg("Blocco dal miner %d rifiutato (codice %d)", i, res);
                blockDestroy(new_block);
                notify_miner(i, block_index, BLOCK_INVALID);
            }
        }
    }
 
    log_msg("Listener thread terminato");
    return NULL;
}