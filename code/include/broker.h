#ifndef BROKER_H
#define BROKER_H

#include <sys/types.h>
#include <limits.h>
#include "block.h"

typedef enum {
    BROKER_MSG_BLOCK = 1
} BrokerMsgType;

/*
 * Messaggio inviato da un nodo al broker sulla FIFO node_i→broker.
 *
 * msg_type   : sempre BROKER_MSG_BLOCK
 * node_id    : id logico del nodo mittente (0-based)
 * csv_line   : blocco serializzato in formato CSV
 */
typedef struct {
    BrokerMsgType msg_type;
    int           node_id;
    int           miner_id;
    char          csv_line[BLOCK_CSV_LINE_SIZE];
} BrokerMessage;


typedef struct {
    int miner_id;               
    char csv_line[BLOCK_CSV_LINE_SIZE];
} BrokerResponse;

//controlli
_Static_assert(sizeof(BrokerMessage) <= PIPE_BUF,
               "BrokerMessage supera PIPE_BUF: protocollo FIFO non atomico");

_Static_assert(sizeof(BrokerResponse) <= PIPE_BUF,
               "BrokerResponse supera PIPE_BUF: protocollo FIFO non atomico");
#endif 
