#ifndef REPL_H
#define REPL_H

#include <signal.h>      // sig_atomic_t
#include <sys/types.h>   // pid_t

/*
 REPL del bootstrap: legge i comandi dell'utente da stdin
 (submit/request/save/pause/resume/stop) finche' *running resta 1
 - child_pgid: process group dei figli, per pause/resume/stop via killpg
 - running:    flag condiviso col gestore dei segnali; 'stop' lo azzera
*/
void repl_run(pid_t child_pgid, volatile sig_atomic_t *running);

#endif // REPL_H
