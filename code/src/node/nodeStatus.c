#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "nodeStatus.h"
#include "childProcess.h"
#include "block.h"
#include "error.h"

struct NodeStatus {
    ChildProcess    *cp;            
    NodeState        state;         
    const Block     *last_block;   
    uint64_t         chain_length; 

    pthread_mutex_t  mutex;         
    pthread_cond_t   cond;          
};


NodeStatus *nodeCreateStatus(void) {
    NodeStatus *s = malloc(sizeof(NodeStatus));
    if (s == NULL) return NULL;

    s->cp = childProcessCreate();
    if (s->cp == NULL) {
        free(s);
        return NULL;
    }

    if (pthread_mutex_init(&s->mutex, NULL) != 0) {
        childProcessDestroy(s->cp);
        free(s);
        return NULL;
    }

    if (pthread_cond_init(&s->cond, NULL) != 0) {
        pthread_mutex_destroy(&s->mutex);
        childProcessDestroy(s->cp);
        free(s);
        return NULL;
    }

    s->state        = NODE_IDLE;
    s->last_block   = NULL;
    s->chain_length = 0;

    return s;
}

void nodeDestroyStatus(NodeStatus *s) {
    if (s == NULL) return;
    pthread_cond_destroy(&s->cond);
    pthread_mutex_destroy(&s->mutex);

    childProcessDestroy(s->cp);
    free(s);
}

int nodeInitStatus(NodeStatus        *s,
                   const ChildProcess *cp,
                   NodeState           state,
                   uint64_t            chain_length) {
    if (s == NULL || cp == NULL) return INVALID_PARAMS;
    if (s->cp == NULL)           return INVALID_PARAMS;

    if (copyCp(cp, s->cp) != 0) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    s->state        = state;
    s->chain_length = chain_length;
    s->last_block   = NULL;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}



int nSGetCPChildProcess(NodeStatus *s, ChildProcess *out) {
    if (s == NULL || out == NULL) return INVALID_PARAMS;
    if (s->cp == NULL)            return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    int ret = copyCp(s->cp, out);
    pthread_mutex_unlock(&s->mutex);

    return ret;
}

int nSSetState(NodeStatus *s, NodeState state) {
    if (s == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    s->state = state;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

int nSSetChainLength(NodeStatus *s, uint64_t length) {
    if (s == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    s->chain_length = length;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

int nSSetLastBlock(NodeStatus *s, const Block *block) {
    if (s == NULL || block == NULL) return INVALID_PARAMS;

    pthread_mutex_lock(&s->mutex);
    s->last_block = block;
    pthread_mutex_unlock(&s->mutex);

    return 0;
}

