#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>
#include <stddef.h>

#include "protocolSocket.h"
#include "block.h"
#include "childProcess.h"

typedef struct message {
    MessageType type;

    int32_t sender_pid;
    uint32_t sender_id;
    int32_t sender_role;

    uint32_t payload_size;
    char payload[MAX_BLOCK_TXS_BUF + 1];
} Message;

Message* messageCreate();
int messageInit(Message *message_ptr);

int messageGetType(const Message *message_ptr, MessageType type_ptr);
int messageGetSize(const Message *message_ptr, uint32_t *payload_size_ptr);
int messageGetSenderPid(const Message *message_ptr, int32_t *sender_pid_ptr);
int messageGetSenderId(const Message *message_ptr, uint32_t *sender_id_ptr);
int messageGetSenderRole(const Message *message_ptr, int32_t *sender_role_ptr);
int messageGetPayload(
    const Message *message_ptr,
    char payload[MAX_BLOCK_TXS_BUF + 1],
    size_t payload_capacity
);

int messageSetType(Message *message_ptr, MessageType type);
int messageSetSize(Message *message_ptr, uint32_t payload_size);
int messageSetSender(Message *message_ptr, const ChildProcess *cp_ptr);
int messageSetPayload(Message *message_ptr, const char *payload_ptr, uint32_t payload_size);

#endif