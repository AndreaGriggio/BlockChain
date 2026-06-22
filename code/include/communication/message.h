#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>
#include <stddef.h>

#include "protocolSocket.h"
#include "block.h"
#include "childProcess.h"
/*
 *Struct opaca : impedisce l'accesso ai campi da fuori
 */
typedef struct message {
    MessageType type;

    int32_t sender_pid;
    uint32_t sender_id;
    int32_t sender_role;

    uint32_t payload_size;
    char payload[MAX_BLOCK_TXS_BUF + 1];
} Message;

/**
 *Alloca memoria per la struct messaggio
 * @return puntatore al messaggio
 */
Message* messageCreate();

/**
 * Inizializza i campi del messaggio
 * @param message_ptr Puntaore al messaggio da inizializzare
 * @return 0 se tutto è andato a buon fine
 */
int messageInit(Message *message_ptr);

/**
 * Prende il tipo di messaggio e lo inserisce all'interno del puntatore del tipo
 * @param message_ptr puntatore al messaggio da cui prendere il tipo
 * @param type_ptr puntatore al type dove viene inserito il tipo letto
 * @return 0 se tutto è andato a buon fine
 */
int messageGetType(const Message *message_ptr, MessageType* type_ptr);

/**
 * Prende il payload di messaggio e lo inserisce all'interno del puntatore del payload
 * @param message_ptr puntatore al messaggio da cui prendere il payload
 * @param payload_size_ptr puntatore al buffer dove viene inserito il payload  del messaggio
 * @return 0 se tutto è andato a buon fine
 */
int messageGetSize(const Message *message_ptr, uint32_t *payload_size_ptr);
/**
 * Prende il pid del sender di messaggio e lo inserisce all'interno del puntatore di pid
 * @param message_ptr puntatore al messaggio da cui prendere il pid
 * @param sender_pid_ptr puntatore al pid dove viene inserito il pid letto
 * @return 0 se tutto è andato a buon fine
 */
int messageGetSenderPid(const Message *message_ptr, int32_t *sender_pid_ptr);
/**
 * Prende l'id del sender di messaggio e lo inserisce all'interno del puntatore id
 * @param message_ptr puntatore al messaggio da cui prendere l' id
 * @param sender_id_ptr puntatore all' id dove viene inserito l' id letto
 * @return 0 se tutto è andato a buon fine
 */
int messageGetSenderId(const Message *message_ptr, uint32_t *sender_id_ptr);

/**
 * Prende il role del sender di messaggio e lo inserisce all'interno del puntatore role
 * @param message_ptr Puntatore al messaggio da cui prendere il role
 * @param sender_role_ptr puntatore al role dove viene inserito il role del messaggio
 * @return 0 se tutto è andato a buon fine
 */
int messageGetSenderRole(const Message *message_ptr, int32_t *sender_role_ptr);

/**
 * Prende il payload di messaggio e lo inserisce all'interno del puntatore del payload
 * @param message_ptr puntatore al messaggio da cui prendere il payload
 * @param payload puntatore al buffer dove viene inserito il payload  del messaggio
 * @param payload_capacity dimensione del buffer payload
 * @return 0 se tutto è andato a buon fine
 */
int messageGetPayload(
    const Message *message_ptr,
    char payload[MAX_BLOCK_TXS_BUF + 1],
    size_t payload_capacity
);

int messageSetType(Message *message_ptr, MessageType type);
int messageSetSize(Message *message_ptr, uint32_t payload_size);
int messageSetSender(Message *message_ptr, const ChildProcess *cp_ptr);
int messageSetPayload(Message *message_ptr, const char *payload_ptr, uint32_t payload_size);
typedef struct MessageHeader {
    uint32_t type;

    int32_t sender_pid;
    uint32_t sender_id;
    uint32_t sender_role;

    uint32_t payload_size;

}MessageHeader;

/**
 * Converte messaggio in byte e li manda sulla socket
 * @param fd File descriptor della socket su cui mandare il messaggio
 * @param message_ptr Messaggio da mandare
 * @return 0 se tutto è andato a buon fine
 */
int sendMessage(int fd, const Message* message_ptr);

/**
 * Riconverte il messaggio e legge dalla socket
 * @param fd File descriptor della socket da cui leggere
 * @param message_ptr Messaggio su cui ricevere
 * @return 0 se tutto è andato a buon fine
 */
int receiveMessage(int fd,Message* message_ptr);
#endif
