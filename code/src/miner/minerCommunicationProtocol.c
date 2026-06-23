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
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/select.h>
#include "communicationProtocol.h"

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
    if (m == NULL) return MEMORY_ERROR;

    ChildProcess* cp = childProcessCreate();
    if (cp == NULL) { free(m); return MEMORY_ERROR; }

    mSGetCPChildProcess(status,cp);

    messageInit(m);
    messageSetSender(m, cp);
    messageSetType(m, MSG_BLOCK_MINED);

    size_t payload_size = strlen(payload);

    messageSetPayload(m, payload, (uint32_t)payload_size);

    int sm = sendMessage(fd, m);
    if (sm != 0) {childProcessDestroy(cp);free(m);return FIFO_ERROR;}


    childProcessDestroy(cp);
    free(m);

    return 0;
}


/**
 * Prende Hash Code id miner_id di un blocco e li utilizza per aggiornare la pendingpool
 * @param miner miner incaricato di rispondere
 * @param fd File descriptor da cui leggere
 * @return 0 se tutto è andato a buon fine
 * @note Implementazione ancora da completare.
 */
int receiveBlockFromNode(Miner* miner, MinerStatus* status, int fd) {
    if ( miner == NULL || status == NULL || fd < 0 ) return INVALID_PARAMS;

    BlockResponse resp;
    ssize_t red = read(fd, &resp, sizeof(BlockResponse));

    if (red == 0 ){return FIFO_CLOSED;}
    if (red < 0 ) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return FIFO_EMPTY;
    }
    if (red != (ssize_t)sizeof(BlockResponse)) return FIFO_ERROR;

    /* result e' un enum: BLOCK_VALID == 0, quindi normalizzo a un booleano */
    int valid = (resp.result == BLOCK_VALID);

    /* block_hash e' un buffer fisso dentro la struct: lo passo direttamente,
     * niente malloc. Aggiorna pending pool + testa della catena. */
    return minerCleanBlocksPool(miner, status, resp.block_hash, valid,
                                resp.miner_id, resp.block_index);
}
/**
 * Riceve un messaggio MSG_NEW_TX dal client, ne estrae e valida la transazione e
 * la inserisce nella transaction pool.
 * @param fd File descriptor da cui ricevere il messaggio
 * @param miner miner struttura dati che si occupera di prendere transazioni
 * @return 0 se tutto è andato a buon fine, SOCKET_ERROR per errori di socket,
 *         INVALID_PARAMS se il messaggio non è valido o non è di tipo MSG_NEW_TX,
 *         INVALID_TRANSACTION se la transazione non supera la validazione
 */
int receiveTransactionFromClient(int fd,Miner* miner){
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
    if (tr == NULL) { free(m); return MEMORY_ERROR; }

    result = messageGetPayload(m, tr, (size_t)(payload_size + 1));
    if ( result != 0 ) { free(tr); free(m);return SOCKET_ERROR;}

    if (validateTransaction(tr)!= 0) { free(tr); free(m);return INVALID_TRANSACTION;}

    result = minerPushTransaction(miner,tr);
    free(tr);
    free(m);
    return result;
}
int pollClientTransaction(int listen_fd, Miner* miner, int timeout_ms) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(listen_fd, &rfds);
    struct timeval tv = { .tv_sec = timeout_ms / 1000,
                          .tv_usec = (timeout_ms % 1000)*1000};

    int s = select(listen_fd + 1, &rfds, NULL, NULL, &tv);
    if (s <= 0 || !FD_ISSET(listen_fd, &rfds)) return 0;   // timeout o EINTR

    int conn_fd = accept(listen_fd, NULL, NULL);
    if (conn_fd < 0) return SOCKET_ERROR;

    int rtx = receiveTransactionFromClient(conn_fd, miner);
    close(conn_fd);
    return (rtx == 0) ? 1 : rtx;
}
