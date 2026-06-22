#ifndef NODE_LISTENER_H
#define NODE_LISTENER_H

#include "nodeContext.h"

/*
 * Entry point del thread di ascolto.
 * Usa select() su tutti i from_miner per ricevere blocchi dai miner.
 *
 * @param arg puntatore a NodeContext
 * @return sempre NULL
 */
void *listener_thread(void *arg);

#endif
