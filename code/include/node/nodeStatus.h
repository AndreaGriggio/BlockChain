#ifndef NODE_STATUS_H
#define NODE_STATUS_H
 
#include <stdint.h>
#include <pthread.h>
#include "../childProcess.h"
#include "../block.h"
 
typedef enum {
    NODE_IDLE,        //in ascolto, nessun blocco in elaborazione 
    NODE_VALIDATING,   //sta validando un blocco ricevuto
    NODE_PROPAGATING   //sta propagando un blocco valido          
} NodeState;
 
typedef struct NodeStatus NodeStatus;
 
/**
 * Alloca e inizializza la struttura NodeStatus.
 * @return puntatore alla struttura, NULL in caso di errore
 */
NodeStatus *nodeCreateStatus(void);
 
/**
 * Dealloca la struttura NodeStatus.
 * @param s puntatore alla struttura da deallocare
 */
void nodeDestroyStatus(NodeStatus *s);
 
/**
 * Inizializza i campi della struttura NodeStatus.
 * @param s             puntatore alla struttura
 * @param cp            ChildProcess del node
 * @param state         stato iniziale
 * @param chain_length  lunghezza iniziale della chain
 * @return 0 se tutto è andato bene
 */
int nodeInitStatus(NodeStatus        *s,
                   const ChildProcess *cp,
                   NodeState           state,
                   uint64_t            chain_length);
 
int nSGetState(NodeStatus *s, NodeState *out);
int nSGetChainLength(NodeStatus *s, uint64_t *out);

int nSGetCPChildProcess(NodeStatus *s, ChildProcess *out);
 
int nSSetState(NodeStatus *s, NodeState state);
int nSSetChainLength(NodeStatus *s, uint64_t length);
int nSSetLastBlock(NodeStatus *s, const Block *block);
int nSSetCP(NodeStatus *s, const ChildProcess *cp);
 
#endif // NODE_STATUS_H
