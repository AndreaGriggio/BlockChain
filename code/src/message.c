//
// Created by andrea on 24/05/26.
//
#include "message.h"
#include "error.h"

int messageGetType(const Message* message_ptr,MessageType* type_ptr) {
    if ( message_ptr == NULL || type_ptr==NULL ) return INVALID_PARAMS;
    *type_ptr = message_ptr->type;
    return 0;
}
int messageGetSize(const Message* message_ptr,size_t* size_ptr) {
    if ( message_ptr == NULL || size_ptr==NULL ) return INVALID_PARAMS;
    *size_ptr = message_ptr->size;
    return 0;
}
int messageGetCP(const Message* message_ptr,ChildProcess* cp_ptr) {
    if (message_ptr == NULL || cp_ptr==NULL ) return INVALID_PARAMS;
    return copyCp(message_ptr->childProcess, cp_ptr);
}
int messageGetPayload(const Message* message_ptr,const size_t payload_size,char payload[MAX_BLOCK_TXS_BUF+1]) {

    if ( message_ptr == NULL || payload==NULL || payload_size == 0 ) return INVALID_PARAMS;

    const int written = snprintf(payload,payload_size,"%s",message_ptr->payload);
    
    if ( written < 0 ||written >= payload_size ) return BUFFER_TOO_SMALL;
    return 0;
}