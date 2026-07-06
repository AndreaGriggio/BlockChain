#ifndef BROKER_H
#define BROKER_H

#include <sys/types.h>
#include "block.h"

typedef enum {
    BROKER_MSG_BLOCK = 1
} BrokerMsgType;

/*
 * Messaggio inviato da un nodo al broker sulla FIFO node_i→broker.
 *
 * msg_type   : sempre BROKER_MSG_BLOCK
 * node_id    : id logico del nodo mittente (0-based)
 * sender_pid : PID reale del nodo mittente, usato dal broker per kill()
 * csv_line   : blocco serializzato in formato CSV
 */
typedef struct {
    BrokerMsgType msg_type;
    int           node_id;
    int           miner_id;
    pid_t         sender_pid;
    char          csv_line[BLOCK_CSV_LINE_SIZE];
} BrokerMessage;


typedef struct {
    int  miner_id;               
    char csv_line[BLOCK_CSV_LINE_SIZE];
} BrokerResponse;

#endif 
