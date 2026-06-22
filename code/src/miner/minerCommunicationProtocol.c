//
// Created by andrea on 19/06/26.
//


#include "block.h"
#include "minerCommunicationProtocol.h"

#include "error.h"
#include "message.h"
#include "minerStatus.h"
#include "protocolSocket.h"
#include "../utils.h"
#include <stdlib.h>
#include <string.h>

/**
 * Serializza un blocco in formato CSV, lo incapsula in un messaggio MSG_BLOCK_MINED
 * con il mittente del miner e lo invia al nodo tramite il file descriptor indicato.
 * @param block_ptr Blocco da inviare
 * @param status Stato del miner usato per ricavare il child process mittente
 * @param fd File descriptor su cui inviare il messaggio
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS per parametri non validi,
 *         FIFO_ERROR in caso di fd non valido o errore di invio
 */
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


/**
 * Riceve un blocco da un nodo tramite il file descriptor indicato.
 * @param block_ptr Buffer di output in cui salvare il blocco ricevuto
 * @param fd File descriptor da cui leggere
 * @return 0 se tutto è andato a buon fine
 * @note Implementazione ancora da completare.
 */
int receiveBlockFromNode(Block* block_ptr, int fd) {
return 0;}
/**
 * Riceve un messaggio MSG_NEW_TX dal client, ne estrae e valida la transazione e
 * la inserisce nella transaction pool.
 * @param fd File descriptor da cui ricevere il messaggio
 * @param pool Transaction pool in cui inserire la transazione ricevuta
 * @return 0 se tutto è andato a buon fine, SOCKET_ERROR per errori di socket,
 *         INVALID_PARAMS se il messaggio non è valido o non è di tipo MSG_NEW_TX,
 *         INVALID_TRANSACTION se la transazione non supera la validazione
 */
int receiveTransactionFromClient(int fd,TransactionPool* pool){
    if (fd < 0) return SOCKET_ERROR;
    char * tr = NULL;

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

    tr = malloc(sizeof(char)*(payload_size+1));

    result = messageGetPayload(m, tr, (size_t)(payload_size + 1));
    if ( result != 0 ) return SOCKET_ERROR;

    if (validateTransaction(tr)!= 0) return INVALID_TRANSACTION;

    result = poolPush(pool,tr);
    free(tr);
    free(m);
    return result;
}