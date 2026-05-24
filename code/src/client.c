#include <stdio.h>
#include "client.h"

#include <ctype.h>

#include "error.h"
#include "utils.h"
#include <stdlib.h>

Client* createClient(){
    return malloc(sizeof(Client));
}

int clientInit(Client* c_ptr,const int frequency) {
    if (c_ptr == NULL){return INVALID_PARAMS;}
    c_ptr -> frequency = frequency;
    return 0;
}
int clientDestroy(Client* c_ptr) {
    if (c_ptr == NULL){return INVALID_PARAMS;}
    free(c_ptr);
    return 0;
}
int clientGenerateTransaction(Client* c_ptr) {
    if (c_ptr == NULL){return INVALID_PARAMS;}

    char sender[MAX_NAME_SIZE+1];
    char receiver[MAX_NAME_SIZE+1];

    const size_t sender_lenght = NUM_MIN_MAX(MIN_NAME_SIZE,MAX_NAME_SIZE);
    const size_t receiver_lenght = NUM_MIN_MAX(MIN_NAME_SIZE,MAX_NAME_SIZE);
    const size_t coins = NUM_MIN_MAX(MIN_COINS,MAX_COINS);

    clientGenRandomName(sender_lenght,sender);
    clientGenRandomName(receiver_lenght,receiver);

    snprintf(c_ptr->transaction,MAX_TR_LENGHT,
        "%s %s %s %lu %s",
               sender,
               "pays",
               receiver,
               coins,
               "coins");

    return 0;
}

int clientGenRandomName(const size_t size,char name[MAX_NAME_SIZE+1]) {
    if (name == NULL||size == 0){return INVALID_PARAMS;}

    int chars = size;
    int i;

    const int n_capital_chars = NUM_MIN_MAX(MIN_NAME_SIZE,chars);
    for (i = 0; i < n_capital_chars; i++) {
        name[i] = STR_CAP_LETTERS[NUM_MAX(N_LETTERS)];
    }
    if (n_capital_chars == chars) return 0;

    chars = chars - n_capital_chars;

    const int n_lowercase_chars = NUM_MIN_MAX(MIN_NAME_SIZE,chars);
    for ( ; i< i+n_lowercase_chars; i++) {
        name[i] = STR_LC_LETTERS[NUM_MAX(N_LETTERS)];
    }
    if (n_lowercase_chars == chars) return 0;
    chars = chars - n_lowercase_chars;


    const int n_number_chars = NUM_MIN_MAX(MIN_NAME_SIZE,chars);
    for (; i < i+n_number_chars; i++) {
        name[i] = STR_NUMBERS[NUM_MAX(N_LETTERS)];
    }


    return 0;
}


int main(void){
    return 0;
}