//
// Created by andrea on 22/05/26.
//

#ifndef PROG_UTILS_H
#define PROG_UTILS_H

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/sha.h>
#define UINT64_TO_CHAR_SIZE 16
#define HASH_HEX_SIZE 64

#define SHA256_DIGEST_LENGHT 32
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
static void sha256_of_string(const unsigned char* input,const size_t input_size, char* output);

static int parse_uint64_hex(const char *str, uint64_t *out);

#define NUM_MIN_MAX(min, max) ((rand() % ((max) - (min) + 1)) + (min))
#define NUM_MAX(max) (rand()%(max+1))
#endif //PROG_UTILS_H
