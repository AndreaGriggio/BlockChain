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

/*
 * Valida un blocco ricevuto e se valido lo aggiunge alla chain locale
 * e al CSV condiviso.
 *
 * @param ctx       puntatore al contesto del node
 * @param new_block blocco da processare
 * @return 0                  se accettato
 *         INVALID_MERKLE     se merkle root non valido
 *         CHAIN_MISMATCH     se index o prev_hash non validi
 *         BLOCK_ALREADY_PRESENT se già presente
 *         CSV_ERROR          in caso di errore I/O
 *         SEM_ERROR          in caso di errore semaforo
 */
int process_block(NodeContext *ctx, Block *new_block);

#endif 
