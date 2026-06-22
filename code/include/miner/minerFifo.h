//
// Created by andrea on 21/06/26.
//

#ifndef MINERFIFO_H
#define MINERFIFO_H

/**
 * Incapsula i canali FIFO di comunicazione bidirezionale tra il miner e i nodi.
 * Possiede gli array dei descrittori: vanno aperti con nodeChannelsOpen() e
 * rilasciati con nodeChannelsClose().
 */
typedef struct {
    int *to_node;    /* descrittori di scrittura miner->nodo (uno per nodo) */
    int *from_node;  /* descrittori di lettura  nodo->miner (uno per nodo)  */
    int  num_nodes;  /* numero di nodi gestiti                              */
    int  miner_id;   /* id del miner, usato per comporre i path delle FIFO  */
} NodeChannels;

/**
 * Crea e apre le FIFO di comunicazione bidirezionale tra il miner e tutti i nodi:
 * per ciascun nodo un canale di scrittura (miner->nodo) e uno di lettura
 * (nodo->miner). In caso di successo ch possiede gli array allocati.
 * @param ch Struttura dei canali da inizializzare e popolare
 * @param num_nodes Numero di nodi con cui stabilire le FIFO
 * @param miner_id Id del miner, usato per comporre i path delle FIFO
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS per parametri non
 *         validi, MEMORY_ERROR se l'allocazione fallisce, FIFO_ERROR in caso di
 *         errore di apertura
 */
int nodeChannelsOpen(NodeChannels *ch, int num_nodes, int miner_id);

/**
 * Chiude tutti i descrittori dei canali, rimuove dal filesystem le FIFO create
 * dal miner e libera gli array dei descrittori, riportando ch allo stato vuoto.
 * @param ch Canali da chiudere e rilasciare (no-op se NULL)
 */
void nodeChannelsClose(NodeChannels *ch);

#endif //MINERFIFO_H
