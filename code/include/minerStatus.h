//
// Created by andrea on 18/06/26.
//

#ifndef _MINERSTATUS_H
#define _MINERSTATUS_H
#include <sys/types.h>
#include <stdint.h>
#include "childProcess.h"
typedef enum MinerState {
    MINER_IDLE        = 0,
    MINER_MINING      = 1,
    MINER_BLOCK_FOUND = 2,
    MINER_STOPPED     = 3,
}MinerState;
typedef struct minerStatus {
    ChildProcess* cp;
    MinerState state;
    size_t nonce_attempts;
    uint64_t transaction_count;
}MinerStatus;

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
 *Fa una copia dell'intera struttura MinerStatus
 *@param minerStatus puntator al miner in cui verrano copiate le informazioni
 * @param minerCPStatus Puntatore al miner status dove verrano inseire informazioni
 * @return 0 se tutto va bene
 */
int mSGetCPStatus(const MinerStatus* minerStatus,MinerStatus* minerCPStatus);


/**
 *
 * @param minerStatus Miner status pointer da cui copiare il childProcess
 * @param cpcopy Puntatore alla copia del childPrcess
 * @return 0 se tutto va bene
 */
int mSGetCPChildProcess(const MinerStatus* minerStatus, ChildProcess * cpcopy);

/**
 *
 * @param minerStatus puntatore allo status del miner
 * @param minerstate copia di minerstate
 * @return 0 se tutto va bene
 */
int mSGetState(const MinerStatus* minerStatus, MinerState* minerstate);
/**
 *
 * @param minerStatus Puntatore allo status del miner
 * @param nonceAttempts copia di nonceAttempts
 * @return 0 se tutto va bene
 */
int mSGetAttempts(const MinerStatus* minerStatus,size_t* nonceAttempts);
/**
 *
 * @param minerstatus Status a cui accedere per prelevare
 * @param transaction_count transaction count su cui verra scritto il valore della struct
 * @return 0 se tuttto va bene
 */
int minerGetTransactionCount(const MinerStatus* minerstatus,uint64_t* transaction_count);

int mSSetCP(const MinerStatus* minerStatus, const ChildProcess* cp);
int mSSetState(MinerStatus* minerStatus, MinerState minerstate);
int mSSetAttempts(MinerStatus* minerStatus, size_t nonceAttempts);
int mSSetTransactionCount(MinerStatus* minerStatus, uint64_t transaction_count);

#endif //_MINERSTATUS_H
