//
// Created by andrea on 18/06/26.
//

#include <stdlib.h>
#include <pthread.h>

#include "minerStatus.h"
#include "childProcess.h"
#include "error.h"

struct MinerStatus {
    ChildProcess    *cp;               /* ownership della struct: alloca/dealloca questa     */
    MinerState       state;            /* stato corrente del miner                           */
    size_t           nonce_attempts;   /* quante volte abbiamo provato un nonce              */
    uint64_t         transaction_count;/* transazioni accumulate dall'ultima pulizia         */

    pthread_mutex_t  mutex;            /* protegge tutti i campi sopra                      */
    pthread_cond_t   cond;             /* il thread di mining ci dorme quando è IDLE         */
};



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
    s->nonce_attempts    = 0;
    s->transaction_count = 0;

    return s;
}

void minerDestroyStatus(MinerStatus *s) {
    if (s == NULL) return;

    /* Distruggi sempre condvar prima del mutex (ordine inverso rispetto alla creazione). */
    pthread_cond_destroy(&s->cond);
    pthread_mutex_destroy(&s->mutex);

    childProcessDestroy(s->cp);

    free(s);
}

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



int msWaitForWork(MinerStatus *s) {
    if (s == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);


    while (s->state == MINER_IDLE) {
        pthread_cond_wait(&s->cond, &s->mutex);
    }

    pthread_mutex_unlock(&s->mutex);
    return 0;
}

int msSignal(MinerStatus *s, MinerState new_state) {
    if (s == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    s->state = new_state;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);

    return 0;
}


int mSGetState(MinerStatus *s, MinerState *out) {
    if (s == NULL || out == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    *out = s->state;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

int mSGetAttempts(MinerStatus *s, size_t *out) {
    if (s == NULL || out == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    *out = s->nonce_attempts;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

int mSGetTransactionCount(MinerStatus *s, uint64_t *out) {
    if (s == NULL || out == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    *out = s->transaction_count;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

int mSGetCPChildProcess(MinerStatus *s, ChildProcess *out) {
    if (s == NULL || out == NULL) return INVALID_PARAMS;
    if (s->cp == NULL)            return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    int ret = copyCp(s->cp, out);
    pthread_mutex_unlock(&s->mutex);

    return ret;
}



int mSSetState(MinerStatus *s, MinerState state) {
    if (s == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    s->state = state;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

int mSSetAttempts(MinerStatus *s, size_t attempts) {
    if (s == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    s->nonce_attempts = attempts;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

int mSSetTransactionCount(MinerStatus *s, uint64_t count) {
    if (s == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    s->transaction_count = count;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

int mSSetCP(const MinerStatus *s,const ChildProcess *cp) {
    if (s == NULL || cp == NULL) return INVALID_PARAMS;

    if (s->cp == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    int ret = copyCp(cp, s->cp);
    pthread_mutex_unlock(&s->mutex);

    return ret;
}