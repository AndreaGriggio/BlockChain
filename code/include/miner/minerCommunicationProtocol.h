//
// Created by andrea on 19/06/26.
//

#ifndef GONZATO_LUONGO_GRIGGIO_MINERCOMMUNICATIONPROTOCOL_H
#define GONZATO_LUONGO_GRIGGIO_MINERCOMMUNICATIONPROTOCOL_H

//
// Created by andrea on 19/06/26.
//


<<<<<<< HEAD:code/include/miner/minerCommunicationProtocol.h
#include "../block.h"
#include "../communication/transactionPool.h"
=======
#include "block.h"
#include "miner.h"
#include "transactionPool.h"
>>>>>>> Sviluppo-Processo-Miner:code/include/minerCommunicationProtocol.h
#include "minerStatus.h"


int sendBlockToNode(Block* block_ptr,MinerStatus * Status, int fd);
int receiveBlockFromNode(Miner* miner, int fd);
int receiveTransactionFromClient(int fd,Miner* miner);

#endif //GONZATO_LUONGO_GRIGGIO_MINERCOMMUNICATIONPROTOCOL_H
