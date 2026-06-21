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

/* 
Copia l'intero CSV condiviso nella copia locale del node (node_<id>_blockchain.csv)
Viene chiamata quando si utilizza il semaforo, cosi' la copia riflette uno stato consistente
del CSV condiviso (nessuna riga letta a meta' mentre un altro nodo aggiunge un blocco)
La copia locale si riscrive da capo ogni volta che viene consultato il CSV 
*/ 
static int mirror_shared_to_local(void) {
    char local_path[64];
    snprintf(local_path, sizeof(local_path), "node_%d_blockchain.csv", node_id);

    FILE *src = fopen(CSV_FILE_NAME, "r");
    if (src == NULL) {
        log_msg("ERROR: apertura CSV condiviso per mirror fallita: %s", strerror(errno));
        return CSV_ERROR;
    }
    FILE *dst = fopen(local_path, "w");
    if (dst == NULL) {
        log_msg("ERROR: apertura copia locale fallita: %s", strerror(errno));
        fclose(src);
        return CSV_ERROR;
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) {
            log_msg("ERROR: scrittura copia locale fallita");
            fclose(src);
            fclose(dst);
            return CSV_ERROR;
        }
    }
    int read_err = ferror(src);
    fclose(src);
    fclose(dst);
    if (read_err) {
        log_msg("ERROR: lettura CSV condiviso per mirror fallita");
        return CSV_ERROR;
    }
    return 0;
}

// Chiamata protetta dal mutex, precedentemente preso dal process_block
// Quando accede alla CS , legge la blockchain, aggiorna la propria,
// e verifica che il blocco sia effettivamente corretto, se sì
// lo mette come successivo, se invalido allora lo rifiuta 
static int commit_block_to_shared_csv(Block *new_block) {
    sem_t *sem = sem_open(CSV_SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        log_msg("ERROR: sem_open fallito: %s", strerror(errno));
        return SEM_ERROR;
    }
    if (sem_wait(sem) == -1) {
        log_msg("ERROR: sem_wait fallito: %s", strerror(errno));
        sem_close(sem);
        return SEM_ERROR;
    }

    int rc = 0;
    FILE *f = NULL;
    Block *csv_tail = NULL;
    int found = 0;
    uint64_t tail_index = 0;
    uint64_t new_index = 0;
    char line[BLOCK_CSV_LINE_SIZE];
    char last_line[BLOCK_CSV_LINE_SIZE];
    char out_line[BLOCK_CSV_LINE_SIZE];

    // Legge la coda della blockchain all'interno del CSV condiviso
    f = fopen(CSV_FILE_NAME, "r");
    if (f == NULL) {
        log_msg("ERROR: apertura del CSV condiviso in lettura fallita: %s", strerror(errno));
        rc = CSV_ERROR;
        goto out;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0' || strncmp(line, "index,", 6) == 0) continue;
        strncpy(last_line, line, sizeof(last_line) - 1);
        last_line[sizeof(last_line) - 1] = '\0';
        found = 1;
    }
    if (ferror(f)) {
        log_msg("ERROR: lettura del CSV condiviso fallita");
        rc = CSV_ERROR;
        goto out;
    }
    fclose(f);
    f = NULL;
    if (!found) {
        log_msg("ERROR: Il CSV condiviso non presenta alcun blocco");
        rc = CSV_ERROR;
        goto out;
    }

    csv_tail = blockCreate();
    if (csv_tail == NULL) {
        log_msg("ERROR: L'aggiunta del Blocco in coda è fallita ");
        rc = -1;
        goto out;
    }
    if (blockFromCsv(csv_tail, last_line) != 0) {
        log_msg("ERROR: La coda del CSV condiviso non valida");
        rc = INVALID_BLOCK;
        goto out;
    }

    // Verifica del blocco
    if (blockValidate(new_block, csv_tail) != 0) {
        // Non è valido: Il node ri-sincronizza la chain in memoria
        blockGetIndex(csv_tail, &tail_index);
        if (last_block != NULL) blockDestroy(last_block);
        last_block = csv_tail;      
        csv_tail = NULL;            
        chain_length = tail_index + 1;
        log_msg("Blocco rifiutato dal CSV condiviso, chain ri-sincronizzata");
        rc = CHAIN_MISMATCH;
        goto out;
    }

    // Se il blocco è valido allora, lo si conferma e lo si aggiunge alla blockchain
    if (blockToCsv(new_block, out_line, sizeof(out_line)) != 0) {
        log_msg("ERROR: blockToCsv fallito");
        rc = CSV_ERROR;
        goto out;
    }
    f = fopen(CSV_FILE_NAME, "a");
    if (f == NULL) {
        log_msg("ERROR: apertura del CSV condiviso per l'aggiunta del blocco fallita: %s", strerror(errno));
        rc = CSV_ERROR;
        goto out;
    }
    if (fprintf(f, "%s\n", out_line) < 0) {
        log_msg("ERROR: scrittura sul CSV condiviso fallita");
        rc = CSV_ERROR;
        goto out;
    }
    fclose(f);
    f = NULL;

    // Fa avanzare la chain in memoria al blocco appena scritto
    blockGetIndex(new_block, &new_index);
    if (last_block != NULL) blockDestroy(last_block);
    last_block = new_block;         // il globale prende possesso di new_block
    chain_length = new_index + 1;

out:
    if (f != NULL) fclose(f);
    if (csv_tail != NULL) 
    blockDestroy(csv_tail);
    sem_post(sem);
    // aggiorna la copia locale del node copiandola dal CSV condiviso
    if( rc == 0 || rc == CHAIN_MISMATCH) {
        mirror_shared_to_local();
    }
    sem_close(sem);
    return rc;
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

static int process_block(Block *new_block) {
    if (new_block == NULL) return INVALID_BLOCK;

    char hash[HASH_HEX_SIZE + 1];
    if (blockGetHash(new_block, hash) != 0) {
        log_msg("ERROR: blockGetHash fallito");
        return -1;
    }

    // Validazione del Blocco ( essendo che dipende solo dalle transazioni,
    // la validazione la facciamo al di fuori del lock
        if (validate_merkle(new_block) != 0) {
        log_msg("ERROR: INVALID_MERKLE hash=%.16s...", hash);
        return INVALID_MERKLE;
    }

    pthread_mutex_lock(&chain_mutex);

    // L'autorità è della coda del CSV condiviso quindi si ri-sincronizza la blockchain,
    // basandosi su quella e in caso di Blocco validato la si aggiorna

    int rc = commit_block_to_shared_csv(new_block);
    if (rc != 0) {
        pthread_mutex_unlock(&chain_mutex);
        return rc;
    }

    uint64_t len = chain_length;
    pthread_mutex_unlock(&chain_mutex);

    nSSetLastBlock(status, new_block);
    nSSetChainLength(status, len);
    nSSetState(status, NODE_IDLE);

    log_msg("Blocco accettato: index=%llu hash=%.16s...",
            (unsigned long long)(len - 1), hash);
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
                log_msg("Blocco dal miner %d accettato", i);
                notify_all_miners(block_index, BLOCK_VALID);
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

int main (int argc, char* argv[]){

    // Il processo node viene avviato dal bootstrap/launcher

    if (argc < 4 ) {
        fprintf(stderr, "Utilizzo: %s <node_id> <num_nodes> <num_miners>\n",argv[0]);
    return INVALID_PARAMS;
    }


    node_id= atoi(argv[1]); // identifica il nodo
    num_nodes = atoi(argv[2]); // totale del numero di nodi nel sistema
    num_miners = atoi(argv[3]); // numero totale di miners (serve per aprire le fifo verso i miner)


    if (node_id < 0 || 
        num_nodes <= 0 || 
        num_miners <= 0 || 
        node_id >= num_nodes){
            fprintf(stderr, "NODE: argomenti non validi\n");
            return INVALID_PARAMS;
    }

    char log_path[64]; // ogni processo deve avere il suo file di log
    snprintf(log_path, 
            sizeof(log_path),
            "node-%d.log",
            (int) getpid()); // usiamo il pid reale

    log_file = fopen(log_path, "a");
    if (log_file == NULL) {
        fprintf(stderr, "NODE %d: impossibile aprire il log%s:%s\n", 
            node_id, 
            log_path, 
            strerror(errno));
        return -1;
    }

    /*Singal handler : se arriva sigterm o sigint, 
    handle_signal mette runnin g = 0
    In modo che il processo esca in modo sicuro
    */ 
    
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if(sigaction(SIGTERM, &sa, NULL) == -1 ||
       sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, "NODE %d: sigaction fallita: %s\n",
            node_id,
            strerror(errno));
        fclose(log_file);
        return -1;
       }

    log_msg("Avvio node");

    /*
    NodeStatus contiene lo stato logico del node
    ovvero non possedendo i blocchi: conserva solo riferimenti e metadati
    protetti da un mutex
    */

    status = nodeCreateStatus();
    if(status == NULL) {
        log_msg("ERROR: nodeCreateStatus fallita ");
        fclose(log_file);
        return -1;
    }
    
    /*
    ChildProcess descrive questo processo dal punto di vista logico
    ovvero pid reale ma id logico e ruolo NODE
    */

    ChildProcess * cp = childProcessCreate();
    if (cp == NULL) {
        log_msg("ERROR: childProcessCreate fallita");
        nodeDestroyStatus(status);
        fclose(log_file);
        return -1;
    }

    if(childProcessInit(cp,getpid(),node_id,NODE) != 0){
        log_msg("ERROR: childProcessInit fallita");
        childProcessDestroy(cp);
        nodeDestroyStatus(status);
        fclose(log_file);
        return INVALID_PARAMS;
    }

    /*
    Copio le informazioni del ChildProcess dentro NodeStatus
    In modo che NodeStatus abbia una copia , e quindi possiamo distruggere cp
    */

    if (nodeInitStatus(status, cp, NODE_IDLE, 0) != 0) {
        log_msg("ERROR: nodeInitStatus fallita");
        childProcessDestroy(cp);
        nodeDestroyStatus(status);
        fclose(log_file);
        return INVALID_PARAMS;
    }

    childProcessDestroy(cp);

    /* 
    Carico la blockchain iniziale dal CVS condiviso
    Il bootstrap crea questo file prima di lanciare i node 
    */

     if (load_initial_state(CSV_FILE_NAME) != 0) {
        log_msg("ERROR: load_initial_state fallita");
        nodeDestroyStatus(status);
        fclose(log_file);
        return CSV_ERROR;
    }

    /*
    Sincronizzazione di NodeStatus con lo stato caricato da CSV
    l'ultimo blocco rimane comunque a node.c mentre
    NodeStatus possiede il puntatore const
    */

    pthread_mutex_lock(&chain_mutex);
    const Block *loaded_last_block = last_block;
    uint64_t loaded_chain_length = chain_length;
    pthread_mutex_unlock(&chain_mutex);

    if(loaded_last_block != NULL) {
        nSSetLastBlock(status, loaded_last_block);
    }
    nSSetChainLength(status,loaded_chain_length);

    /*
    Creazione del canale di comunicazione node -> Miners    
    */

    if(createNodeFifos(num_miners) != 0){
        log_msg("ERROR: createNodeFifos fallita");
        nodeDestroyStatus(status);
        if (last_block != NULL) blockDestroy(last_block);
        fclose(log_file);
        return FIFO_ERROR;
    }

    /*
    Il thread di comunicazione resta in ascolto sul canale dei miner
    */

    pthread_t listener;
    if(pthread_create(&listener, NULL, listener_thread, NULL) != 0){
        log_msg("ERROR: pthread_create listener fallita");
        destroyNodeFifos(num_miners);
        nodeDestroyStatus(status);
        if (last_block != NULL) blockDestroy(last_block);
        fclose(log_file);
        return -1;
    }

    log_msg("Node avviato correttamente");

    /*
    Il processo si ferma solo in caso di SIGTERM o SIGINT
    */

    while (running){
        pause();
    }

    log_msg("Terminazione del blocco richiesto");

    /*
    Appena il thred di comunicazione termina , rilasciamo le risorse condivise
    */

    pthread_join(listener, NULL);

    destroyNodeFifos(num_miners);

    nodeDestroyStatus(status);

    status = NULL;

    if(last_block != NULL) {
        blockDestroy(last_block);
        last_block = NULL;
    }

    if(log_file != NULL) {
        log_msg("Node terminato");
        fclose(log_file);
        log_file = NULL;
    }

    return 0;
}