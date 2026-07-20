#ifndef NODE_VALIDATION_H
#define NODE_VALIDATION_H

#include "nodeContext.h"
#include "../block.h"

/*
 * Valida il merkle root di un blocco ricalcolandolo dalle transazioni
 * e confrontandolo con quello dichiarato nel blocco.
 *
 * @param ctx       puntatore al contesto del node
 * @param block_ptr blocco da validare
 * @return 0 se valido, INVALID_MERKLE se non coincide, -1 in caso di errore
 */
int validate_merkle(NodeContext *ctx, const Block *block_ptr);


#endif 
