//
// Created by andrea on 21/05/26.
//
#ifndef _CLIENT_H
#define _CLIENT_H

#define MAX_TR_LENGHT 50
#define MAX_NAME_SIZE 10
#define MIN_NAME_SIZE 1

#define MAX_COINS 99
#define MIN_COINS 1
#define N_LETTERS 26
#define N_NUMBERS 10
#define TOTAL_SUBSETS 3

#define STR_CAP_LETTERS "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define STR_LC_LETTERS "abcdefghijklmnopqrstuwxyz"
#define STR_NUMBERS "0123456789"

#include <sys/types.h>

typedef struct {
    int frequency;
    char transaction[MAX_TR_LENGHT];
}Client;

//metodi di costruzione, inizializzazione,distruzione
Client* createClient();
int clientInit(Client* c_ptr,const int frequency);
int clientDestroy(Client* c_ptr);


//metodi di struct
int clientGenerateTransaction(Client* c_ptr);

int clientGenRandomName(const size_t size, char name[MAX_NAME_SIZE+1]);



#endif //_CLIENT_H
