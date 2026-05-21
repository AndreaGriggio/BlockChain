//
// Created by andrea on 21/05/26.
//

#ifndef BLOCCO_H
#define BLOCCO_H
#include <stdio.h>
#include <sys/types.h>

#define HASH_HEX_SIZE 65      //64 caratteri hex + '\0'
#define MAX_BLOCK_TXS_BUF 4096  //buffer per le transazioni separare da ::

typedef struct {
    u_int64_t index;
    u_int64_t timestamp;
    char prev_hash[HASH_HEX_SIZE];
    char merkle_root[HASH_HEX_SIZE];
    u_int64_t nonce;
    char *transactions;
}Block;


#endif //BLOCCO_H
