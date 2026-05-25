//
// Created by andrea on 21/05/26.
//

#ifndef CHILD_PROCESS_H
#define CHILD_PROCESS_H

#include <sys/types.h>

/*
    Rappresenta il ruolo logico di un processo figlio
    all'interno del sistema blockchain.
*/
typedef enum Ruolo {
    ROLE_INVALID = -1,
    CLIENT = 0,
    MINER = 1,
    NODE = 2
} Ruolo;

/*
    Struct opaca: i campi reali sono definiti solo in childProcess.c.
    In questo modo il resto del programma può usare ChildProcess
    solo tramite le funzioni pubbliche.
*/
typedef struct ChildProcess ChildProcess;

/*
    Alloca dinamicamente una struttura ChildProcess.

    Ritorna:
    - puntatore valido se l'allocazione va a buon fine
    - NULL in caso di errore
*/
ChildProcess *childProcessCreate(void);

/*
    Inizializza una struttura ChildProcess già allocata.

    Parametri:
    - child_ptr: struttura da inizializzare
    - pid: PID del processo figlio
    - id: identificativo logico assegnato dal programma
    - role: ruolo del processo

    Ritorna:
    - 0 se tutto va bene
    - codice di errore altrimenti
*/
int childProcessInit(ChildProcess *child_ptr, pid_t pid, int id, Ruolo role);

/*
    Libera una struttura ChildProcess allocata dinamicamente.
*/
void childProcessDestroy(ChildProcess *child_ptr);

/*
    Crea una stringa descrittiva del processo.

    Attenzione:
    chi chiama questa funzione deve fare free() della stringa ritornata.

    Ritorna:
    - stringa allocata dinamicamente se tutto va bene
    - NULL in caso di errore
*/
char *getCpToString(const ChildProcess *child_ptr);

/*
    Getter dell'id logico del processo.
*/
int getCpId(const ChildProcess *child_ptr, int *id_ptr);

/*
    Getter del pid del processo.
*/
int getCpPid(const ChildProcess *child_ptr, pid_t *pid_ptr);

/*
    Getter del ruolo del processo.
*/
int getCpRole(const ChildProcess *child_ptr, Ruolo *role_ptr);

/*
    Copia i dati da un ChildProcess sorgente a un ChildProcess destinazione.
*/
int copyCp(const ChildProcess *src_ptr, ChildProcess *dest_ptr);

#endif