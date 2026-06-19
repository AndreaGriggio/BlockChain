//
// Created by andrea on 18/06/26.
//

#ifndef _MINERSTATUS_H
#define _MINERSTATUS_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "childProcess.h"
typedef enum MinerState {
    MINER_IDLE        = 0,//Non fa mining funziona solo la comunicazione
    MINER_MINING      = 1,//Fa mining
    MINER_BLOCK_FOUND = 2,//Quando trova un blocco
    MINER_STOPPED     = 3,//Il thread viene terminato
    MINER_RESTART     = 4,//fa ripartire il ciclo di mining
}MinerState;

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
 *Fa una copia dell'intera struttura MinerStatus
 *@param minerStatus puntator al miner in cui verrano copiate le informazioni
 * @param minerCPStatus Puntatore al miner status dove verrano inseire informazioni
 * @return 0 se tutto va bene
 */
int mSGetCPStatus(const MinerStatus* minerStatus,MinerStatus* minerCPStatus);


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
int mSGetState(MinerStatus *status, MinerState *out);/**
 *
 * @param status Puntatore allo status del miner
 * @param out copia di nonceAttempts
 * @return 0 se tutto va bene
 */
int mSGetAttempts(MinerStatus *status, size_t *out);


int mSGetTransactionCount(MinerStatus *status, uint64_t *out);


/**
 *
 * @param status Status a cui accedere per prelevare
 * @param out transaction count su cui verra scritto il valore della struct
 * @return 0 se tuttto va bene
 */
int minerGetTransactionCount(MinerStatus *status, uint64_t *out);
/**
 * Serve per settare il ChildProcess all'interno della struttura MinerStatus
 * @param minerStatus Puntatore Miner status all'interno del quale settare il nuovo cp
 * @param cp cp da settare
 * @return 0 se tutto va bene
 */
int mSSetCP(const MinerStatus* minerStatus, const ChildProcess* cp);

/**
 * Serve per settare lo stato del miner
 * @param status Puntatore dove settare il nuovo stato miner
 * @param state Nuovo stato del miner
 * @return 0 se tutto è andato bene
 */
int mSSetState(MinerStatus *status, MinerState state);

/**
 * Serve per settare il numero di attempts che il miner ha compiuto
 * @param status Puntatore dove settare i nuovi attempts
 * @param attempts Attempts da settare
 * @return 0 se tutto va bene
 */
int mSSetAttempts(MinerStatus *status, size_t attempts);
/**
 * Serve per settare il numero di transazioni contenute all'interno del miner
 * @param status Puntatore dove settare il nuovo transaction count
 * @param count transaction count da settare
 * @return 0 se tutto va bene
 */
int mSSetTransactionCount(MinerStatus *status, uint64_t count);
#endif //_MINERSTATUS_H
