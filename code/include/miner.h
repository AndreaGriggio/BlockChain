//
// Created by andrea on 21/05/26.
//

#ifndef PROGETTO_MINER_H
#define PROGETTO_MINER_H

#include "message.h"
typedef struct Miner Miner;

/*
Alloca spazio per un miner e inizializza i campi
*/
Miner* minerCreate();
/*
Dealloca spazio per un miner allocato con malloc
*/
int minerDestroy(Miner* miner);
/*
Inizializza un miner con una diffocolta per trovare il blocco

*/
int minerInit(Miner* miner,int miner_difficulty);

int minerGetTransactionsFromMessage(Miner* miner, const Message *message_ptr);

/**
*Convalida le transazioni solamente se già fanno parte del miner
*NON può convalidare transazioni esterne
*funziona solo se miner->transaction == transaction
* @param miner_ptr: miner del quale si vogliono validare le transazioni
* @param number_of_transactions : Il numero delle transazioni da convalidare
 */
int minerValidateTransactions( Miner* miner_ptr, size_t number_of_transactions);

/**
 *
 * @param miner_ptr : puntatore al miner
 * @param transaction_idx
 * @return
 */
int minerValidateTransaction(const Miner* miner_ptr, const size_t *transaction_idx);
int minerRemoveTransactions(Miner* miner,const char transactions[MAX_TX_PER_BLOCK][MAX_TX_SIZE+1],size_t  number_of_transactions);

int startMining(Miner * miner);
int stopMining(Miner * miner);

#endif //PROGETTO_MINER_H
