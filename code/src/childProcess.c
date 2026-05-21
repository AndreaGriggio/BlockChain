//
// Created by andrea on 21/05/26.
//

#include "childProcess.h"
#include "stdlib.h"
#include <stdio.h>
typedef struct ChildProcess {
    pid_t pid;
    Ruolo r;
    int id;
}ChildProcess;

ChildProcess* childProcessCreate() {return malloc(sizeof(ChildProcess));}

int childProcessInit(ChildProcess* child_ptr,pid_t pid,int id,Ruolo r) {
    if (child_ptr == NULL) return 1;
    child_ptr->pid = pid;
    child_ptr->r = r;
    child_ptr->id = id;
    return 0;
}
void childProcessDestroy(ChildProcess *child_ptr) {
    if (child_ptr == NULL) return;
    free(child_ptr);
}

int getCpToString(const ChildProcess *child_ptr, char *buffer, size_t buffer_size) {
    if (child_ptr == NULL || buffer == NULL || buffer_size == 0) {
        return 1;
    }
    const char *role_str;

    switch (child_ptr->r) {
        case CLIENT:role_str = "Client";break;
        case MINER:role_str = "Miner";break;
        case NODE:role_str = "Node";break;
        default:role_str = "No Role";break;
    }

    snprintf(buffer,
             buffer_size,
             "ChildProcess { pid: %d, id: %d, role: %s }",
             child_ptr->pid,
             child_ptr->id,
             role_str);

    return 0;
}
