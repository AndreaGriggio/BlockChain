#include <stdio.h>

#define HASH_HEX_SIZE = 65      //64 caratteri hex + '\0'
#define MAX_BLOCK_TXS_BUF 4096  //buffer per le transazioni separare da ::

typedef struct {
    u_int64_t index;
    u_int64_t timestamp;
    char prev_hash[HASH_HEX_SIZE];
    char merkle_root[HASH_HEX_SIZE];
    u_int64_t nonce;
    char transactions[];
} Block;

int main(void){
    return 0;
}