//
// Created by andrea on 19/06/26.
//

#ifndef GONZATO_LUONGO_GRIGGIO_MINERCOMMUNICATIONPROTOCOL_H
#define GONZATO_LUONGO_GRIGGIO_MINERCOMMUNICATIONPROTOCOL_H

//
// Created by andrea on 19/06/26.
//


#include "block.h"

int sendBlockToNode(Block* block_ptr, int fd);
int receiveBlockFromNode(Block* block_ptr, int fd);
int receiveTransactionFromClient(int fd,char* transaction[MAX_TX_SIZE+1]);

#endif //GONZATO_LUONGO_GRIGGIO_MINERCOMMUNICATIONPROTOCOL_H
