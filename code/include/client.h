//
// Created by andrea on 21/05/26.
//
#ifndef _CLIENT_H
#define _CLIENT_H



#include <stdint.h>
#include <sys/types.h>
#include "constants.h"
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
