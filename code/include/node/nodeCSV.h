#ifndef NODECSV_H
#define NODECSV_H

#include "nodeContext.h"
#include "../block.h"

/*
 * Aggiunge al CSV locale del nodo (node_<id>_blockchain.csv) il blocco
 * ricevuto dal broker via broadcast e aggiorna last_block e chain_length.
 *
 * Chiamata SOLO dal main loop di node.c quando pending_broker == 1,
 * cioè dopo che il broker ha già distribuito il blocco a tutti i nodi.
 * Non usa semafori: ogni nodo scrive esclusivamente sul proprio file.
 *
 * Il blocco viene prima verificato rispetto a last_block:
 *   - se è già presente (stesso hash):      ritorna BLOCK_ALREADY_PRESENT
 *   - se non si aggancia (index/hash errati): ritorna CHAIN_MISMATCH
 *   - se è valido: lo appende al CSV e aggiorna lo stato in memoria
 *
 * @param ctx       contesto del nodo
 * @param new_block blocco da appendere (la ownership rimane al chiamante)
 * @return 0                   blocco scritto correttamente
 *         BLOCK_ALREADY_PRESENT  blocco già presente come testa corrente
 *         CHAIN_MISMATCH      blocco non collegabile alla catena locale
 *         INVALID_PARAMS      ctx o new_block NULL
 *         MEMORY_ERROR        blockCreate per la copia fallita
 *         CSV_ERROR           errore I/O sul file
 */
int commit_block_to_local_csv(NodeContext *ctx, Block *new_block);

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
