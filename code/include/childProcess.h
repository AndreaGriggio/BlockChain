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
 * @param child Puntat
 */
void childProcessDestroy(ChildProcess* child_ptr);

const char* getCpToString(const ChildProcess* c);
int getCpInt(const ChildProcess* c);
pid_t getCpPid(const ChildProcess* c);



#endif //CHILDPROCESS_H
