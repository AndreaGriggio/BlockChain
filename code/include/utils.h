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
static void sha256_of_string(const unsigned char* input,const size_t input_size, char* output) {
    unsigned char raw_hash[SHA256_DIGEST_LENGHT];

    SHA256(input,input_size,raw_hash);

    for (int i = 0; i < SHA256_DIGEST_LENGHT; i++) {
        sprintf(output+i*2,"%02x",raw_hash[i]);//conversione binario -> esadecimale
    }
    output[HASH_HEX_SIZE]='\0';
}
static int parse_uint64_hex(const char *str, uint64_t *out) {
    if (str == NULL || out == NULL) {
        return INVALID_PARAMS;
    }

    errno = 0;

    char *endptr = NULL;
    const unsigned long long value = strtoull(str, &endptr, 16);

    if (errno == ERANGE) {
        return INVALID_PARAMS;
    }

    if (endptr == str || *endptr != '\0') {
        return INVALID_PARAMS;
    }

    *out = (uint64_t)value;
    return 0;
}
#endif //PROG_UTILS_H
