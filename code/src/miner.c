
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "miner.h"

#include <string.h>

#include "client.h"
#include "utils.h"

typedef struct Miner {
    
    u_int64_t last_block_index;
    char transactions[MAX_TX_PER_BLOCK][MAX_TX_SIZE+1];
    size_t transactions_count;
    int difficulty;
    Block* mined_block;
}Miner;

Miner* minerCreate(){
    Miner* miner = malloc(sizeof(Miner));
    if (miner == NULL) {
        return NULL;
    }

    return miner;
}

int minerDestroy(Miner* miner) {
    if (miner == NULL)return 1;

    if (miner->mined_block != NULL) {
        free(miner->mined_block);
        free(miner);
    }

    return 0;
}

int minerInit(Miner* miner,int miner_difficulty){
    if (miner == NULL) {
        return -1;
    }
    miner->difficulty = miner_difficulty;
    miner->transactions_count = 0;
    miner->last_block_index = 0;
    miner->difficulty = 0;
    miner->mined_block = NULL;
    return 0;
}



int minerValidateTransactions(Miner* miner_ptr,
                              const size_t number_of_transactions)
{
    if (miner_ptr == NULL  || number_of_transactions > MAX_TX_PER_BLOCK) {
        return INVALID_PARAMS;
    }


    size_t write_idx = 0;

    //Riscrive solamente le transaction valide nel primo posto "libero" = write_idx
    for (size_t i = 0; i < number_of_transactions; i++) {
        if (minerValidateTransaction(miner_ptr,&i) == 0) {
            if (write_idx != i) {
                strcpy(miner_ptr->transactions[write_idx], miner_ptr->transactions[i]);
            }
            write_idx++;
        }
    }
    miner_ptr->transactions_count = write_idx+1;
    return write_idx;   // numero di transazioni valide rimaste nell'array
}
int minerValidateTransaction(const Miner *miner_ptr, const size_t *transaction_idx) {
    if (miner_ptr == NULL || transaction_idx == NULL) return INVALID_PARAMS;
    if ( miner_ptr->transactions_count < *transaction_idx) {}
    if (strlen(miner_ptr->transactions[*transaction_idx]) > MAX_TX_SIZE)    return INVALID_PARAMS;

    return validateTransaction(miner_ptr->transactions[*transaction_idx]);
}



int minerGetTransactionsFromMessage(Miner* miner, const Message *message_ptr){
    //Cose da verificare : che il messaggio no sia null che miner non sia null
    //che il payload del messaggio non sia null e che la dimensione del payload sia accettabile
    //Ritorna 0 se tutto è andato a buon fine
    if (miner == NULL || message_ptr == NULL) return INVALID_PARAMS;

    if (message_ptr->payload_size > MAX_BLOCK_TXS_BUF) return BUFFER_TOO_SMALL;


    size_t tx_len = 0;
    int count = 0;
    const char *cursor = message_ptr->payload;   // puntatore che avanza nel payload
    while (*cursor != '\0' && count < MAX_TX_PER_BLOCK) {

        const char *delim  = "::";
        const size_t delim_len = 2;
        const char *end = strstr(cursor, delim); // trova il prossimo "::"

        // lunghezza della transazione corrente
        if (end != NULL) {
            tx_len = (size_t)(end - cursor);  // fino al delimitatore
        }else{
            tx_len = strlen(cursor);          // fino al '\0' (ultima tx)
        }

        if (tx_len > 0 && tx_len <= MAX_TX_SIZE) {
            strncpy(miner->transactions[count], cursor, tx_len);
            miner->transactions[count][tx_len] = '\0';  // termina esplicitamente
            count++;
        }

        if (end == NULL) break;              // era l'ultima transazione
        cursor = end + delim_len;            // salta il "::" e continua
    }
    miner->transactions_count = count;

    return 0;
}