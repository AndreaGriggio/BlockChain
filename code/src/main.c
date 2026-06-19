#include "error.h" /* codici di errore comuni  */
#include "block.h" // Block, TxList, blockCreate, blockInit, blockToCsv, blockDestroy
#include <stdio.h>
#include <time.h> // time() per il timestamp del blocco genesis
#include <stdlib.h> // atoi()
#include <string.h> 
#include <errno.h> // errno + strerror() per i messaggi di errore sulle operazioni sui file
#include <semaphore.h> // sem_open, sem_close, sem_unlink
#include <fcntl.h> // O_CREAT, O_EXCL per sem_open 

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
	
	int rc3 = createCsvSemaphore();
	if (rc3 != 0){
		return rc3;
	}
	printf("Semaforo %s creato con successo\n", CSV_SEM_NAME);
	
	return 0;
}
	