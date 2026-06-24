//
// Created by andrea on 18/06/26.
//

#ifndef MINERSTATUS_H
#define MINERSTATUS_H


#include <stdint.h>


#include "../childProcess.h"
typedef enum MinerState {
    MINER_IDLE        = 0,//Non fa mining funziona solo la comunicazione
    MINER_MINING      = 1,//Fa mining
    MINER_STOPPED     = 3,//Il thread viene terminato
    MINER_RESTART     = 4,//fa ripartire il ciclo di mining
}MinerState;

typedef enum MinerBlockState {
    MINER_BLOCK_FOUND     = 1,
    MINER_BLOCK_NOT_FOUND = 0,
}MinerBlockState;

typedef struct MinerStatus MinerStatus;

/**
 *
 * @return Puntatore alla memoria allocata per la struct status
 */
MinerStatus* minerCreateStatus();

/**
 *
 * @param minerStatus MinerStatus da deallocare
 */
void minerDestroyStatus(MinerStatus* minerStatus);

/**
 * Serve per scrivere dei valori in maniera corretta all'interno della struct MinerStatus
 * @param minerStatus Puntatore MinerStatus da inizializzare
 * @param cp Puntatore Tipo ChildProcess
 * @param minerstate Stato del minatore inziale
 * @param nonce_attempts Numero di attempts iniziali
 * @param transaction_count Numero di transazioni contenute all'interno del miner
 * @return
 */
int minerInitStatus(MinerStatus* minerStatus,
                    const ChildProcess* cp,
                    MinerState minerstate,
                    size_t nonce_attempts,
                    size_t transaction_count);

/**
 * Chiamata bloccante mette il thread in uno stato di waiting fino al prossimo segnale
 * @param status Status che aspetta fino a che qualcuno non da signal
 * @return 0 se tutto va bene
 */
int msWaitForWork(MinerStatus *status);

/**
 * Chimata sbloccante permette a msWaitForWork di essere sbloccata
 * @param status status che riparte
 * @param new_state nuovo stato con cui riparte
 * @return 0 se tutto va bene
 */
int msSignal(MinerStatus *status, MinerState new_state);

/**
 *
 * @param status Miner status pointer da cui copiare il childProcess
 * @param out Puntatore alla copia del childPrcess
 * @return 0 se tutto va bene
 */
int mSGetCPChildProcess(MinerStatus *status, ChildProcess *out);
/**
 *
 * @param status puntatore allo status del miner
 * @param out copia di minerstate
 * @return 0 se tutto va bene
 */
int mSGetState(MinerStatus *status, MinerState *out);
int msGetBlockState(MinerStatus* s, MinerBlockState* out);

/**
 *
 * @param status Puntatore allo status del miner
 * @param out copia di nonceAttempts
 * @return 0 se tutto va bene
 */
int mSGetAttempts(MinerStatus *status, size_t *out);



/**
 * Serve per settare il ChildProcess all'interno della struttura MinerStatus
 * @param minerStatus Puntatore Miner status all'interno del quale settare il nuovo cp
 * @param cp cp da settare
 * @return 0 se tutto va bene
 */
int mSSetCP(MinerStatus* minerStatus, const ChildProcess* cp);

/**
 * Serve per settare lo stato del miner
 * @param status Puntatore dove settare il nuovo stato miner
 * @param state Nuovo stato del miner
 * @return 0 se tutto è andato bene
 */
int mSSetState(MinerStatus *status, MinerState state);


int mSSetBlockState(MinerStatus *s, MinerBlockState state);

/**
 * Imposta atomicamente found_block_state = MINER_BLOCK_FOUND e state = MINER_IDLE
 * (singola sezione critica): usata dal thread di mining quando trova un blocco e
 * si auto-parcheggia, per editor che il comm thread veda BLOCK_FOUND con lo
 * stato ancora MINER_MINING.
 * @param s Stato del miner da aggiornare
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS se s è NULL
 */
int msSetBlockFoundAndIdle(MinerStatus *s);
/**
 * Serve per settare il numero di attempts che il miner ha compiuto
 * @param status Puntatore dove settare i nuovi attempts
 * @param attempts Attempts da settare
 * @return 0 se tutto va bene
 */
int mSSetAttempts(MinerStatus *status, size_t attempts);

#endif //MINERSTATUS_H
