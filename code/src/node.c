#include "node.h"
#include "block.h"
#include "message.h"
#include "childProcess.h"
#include "error.h"
#include "protocolSocket.h"
#include "constants.h"

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


// Puntatore all'ultimo blocco della catena locale 
static Block   *last_block   = NULL; 
static uint64_t chain_length = 0;

static pthread_mutex_t chain_mutex = PTHREAD_MUTEX_INITIALIZER;


static int    node_id    = -1;
static int    num_nodes  = 0;
static int    num_miners = 0;

/* Array degli fd di lettura (da ogni miner) e scrittura (verso ogni miner) */
static int *read_fds  = NULL;   /* read_fds[i]  = pipe miner_i → questo node */
static int *write_fds = NULL;   /* write_fds[i] = pipe questo node → miner_i  */

static FILE *log_file = NULL;


typedef struct {
    int *read_fds;
    int  num_miners;
} ListenerArgs;

static void handle_signal(int sig) {
    if (sig == SIGTERM || sig == SIGINT) {
        running = 0;
    }
}

//un blocco arriva e viene validato, se valido blocca il thread di comunicazione.
//se non valido rimandato indieto al miner che lo ha minato. 



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

//funzione per caricare lo stato iniziale della blockchain da un file csv
static int load_initial_state(const char *csv_path) {
    if (csv_path == NULL || strlen(csv_path) == 0) return 0;
 
    FILE *f = fopen(csv_path, "r");
    if (f == NULL) {
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

