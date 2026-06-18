//
// Created by andrea on 18/06/26.
//

#include <stdlib.h>
#include "minerStatus.h"

#include "error.h"

MinerStatus* minerCreateStatus() {
    MinerStatus* status = malloc(sizeof(MinerStatus));
    return status;
}

int mSGetCPStatus(const MinerStatus* minerStatus,MinerStatus* minerCPStatus) {
    if (minerCPStatus == NULL || minerStatus == NULL) return INVALID_PARAMS;
    if (minerStatus->cp == NULL) return INVALID_PARAMS;

    mSGetCPChildProcess( minerStatus,minerCPStatus->cp);
    minerCPStatus->state             = minerStatus->state;
    minerCPStatus->nonce_attempts    = minerStatus->nonce_attempts;
    minerCPStatus->transaction_count = minerStatus->transaction_count;

    return 0;
}
int mSGetCPChildProcess(const MinerStatus* minerStatus, ChildProcess * cpcopy) {
    if (minerStatus == NULL || cpcopy == NULL) return INVALID_PARAMS;
    if (minerStatus->cp == NULL) return INVALID_PARAMS;
    copyCp(minerStatus->cp,cpcopy);
    return 0;
}

int mSGetState(const MinerStatus* minerStatus, MinerState* minerstate) {
    if (minerStatus == NULL || minerstate == NULL) return INVALID_PARAMS;
    *minerstate = minerStatus->state;
    return 0;
}

int mSGetAttempts(const MinerStatus* minerStatus,size_t* nonceAttempts) {
    if (minerStatus == NULL || nonceAttempts == NULL) return INVALID_PARAMS;
    *nonceAttempts = minerStatus->nonce_attempts;
    return 0;
}


int minerGetTransactionCount(const MinerStatus* minerstatus,uint64_t* transaction_count) {
    if (minerstatus == NULL || transaction_count == NULL) return INVALID_PARAMS;
    *transaction_count = minerstatus->transaction_count;
    return 0;
}

int mSSetCP(const MinerStatus* minerStatus, const ChildProcess* cp) {
    if (minerStatus == NULL || cp == NULL) return INVALID_PARAMS;
    if (minerStatus->cp == NULL) return INVALID_PARAMS;
    copyCp(cp,minerStatus->cp);
    return 0;
}
int mSSetStatus(MinerStatus* minerStatus,const MinerState minerstate) {
    if (minerStatus == NULL) return INVALID_PARAMS;
    minerStatus->state = minerstate;
    return 0;
}
int mSSetAttempts(MinerStatus* minerStatus,const size_t nonceAttempts) {
    if (minerStatus == NULL) return INVALID_PARAMS;
    minerStatus->nonce_attempts = nonceAttempts;
    return 0;
}
int mSSetTransactionCount(MinerStatus* minerStatus,const uint64_t transaction_count) {
    if (minerStatus == NULL) return INVALID_PARAMS;
    minerStatus->transaction_count = transaction_count;
    return 0;
}
