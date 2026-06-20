//
// Created by andrea on 23/05/26.
//

#ifndef ERROR_H
#define ERROR_H
typedef enum error {
    //teniamo lo zero utile per conferamare che qualcosa è avvenuto con successo
    //errori definiti da consegna
    CHAIN_MISMATCH = 1,
    INVALID_TRANSACTION = 2,
    BLOCK_NOT_FOUND = 3,
    INVALID_BLOCK = 4,
    //errori che possiamo aggiungere noi se ci fa comodo
    INVALID_HASH = 5,
    INVALID_PARAMS = 6,
    BUFFER_TOO_SMALL = 7,
    SOCKET_ERROR = 8,
    SOCKET_CLOSED = 9,
    //errori aggiunti in node
    INVALID_MERKLE = 10,      // merkle root non coincide con quello ricalcolato
    BLOCK_TOO_FAR = 11,       // blocco con index troppo avanti, mancano intermedi
    CSV_ERROR = 12,           // errore lettura/scrittura file CSV
    SEM_ERROR = 13,           // errore apertura/uso semaforo
    GENESIS_ERROR = 14,        // errore sul blocco genesis
    FIFO_ERROR = 15,
    FIFO_CLOSED = 16,
    MEMORY_ERROR = 17          // malloc/realloc fallita
}error;
#endif //ERROR_H
