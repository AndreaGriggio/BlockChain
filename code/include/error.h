//
// Created by andrea on 23/05/26.
//

#ifndef GONZATO_LUONGO_GRIGGIO_ERROR_H
#define GONZATO_LUONGO_GRIGGIO_ERROR_H
typedef enum error {
    //teniamo lo zero utile per conferamare che qualcosa è avvenuto con successo
    //errori definiti da consegna
    CHAIN_MISMATCH = 1,
    INVALID_TRANSACTION = 2,
    BLOCK_NOT_FOUND = 3,
    INVALID_BLOCK = 4,
    //errori che possiamo aggiungere noi se ci fa comodo
    INVALID_HASH = 5,
    INVALID_PARAMS = 6
}error;
#endif //GONZATO_LUONGO_GRIGGIO_ERROR_H
