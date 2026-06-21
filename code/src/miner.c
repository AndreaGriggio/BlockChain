#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "error.h"
#include "miner.h"

#include <pthread.h>

#include "client.h"
#include "minerStatus.h"
#include "utils.h"

typedef struct Miner {
    
    u_int64_t last_block_index;
    char transactions[MAX_TX_PER_BLOCK][MAX_TX_SIZE+1];
    size_t transactions_count;
    uint difficulty;
    Block* mined_block;
}Miner;

/**
 * Alloca sull'heap una nuova struttura Miner (non inizializzata).
 * @return Puntatore al miner allocato, oppure NULL se l'allocazione fallisce
 */
Miner* minerCreate(){
    Miner* miner = malloc(sizeof(Miner));
    if (miner == NULL) {
        return NULL;
    }

    return miner;
}

/**
 * Distrugge un miner liberando sia l'eventuale blocco minato che la struttura stessa.
 * @param miner Miner da distruggere
 * @return 0 se tutto è andato a buon fine, 1 se miner è NULL
 */
int minerDestroy(Miner* miner) {
    if (miner == NULL)return 1;

    if (miner->mined_block != NULL) free(miner->mined_block);


    free(miner);
    return 0;
}

/**
 * Inizializza i campi di un miner già allocato impostando la difficoltà e
 * azzerando contatori e blocco minato.
 * @param miner Miner da inizializzare
 * @param miner_difficulty Difficoltà di mining da assegnare al miner
 * @return 0 se tutto è andato a buon fine, -1 se miner è NULL
 */
int minerInit(Miner* miner,uint miner_difficulty){
    if (miner == NULL) {
        return -1;
    }
    miner->difficulty = miner_difficulty;
    miner->transactions_count = 0;
    miner->last_block_index = 0;
    miner->mined_block = NULL;
    return 0;
}



/**
 * Valida tutte le transazioni del miner compattando quelle valide all'inizio
 * dell'array (le non valide vengono scartate) e aggiornando il conteggio.
 * @param miner_ptr Miner di cui validare le transazioni
 * @param number_of_transactions Numero di transazioni da esaminare
 * @return Numero di transazioni valide rimaste, oppure INVALID_PARAMS per
 *         parametri non validi
 */
int minerValidateTransactions(Miner* miner_ptr,
                              const size_t number_of_transactions)
{
    if (miner_ptr == NULL  || number_of_transactions > MAX_TX_PER_BLOCK) {
        return INVALID_PARAMS;
    }


    size_t write_idx = 0;

    //Riscrive solamente le transaction valide nel primo posto "libero" = write_idx
    for (size_t i = 0; i < number_of_transactions; i++) {
        if (minerValidateTransaction(miner_ptr,&i) == 0) {
            if (write_idx != i) {
                strcpy(miner_ptr->transactions[write_idx], miner_ptr->transactions[i]);
            }
            write_idx++;
        }
    }
    miner_ptr->transactions_count = write_idx;
    return write_idx;   // numero di transazioni valide rimaste nell'array
}
/**
 * Valida la singola transazione del miner all'indice indicato (controlla indice,
 * lunghezza e validità del contenuto).
 * @param miner_ptr Miner che contiene la transazione
 * @param transaction_idx Puntatore all'indice della transazione da validare
 * @return 0 se la transazione è valida, INVALID_PARAMS altrimenti
 */
int minerValidateTransaction(const Miner *miner_ptr, const size_t *transaction_idx) {
    if (miner_ptr == NULL || transaction_idx == NULL) return INVALID_PARAMS;
    if ( miner_ptr->transactions_count < *transaction_idx) {return INVALID_PARAMS;}
    if (strlen(miner_ptr->transactions[*transaction_idx]) > MAX_TX_SIZE)    return INVALID_PARAMS;

    return validateTransaction(miner_ptr->transactions[*transaction_idx]);
}



/**
 * Estrae le transazioni dal payload di un messaggio (separate da "::") e le
 * inserisce nell'array del miner, aggiornando il relativo conteggio.
 * @param miner Miner in cui salvare le transazioni
 * @param message_ptr Messaggio contenente le transazioni nel payload
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS per parametri nulli,
 *         BUFFER_TOO_SMALL se il payload eccede la dimensione massima
 */
int minerGetTransactionsFromMessage(Miner* miner, const Message *message_ptr){
    //Cose da verificare : che il messaggio no sia null che miner non sia null
    //che il payload del messaggio non sia null e che la dimensione del payload sia accettabile
    //Ritorna 0 se tutto è andato a buon fine
    if (miner == NULL || message_ptr == NULL) return INVALID_PARAMS;

    if (message_ptr->payload_size > MAX_BLOCK_TXS_BUF) return BUFFER_TOO_SMALL;


    size_t tx_len = 0;
    int count = 0;
    const char *cursor = message_ptr->payload;   // puntatore che avanza nel payload
    while (*cursor != '\0' && count < MAX_TX_PER_BLOCK) {

        const char *delim  = "::";
        const size_t delim_len = 2;
        const char *end = strstr(cursor, delim); // trova il prossimo "::"

        // lunghezza della transazione corrente
        if (end != NULL) {
            tx_len = (size_t)(end - cursor);  // fino al delimitatore
        }else{
            tx_len = strlen(cursor);          // fino al '\0' (ultima tx)
        }

        if (tx_len > 0 && tx_len <= MAX_TX_SIZE) {
            strncpy(miner->transactions[count], cursor, tx_len);
            miner->transactions[count][tx_len] = '\0';  // termina esplicitamente
            count++;
        }

        if (end == NULL) break;              // era l'ultima transazione
        cursor = end + delim_len;            // salta il "::" e continua
    }
    miner->transactions_count = count;

    return 0;
}

/**
 * Trasferisce al chiamante il blocco minato dal miner, cedendone la proprietà
 * (dopo la chiamata il miner non punta più al blocco).
 * @param miner Miner da cui prelevare il blocco minato
 * @param block_ptr Puntatore di output al blocco minato
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS se i parametri sono
 *         nulli o se non esiste un blocco minato
 */
int minerGetBlock(Miner* miner,Block* block_ptr) {
    if (miner == NULL || block_ptr == NULL) return INVALID_PARAMS;
    if (miner->mined_block == NULL) return INVALID_PARAMS;
    block_ptr = miner->mined_block;
    miner->mined_block= NULL;

    return 0;
}
/**
 * Simula un singolo tentativo di mining: la probabilità di successo dipende
 * dalla difficoltà richiesta.
 * @param difficulty Difficoltà del mining
 * @return 1 se il tentativo ha avuto successo, 0 altrimenti
 */
static int minerMiningAttempt(const uint difficulty) {
    uint num = NUM_MIN_MAX(0,difficulty-1);
    return num == 0 ? 1 : 0;
}


/**
 * Crea un nuovo blocco vuoto e lo collega al miner come blocco minato corrente
 * (il blocco non viene ancora riempito con i dati).
 * @param miner Miner a cui associare il nuovo blocco
 * @param new Puntatore di output al blocco appena creato
 */
static void minerCreateBlock(Miner* miner, Block** new) {
    //blocco il la scrittura sul miner
    *new = blockCreate();

    //Collego il blocco ma non lo riempio non ho le informazioni
    miner->mined_block = *new;
    //libero la scrittura
}


/**
 * Ciclo principale di mining eseguito dal thread dedicato. Attende il segnale di
 * lavoro, tenta ripetutamente di minare un blocco e aggiorna lo stato condiviso;
 * termina solo quando lo stato passa a MINER_STOPPED.
 * @param miner Miner che esegue il mining
 * @param status Stato condiviso usato per sincronizzazione e segnalazioni
 * @return 0 all'uscita dal ciclo, INVALID_PARAMS se i parametri sono nulli
 */
int minerMiningLoop(Miner* miner, MinerStatus* status) {
    if (miner == NULL || status == NULL) return INVALID_PARAMS;
    //ciclio infinito una volta iniziato non si interrompe finchè state = MINER_STOPPED
    while (1) {
        /*Con questa chiama bloccante controlliamo quando entra all'interno del ciclo di mining*/
        //rimane bloccata finchè state = MINER_IDLE
        msWaitForWork(status);

        MinerState s;
        mSGetState(status, &s);
        if (s == MINER_STOPPED) break;

        mSSetState(status, MINER_MINING);

        int trovato = 0;
        int sleeping_time = NUM_MIN_MAX(MIN_SLEEPING_TIME, MAX_SLEEPING_TIME);
        Block * new = NULL;


        while (!trovato) {
            sleep(sleeping_time);

            // check abort — il comm thread ha chiamato msSignal(RESTART)?
            mSGetState(status, &s);
            if (s == MINER_RESTART || s == MINER_STOPPED) break;

            if (minerMiningAttempt(miner->difficulty)) {
                minerCreateBlock(miner, &new);
                mSSetState(status, MINER_BLOCK_FOUND);
                trovato = 1;
            } else {
                size_t attempts;
                mSGetAttempts(status, &attempts);
                mSSetAttempts(status, attempts + 1);
            }
        }

        if (s == MINER_STOPPED) break;

    }

    return 0;
}



