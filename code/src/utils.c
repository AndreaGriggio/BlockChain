//
// Created by andrea on 25/05/26.
//
#include "utils.h"
#include "error.h"
void sha256_of_string(const unsigned char* input,const size_t input_size, char* output){
    unsigned char raw_hash[SHA256_DIGEST_LENGHT];

    SHA256(input,input_size,raw_hash);

    for (int i = 0; i < SHA256_DIGEST_LENGHT; i++) {
        sprintf(output+i*2,"%02x",raw_hash[i]);//conversione binario -> esadecimale
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