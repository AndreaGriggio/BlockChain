#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "error.h"
#include "miner.h"
#include "transactionPool.h"
#include "client.h"
#include "minerStatus.h"
#include "utils.h"
#include "blocksPool.h"

typedef struct Miner
{
    BlocksPool *pending_pool;
    TransactionPool *transaction_pool;

    uint difficulty;
    char previous_hash[HASH_HEX_SIZE + 1];
    uint64_t previous_index;
    Block *mined_block;

    pthread_mutex_t lock;

} Miner;

/**
 * Alloca sull'heap una nuova struttura Miner (non inizializzata).
 * @return Puntatore al miner allocato, oppure NULL se l'allocazione fallisce
 */
Miner *minerCreate(uint difficulty, const char *previous_hash, const uint64_t previous_index)
{
    Miner *miner = malloc(sizeof(Miner));
    if (miner == NULL)
    {
        return NULL;
    }

    miner->pending_pool = createBlocksPool();

    if (miner->pending_pool == NULL)
    {
        free(miner);
        return NULL;
    }

    miner->transaction_pool = createTransactionPool();

    if (miner->transaction_pool == NULL)
    {
        destroyBlocksPool(miner->pending_pool);
        free(miner);
        return NULL;
    }

    // Salviamo solo hash e indice dell'ultimo blocco: genesi (NULL) -> tutti '0'
    if (previous_hash == NULL)
    {
        memset(miner->previous_hash, '0', HASH_HEX_SIZE);
    }
    else
    {
        if (strlen(previous_hash) != HASH_HEX_SIZE)
        {
            destroyTransactionPool(miner->transaction_pool);
            destroyBlocksPool(miner->pending_pool);
            free(miner);
            return NULL;
        }
        memcpy(miner->previous_hash, previous_hash, HASH_HEX_SIZE);
    }
    miner->previous_hash[HASH_HEX_SIZE] = '\0';
    miner->previous_index = previous_index;

    miner->mined_block = NULL;

    if (pthread_mutex_init(&miner->lock, NULL) != 0)
    {

        destroyTransactionPool(miner->transaction_pool);
        destroyBlocksPool(miner->pending_pool);
        free(miner);
        return NULL;
    }

    minerInit(miner, difficulty);

    return miner;
}

/**
 * Inizializza i campi di un miner già allocato impostando la difficoltà e
 * azzerando contatori e blocco minato.
 * @param miner Miner da inizializzare
 * @param miner_difficulty Difficoltà di mining da assegnare al miner
 * @return 0 se tutto è andato a buon fine, -1 se miner è NULL
 */
int minerInit(Miner *miner, uint miner_difficulty)
{
    if (miner == NULL || miner_difficulty == 0)
    {
        return -1;
    }

    poolBlocksSetState(miner->pending_pool, BLOCK_WAITING);

    miner->difficulty = miner_difficulty;

    return 0;
}

int minerPushTransaction(Miner *miner, const char *tx)
{
    if (miner == NULL || tx == NULL)
    {
        return INVALID_PARAMS;
    }

    pthread_mutex_lock(&miner->lock);

    int rc = poolPush(miner->transaction_pool, tx);

    pthread_mutex_unlock(&miner->lock);

    return rc;
}

// Reinserisce le transazioni di un blocco nella transaction pool
static int requeue_block_transactions_locked(Miner *miner, const Block *block)
{
    if (miner == NULL || block == NULL)
    {
        return INVALID_PARAMS;
    }

    TxList list;

    int rc = unpack_transactions(block, &list);
    if (rc != 0)
    {
        return rc;
    }

    size_t inserted = 0;

    for (size_t i = 0; i < list.count; i++)
    {
        rc = poolPush(miner->transaction_pool, list.strings[i]);

        if (rc != 0)
        {
            // Il mutex impedisce ad altri thread di modificare la pool durante questa operazione
            while (inserted > 0)
            {
                char *tx = poolRemoveLast(
                    miner->transaction_pool);
                free(tx);
                inserted--;
            }
            return rc;
        }
        inserted++;
    }
    return 0;
}

int minerRequeueBlockTransactions(Miner *miner, const Block *block)
{
    if (miner == NULL || block == NULL)
    {
        return INVALID_PARAMS;
    }
    pthread_mutex_lock(&miner->lock);

    int rc = requeue_block_transactions_locked(miner, block);

    pthread_mutex_unlock(&miner->lock);

    return rc;
}

static int minerInitMinedBlock(Miner *miner, u_int64_t nonce)
{
    if (miner == NULL)
        return INVALID_PARAMS;
    if (miner->mined_block == NULL)
        return INVALID_PARAMS;

    pthread_mutex_lock(&miner->lock);
    uint64_t index = miner->previous_index + 1;

    TxList *list = poolTrxCreateList(miner->transaction_pool);

    if (list == NULL)
    {
        pthread_mutex_unlock(&miner->lock);
        return MEMORY_ERROR;
    }
    if (list->count == 0)
    {
        free(list);
        pthread_mutex_unlock(&miner->lock);
        return INVALID_BLOCK;
    }
    int res = blockInit(miner->mined_block,
                        index,
                        nowUnix(),
                        miner->previous_hash,
                        nonce,
                        list);
    if (res != 0)
    {

        for (size_t i = 0; i < list->count; i++)
        {
            poolPush(miner->transaction_pool, list->strings[i]);
        }
    }
    free(list);
    pthread_mutex_unlock(&miner->lock);

    return res;
}

int minerAddBlockToPending(Miner *miner, Block *block)
{
    if (miner == NULL || block == NULL)
    {
        return INVALID_PARAMS;
    }
    pthread_mutex_lock(&miner->lock);

    int rc = poolPushBlock(
        miner->pending_pool,
        block);

    pthread_mutex_unlock(&miner->lock);
    return rc;
}

/**
 * Trasferisce al chiamante il blocco minato dal miner, cedendone la proprietà
 * (dopo la chiamata il miner non punta più al blocco).
 * @param miner Miner da cui prelevare il blocco minato
 * @param block_ptr Puntatore di output al blocco minato
 * @return 0 se tutto è andato a buon fine, INVALID_PARAMS se i parametri sono
 *         nulli o se non esiste un blocco minato
 */
int minerPopMinedBlock(Miner *miner, Block **block_ptr)
{
    if (miner == NULL || block_ptr == NULL)
        return INVALID_PARAMS;

    pthread_mutex_lock(&miner->lock);
    if (miner->mined_block == NULL)
    {
        pthread_mutex_unlock(&miner->lock);
        return INVALID_PARAMS;
    }
    *block_ptr = miner->mined_block;
    miner->mined_block = NULL;
    pthread_mutex_unlock(&miner->lock);

    return 0;
}
/**
 * Simula un singolo tentativo di mining: la probabilità di successo dipende
 * dalla difficoltà richiesta.
 * @param difficulty Difficoltà del mining
 * @param nonce valore da trovare
 * @return 1 se il tentativo ha avuto successo, 0 altrimenti
 */
static int minerMiningAttempt(const uint difficulty, const uint nonce)
{
    const uint num = NUM_MIN_MAX(0, difficulty - 1);
    return num == nonce ? 1 : 0;
}
static uint minerInitNonce(const uint difficulty)
{
    return NUM_MIN_MAX(0, difficulty - 1);
}

/**
 * Crea un nuovo blocco vuoto e lo collega al miner come blocco minato corrente
 * (il blocco non viene ancora riempito con i dati).
 * @param miner Miner a cui associare il nuovo blocco
 * @param new Puntatore di output al blocco appena creato
 */
static int minerCreateBlock(Miner *miner, Block **new, u_int64_t nonce)
{
    if (miner == NULL || new == NULL)
        return INVALID_PARAMS;

    Block *b = blockCreate();
    if (b == NULL)
        return MEMORY_ERROR;

    pthread_mutex_lock(&miner->lock);
    // Libero un eventuale blocco precedente non ancora consumato per evitare leak
    if (miner->mined_block != NULL)
    {
        blockDestroy(miner->mined_block);
    }
    // Collego il blocco ma non lo riempio non ho le informazioni
    miner->mined_block = b;
    pthread_mutex_unlock(&miner->lock);

    int res = minerInitMinedBlock(miner, nonce);
    if (res != 0)
    {
        // Init fallita: scollego e distruggo il blocco, non lo lascio agganciato
        pthread_mutex_lock(&miner->lock);
        if (miner->mined_block == b)
        {
            blockDestroy(miner->mined_block);
            miner->mined_block = NULL;
        }
        pthread_mutex_unlock(&miner->lock);
        return res;
    }

    *new = b;
    return 0;
}

/**
 * Ciclo principale di mining eseguito dal thread dedicato. Attende il segnale di
 * lavoro, tenta ripetutamente di minare un blocco e aggiorna lo stato condiviso;
 * termina solo quando lo stato passa a MINER_STOPPED.
 * @param miner Miner che esegue il mining
 * @param status Stato condiviso usato per sincronizzazione e segnalazioni
 * @return 0 all'uscita dal ciclo, INVALID_PARAMS se i parametri sono nulli
 */
int minerMiningLoop(Miner *miner, MinerStatus *status)
{
    if (miner == NULL || status == NULL)
        return INVALID_PARAMS;
    // ciclio infinito una volta iniziato non si interrompe finchè state = MINER_STOPPED
    while (1)
    {
        /*Con questa chiama bloccante controlliamo quando entra all'interno del ciclo di mining*/
        // rimane bloccata finchè state = MINER_IDLE
        msWaitForWork(status);

        MinerState s;
        mSGetState(status, &s);
        mSSetBlockState(status, MINER_BLOCK_NOT_FOUND);
        if (s == MINER_STOPPED)
            break;
        if (s == MINER_RESTART)
        {
            msSignal(status, MINER_MINING);
            s = MINER_MINING;
        }

        int trovato = 0;
        const int sleeping_time = NUM_MIN_MAX(MIN_SLEEPING_TIME, MAX_SLEEPING_TIME);
        const uint nonce = minerInitNonce(miner->difficulty);
        size_t attempts = 0;
        Block *new = NULL;

        while (!trovato && s != MINER_IDLE)
        {
            sleep(sleeping_time);

            mSGetState(status, &s);
            if (s == MINER_RESTART || s == MINER_STOPPED)
                break;

            if (minerMiningAttempt(miner->difficulty, nonce))
            {
                int cb = minerCreateBlock(miner, &new, nonce);
                if (cb == 0)
                {
                    /* set di found + park in un'unica sezione critica */
                    msSetBlockFoundAndIdle(status);
                    trovato = 1;
                }
            }
            else
            {
                mSGetAttempts(status, &attempts);
                mSSetAttempts(status, attempts + 1);
            }
        }

        if (s == MINER_STOPPED)
            break;
    }

    return 0;
}

int minerCleanBlocksPool(Miner *miner, MinerStatus *status, const char *accepted_hash, int valid, int miner_id, uint64_t block_index, MinerCleanupStats *stats_out)
{
    if (stats_out != NULL)
    {
        stats_out->own_block_won = 0;
        stats_out->losers_removed = 0;
        stats_out->tx_requeued = 0;
    }
    if (miner == NULL || status == NULL || accepted_hash == NULL)
    {
        return INVALID_PARAMS;
    }

    (void)status;
    (void)miner_id;

    // La pulizia viene eseguita soltanto su una conferma valida.
    if (!valid)
    {
        return 0;
    }

    if (strlen(accepted_hash) != HASH_HEX_SIZE)
    {
        return INVALID_HASH;
    }

    Block *tmp = blockCreate();
    if (tmp == NULL)
    {
        return MEMORY_ERROR;
    }

    pthread_mutex_lock(&miner->lock);

    BlockState state;

    int rc = poolBlocksGetState(miner->pending_pool, &state);

    if (rc != 0 || state != BLOCK_WAITING)
    {
        pthread_mutex_unlock(&miner->lock);
        blockDestroy(tmp);
        return INVALID_PARAMS;
    }

    size_t i = 0;

    while (i < miner->pending_pool->count)
    {
        uint64_t pending_index = 0;
        char pending_hash[HASH_HEX_SIZE + 1];

        rc = poolBlockGet(miner->pending_pool, tmp, i);

        if (rc != 0 ||
            blockGetIndex(tmp, &pending_index) != 0 ||
            blockGetHash(tmp, pending_hash) != 0)
        {
            i++;
            continue;
        }

        if (pending_index > block_index)
        {
            i++;
            continue;
        }

        int is_accepted_block =
            pending_index == block_index &&
            strcmp(
                pending_hash,
                accepted_hash) == 0;

        if (is_accepted_block)
        {
            if (stats_out != NULL)
                stats_out->own_block_won = 1;
        }
        else
        {
            // conta le transazioni PRIMA di richiamarle (serve solo per il numero da loggare)
            TxList lost_txs;
            size_t lost_tx_count = 0;
            if (unpack_transactions(tmp, &lost_txs) == 0)
                lost_tx_count = lost_txs.count;

            rc = requeue_block_transactions_locked(miner, tmp); 
            if (rc != 0)
            {
                pthread_mutex_unlock(&miner->lock);
                blockDestroy(tmp);
                return rc;
            }

            if (stats_out != NULL)
            {
                stats_out->losers_removed++;
                stats_out->tx_requeued += lost_tx_count;
            }
        }
        // Caso vincitore: rimuove il pending senza recupero.
        // Caso perdente: il recupero è riuscito, quindi rimuove il pending.

        rc = poolBlockRemoveAt(miner->pending_pool, i);

        if (rc != 0)
        {
            pthread_mutex_unlock(&miner->lock);
            blockDestroy(tmp);
            return rc;
        }
    }
    memcpy(miner->previous_hash, accepted_hash, HASH_HEX_SIZE);

    miner->previous_hash[HASH_HEX_SIZE] = '\0';
    miner->previous_index = block_index;

    pthread_mutex_unlock(&miner->lock);
    blockDestroy(tmp);

    return 0;
}

int minerRecoverTransactions(Miner *miner, const char *block_hash, uint64_t block_index)
{
    if (miner == NULL || block_hash == NULL)
        return INVALID_PARAMS;

    Block *tmp = blockCreate();
    if (tmp == NULL)
        return MEMORY_ERROR;

    pthread_mutex_lock(&miner->lock);

    int found = 0;
    for (size_t i = 0; i < miner->pending_pool->count; i++)
    {
        if (poolBlockGet(miner->pending_pool, tmp, i) != 0)
            continue;

        char hash[HASH_HEX_SIZE + 1];
        uint64_t idx = 0;
        blockGetHash(tmp, hash);
        blockGetIndex(tmp, &idx);

        if (idx == block_index && strcmp(hash, block_hash) == 0)
        {
            found = 1;

            int rc = requeue_block_transactions_locked(miner, tmp);

            if (rc != 0)
            {
                pthread_mutex_unlock(&miner->lock);
                blockDestroy(tmp);
                return rc;
            }
            rc = poolBlockRemoveAt(miner->pending_pool, i);

            if (rc != 0)
            {
                pthread_mutex_unlock(&miner->lock);
                blockDestroy(tmp);
                return rc;
            }
            break;
        }
    }

    pthread_mutex_unlock(&miner->lock);
    blockDestroy(tmp);

    if (!found)
        return BLOCK_NOT_FOUND;
    return 0;
}

int minerDestroy(Miner* miner){
    if ( miner == NULL ) return INVALID_PARAMS;

    pthread_mutex_lock(&miner->lock);

    destroyBlocksPool(miner->pending_pool);
    destroyTransactionPool(miner->transaction_pool);
   
    if (miner->mined_block != NULL) blockDestroy(miner->mined_block);
    pthread_mutex_unlock(&miner->lock);
    pthread_mutex_destroy(&miner->lock);
    free(miner);
    return 0;
}
