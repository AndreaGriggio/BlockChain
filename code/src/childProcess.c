//
// Created by andrea on 21/05/26.
//

#include "childProcess.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>

struct ChildProcess {
    pid_t pid;
    Ruolo role;
    int id;
};

ChildProcess *childProcessCreate(void) {
    return malloc(sizeof(ChildProcess));
}

int childProcessInit(ChildProcess *child_ptr, pid_t pid, int id, Ruolo role) {
    if (child_ptr == NULL) {
        return INVALID_PARAMS;
    }

    if (pid < 0 || id < 0 || role == ROLE_INVALID) {
        return INVALID_PARAMS;
    }

    child_ptr->pid = pid;
    child_ptr->id = id;
    child_ptr->role = role;

    return 0;
}

void childProcessDestroy(ChildProcess *child_ptr) {
    if (child_ptr == NULL) {
        return;
    }

    free(child_ptr);
}

int getCpId(const ChildProcess *child_ptr, int *id_ptr) {
    if (child_ptr == NULL || id_ptr == NULL) {
        return INVALID_PARAMS;
    }

    *id_ptr = child_ptr->id;

    return 0;
}

int getCpPid(const ChildProcess *child_ptr, pid_t *pid_ptr) {
    if (child_ptr == NULL || pid_ptr == NULL) {
        return INVALID_PARAMS;
    }

    *pid_ptr = child_ptr->pid;

    return 0;
}

int getCpRole(const ChildProcess *child_ptr, Ruolo *role_ptr) {
    if (child_ptr == NULL || role_ptr == NULL) {
        return INVALID_PARAMS;
    }

    *role_ptr = child_ptr->role;

    return 0;
}

int copyCp(const ChildProcess *src_ptr, ChildProcess *dest_ptr) {
    if (src_ptr == NULL || dest_ptr == NULL) {
        return INVALID_PARAMS;
    }

    dest_ptr->pid = src_ptr->pid;
    dest_ptr->id = src_ptr->id;
    dest_ptr->role = src_ptr->role;

    return 0;
}