//
// Created by andrea on 21/05/26.
//

#ifndef PROGETTO_MINER_H
#define PROGETTO_MINER_H

#include "message.h"
#include "minerStatus.h"
typedef struct Miner Miner;

/**
 * Alloca la struttura di miner
 * @return puntatore al malloc
 */
Miner* minerCreate();
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

/**
 *Prende le transazioni contenute all'interno di un messaggio
 * @param miner puntatore di miner all'interno del quale inserire le transazioni del messaggio
 * @param message_ptr puntatore del messaggio da cui prelevare la transazioni
 * @return 0 se tutto è andato bene
 */
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
 *Valida transazione a indice transaction_idx
 * @param miner_ptr : puntatore al miner
 * @param transaction_idx valida la transazione all'indice
 * @return 0 se tutto è andato bene
 */
int minerValidateTransaction(const Miner* miner_ptr, const size_t *transaction_idx);

/**
 *Rimuove le transazioni dal miner e le inserisce all'interno di transaction
 * @param miner puntatore al miner da cui rimuovere le transazioni
 * @param transactions buffer delle transazioni
 * @param number_of_transactions numero delle transazioni
 * @return 0 se tutto è andato bene
 */
int minerRemoveTransactions(Miner* miner,const char transactions[MAX_TX_PER_BLOCK][MAX_TX_SIZE+1],size_t  number_of_transactions);

/**
 * Loop che continua finchè non trova un nuovo blocco o viene esternamente interrotto
 * @param miner Puntatore al miner che inizierà a provare valori di nonce finchè non ne trova uno appropriato per il nuovo blocco
 * @param status stato del miner
 * @param miner_mutex mutex sulla scrittura di miner e status
 * @return 0 se tutto è andato bene
 */
int minerMiningLoop(Miner* miner,MinerStatus* status);
#endif //PROGETTO_MINER_H