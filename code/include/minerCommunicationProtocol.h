//
// Created by andrea on 19/06/26.
//

#ifndef GONZATO_LUONGO_GRIGGIO_MINERCOMMUNICATIONPROTOCOL_H
#define GONZATO_LUONGO_GRIGGIO_MINERCOMMUNICATIONPROTOCOL_H

//
// Created by andrea on 19/06/26.
//


#include "block.h"
#include "transactionPool.h"
#include "minerStatus.h"


int sendBlockToNode(Block* block_ptr,MinerStatus * Status, int fd);
int receiveBlockFromNode(Block* block_ptr, int fd);
int receiveTransactionFromClient(int fd,TransactionPool* pool);

#endif //GONZATO_LUONGO_GRIGGIO_MINERCOMMUNICATIONPROTOCOL_H
