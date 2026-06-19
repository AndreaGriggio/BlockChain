//
// Created by andrea on 25/05/26.
//
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"



#include "error.h"
#include "constants.h"
void sha256_of_string(const unsigned char* input,const size_t input_size, char* output){
    unsigned char raw_hash[SHA256_DIGEST_LENGHT];

    SHA256(input,input_size,raw_hash);

    for (int i = 0; i < SHA256_DIGEST_LENGHT; i++) {
        snprintf(output+i*2,3,"%02x",raw_hash[i]);//conversione binario -> esadecimale
    }
    output[HASH_HEX_SIZE]='\0';
}
int parse_uint64_hex(const char *str, uint64_t *out){
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
int validateTransaction(const char transaction[MAX_TX_SIZE+1]) {
    regex_t re;
    int ret = regcomp(&re, "^[A-Za-z0-9]+ pays [A-Za-z0-9]+ [1-9][0-9]* coins$",REG_EXTENDED);
    if (ret != 0) return INVALID_TRANSACTION;

    ret = regexec(&re, transaction, 0, NULL, 0);
    regfree(&re);

    return (ret == 0) ? 0 : INVALID_TRANSACTION;
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