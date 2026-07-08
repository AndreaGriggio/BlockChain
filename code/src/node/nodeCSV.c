#include "nodeCSV.h"
#include "nodeLog.h"
#include "error.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

/*
 * Restituisce il path del CSV locale del nodo.
 * Funzione statica di supporto, usata solo in questo file.
 */
static void local_csv_path(int node_id, char *out, size_t out_size) {
    snprintf(out, out_size, "node_%d_blockchain.csv", node_id);
}



int commit_block_to_local_csv(NodeContext *ctx, Block *new_block) {
    if (ctx == NULL || new_block == NULL) return INVALID_PARAMS;

    char new_hash[HASH_HEX_SIZE + 1];
    blockGetHash(new_block, new_hash);

    pthread_mutex_lock(&ctx->chain_mutex);

    if (ctx->last_block != NULL) {
        char stored_hash[HASH_HEX_SIZE + 1];
        blockGetHash(ctx->last_block, stored_hash);

        if (strcmp(stored_hash, new_hash) == 0) {
            pthread_mutex_unlock(&ctx->chain_mutex);
            log_msg(ctx, "Blocco dal broker gia' presente (hash=%.16s...), scarto",
                    new_hash);
            return BLOCK_ALREADY_PRESENT;
        }

        if (blockValidate(new_block, ctx->last_block) != 0) {
            pthread_mutex_unlock(&ctx->chain_mutex);
            log_msg(ctx, "Blocco dal broker non collegabile alla catena "
                    "(hash=%.16s...), scarto", new_hash);
            return CHAIN_MISMATCH;
        }
    }

    pthread_mutex_unlock(&ctx->chain_mutex);

    char out_line[BLOCK_CSV_LINE_SIZE];
    if (blockToCsv(new_block, out_line, sizeof(out_line)) != 0) {
        log_msg(ctx, "ERROR: blockToCsv fallito (hash=%.16s...)", new_hash);
        return CSV_ERROR;
    }

    char local_path[64];
    local_csv_path(ctx->node_id, local_path, sizeof(local_path));

    FILE *f = fopen(local_path, "a");
    if (f == NULL) {
        log_msg(ctx, "ERROR: apertura %s fallita: %s",
                local_path, strerror(errno));
        return CSV_ERROR;
    }

    fseek(f, 0, SEEK_END);
    if (ftell(f) == 0) {
        fprintf(f, "index,timestamp,prev_hash,merkle_root,nonce,transactions\n");
    }

    if (fprintf(f, "%s\n", out_line) < 0) {
        log_msg(ctx, "ERROR: scrittura su %s fallita", local_path);
        fclose(f);
        return CSV_ERROR;
    }
    fflush(f);
    fclose(f);

    Block *copy = blockCreate();
    if (copy == NULL) {
        log_msg(ctx, "ERROR: blockCreate per copia last_block fallita");
        return MEMORY_ERROR;
    }

    if (blockCopy(copy, new_block) != 0) {
        log_msg(ctx, "ERROR: blockCopy fallita");
        blockDestroy(copy);
        return MEMORY_ERROR;
    }

    uint64_t new_index = 0;
    blockGetIndex(new_block, &new_index);

    pthread_mutex_lock(&ctx->chain_mutex);
    if (ctx->last_block != NULL) blockDestroy(ctx->last_block);
    ctx->last_block   = copy;
    ctx->chain_length = new_index + 1;
    pthread_mutex_unlock(&ctx->chain_mutex);

    log_msg(ctx, "Blocco index=%llu scritto su %s (hash=%.16s...)",
            (unsigned long long)new_index, local_path, new_hash);

    return 0;
}

int load_initial_state(NodeContext *ctx, const char *csv_path) {
    if (csv_path == NULL || strlen(csv_path) == 0) return 0;

    FILE *f = fopen(csv_path, "r");
    if (f == NULL) {
        if (errno == ENOENT) {
            f = fopen(csv_path, "w");
            if (f == NULL) {
                log_msg(ctx, "ERROR: creazione %s fallita: %s",
                        csv_path, strerror(errno));
                return CSV_ERROR;
            }
            fclose(f);
            log_msg(ctx, "CSV non esistente, creato nuovo file: %s", csv_path);
            return 0;
        }
        log_msg(ctx, "ERROR: impossibile aprire %s: %s",
                csv_path, strerror(errno));
        return CSV_ERROR;
    }

    char line[BLOCK_CSV_LINE_SIZE];
    Block *prev = NULL;

    long first_pos = ftell(f);
    if (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, "index,", 6) != 0) {
            fseek(f, first_pos, SEEK_SET);
        }
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        Block *b = blockCreate();
        if (b == NULL) {
            log_msg(ctx, "ERROR: blockCreate fallito");
            fclose(f);
            if (prev) blockDestroy(prev);
            return -1;
        }

        if (blockFromCsv(b, line) != 0) {
            log_msg(ctx, "ERROR: blockFromCsv fallito su: %s", line);
            blockDestroy(b);
            fclose(f);
            if (prev) blockDestroy(prev);
            return INVALID_BLOCK;
        }

        if (prev) blockDestroy(prev);
        prev = b;
        ctx->chain_length++;
    }

    fclose(f);

    pthread_mutex_lock(&ctx->chain_mutex);
    if (ctx->last_block) blockDestroy(ctx->last_block);
    ctx->last_block = prev;
    pthread_mutex_unlock(&ctx->chain_mutex);

    log_msg(ctx, "Stato iniziale caricato: %llu blocchi",
            (unsigned long long)ctx->chain_length);

    /* Sovrascrive sempre il CSV locale con quello del bootstrap,
    * garantendo che tutti i nodi partano dallo stesso genesis. */
    char local_path[64];
    local_csv_path(ctx->node_id, local_path, sizeof(local_path));

    FILE *src = fopen(csv_path, "r");
    FILE *dst = fopen(local_path, "w");
    if (src != NULL && dst != NULL) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
            fwrite(buf, 1, n, dst);
        log_msg(ctx, "CSV locale %s inizializzato da %s", local_path, csv_path);
    } else {
        log_msg(ctx, "ERROR: impossibile inizializzare CSV locale %s", local_path);
    }
    if (src) fclose(src);
    if (dst) fclose(dst);

    return 0;
}