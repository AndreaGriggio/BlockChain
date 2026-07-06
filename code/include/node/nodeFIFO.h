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

/* Apre le FIFO di comunicazione tra il broker e questo nodo:
 *   fd_to_broker  : nodo → broker (NODE_BROKER_FIFO_<node_id>)
 *   fd_from_broker: broker → nodo (BROKER_NODE_FIFO_<node_id>)
 *
 * Le FIFO sono create da main.c prima del fork, quindi qui vengono
 * solo aperte. Usa retry con usleep in caso il broker non abbia ancora
 * aperto l'altro capo.
 *
 * @param ctx contesto del nodo (usa ctx->node_id per comporre i path)
 * @return 0 se entrambe le FIFO sono state aperte correttamente,
 *         FIFO_ERROR in caso di errore,
 *         INVALID_PARAMS se ctx è NULL
 */
int openBrokerFifos(NodeContext *ctx);

/*
 * Chiude i descrittori delle FIFO broker <-> node.
 * Operazione idempotente: sicura anche se i fd sono già -1.
 *
 * @param ctx contesto del nodo
 */
void closeBrokerFifos(NodeContext *ctx);

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
