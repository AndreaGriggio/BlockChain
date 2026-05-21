/* Definizione dei protocolli di comunicazione fra processi tramite l'uso dei socket
*/

#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PREFIX "./tmp/blockchain_node_"

typedef enum {
    MSG_NEW_TX,         //dal client al miner
    MSG_BLOCK_MINED,    //dal miner al node
    MSG_PROPAGATE       //da node a node
}MessageType;

