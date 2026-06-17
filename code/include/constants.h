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
#define MAX_BLOCK_TXS_BUF 4098  // dimensione massima buffer transazioni
#define MAX_TX_PER_BLOCK 30    // Numero massimo di transazioni per blocco
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

#endif //PROGETTO_CONSTANTS_H
