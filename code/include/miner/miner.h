//
// Created by andrea on 21/05/26.
//

#ifndef PROGETTO_MINER_H
#define PROGETTO_MINER_H

#include "../communication/message.h"
#include "minerStatus.h"
typedef struct Miner Miner;

/**
 * Alloca la struttura di miner
 * @return puntatore al malloc
 */
Miner* minerCreate(uint difficulty,const char* previous_hash,uint64_t previous_index);
/**
* Dealloca spazio per un miner allocato con malloc
* @return 0 se tutto è andato bene
*/
int minerDestroy(Miner* miner);
/**
*Inizializza un miner con una diffocolta per trovare il blocco
*@param miner miner da inizializzare
*@param miner_difficulty difficolta con cui trovare il prossimo blocco
*@return 0 se tutto è andato bene
*/
int minerInit(Miner* miner,uint miner_difficulty);

int minerPushTransaction( Miner* miner,const char* tx);

int minerAddBlockToPending(Miner* miner,Block* block);

int minerPopMinedBlock(Miner*miner,Block** block_ptr);
/**
 * Loop che continua finchè non trova un nuovo blocco o viene esternamente interrotto
 * @param miner Puntatore al miner che inizierà a provare valori di nonce finchè non ne trova uno appropriato per il nuovo blocco
 * @param status stato del miner
 * @return 0 se tutto è andato bene
 */
int minerMiningLoop(Miner* miner,MinerStatus* status);
int minerUpdatePrevious(Miner* miner, const char* new_hash, uint64_t new_index);
#endif //PROGETTO_MINER_H