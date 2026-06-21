//
// Created by andrea on 21/06/26.
//

#include "minerThread.h"

#include <pthread.h>

#include "error.h"
#include "miner.h"
#include "minerStatus.h"

/**
 * Entry point del thread di mining: estrae gli argomenti e avvia il ciclo di
 * mining vero e proprio.
 * @param arg Puntatore a MiningThreadArgs con miner e status
 * @return Sempre NULL
 */
static void* minerMining_Thread_Entry_point(void* arg) {
    MiningThreadArgs* args = (MiningThreadArgs*)arg;
    minerMiningLoop(args->miner,
                    args->status);
    return NULL;
}

int minerThreadStart(pthread_t *thread, MiningThreadArgs *args) {
    if (thread == NULL || args == NULL) return INVALID_PARAMS;

    return pthread_create(thread,
                          NULL,
                          minerMining_Thread_Entry_point,
                          args);
}

int minerThreadStop(MinerStatus *status, const pthread_t *thread) {
    if (status == NULL || thread == NULL) return INVALID_PARAMS;

    msSignal(status, MINER_STOPPED);
    pthread_join(*thread, NULL);
    return 0;
}

int minerThreadPause(MinerStatus *status) {
    if (status == NULL) return INVALID_PARAMS;

    msSignal(status, MINER_IDLE);
    return 0;
}

int minerThreadRestart(MinerStatus *status) {
    if (status == NULL) return INVALID_PARAMS;

    msSignal(status, MINER_RESTART);
    return 0;
}
int minerThreadMine(MinerStatus *status) {
    if (status == NULL) return INVALID_PARAMS;

    msSignal(status,MINER_MINING);
    return 0;
}
