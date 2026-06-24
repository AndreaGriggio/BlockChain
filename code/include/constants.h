//
// Created by andrea on 17/06/26.
//

#ifndef PROGETTO_CONSTANTS_H
#define PROGETTO_CONSTANTS_H
//Utils constants
#define UINT64_TO_CHAR_SIZE 16
#define HASH_HEX_SIZE 64

#define SHA256_DIGEST_LENGHT 32


//Block constants
//teniamo costanti pulite +1 vanno dopo altrimenti cambiamo nome della costante
    //64 caratteri hex
#define MERKLE_ROOT_HEX_SIZE HASH_HEX_SIZE
#define MAX_BLOCK_TXS_BUF 3895  // dimensione massima buffer transazioni massimo teorico fisicamente il buffer transazioni max è 129*25 (dim transazioni) + 24*2(caratteri seperatori) 
#define MAX_TX_PER_BLOCK 25    // Numero massimo di transazioni per blocco
#define MAX_TX_SIZE 128         // Dimensione massima di una singola transazione


//Client Constants
#define MAX_TR_LENGHT 50
#define MAX_NAME_SIZE 10
#define MIN_NAME_SIZE 1

#define MAX_COINS 99
#define MIN_COINS 1
#define N_LETTERS 26
#define N_NUMBERS 10
#define TOTAL_SUBSETS 3

#define STR_CAP_LETTERS "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define STR_LC_LETTERS "abcdefghijklmnopqrstuvwxyz"
#define STR_NUMBERS "0123456789"

//socket constants
#define NODE_SOCKET "./tmp/blockchain_node_"
#define CLIENT_MANAGER_SOCKET "./tmp/client_manager.sock"
#define MINERS_SOCKET "./tmp/miners.sock"
#define MAX_CONNECTION_TRIES 10

//fifo constants
#define MINER_NODE_FIFO  "./tmp/miner_node_"
#define NODE_MINER_FIFO  "./tmp/node_miner_"
#define FIFO_WAIT_MS 100


//miner constants
#define MAX_SLEEPING_TIME 5
#define MIN_SLEEPING_TIME 1

//transaction pool constants
#define POOL_INITIAL_CAPACITY 16   // slot allocati alla prima push
#define POOL_GROWTH_FACTOR 2       // fattore di crescita quando il pool è pieno

//node constants
#define CSV_SEM_NAME  "/blockchain_csv"
#define CSV_FILE_NAME "./blockchain.csv"

//argoments constants
#define MAX_NODE_COUNT 100
#endif 

