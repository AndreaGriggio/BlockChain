//
// Created by andrea on 24/05/26.
//

#ifndef MESSAGE_H
#define MESSAGE_H
#include <protocolSocket.h>
#include "block.h"
#include "childProcess.h"
typedef struct message {
    MessageType type;
    size_t size;
    ChildProcess* childProcess;
    char payload[MAX_BLOCK_TXS_BUF+1];
}Message;

int messageGetType( const Message* message_ptr,MessageType* type_ptr);
int messageGetSize(const Message* message_ptr,size_t* size_ptr);
int messageGetCP(const Message* message_ptr,ChildProcess* cp_ptr);
int messageGetPayload(const Message* message_ptr, const size_t payload_size,char payload[MAX_BLOCK_TXS_BUF+1]);

int messageSetType(Message* message_ptr,const MessageType* type_ptr);
int messageSetSize(Message* message_ptr, const size_t* size_ptr);
int messageSetCP(Message* message_ptr,const ChildProcess* cp_ptr);
int messageSetPayload(Message* message_ptr,const size_t* size,const Message* payload_ptr);

#endif //MESSAGE_H
