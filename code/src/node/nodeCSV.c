#include "nodeCSV.h"
#include "nodeLog.h"
#include "error.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>

int mirror_shared_to_local(NodeContext *ctx) {
    char local_path[64];
    snprintf(local_path, sizeof(local_path),
             "node_%d_blockchain.csv", ctx->node_id);

    FILE *src = fopen(CSV_FILE_NAME, "r");
    if (src == NULL) {
        log_msg(ctx, "ERROR: apertura CSV condiviso per mirror fallita: %s",
                strerror(errno));
        return CSV_ERROR;
    }

    FILE *dst = fopen(local_path, "w");
    if (dst == NULL) {
        log_msg(ctx, "ERROR: apertura copia locale fallita: %s",
                strerror(errno));
        fclose(src);
        return CSV_ERROR;
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) {
            log_msg(ctx, "ERROR: scrittura copia locale fallita");
            fclose(src);
            fclose(dst);
            return CSV_ERROR;
        }
    }

    int read_err = ferror(src);
    fclose(src);
    fclose(dst);

    if (read_err) {
        log_msg(ctx, "ERROR: lettura CSV condiviso per mirror fallita");
        return CSV_ERROR;
    }

    return 0;
}

int commit_block_to_shared_csv(NodeContext *ctx, Block *new_block) {
    sem_t *sem = sem_open(CSV_SEM_NAME, 0);
    if (sem == SEM_FAILED) {
        log_msg(ctx, "ERROR: sem_open fallito: %s", strerror(errno));
        return SEM_ERROR;
    }

    if (sem_wait(sem) == -1) {
        log_msg(ctx, "ERROR: sem_wait fallito: %s", strerror(errno));
        sem_close(sem);
        return SEM_ERROR;
    }

    int rc = 0;
    FILE *f = NULL;
    Block *csv_tail = NULL;
    int found = 0;
    uint64_t tail_index = 0;
    uint64_t new_index  = 0;
    char line[BLOCK_CSV_LINE_SIZE];
    char last_line[BLOCK_CSV_LINE_SIZE];
    char out_line[BLOCK_CSV_LINE_SIZE];

    f = fopen(CSV_FILE_NAME, "r");
    if (f == NULL) {
        log_msg(ctx, "ERROR: apertura CSV condiviso in lettura fallita: %s",
                strerror(errno));
        rc = CSV_ERROR;
        goto out;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0' || strncmp(line, "index,", 6) == 0) continue;
        strncpy(last_line, line, sizeof(last_line) - 1);
        last_line[sizeof(last_line) - 1] = '\0';
        found = 1;
    }

    if (ferror(f)) {
        log_msg(ctx, "ERROR: lettura CSV condiviso fallita");
        rc = CSV_ERROR;
        goto out;
    }
    fclose(f);
    f = NULL;

    if (!found) {
        log_msg(ctx, "Chain vuota, accetto blocco genesis");

        if (blockToCsv(new_block, out_line, sizeof(out_line)) != 0) {
            log_msg(ctx, "ERROR: blockToCsv fallito sul genesis");
            rc = CSV_ERROR;
            goto out;
        }

        f = fopen(CSV_FILE_NAME, "a");
        if (f == NULL) {
            log_msg(ctx, "ERROR: apertura CSV per genesis fallita: %s",
                    strerror(errno));
            rc = CSV_ERROR;
            goto out;
        }

        if (fprintf(f, "%s\n", out_line) < 0) {
            log_msg(ctx, "ERROR: scrittura genesis sul CSV fallita");
            rc = CSV_ERROR;
            goto out;
        }

        fclose(f);
        f = NULL;

        blockGetIndex(new_block, &new_index);
        if (ctx->last_block != NULL) blockDestroy(ctx->last_block);
        ctx->last_block   = new_block;
        ctx->chain_length = new_index + 1;
        goto out;
    }

    csv_tail = blockCreate();
    if (csv_tail == NULL) {
        log_msg(ctx, "ERROR: blockCreate per csv_tail fallita");
        rc = -1;
        goto out;
    }

    if (blockFromCsv(csv_tail, last_line) != 0) {
        log_msg(ctx, "ERROR: coda del CSV non valida");
        rc = INVALID_BLOCK;
        goto out;
    }

    if (blockValidate(new_block, csv_tail) != 0) {
        char tail_hash[HASH_HEX_SIZE + 1];
        char new_hash[HASH_HEX_SIZE + 1];
        int same = (blockGetHash(csv_tail, tail_hash) == 0 &&
                    blockGetHash(new_block, new_hash) == 0 &&
                    strcmp(tail_hash, new_hash) == 0);

        blockGetIndex(csv_tail, &tail_index);
        if (ctx->last_block != NULL) blockDestroy(ctx->last_block);
        ctx->last_block   = csv_tail;
        csv_tail          = NULL;
        ctx->chain_length = tail_index + 1;

        if (same) {
            log_msg(ctx, "Blocco già presente nel CSV, copia locale aggiornata");
            rc = BLOCK_ALREADY_PRESENT;
        } else {
            log_msg(ctx, "Blocco perdente rifiutato, chain ri-sincronizzata");
            rc = CHAIN_MISMATCH;
        }
        goto out;
    }

    if (blockToCsv(new_block, out_line, sizeof(out_line)) != 0) {
        log_msg(ctx, "ERROR: blockToCsv fallito");
        rc = CSV_ERROR;
        goto out;
    }

    f = fopen(CSV_FILE_NAME, "a");
    if (f == NULL) {
        log_msg(ctx, "ERROR: apertura CSV per append fallita: %s",
                strerror(errno));
        rc = CSV_ERROR;
        goto out;
    }

    if (fprintf(f, "%s\n", out_line) < 0) {
        log_msg(ctx, "ERROR: scrittura sul CSV fallita");
        rc = CSV_ERROR;
        goto out;
    }

    fclose(f);
    f = NULL;

    blockGetIndex(new_block, &new_index);
    if (ctx->last_block != NULL) blockDestroy(ctx->last_block);
    ctx->last_block   = new_block;
    ctx->chain_length = new_index + 1;

out:
    if (f != NULL)       fclose(f);
    if (csv_tail != NULL) blockDestroy(csv_tail);

    if (rc == 0 || rc == CHAIN_MISMATCH || rc == BLOCK_ALREADY_PRESENT) {
        mirror_shared_to_local(ctx);
    }

    sem_post(sem);
    sem_close(sem);
    return rc;
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
    return 0;
}