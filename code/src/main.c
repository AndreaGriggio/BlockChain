#include "error.h" /* codici di errore comuni  */
#include "block.h" // Block, TxList, blockCreate, blockInit, blockToCsv, blockDestroy
#include <stdio.h>
#include <time.h> // time() per il timestamp del blocco genesis
#include <stdlib.h> // atoi()
#include <string.h> 
#include <errno.h> // errno + strerror() per i messaggi di errore sulle operazioni sui file
#include <semaphore.h> // sem_open, sem_close, sem_unlink
#include <fcntl.h> // O_CREAT, O_EXCL per sem_open 
#include <unistd.h>      // unlink, close
#include <sys/socket.h>  // socket, bind, listen
#include <sys/un.h>      // struct sockaddr_un
#include <sys/stat.h>    // mkdir
#include "constants.h"   // MINERS_SOCKET
#include <signal.h>     // sigaction
#include <sys/wait.h>   // waitpid
#include "message.h"        // Message, MSG_NEW_TX, sendMessage (protocolSocket.h/childProcess.h)
#include "childProcess.h"   // ChildProcess per identificare il mittente
#include "utils.h"          // validateTransaction 


/* 
Path su cui viene scritto/mantenuto lo stato della catena,
deve inoltre combaciare con CSV_FILE_NAME in node.c
perchè i nodes appendono sempre lì i blocchi nuovi:
se fosse diverso da quello caricato all'inizio 
la chain in memoria e quella su disco divergerebbero 
e i nodi non riuscirebbero a validare i blocchi nuovi
*/

#define BLOCKCHAIN_CSV_PATH "./blockchain.csv" 

#define CSV_SEM_NAME "/blockchain_csv" // deve combaciare con CSV_SEM_NAME in node.c per sincronizzare l'accesso al file CSV tra main.c e node.c

#define MAX_CHILDREN 512


/*
Configurazione del bootstrap, letta dagli argomenti della riga di comando.
Usiamo una struct invece di variabili globali perchè
questi valori vaggeranno insieme attraverso le funzioni 
e non vogliamo dipendere da variabili globali.
(creazione genesis, socket, fifo , fork dei nodes/miners/clients, ecc)
*/
typedef struct {
	int num_nodes;
	int num_miners;
	int num_clients;
	int transactions_frequency;
	int difficulty;
	const char * initial_csv;
} BootstrapConfig;

/*
Legge e valida gli argomenti della riga di comando,
 ./blockchain <num_nodes> <num_miners> <num_clients> 
	[<transactions_frequency>] [<difficulty>] [<initial_csv>]
I primi 3 sono obbligatori, gli altri sono opzionali 
e hanno valori di default.
Ritorna 0 se tutto ok, altrimenti un codice di errore. (error.h)
*/
static int parseArgs( int argc, char *argv[], BootstrapConfig *cfg) {
	if (argc < 4) {
		fprintf(stderr, 
			"Utilizzo: %s <num_nodes> <num_miners> <num_clients> "
			"<transactions_frequency> <difficulty> <initial_csv>\n"
			, argv[0]);
			return INVALID_PARAMS;
	}

/*
argv è un array di stringhe (testo):
converte quel testo in un intero  utilizzabile
*/
	cfg->num_nodes = atoi(argv[1]);
	cfg->num_miners = atoi(argv[2]);
	cfg->num_clients = atoi(argv[3]);

/* 
Argomenti opzionali: se l'utente li ha effettivamente scritti
cioè argc > 4, argc > 5, argc > 6 , li parsiamo;
altrimenti usiamo un deafult.
 */
	cfg->transactions_frequency = (argc > 4) ? atoi(argv[4]) : 1;
	cfg->difficulty = (argc > 5) ? atoi(argv[5]) : 12;
	cfg->initial_csv = (argc > 6) ? argv[6] : NULL;

/*
atoi () non fa alcun controllo se il testo è un numero valido,
quindi dobbiamo fare un controllo manuale 
per evitare valori negativi o zero
*/
	if (cfg->num_nodes <= 0 || cfg->num_miners <= 0 || cfg->num_clients <= 0) {
		fprintf(stderr, "Errore: num_nodes, num_miners e num_clients devono essere maggiori di 0.\n");
		return INVALID_PARAMS;
	}

	if (cfg->transactions_frequency <= 0 ) {
		fprintf(stderr, "Errore: transactions_frequency non è valida\n");
		return INVALID_PARAMS;
	}
	if(cfg->difficulty <= 0 ) {
		fprintf(stderr, "Errore: difficulty non è valida\n");
		return INVALID_PARAMS;
	}

	return 0;
}

/*
Copia il file CSV iniziale fornito dall'utente (cfg->initial_csv) in BLOCKCHAIN_CSV_PATH.
Cioè lo stesso path in cui node.c poi appenderebbe i blocchi nuovi (CSV_FILE_NAME).
Ritorna 0 se tutto ok, altrimenti un codice di errore. (error.h)
*/
static int copyInitialCsv(const char *src_path, const char *dst_path) {
	FILE *src = fopen(src_path, "r");
	if (src == NULL) {
		fprintf(stderr, "Errore nell'apertura del file CSV di origine: %s\n", src_path);
		return CSV_ERROR;
	}

	FILE *dst = fopen(dst_path, "w");
	if (dst == NULL) {
		fprintf(stderr, "Errore nell'apertura di %s: %s\n", dst_path, strerror(errno));
		fclose(src);
		return CSV_ERROR;
	}

	/*
	Copiamo byte a byte con un buffer fisso, non riga per riga:
	una riga CSV con molte transazioni potrebbe superare la dimensione del buffer
	*/
	char buffer[4096];
	size_t n;
	while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
		if (fwrite(buffer, 1, n, dst) != n) {
			fprintf(stderr, "Errore nella scrittura su %s: %s\n", dst_path, strerror(errno));
			fclose(src);
			fclose(dst);
			return CSV_ERROR;
		}
	}

	if (ferror(src)) {
		fprintf(stderr, "Errore nella lettura di %s\n", src_path);
		fclose(src);
		fclose(dst);
		return CSV_ERROR;
	}

	fclose(src);
	fclose(dst);
	return 0;
}

/*
Crea il blocco genesis ovvero il primo blocco della blockchain e lo scrive nel file CSV specificato.
Ritorna 0 se tutto ok, altrimenti un codice di errore. (error.h)
Usato solo se l'utente non ha fornito un file CSV iniziale.
*/
static int createGenesisBlock(const char *csv_path) {
		Block *genesis = blockCreate();
		if (genesis == NULL) {
			fprintf(stderr, "Errore nella creazione del blocco genesis\n");
			return 1;
		}


	TxList txs;
	txs.count = 1;
	snprintf(txs.strings[0],MAX_TX_SIZE, "Genesis Block");

	if (blockInit (genesis, 0,(uint64_t)time(NULL),NULL,0,&txs) != 0) {
		fprintf(stderr, "Errore nell'inizializzazione del blocco genesis\n");
		blockDestroy(genesis);
		return INVALID_BLOCK;
	}

	char line[BLOCK_CSV_LINE_SIZE];
	if (blockToCsv(genesis, line, sizeof(line)) != 0) {
		fprintf(stderr, "Errore nella conversione del blocco genesis in CSV\n");
		blockDestroy(genesis);
		return INVALID_BLOCK;
	}

	FILE *f = fopen(csv_path, "w");
	if (f == NULL) {
		fprintf(stderr, "Errore nell'apertura del file CSV: %s\n", strerror(errno));
		blockDestroy(genesis);
		return CSV_ERROR;
	}

	fprintf(f, "index,timestamp,prev_hash,merkle_root,nonce,transactions\n");
	fprintf(f, "%s\n", line);
	fclose(f);

	blockDestroy(genesis);

	return 0;
}

/*
Crea (da zero) il semaforo POSIX condiviso usato da tutti i node
per serializzare l'accesso al file CSV della blockchain (CSV_SEM_NAME).
Deve esistere PRIMA di fare fork() di qualunque node, perchè node.c lo apre con sem_open
senza O_CREAT: se non esiste già, sem_open fallisce e i nodes non partono.
*/

static int createCsvSemaphore(void) {

	/*
	Rimuoviamo un eventuale semaforo con lo stesso nome 
	rimasto da un esecuzione precedente 
	che non è stata chiusa correttamente
	*/
	sem_unlink(CSV_SEM_NAME);

	/*
	O_EXCL garantisce che siamo noi a creare il semaforo 
	Valore iniziale 1: si comporta come un mutex, un solo node alla volta
	può scrivere sul CSV 
	- sem_wait -> lo decrementa a 0
	- sem_post -> lo riporta ad 1 
	*/
	sem_t *sem = sem_open(CSV_SEM_NAME, O_CREAT | O_EXCL, 0644, 1);
	if (sem == SEM_FAILED) {
		fprintf(stderr, "Errore nella creazione del semaforo %s: %s\n", CSV_SEM_NAME, strerror(errno));
		return SEM_ERROR;
	}

	/*
	Chiudiamo l'handel:
	il semaforo resta vivo,
	ogni node lo riaprirà per conto proprio 
	*/

	sem_close(sem);
	return 0;
}

/*
La cartella ./tmp/ deve esistere prima di creare socket e FIFO
*/
static int ensureTmpDir(void) {
    if (mkdir("tmp", 0755) < 0 && errno != EEXIST) {
        fprintf(stderr, "Errore creazione cartella tmp: %s\n", strerror(errno));
        return CSV_ERROR;
    }
    return 0;
}

/*
Crea il socket Unix su cui i miner riceveranno le transazioni dai client
Il bootstrap lo crea (bind+listen) 
PRIMA del fork: i miner erediteranno l'fd e faranno accept(), e i client faranno connect() 
*/
static int createMinersSocket(void) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "Errore socket(): %s\n", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MINERS_SOCKET, sizeof(addr.sun_path) - 1);

    // rimuove un eventuale socket rimasto da un'esecuzione precedente,
    // altrimenti bind() fallisce con "Address already in use"
    unlink(MINERS_SOCKET);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Errore bind(%s): %s\n", MINERS_SOCKET, strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, SOMAXCONN) < 0) {
        fprintf(stderr, "Errore listen(): %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

static pid_t children[MAX_CHILDREN];
static int   n_children = 0;
static pid_t child_pgid = 0;
static volatile sig_atomic_t running = 1;

static void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) running = 0;
}

static pid_t spawn_child(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }
    if (pid == 0) {
        setpgid(0, child_pgid);
        execv(argv[0], argv);
        perror("execv");
        _exit(127);
    }
    if (child_pgid == 0) child_pgid = pid;
    setpgid(pid, child_pgid);
    if (n_children < MAX_CHILDREN) children[n_children++] = pid;
    return pid;
}

/*
 Invia una transazione ai miner connettendosi a MINERS_SOCKET,
 con lo stesso protocollo del client (Message di tipo MSG_NEW_TX).
 Il padre si presenta come mittente con ruolo CLIENT.
 Ritorna 0 se inviata, un codice di errore se la tx e' malformata
 oppure se la connessione/invio falliscono.
*/
static int submit_transaction(const char *tx) {
    // Verifica su lunghezza max
    if (tx == NULL || strlen(tx) > MAX_TX_SIZE) return INVALID_TRANSACTION;

    // Buffer
    char buf[MAX_TX_SIZE + 1];
    strncpy(buf, tx, sizeof buf - 1);
    buf[sizeof buf - 1] = '\0';

    // Verifica Contenuto
    if (validateTransaction(buf) != 0) return INVALID_TRANSACTION;

    // il padre si identifica come mittente: ruolo CLIENT, id convenzionale 0
    ChildProcess *self = childProcessCreate();
    if (self == NULL) return MEMORY_ERROR;
    if (childProcessInit(self, getpid(), 0, CLIENT) != 0) {
        childProcessDestroy(self);
        return INVALID_PARAMS;
    }

    // messaggio
    Message message;
    messageInit(&message);
    messageSetType(&message, MSG_NEW_TX);
    messageSetSender(&message, self);
    messageSetPayload(&message, buf, (uint32_t)strlen(buf));

    // Connessione Miners_Socket
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MINERS_SOCKET, sizeof addr.sun_path - 1);
    addr.sun_path[sizeof addr.sun_path - 1] = '\0';

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        childProcessDestroy(self);
        return SOCKET_ERROR;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        close(fd);
        childProcessDestroy(self);
        return SOCKET_ERROR;
    }

    int rc = sendMessage(fd, &message);   // serializza header+payload 
    close(fd);
    childProcessDestroy(self);
    return rc;
}


int main(int argc, char *argv[]) {
	BootstrapConfig cfg;
	int rc = parseArgs(argc, argv, &cfg);
	if (rc != 0) {
		return rc;
	}
	printf("Bootstrap: nodes=%d, miners=%d, clients=%d, transactions_frequency=%d, difficulty=%d, initial_csv=%s\n",
		cfg.num_nodes, cfg.num_miners, cfg.num_clients,
		cfg.transactions_frequency, cfg.difficulty,
		cfg.initial_csv ? cfg.initial_csv : "None");

	
	if (cfg.initial_csv == NULL) {
		int rc2 = createGenesisBlock(BLOCKCHAIN_CSV_PATH);
		if (rc2 != 0) {
			return rc2;
		}
			printf("Genesis block creato con successo in %s\n", BLOCKCHAIN_CSV_PATH);
		}else{
		int rc2 = copyInitialCsv(cfg.initial_csv, BLOCKCHAIN_CSV_PATH);
			if (rc2 != 0) {
			return rc2;
			}
			printf("File CSV iniziale copiato da %s a %s con successo\n", cfg.initial_csv, BLOCKCHAIN_CSV_PATH);
			}
	
		if (ensureTmpDir() != 0) {
        return CSV_ERROR;
    }

    int miners_fd = createMinersSocket();
	
    if (miners_fd < 0) {
        return SOCKET_ERROR;
    }
    printf("Socket dei miner creato su %s (fd=%d)\n", MINERS_SOCKET, miners_fd);

	int rc3 = createCsvSemaphore();
	if (rc3 != 0){
		return rc3;
	}
	printf("Semaforo %s creato con successo\n", CSV_SEM_NAME);
	
	/* 
	Handler per SIGINT/SIGTERM: lo shutdown del sistema parte da qui,
	abbassando running cosi' il loop principale esce e si fa il clean-up
	*/

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* 
	I parametri vanno passati ai figli via argv come stringhe:
    li convertiamo una volta sola, idbuf viene riusato a ogni fork
    (fork copia la memoria, il figlio legge il suo valore)
	*/
    char nnodes[12], nminers[12], diffbuf[12], freqbuf[12], fdbuf[12], idbuf[12];
    snprintf(nnodes,  sizeof nnodes,  "%d", cfg.num_nodes);
    snprintf(nminers, sizeof nminers, "%d", cfg.num_miners);
    snprintf(diffbuf, sizeof diffbuf, "%d", cfg.difficulty);
    snprintf(freqbuf, sizeof freqbuf, "%d", cfg.transactions_frequency);
    snprintf(fdbuf,   sizeof fdbuf,   "%d", miners_fd);

    /* 
	I Miner creano le FIFO che i node poi aprono in lettura
    Ogni miner riceve difficolta', il proprio id, il numero di nodi e
    l'fd del socket in ascolto (da ereditare per fare accept)
	*/

    for (int i = 0; i < cfg.num_miners; i++) {
        snprintf(idbuf, sizeof idbuf, "%d", i);
        char *argv_m[] = { "./code/bin/miner", diffbuf, idbuf, nnodes, fdbuf, NULL };
        spawn_child(argv_m);
    }
    /* 
	Un processo node per ciascun nodo: 
	riceve il proprio id, il totale dei nodi e il numero di miner 
	*/
    for (int i = 0; i < cfg.num_nodes; i++) {
        snprintf(idbuf, sizeof idbuf, "%d", i);
        char *argv_n[] = { "./code/bin/node", idbuf, nnodes, nminers, NULL };
        spawn_child(argv_n);
    }
    /* I client: ricevono la frequenza di generazione e il proprio id. */
    for (int i = 0; i < cfg.num_clients; i++) {
        snprintf(idbuf, sizeof idbuf, "%d", i);
        char *argv_c[] = { "./code/bin/client", freqbuf, idbuf, NULL };
        spawn_child(argv_c);
    }

    printf("Avviati %d processi (pgid=%d). Ctrl-C per terminare.\n", n_children, child_pgid);

    /*
    Il padre fa da supervisore: invece di dormire aspettando un segnale,
    apre una REPL e resta in ascolto dei comandi dell'utente su stdin
    Se arriva SIGINT/SIGTERM mentre fgets e' bloccata, fgets ritorna NULL
    Ctrl-D (EOF) lo trattiamo come 'stop'.
    */
    char line[512];
    printf("> ");
    fflush(stdout);

    while (running && fgets(line, sizeof line, stdin) != NULL) {

        // toglie il newline che fgets lascia in fondo alla riga
        line[strcspn(line, "\n")] = '\0';

        // il primo token e' il comando
        char *cmd = strtok(line, " ");
        if (cmd == NULL) {           
            printf("> ");
            fflush(stdout);
            continue;
        }

        if (strcmp(cmd, "pause") == 0) {
            // sospendo tutti i figli (miner/node/client), il padre resta vivo
            killpg(child_pgid, SIGSTOP);
            printf("Sistema in pausa\n");

        } else if (strcmp(cmd, "resume") == 0) {
            killpg(child_pgid, SIGCONT);
            printf("Sistema ripreso\n");

		        } else if (strcmp(cmd, "submit") == 0) {
            // Argomento dopo "submit"
            char *arg = strtok(NULL, "");
            if (arg == NULL) {
                printf("Uso: submit \"Mittente pays Destinatario N coins\"\n");
            } else {
                // Pulizia virgolette
                if (*arg == '"') arg++;
                size_t alen = strlen(arg);
                if (alen > 0 && arg[alen - 1] == '"') arg[alen - 1] = '\0';

                // Invio
                if (submit_transaction(arg) == 0)
                    printf("Transazione inviata: %s\n", arg);
                else
                    printf("Transazione rifiutata (malformata o invio fallito)\n");
            }

        } else if (strcmp(cmd, "stop") == 0) {
            // Uscita dal loop, poi terminazione
            running = 0;
            break;

        } else {
            printf("Comando sconosciuto: %s\n", cmd);
        }


        printf("> ");
        fflush(stdout);
    }

    /* 
	Shutdown : prima avviamo la terminazione ordinata (SIGTERM),
	poi con SIGKILL terminiamo chiunque sia rimasto bloccato o crashato
	Waitpid raccoglie tutti gli zombie. 
	*/
    killpg(child_pgid, SIGCONT);   // sveglia quelli in pausa
    killpg(child_pgid, SIGTERM);
    sleep(2);
    killpg(child_pgid, SIGKILL);
    for (int i = 0; i < n_children; i++) waitpid(children[i], NULL, 0);

    /* 
	Pulizia delle risorse IPC create dal padre
	*/
    unlink(MINERS_SOCKET); 
	
	/* 
	I figli creano delle FIFO in ./tmp/, ma quando vengono terminati
	con SIGKILL non fanno in tempo a rimuoverle quindi
	le ripulisce il padre, che conosce gli id
	*/
    for (int m = 0; m < cfg.num_miners; m++) {
        for (int n = 0; n < cfg.num_nodes; n++) {
            char p[64];
            snprintf(p, sizeof p, "%s%d%d", MINER_NODE_FIFO, m, n); unlink(p);
            snprintf(p, sizeof p, "%s%d%d", NODE_MINER_FIFO, n, m); unlink(p);
        }
    }
    sem_unlink(CSV_SEM_NAME);
    return 0;
}

	