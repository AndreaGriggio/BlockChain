//
// Created by andrea on 23/05/26.
//

#ifndef GONZATO_LUONGO_GRIGGIO_ERROR_H
#define GONZATO_LUONGO_GRIGGIO_ERROR_H
typedef enum error {
    //errori definiti da consegna
    CHAIN_MISMATCH = 0,
    INVALID_TRANSACTION = 1,
    BLOCK_NOT_FOUND = 2,
    INVALID_BLOCK = 3,
    //errori che possiamo aggiungere noi se ci fa comodo
    INVALID_HASH = 4,
    INVALID_PARAMS = 5
}error;
#endif //GONZATO_LUONGO_GRIGGIO_ERROR_H
