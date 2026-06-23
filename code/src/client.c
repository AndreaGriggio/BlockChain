#include <stdio.h>
#include "client.h"
#include <ctype.h>
#include "error.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

typedef struct Client{
    int frequency;
    uint32_t trLen;
    char transaction[MAX_TR_LENGHT];
}Client;

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

    const int written = snprintf(c_ptr->transaction,MAX_TR_LENGHT,
                                "%s %s %s %lu %s",
                                       sender,
                                       "pays",
                                       receiver,
                                       coins,
                                       "coins");
    if (written < 0 || written >= MAX_TR_LENGHT) return BUFFER_TOO_SMALL;

    c_ptr->trLen= (uint32_t)written;

    return 0;
}
int clientGenerateTransactionInPlace(const uint32_t output_size, char output[MAX_TR_LENGHT+1]) {
    if ( output_size == 0 || output == NULL){return INVALID_PARAMS;}

    char sender[MAX_NAME_SIZE+1];
    char receiver[MAX_NAME_SIZE+1];

    const size_t sender_lenght = NUM_MIN_MAX(MIN_NAME_SIZE,MAX_NAME_SIZE);
    const size_t receiver_lenght = NUM_MIN_MAX(MIN_NAME_SIZE,MAX_NAME_SIZE);
    const size_t coins = NUM_MIN_MAX(MIN_COINS,MAX_COINS);

    clientGenRandomName(sender_lenght,sender);
    clientGenRandomName(receiver_lenght,receiver);

    const int written = snprintf(output,output_size,
                                "%s %s %s %zu %s",
                                       sender,
                                       "pays",
                                       receiver,
                                       coins,
                                       "coins");
    if (written < 0 || written >= (int)output_size) return BUFFER_TOO_SMALL;

    return 0;
}

int clientGenRandomName(const size_t size,char name[MAX_NAME_SIZE+1]) {
    if (name == NULL || size == 0 || size > MAX_NAME_SIZE) {
        return INVALID_PARAMS;
    }

    const char charset[] =
        STR_CAP_LETTERS
        STR_LC_LETTERS
        STR_NUMBERS;

    const size_t charset_size = sizeof(charset) - 1; // tolgo '\0'

    for (size_t i = 0; i < size; i++) {
        size_t random_index = rand() % charset_size;
        name[i] = charset[random_index];
    }

    name[size] = '\0';

    return 0;
}
int clientGetTrLen(const Client* c_ptr, uint32_t *len) {
    if (c_ptr == NULL || len == NULL){return INVALID_PARAMS;}
    *len = c_ptr->trLen;
    return 0;
}

int clientGetTransaction(const Client* c_ptr ,const uint32_t output_size, char output[MAX_TR_LENGHT+1]) {
    if (c_ptr == NULL || output_size == 0 || output == NULL){return INVALID_PARAMS;}

    if (c_ptr->trLen == 0){return INVALID_PARAMS;}

    if (c_ptr->trLen+1 > output_size ){return BUFFER_TOO_SMALL;}

    memcpy(output, c_ptr->transaction, c_ptr->trLen);

    output[c_ptr->trLen] = '\0';
    return 0;
}

