//
// Created by andrea on 19/06/26.
//


#include "block.h"
#include "minerCommunicationProtocol.h"

#include "error.h"
#include "message.h"
#include "minerStatus.h"
#include "protocolSocket.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

int sendBlockToNode(Block* block_ptr,MinerStatus* status, int fd) {
    //Trasformare il blocco in un messaggio da mandare
    if (block_ptr == NULL || status == NULL )return INVALID_PARAMS;
    if (fd < 0)return FIFO_ERROR;

    char payload[BLOCK_CSV_LINE_SIZE+1];

    if (blockToCsv(block_ptr,payload,BLOCK_CSV_LINE_SIZE) != 0 )return INVALID_PARAMS;

    Message* m = messageCreate();
    ChildProcess* cp = childProcessCreate();

    mSGetCPChildProcess(status,cp);

    messageInit(m);
    messageSetSender(m, cp);
    messageSetType(m, MSG_BLOCK_MINED);

    size_t payload_size = strlen(payload);

    messageSetPayload(m, payload, (uint32_t)payload_size);

    if (sendMessage(fd, m) != 0) return FIFO_ERROR;


    childProcessDestroy(cp);
    free(m);

    return 0;
}


int receiveBlockFromNode(Block* block_ptr, int fd) {
return 0;}
int receiveTransactionFromClient(int fd,char* tr){
    if (fd < 0) return SOCKET_ERROR;

    Message* m = messageCreate();
    if (m == NULL) return INVALID_PARAMS;

    messageInit(m);

    int result = receiveMessage(fd, m);
    if (result != 0) {
        free(m);
        return result;
    }

    MessageType type;
    if (messageGetType(m, &type) != 0 || type != MSG_NEW_TX) {
        free(m);
        return INVALID_PARAMS;
    }

    uint32_t payload_size = 0;
    messageGetSize(m, &payload_size);

    if (tr == NULL) tr = malloc(sizeof(char)*(payload_size+1));

    result = messageGetPayload(m, tr, (size_t)(payload_size + 1));

    if (validateTransaction(tr)!= 0) return INVALID_TRANSACTION;

    free(m);
    return result;
}