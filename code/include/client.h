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
#define STR_LC_LETTERS "abcdefghijklmnopqrstuvwxyz"
#define STR_NUMBERS "0123456789"

#include <stdint.h>
#include <sys/types.h>

typedef struct Client Client;

//metodi di costruzione, inizializzazione,distruzione
Client* createClient();
int clientInit(Client* c_ptr,const int frequency);
int clientDestroy(Client* c_ptr);


//metodi di struct
/**
 * Genera una transazione come specificato dalla consegna
 * Formato : [Nome1] pays [Nome2] [n] coins
 * @param c_ptr Puntatore del cliente all'interno del quale generare la nuova transazione
 * @return 0 se tutto è andato a buon fine
 */
int clientGenerateTransaction(Client* c_ptr);
int clientGenerateTransactionInPlace(const uint32_t output_size, char output[MAX_TR_LENGHT+1]);

/**
 * Genera un nome come specificato dalla consegna
 * 1. Nomi casuali
 * 2. Lento
 * 3. Fa quello che è richiesto
 *
 * - Formato : [A-Z a-z 0-9]
 * @param size Dimensione del nome
 * @param name array di chars dove verrà inserito il nome
 * @return 0 se tutto è andato a buon fine
 */
int clientGenRandomName(const size_t size, char name[MAX_NAME_SIZE+1]);
int clientGetTrLen(const Client* c_ptr, uint32_t *len);
int clientGetTransaction(const Client* c_ptr ,const uint32_t output_size, char output[MAX_TR_LENGHT+1]);

#endif //_CLIENT_H
