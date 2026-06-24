//
// Created by andrea on 18/06/26.
//

#include <stdlib.h>
#include <pthread.h>

#include "minerStatus.h"
#include "childProcess.h"
#include "error.h"

typedef struct MinerStatus {
    ChildProcess    *cp;               /* ownership della struct: alloca/dealloca questa     */
    MinerState       state;            /* stato corrente del miner      */
    MinerBlockState  found_block_state;
    size_t           nonce_attempts;   /* quante volte abbiamo provato un nonce              */
    uint64_t         transaction_count;/* transazioni accumulate dall'ultima pulizia         */

    pthread_mutex_t  mutex;            /* protegge tutti i campi sopra                      */
    pthread_cond_t   cond;             /* il thread di mining ci dorme quando è IDLE         */
}MinerStatus;



/**
 * Alloca e inizializza una nuova struttura di stato condiviso del miner,
 * compresi il child process associato, il mutex e la condition variable.
 * @return Puntatore allo stato creato, oppure NULL se una qualsiasi allocazione
 *         o inizializzazione fallisce
 */
MinerStatus *minerCreateStatus(void) {
    MinerStatus *s = malloc(sizeof(MinerStatus));
    if (s == NULL) return NULL;

    s->cp = childProcessCreate();
    if (s->cp == NULL) {
        free(s);
        return NULL;
    }

    if (pthread_mutex_init(&s->mutex, NULL) != 0) {
        childProcessDestroy(s->cp);
        free(s);
        return NULL;
    }

    if (pthread_cond_init(&s->cond, NULL) != 0) {
        pthread_mutex_destroy(&s->mutex);
        childProcessDestroy(s->cp);
        free(s);
        return NULL;
    }

    s->state             = MINER_IDLE;
    s->found_block_state = MINER_BLOCK_NOT_FOUND;
    s->nonce_attempts    = 0;
    s->transaction_count = 0;

    return s;
}

/**
 * Distrugge lo stato del miner liberando condition variable, mutex, child process
 * e la struttura stessa (la condvar viene distrutta prima del mutex, in ordine
 * inverso rispetto alla creazione).
 * @param s Stato da distruggere (no-op se NULL)
 */
void minerDestroyStatus(MinerStatus *s) {
    if (s == NULL) return;

    /* Distruggi sempre condvar prima del mutex (ordine inverso rispetto alla creazione). */
    pthread_cond_destroy(&s->cond);
    pthread_mutex_destroy(&s->mutex);

    childProcessDestroy(s->cp);

    free(s);
}

/**
 * Inizializza i campi dello stato del miner copiando il child process fornito e
 * impostando stato, tentativi e conteggio transazioni (operazione protetta da mutex).
 * @param s Stato da inizializzare
 * @param cp Child process da copiare nello stato
 * @param state Stato iniziale del miner
 * @param nonce_attempts Numero iniziale di tentativi sul nonce
 * @param transaction_count Numero iniziale di transazioni
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS per parametri non validi
 */
int minerInitStatus(MinerStatus       *s,
                    const ChildProcess *cp,
                    MinerState          state,
                    size_t              nonce_attempts,
                    uint64_t            transaction_count) {
    if (s == NULL || cp == NULL) return INVALID_PARAMS;
    if (s->cp == NULL) return INVALID_PARAMS;

    if (copyCp(cp, s->cp) != 0) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    s->state             = state;
    s->nonce_attempts    = nonce_attempts;
    s->transaction_count = transaction_count;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}



/**
 * Blocca il thread chiamante sulla condition variable finché lo stato resta
 * MINER_IDLE, risvegliandolo quando arriva del lavoro (chiamata bloccante).
 * @param s Stato del miner su cui attendere
 * @return 0 al risveglio, INVALID_PARAMS se s è NULL
 */
int msWaitForWork(MinerStatus *s) {
    if (s == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);


    while (s->state == MINER_IDLE) {
        pthread_cond_wait(&s->cond, &s->mutex);
    }

    pthread_mutex_unlock(&s->mutex);
    return 0;
}

/**
 * Imposta un nuovo stato e risveglia il thread eventualmente in attesa sulla
 * condition variable (usata per comunicare al thread di mining un cambio di stato).
 * @param s Stato del miner da aggiornare
 * @param new_state Nuovo stato da impostare
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS se s è NULL
 */
int msSignal(MinerStatus *s, MinerState new_state) {
    if (s == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    s->state = new_state;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);

    return 0;
}


/**
 * Legge in modo thread-safe lo stato corrente del miner.
 * @param s Stato del miner da leggere
 * @param out Buffer di output in cui scrivere lo stato
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS per parametri nulli
 */
int mSGetState(MinerStatus *s, MinerState *out) {
    if (s == NULL || out == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    *out = s->state;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

int msGetBlockState(MinerStatus* s, MinerBlockState* out) {
    if (s == NULL || out == NULL) return INVALID_PARAMS;
    pthread_mutex_lock(&s->mutex);
    *out = s->found_block_state;
    pthread_mutex_unlock(&s->mutex);
    return 0;
}

/**
 * Legge in modo thread-safe il numero di tentativi sul nonce effettuati.
 * @param s Stato del miner da leggere
 * @param out Buffer di output in cui scrivere il numero di tentativi
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS per parametri nulli
 */
int mSGetAttempts(MinerStatus *s, size_t *out) {
    if (s == NULL || out == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    *out = s->nonce_attempts;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}


/**
 * Copia in modo thread-safe il child process associato allo stato nel buffer
 * fornito.
 * @param s Stato del miner da cui leggere il child process
 * @param out Child process di output in cui copiare i dati
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS per parametri nulli o
 *         se lo stato non possiede un child process
 */
int mSGetCPChildProcess(MinerStatus *s, ChildProcess *out) {
    if (s == NULL || out == NULL) return INVALID_PARAMS;
    if (s->cp == NULL)            return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    int ret = copyCp(s->cp, out);
    pthread_mutex_unlock(&s->mutex);

    return ret;
}



/**
 * Imposta in modo thread-safe lo stato del miner (senza segnalare la condvar).
 * @param s Stato del miner da aggiornare
 * @param state Nuovo stato da impostare
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS se s è NULL
 */
int mSSetState(MinerStatus *s, MinerState state) {
    if (s == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    s->state = state;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

int mSSetBlockState(MinerStatus *s, MinerBlockState state){
    if (s == NULL) return INVALID_PARAMS;
    pthread_mutex_lock(&s->mutex);
    s->found_block_state = state;
    pthread_mutex_unlock(&s->mutex);
    return 0;
}

/**
 * Marca atomicamente il ritrovamento di un blocco e parcheggia il thread di
 * mining: imposta found_block_state = MINER_BLOCK_FOUND e state = MINER_IDLE
 * sotto un'unica acquisizione del mutex, così nessun lettore può osservare
 * BLOCK_FOUND mentre lo stato è ancora MINER_MINING.
 * @param s Stato del miner da aggiornare
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS se s è NULL
 */
int msSetBlockFoundAndIdle(MinerStatus *s) {
    if (s == NULL) return INVALID_PARAMS;
    pthread_mutex_lock(&s->mutex);
    s->found_block_state = MINER_BLOCK_FOUND;
    s->state             = MINER_IDLE;
    pthread_mutex_unlock(&s->mutex);
    return 0;
}
/**
 * Imposta in modo thread-safe il numero di tentativi sul nonce.
 * @param s Stato del miner da aggiornare
 * @param attempts Nuovo numero di tentativi
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS se s è NULL
 */
int mSSetAttempts(MinerStatus *s, size_t attempts) {
    if (s == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    s->nonce_attempts = attempts;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}


/**
 * Copia in modo thread-safe il child process fornito dentro lo stato del miner.
 * @param s Stato del miner da aggiornare
 * @param cp Child process sorgente da copiare nello stato
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS per parametri nulli o
 *         se lo stato non possiede un child process
 */
int mSSetCP( MinerStatus *s,const ChildProcess *cp) {
    if (s == NULL || cp == NULL) return INVALID_PARAMS;

    if (s->cp == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    int ret = copyCp(cp, s->cp);
    pthread_mutex_unlock(&s->mutex);

    return ret;
}