//
// Created by andrea on 21/05/26.
//

#ifndef PROGETTO_MINER_H
#define PROGETTO_MINER_H
#include <sys/types.h>
#include "block.h"
#include "message.h"
#include "childProcess.h"
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

int minerGetTransactionsFromMessage(Miner* miner,Message* message_ptr);

/*
Convalida le transazioni */
int minerValidateTransactions(Miner* miner,const char transactions[MAX_TX_PER_BLOCK][MAX_TX_SIZE+1],size_t  number_of_transactions);
int minerValidateTransaction(Miner* miner,const char transaction[MAX_TX_SIZE+1]);
int minerRemoveTransactions(Miner* miner,const char transactions[MAX_TX_PER_BLOCK][MAX_TX_SIZE+1],size_t  number_of_transactions);

int startMining(Miner * miner);
int stopMining(Miner * miner);







#endif //PROGETTO_MINER_H
