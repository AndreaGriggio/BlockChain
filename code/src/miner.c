#include <regex.h>
#include <stdio.h>
#include <stdbool.h>

#include "error.h"
#include "miner.h"

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



int minerValidateTransactions(Miner* miner,
                              char transactions[MAX_TX_PER_BLOCK][MAX_TX_SIZE+1],
                              size_t number_of_transactions)
{
    if (miner == NULL || transactions == NULL || number_of_transactions > MAX_TX_PER_BLOCK) {
        return INVALID_PARAMS;
    }

    int write_idx = 0;

    for (size_t i = 0; i < number_of_transactions; i++) {
        if (minerValidateTransaction(miner, transactions[i]) == 0) {
            if (write_idx != (int)i) {                          // evita strcpy su sé stesso
                strcpy(transactions[write_idx], transactions[i]);
            }
            write_idx++;
        }
    }

    return write_idx;   // numero di transazioni valide rimaste nell'array
}
int minerValidateTransaction(Miner *miner, const char transaction[MAX_TX_SIZE+1]) {
    if (miner == NULL || transaction == NULL) return INVALID_PARAMS;
    if (strlen(transaction) > MAX_TX_SIZE)    return INVALID_PARAMS;

    char sender[MAX_NAME_SIZE+1];
    char pays_word[8];
    char receiver[MAX_NAME_SIZE+1];
    unsigned long amount;
    char coins_word[8];

    // parsa tutti e 5 i token in una sola chiamata
    int parsed = sscanf(transaction, "%32s %7s %32s %lu %7s",
                        sender, pays_word, receiver, &amount, coins_word);

    if (parsed != 5)                        return INVALID_TRANSACTION;
    if (strcmp(pays_word,  "pays")  != 0)   return INVALID_TRANSACTION;
    if (strcmp(coins_word, "coins") != 0)   return INVALID_TRANSACTION;

    // validazione nomi con regex POSIX
    if (validateName(sender)   != 0)        return INVALID_TRANSACTION;
    if (validateName(receiver) != 0)        return INVALID_TRANSACTION;

    return 0;
}

int validateName(const char *name) {
    if (name == NULL) return -1;

    regex_t re;
    // solo lettere maiuscole, minuscole e cifre — esattamente come clientGenRandomName
    int ret = regcomp(&re, "^[A-Za-z0-9]+$", REG_EXTENDED);
    if (ret != 0) return -1;

    ret = regexec(&re, name, 0, NULL, 0);
    regfree(&re);

    return (ret == 0) ? 0 : -1;  // 0 = match, REG_NOMATCH = fallimento
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