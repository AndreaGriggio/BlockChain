//
// Created by andrea on 22/05/26.
//

#ifndef PROG_UTILS_H
#define PROG_UTILS_H

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include "constants.h"
#include <openssl/sha.h>

/**
 *Fa encoding dei campi del blocco
 * @param index indice del blocco
 * @param timestamp timestamp di creazione
 * @param prev_hash hash del blocco precedente
 * @param merkle_root merkle root del blocco
 * @param nonce nonce del blocco
 * @param out stringa in esadecimale del valore big endian dei campi del blocco
 */
#define BLOCK_TO_HEX_BE(index,timestamp,prev_hash,merkle_root,nonce, out,out_size) \
snprintf((out),(out_size),\
        "%016" PRIx64 "%016" PRIx64 "%s%s%016" PRIx64\
        ,(uint64_t)(index)\
        ,(uint64_t)(timestamp)\
        ,(prev_hash)\
        ,(merkle_root)\
        ,(uint64_t)(nonce))
/**
 * Utilizza l'algoritmo per trovare un Hash Code a 256 bit
 * Poi lo converte in testo utilizzando sprintf
 * @param input dati da hashare
 * @param input_size quanti byte leggere da input
 * @param output buffer dove scrivere l'hash finale come testo
 */
void sha256_of_string(const unsigned char* input, size_t input_size, char* output);

/**
 *si occupa di convertire una stringa hesadecimale in un intero a 64 bit
 * @param str stringa esadecimale
 * @param out numero convertito
 * @return 0 se tutto va bene
 */
int parse_uint64_hex(const char *str, uint64_t *out);

/**
 * Si occupa di validare una transazione si aspetta in campi :
 * Nome1 : 10 caratteri max + 1 per terminatore
 * "pays" : letteralmente la stringa pays
 * Nome2 : 10 caratteri max + 1 per terminatore
 * "coins" : letteralmente la stringa coins
 * @param transaction transazione da validare
 * @return 0 se tutto va bene
 */
int validateTransaction(const char transaction[MAX_TX_SIZE+1]);


/**
 * Restituisce il timestamp corrente (secondi dall'epoch Unix) come intero a 64 bit.
 * Usa clock_gettime(CLOCK_REALTIME) che legge l'istante in un'unica chiamata
 * coerente: il valore restituito non è mai "spezzato" e la funzione è
 * thread-safe (nessuno stato condiviso, ogni chiamante riceve la propria copia).
 * @return secondi trascorsi dall'epoch Unix
 */
uint64_t nowUnix(void);

/**
 * Intervallo di valori [min,max] \in \mathbb{N}
 * @param min Numero minimo da generare
 * @param max Numero max da generare
 */
#define NUM_MIN_MAX(min, max) ((rand() % ((max) - (min) + 1)) + (min))
#endif //PROG_UTILS_H
