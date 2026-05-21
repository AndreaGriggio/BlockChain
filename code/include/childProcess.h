//
// Created by andrea on 21/05/26.
//

#ifndef CHILDPROCESS_H
#define CHILDPROCESS_H
#include <signal.h>

/**
 *indica il ruolo del processo :
 */
typedef enum Ruolo {
    CLIENT = 0,
    MINER = 1,
    NODE = 2
}Ruolo;
typedef struct ChildProcess ChildProcess;//Non ha una struttura è definita in childprocess.c

/**
 *Alloca spazio per la struttura di ChildProcess
 *
 */
ChildProcess* childProcessCreate();
/**
 *Inizializza la struttura ChildProcess
 *
 *La struttura serve per tenere traccia di tutti i processi è compagna di tutti i processi generati dal processo
 *@param child_ptr zona di memoria in cui mettere la struct
 *@param pid pid del processo in cui viene memorizzata la struttura
 *@param id id del processo assegnato al punto di creazione del processo
 *@param r ruolo del processo
 *
 */
int childProcessInit(ChildProcess* child_ptr,pid_t pid,int id , Ruolo r);


/**
 *
 * @param child_ptr Puntatore al processo figlio
 */
void childProcessDestroy(ChildProcess* child_ptr);

/**
 *CHI CHIAMA il ToString deve fare free quando finisce di utilizzare la stringa per la stampa
 * @param child_ptr  Child processo ptr
 * @return puntatore al buffer della stringa
 */
const char* getCpToString(const ChildProcess* child_ptr);

/**
 *
 * @param child_ptr
 * @return intero id del processo figlio
 */
int getCpId(const ChildProcess* child_ptr);


/**
 *
 * @param child_ptr
 * @return pid del processo figlio
 */
pid_t getCpPid(const ChildProcess* child_ptr);

#endif //CHILDPROCESS_H
