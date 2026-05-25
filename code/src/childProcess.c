//
// Created by andrea on 21/05/26.
//

#include "childProcess.h"
#include "stdlib.h"
#include <stdio.h>

#include "error.h"

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

const char* getCpToString(const ChildProcess *child_ptr) {
    if (child_ptr == NULL ) {
        return NULL;
    }
    const char *role_str;

    switch (child_ptr->r) {
        case CLIENT:role_str = "Client";break;
        case MINER:role_str = "Miner";break;
        case NODE:role_str = "Node";break;
        default:role_str = "No Role";break;
    }

    const int buffer_size = snprintf(NULL,
                         0,
                         "ChildProcess { pid: %d, id: %d, role: %s }",
                         child_ptr->pid,
                         child_ptr->id,
                         role_str);
    if (buffer_size < 0) return NULL;

    char* buffer = malloc(sizeof(char)*(buffer_size+1));
    if (buffer == NULL) return NULL;
    snprintf(buffer,
             buffer_size,
             "ChildProcess{ pird : %d, id: %d, role: %s }",
             child_ptr->pid,
             child_ptr->id,
             role_str
             );
    return buffer;
}

int getCpId(const ChildProcess* child_ptr,int* id_ptr) {
    if (child_ptr == NULL || id_ptr == NULL ) return INVALID_PARAMS;
    *id_ptr = child_ptr->pid;
    return 0;
}
int getCpPid(const ChildProcess* child_ptr,pid_t* pid_ptr) {
    if (child_ptr == NULL || pid_ptr == NULL ) return INVALID_PARAMS;
    *pid_ptr = child_ptr->pid;
    return 0;
}

int copyCp(const ChildProcess* c1_ptr, ChildProcess* c2_ptr) {
    if (c1_ptr == NULL || c2_ptr == NULL ) return INVALID_PARAMS;
    c2_ptr->r = c1_ptr->r;
    c2_ptr->pid = c1_ptr->pid;
    c2_ptr->id = c1_ptr->id;
    return 0;
}
