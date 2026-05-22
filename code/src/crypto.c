#include "crypto.h"
#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/**
 * @brief Calcola l'hash SHA256 di una stringa di testo e lo restituisce in formato esadecimale.
 *
 * La funzione funge da wrapper moderno per le librerie crittografiche di OpenSSL (versione 3.0+),
 * utilizzando le API EVP ad alto livello (EVP_Q_digest) per garantire la conformità con i sistemi
 * Linux recenti ed evitare l'uso di funzioni deprecate.
 * * Prende un input di lunghezza arbitraria, ne calcola l'impronta crittografica a 256 bit (32 byte)
 * e la converte in una stringa di testo esadecimale standard di esattamente 64 caratteri.
 *
 * @param[in]  input   Puntatore alla stringa di testo (null-terminated) da sottoporre ad hashing.
 * Il contenuto originale non viene modificato.
 * @param[out] output  Puntatore al buffer di destinazione allocato dal chiamante.
 * Deve avere una dimensione minima di 65 byte (64 caratteri hex + '\0').
 * In caso di errore critico di OpenSSL, viene restituita una stringa vuota ("").
 *
 * @note Richiede il collegamento delle librerie OpenSSL in fase di compilazione (flag -lcrypto).
 * * @warning La funzione include una macro assert() per verificare a tempo di debug che l'algoritmo
 * abbia prodotto esattamente 32 byte. Se la condizione fallisce, il singolo processo
 * chiamante viene terminato immediatamente tramite segnale SIGABRT.
 * @warning Se il buffer 'output' non è sufficientemente grande (almeno 65 byte), l'aritmetica
 * dei puntatori interna causerà una grave corruzione della memoria (Buffer Overflow).
 */

void sha256_string(const char *input, char *output) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    // EVP_Q_digest restituisce 1 in caso di successo, 0 in caso di errore
    if (EVP_Q_digest(NULL, "SHA256", NULL, input, strlen(input), hash, &hash_len) != 1) {
        fprintf(stderr, "Errore critico: impossibile calcolare l'hash SHA256\n");
        output[0] = '\0';
        return;
    }

    assert(hash_len == 32);

    char *ptr = output;
    for (unsigned int i = 0; i < hash_len; i++) {
        // snprintf impedisce di andare oltre i byte rimanenti (65 - spazio già usato)
        snprintf(ptr, 3, "%02x", hash[i]);
        ptr += 2;
    }

    output[64] = '\0';
}