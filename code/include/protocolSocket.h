/* Definizione dei protocolli di comunicazione fra processi tramite l'uso dei socket
*/


#ifndef  PROTOCOL_SOCKET_
#define  PROTOCOL_SOCKET_
#include <sys/socket.h>
#include <sys/un.h>

#define NODE_SOCKET "./tmp/blockchain_node_"
#define CLIENT_MANAGER_SOCKET "./tmp/client_manager.sock"
typedef enum {
    MSG_NEW_TX,         //dal client al miner
    MSG_BLOCK_MINED,    //dal miner al node
    MSG_PROPAGATE       //da node a node
}MessageType;

/**
 *
 * @param fd File descriptor della socket
 * @param buffer Buffer in cui è contenuto il messaggio da mandare
 * @param size Dimensione del buffer da mandare nella socket
 * @return 0 se tutto è andato a buon fine
 */

int sendAll(int fd, const void *buffer, size_t size);

/**
 *
 * @param fd File descriptor della socket
 * @param buffer Buffer in cui verrà contenuto il messaggio da ricevere
 * @param size Dimensione del buffer ricevitore dalla socket
 * @return 0 se tutto è andato a buon fine
 */
int recvAll(int fd, void* buffer, size_t size);
#endif
