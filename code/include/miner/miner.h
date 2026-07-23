//
// Created by andrea on 21/05/26.
//

#ifndef PROGETTO_MINER_H
#define PROGETTO_MINER_H

#include "../communication/message.h"
#include "minerStatus.h"
typedef struct Miner Miner;

// struct per migliorare i log
typedef struct
{
    int own_block_won;
    size_t losers_removed;
    size_t tx_requeued;
} MinerCleanupStats;

/**
 * Alloca la struttura di miner
 * @return puntatore al malloc
 */
Miner *minerCreate(uint difficulty, const char *previous_hash, uint64_t previous_index);
/**
 *Inizializza un miner con una diffocolta per trovare il blocco
 *@param miner miner da inizializzare
 *@param miner_difficulty difficolta con cui trovare il prossimo blocco
 *@return 0 se tutto è andato bene
 */
int minerInit(Miner *miner, uint miner_difficulty);

int minerPushTransaction(Miner *miner, const char *tx);

/*
 * Inserisce una copia del blocco nella pending pool.
 * Il blocco originale continua ad appartenere al chiamante.
 */
int minerAddBlockToPending(Miner *miner, Block *block);

/*
 * Reinserisce nella transaction pool tutte le
 * transazioni contenute nel blocco.
 */
int minerRequeueBlockTransactions(Miner *miner, const Block *block);

int minerPopMinedBlock(Miner *miner, Block **block_ptr);

/*
 * Scarta il blocco minato non ancora consumato recuperandone le transazioni
 * nella pool. No-op se non c'e' un blocco in sospeso.
 */
int minerDiscardMinedBlock(Miner *miner);
/**
 * Loop che continua finchè non trova un nuovo blocco o viene esternamente interrotto
 * @param miner Puntatore al miner che inizierà a provare valori di nonce finchè non ne trova uno appropriato per il nuovo blocco
 * @param status stato del miner
 * @return 0 se tutto è andato bene
 */
int minerMiningLoop(Miner *miner, MinerStatus *status);

int minerCleanBlocksPool(Miner *miner, MinerStatus *status, const char *prev_hash, int valid, int miner_id, uint64_t block_index, MinerCleanupStats *stats_out);

int minerRecoverTransactions(Miner *miner, const char *block_hash, uint64_t block_index);

int minerDestroy(Miner* miner);
#endif // PROGETTO_MINER_H
