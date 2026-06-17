#include <stdio.h>
#include "miner.h"

#include <stdbool.h>

#include "error.h"


typedef struct Miner {
    
    u_int64_t last_block_index;
    char transactions[MAX_TX_PER_BLOCK][MAX_TX_SIZE+1];
    int frequency;
    Block* mined_block;
}Miner;

Miner* minerCreate(){
    Miner* miner = malloc(sizeof(Miner));
    if (miner == NULL) {
        return NULL;
    }
    miner->last_block_index = 0;
    miner->frequency = 0;
    miner->mined_block = NULL;
    return miner;
}

int minerDestroy(Miner* miner){
    if (miner == NULL) {
        return -1;
    }
    free(miner);
    return 0;
}

int minerInit(Miner* miner,int miner_difficulty){
    if (miner == NULL) {
        return -1;
    }
    miner->frequency = miner_difficulty;
    return 0;
}

int minerValidateTransactions(Miner* miner,const char transactions[MAX_TX_PER_BLOCK][MAX_TX_SIZE+1],size_t  number_of_transactions){
    if (miner == NULL || transactions == NULL || number_of_transactions > MAX_TX_PER_BLOCK) {
        return INVALID_PARAMS;
    }
    for (size_t i = 0; i < number_of_transactions; i++) {
        if (strlen(transactions[i]) > MAX_TX_SIZE) {
            return INVALID_PARAMS;
        }
    }
    
    return 0; // Ritorna

}
int minerValidateTransaction(Miner* miner,const char transaction[MAX_TX_SIZE+1]) {
    if (miner == NULL || transaction == NULL) return INVALID_PARAMS;
    if (strlen(transaction)> MAX_TX_SIZE ) return INVALID_PARAMS;
    const char * cursor = transaction;
    bool valid = true;
    const char* sender;
    const char* receiver;
    const char* pays = "pays";
    const char* coins = "coins";
    while (cursor != NULL && valid ) {


    }

    return 0;
}


int minerGetTransactionsFromMessage(Miner* miner,Message* message_ptr){
    //Cose da verificare : che il messaggio no sia null che miner non sia null
    //che il payload del messaggio non sia null e che la dimensione del payload sia accettabile
    //Ritorna 0 se tutto è andato a buon fine
        


    if (miner == NULL || message_ptr == NULL) return INVALID_PARAMS;
    if (message_ptr->payload == NULL )return INVALID_PARAMS;
    if (message_ptr->payload_size > MAX_BLOCK_TXS_BUF) return BUFFER_TOO_SMALL;

    char *cursor = message_ptr->payload;   // puntatore che avanza nel payload
    const char *delim  = "::";
    const char *end;
    const size_t delim_len = 2;
    size_t tx_len = 0;
    int count = 0;

    while (*cursor != '\0' && count < MAX_TX_PER_BLOCK) {

        *end = strstr(cursor, delim); // trova il prossimo "::"

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

    return 0;
}