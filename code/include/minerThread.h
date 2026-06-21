//
// Created by andrea on 21/06/26.
//

#ifndef MINERTHREAD_H
#define MINERTHREAD_H

#include <pthread.h>

#include "miner.h"
#include "minerStatus.h"

/**
 * Argomenti passati al thread di mining. La struttura deve restare viva per tutta
 * la durata del thread (il thread la usa dopo l'avvio).
 */
typedef struct MiningThreadArgs {
    Miner*       miner;
    MinerStatus* status;
} MiningThreadArgs;

/**
 * Avvia il thread di mining creando un nuovo pthread sull'entry point dedicato.
 * @param thread Puntatore all'handle del thread da inizializzare
 * @param args Argomenti passati al thread (devono restare validi finché il thread vive)
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS per parametri nulli,
 *         valore non nullo se pthread_create fallisce
 */
int minerThreadStart(pthread_t *thread, MiningThreadArgs *args);

/**
 * Ferma il thread di mining segnalando MINER_STOPPED e attendendone la terminazione.
 * @param status Stato condiviso usato per segnalare lo stop
 * @param thread Handle del thread da attendere
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS per parametri nulli
 */
int minerThreadStop(MinerStatus *status, const pthread_t *thread);

/**
 * Mette in pausa il mining portando lo stato a MINER_IDLE.
 * @param status Stato condiviso da aggiornare
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS se status è NULL
 */
int minerThreadPause(MinerStatus *status);

/**
 * Richiede il riavvio del ciclo di mining portando lo stato a MINER_RESTART.
 * @param status Stato condiviso da aggiornare
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS se status è NULL
 */
int minerThreadRestart(MinerStatus *status);

int minerThreadMine(MinerStatus * status);

#endif //MINERTHREAD_H
