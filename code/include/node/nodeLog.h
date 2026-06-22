#ifndef NODELOG_H
#define NODELOG_H

#include "nodeContext.h"

/*
 * Scrive un messaggio di log sul file di log del node.
 * Il messaggio viene preceduto da timestamp, id node e PID.
 *
 * Formato:
 *   [2026-06-17 14:03:21.042] [NODE-0|PID-12345] messaggio
 *
 * @param ctx puntatore al contesto del node
 * @param fmt stringa di formato (come printf)
 * @param ... argomenti variabili
 */
void log_msg(NodeContext *ctx, const char *fmt, ...);

#endif 
