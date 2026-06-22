#ifndef NODECSV_H
#define NODECSV_H

#include "nodeContext.h"
#include "../block.h"

/*
 * Copia il CSV condiviso nella copia locale del node.
 *
 * @param ctx puntatore al contesto del node
 * @return 0 se tutto è andato bene, CSV_ERROR in caso di errore
 */
int mirror_shared_to_local(NodeContext *ctx);

/*
 * Scrive un blocco validato sul CSV condiviso.
 * Protetta dal semaforo POSIX internamente.
 * Legge la coda del CSV, valida il blocco rispetto ad essa,
 * e se valido lo aggiunge in append.
 * Aggiorna last_block e chain_length nel contesto.
 *
 * @param ctx       puntatore al contesto del node
 * @param new_block blocco da scrivere
 * @return 0                 se il blocco è stato scritto
 *         BLOCK_ALREADY_PRESENT se il blocco era già presente
 *         CHAIN_MISMATCH    se il blocco non è valido rispetto alla coda
 *         CSV_ERROR         in caso di errore I/O
 *         SEM_ERROR         in caso di errore semaforo
 */
int commit_block_to_shared_csv(NodeContext *ctx, Block *new_block);

/*
 * Carica lo stato iniziale della blockchain dal CSV condiviso.
 * Se il file non esiste lo crea vuoto.
 * Aggiorna last_block e chain_length nel contesto.
 *
 * @param ctx      puntatore al contesto del node
 * @param csv_path path del CSV da caricare
 * @return 0 se tutto è andato bene, CSV_ERROR o INVALID_BLOCK in caso di errore
 */
int load_initial_state(NodeContext *ctx, const char *csv_path);

#endif 
