#ifndef NODE_FIFO_H
#define NODE_FIFO_H

#include "nodeContext.h"
#include "../communication/communicationProtocol.h"

/*
 * Crea e apre le FIFO di comunicazione bidirezionale tra il node e tutti i miner.
 *
 * @param ctx       puntatore al contesto del node
 * @param num_miners numero di miner con cui stabilire le FIFO
 * @return 0 se tutto è andato bene, -1 in caso di errore
 */
int createNodeFifos(NodeContext *ctx, int num_miners);

/*
 * Chiude tutti i descrittori delle FIFO e libera gli array
 * to_miner e from_miner nel contesto.
 *
 * @param ctx       puntatore al contesto del node
 * @param num_miners numero di miner gestiti
 */
void destroyNodeFifos(NodeContext *ctx, int num_miners);

/*
 * Manda una BlockResponse a un singolo miner.
 *
 * @param ctx        puntatore al contesto del node
 * @param miner_idx  indice del miner destinatario
 * @param block_index indice del blocco a cui si riferisce la risposta
 * @param result     BLOCK_VALID o BLOCK_INVALID
 * @return 0 se tutto è andato bene, -1 in caso di errore
 */
int notify_miner(NodeContext *ctx, int miner_idx,
                 uint64_t block_index, const char *block_hash, BlockValidationResult result);

/*
 * Manda una BlockResponse a tutti i miner.
 *
 * @param ctx        puntatore al contesto del node
 * @param block_index indice del blocco a cui si riferisce la risposta
 * @param result     BLOCK_VALID o BLOCK_INVALID
 */
void notify_all_miners(NodeContext *ctx,
                       uint64_t block_index, const char *block_hash, BlockValidationResult result);

#endif 
