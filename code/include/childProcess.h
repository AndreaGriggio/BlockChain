//
// Created by andrea on 21/05/26.
//

#ifndef CHILDPROCESS_H
#define CHILDPROCESS_H
#include <signal.h>

typedef enum Ruolo {
    CLIENT = 0,
    MINER = 1,
    NODE = 2
}Ruolo;
typedef struct ChildProcess ChildProcess;

ChildProcess * childProcessCreate(pid_t pid,int id , Ruolo r);//costruttore

void childProcessDestroy(ChildProcess* child);//distruttore

const char* getCpToString(const ChildProcess* c);
int getCpInt(const ChildProcess* c);
pid_t getCpPid(const ChildProcess* c);



#endif //CHILDPROCESS_H
