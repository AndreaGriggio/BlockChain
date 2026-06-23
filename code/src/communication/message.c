//
// Created by andrea on 24/05/26.
//

#include <arpa/inet.h>
#include "message.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

/**
 *
 * @return Alloca spazio in memoria per Message
 */
Message* messageCreate() {
    return malloc(sizeof(Message));
}

int messageInit(Message *message_ptr) {
    if (message_ptr == NULL) {
        return INVALID_PARAMS;
    }

    memset(message_ptr, 0, sizeof(Message));

    return 0;
}

int messageGetType(const Message *message_ptr, MessageType* type_ptr) {
    if (message_ptr == NULL || type_ptr == NULL) {
        return INVALID_PARAMS;
    }

    *type_ptr = message_ptr->type;

    return 0;
}

int messageGetSize(const Message *message_ptr, uint32_t *payload_size_ptr) {
    if (message_ptr == NULL || payload_size_ptr == NULL) {
        return INVALID_PARAMS;
    }

    *payload_size_ptr = message_ptr->payload_size;

    return 0;
}

int messageGetSenderPid(const Message *message_ptr, int32_t *sender_pid_ptr) {
    if (message_ptr == NULL || sender_pid_ptr == NULL) {
        return INVALID_PARAMS;
    }

    *sender_pid_ptr = message_ptr->sender_pid;

    return 0;
}

int messageGetSenderId(const Message *message_ptr, uint32_t *sender_id_ptr) {
    if (message_ptr == NULL || sender_id_ptr == NULL) {
        return INVALID_PARAMS;
    }

    *sender_id_ptr = message_ptr->sender_id;

    return 0;
}

int messageGetSenderRole(const Message *message_ptr, int32_t *sender_role_ptr) {
    if (message_ptr == NULL || sender_role_ptr == NULL) {
        return INVALID_PARAMS;
    }

    *sender_role_ptr = message_ptr->sender_role;

    return 0;
}

int messageGetPayload(
    const Message *message_ptr,
    char payload[MAX_BLOCK_TXS_BUF + 1],
    size_t payload_capacity
) {
    if (message_ptr == NULL || payload == NULL) {
        return INVALID_PARAMS;
    }

    if (payload_capacity == 0) {
        return INVALID_PARAMS;
    }

    if (payload_capacity <= message_ptr->payload_size) {
        return BUFFER_TOO_SMALL;
    }

    memcpy(payload, message_ptr->payload, message_ptr->payload_size);
    payload[message_ptr->payload_size] = '\0';

    return 0;
}

int messageSetType(Message *message_ptr, MessageType type) {
    if (message_ptr == NULL) {
        return INVALID_PARAMS;
    }

    message_ptr->type = type;

    return 0;
}

int messageSetSender(Message *message_ptr, const ChildProcess *cp_ptr) {
    if (message_ptr == NULL || cp_ptr == NULL) {
        return INVALID_PARAMS;
    }

    pid_t pid;
    int id;
    Ruolo role;

    if (getCpPid(cp_ptr, &pid) != 0) {
        return INVALID_PARAMS;
    }

    if (getCpId(cp_ptr, &id) != 0) {
        return INVALID_PARAMS;
    }

    if (getCpRole(cp_ptr, &role) != 0) {
        return INVALID_PARAMS;
    }

    if (pid < 0 || id < 0 || role == ROLE_INVALID) {
        return INVALID_PARAMS;
    }

    message_ptr->sender_pid = (int32_t)pid;
    message_ptr->sender_id = (uint32_t)id;
    message_ptr->sender_role = (int32_t)role;

    return 0;
}

int messageSetPayload(Message *message_ptr, const char *payload_ptr, uint32_t payload_size) {
    if (message_ptr == NULL) {
        return INVALID_PARAMS;
    }

    if (payload_size > MAX_BLOCK_TXS_BUF) {
        return BUFFER_TOO_SMALL;
    }

    if (payload_size > 0 && payload_ptr == NULL) {
        return INVALID_PARAMS;
    }

    if (payload_size > 0) {
        memcpy(message_ptr->payload, payload_ptr, payload_size);
    }

    message_ptr->payload[payload_size] = '\0';
    message_ptr->payload_size = payload_size;

    return 0;
}


int sendMessage(int fd, const Message* message_ptr) {
    if (message_ptr == NULL){return INVALID_PARAMS;}


    MessageHeader header;

    header.type = htonl((uint32_t)message_ptr->type);//convertito in host to network long 32bit essenzialmente byte
    header.sender_pid = htonl((uint32_t)message_ptr->sender_pid);
    header.sender_id = htonl(message_ptr->sender_id);
    header.sender_role = htonl((uint32_t)message_ptr->sender_role);
    header.payload_size = htonl(message_ptr->payload_size);

    int r = sendAll(fd, &header, sizeof(header));
    if (r != 0) return r;  // errore se sendAll non riesce a inviare l'header

    if (message_ptr->payload_size > 0) {
        r = sendAll(fd, message_ptr->payload, message_ptr->payload_size);
        if (r != 0) return r;
    }

    return 0;
}
int receiveMessage(int fd, Message* message_ptr) {
    /*
     *TODO : si potrebbe decidere in base al tipo di messaggio più formati di come interpretare i byte letti
     *Ora il formato è molto semplice è una stringa mandando blocchi occorrerà o cambiare protocollo oppure imporre un formato
     */
    if (fd < 0 || message_ptr == NULL) {
        return INVALID_PARAMS;
    }

    MessageHeader header;

    int result = recvAll(fd, &header, sizeof(MessageHeader));
    if (result != 0) {
        return result;
    }

    message_ptr->type = (MessageType)ntohl(header.type); //viene convertito in network to host long 32 bit
    message_ptr->sender_pid = (int32_t)ntohl(header.sender_pid);
    message_ptr->sender_id = ntohl(header.sender_id);
    message_ptr->sender_role = (int32_t)ntohl(header.sender_role);
    message_ptr->payload_size = ntohl(header.payload_size);

    if (message_ptr->payload_size > MAX_BLOCK_TXS_BUF) {
        return BUFFER_TOO_SMALL;
    }

    memset(message_ptr->payload, 0, MAX_BLOCK_TXS_BUF + 1);

    if (message_ptr->payload_size > 0) {
        result = recvAll(
            fd,
            message_ptr->payload,
            message_ptr->payload_size
        );

        if (result != 0) {
            return result;
        }
    }

    message_ptr->payload[message_ptr->payload_size] = '\0';

    return 0;
}