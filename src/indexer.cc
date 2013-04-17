/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*
 * Copyright (c) 2010 Tokutek Inc.  All rights reserved." 
 * The technology is licensed by the Massachusetts Institute of Technology, 
 * Rutgers State University of New Jersey, and the Research Foundation of 
 * State University of New York at Stony Brook under United States of America 
 * Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
 */
#ident "$Id$"

/*
 *   The indexer
 */
#include <stdio.h>
#include <string.h>
#include <toku_portability.h>
#include "toku_assert.h"
#include "ydb-internal.h"
#include <ft/le-cursor.h>
#include "indexer.h"
#include <ft/tokuconst.h>
#include <ft/ft-ops.h>
#include <ft/leafentry.h>
#include <ft/ule.h>
#include <ft/xids.h>
#include <ft/log-internal.h>
#include <ft/checkpoint.h>

///////////////////////////////////////////////////////////////////////////////////
// Engine status
//
// Status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.

static INDEXER_STATUS_S indexer_status;

#define STATUS_INIT(k,t,l) { \
        indexer_status.status[k].keyname = #k; \
        indexer_status.status[k].type    = t;  \
        indexer_status.status[k].legend  = "indexer: " l; \
    }

static void
status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(INDEXER_CREATE,      UINT64, "number of indexers successfully created");
    STATUS_INIT(INDEXER_CREATE_FAIL, UINT64, "number of calls to toku_indexer_create_indexer() that failed");
    STATUS_INIT(INDEXER_BUILD,       UINT64, "number of calls to indexer->build() succeeded");
    STATUS_INIT(INDEXER_BUILD_FAIL,  UINT64, "number of calls to indexer->build() failed");
    STATUS_INIT(INDEXER_CLOSE,       UINT64, "number of calls to indexer->close() that succeeded");
    STATUS_INIT(INDEXER_CLOSE_FAIL,  UINT64, "number of calls to indexer->close() that failed");
    STATUS_INIT(INDEXER_ABORT,       UINT64, "number of calls to indexer->abort()");
    STATUS_INIT(INDEXER_CURRENT,     UINT64, "number of indexers currently in existence");
    STATUS_INIT(INDEXER_MAX,         UINT64, "max number of indexers that ever existed simultaneously");
    indexer_status.initialized = true;
}
#undef STATUS_INIT

void
toku_indexer_get_status(INDEXER_STATUS statp) {
    if (!indexer_status.initialized)
        status_init();
    *statp = indexer_status;
}

#define STATUS_VALUE(x) indexer_status.status[x].value.num

#include "indexer-internal.h"

static int build_index(DB_INDEXER *indexer);
static int close_indexer(DB_INDEXER *indexer);
static int abort_indexer(DB_INDEXER *indexer);
static void free_indexer_resources(DB_INDEXER *indexer);
static void free_indexer(DB_INDEXER *indexer);
static int update_estimated_rows(DB_INDEXER *indexer);
static int maybe_call_poll_func(DB_INDEXER *indexer, uint64_t loop_count);

static int
associate_indexer_with_hot_dbs(DB_INDEXER *indexer, DB *dest_dbs[], int N) {
    int result =0;
    for (int i = 0; i < N; i++) {
        result = toku_db_set_indexer(dest_dbs[i], indexer);
        if (result != 0) {
            for (int j = 0; j < i; j++) {
                int result2 = toku_db_set_indexer(dest_dbs[j], NULL);
                lazy_assert(result2 == 0);
            }
            break;
        }
    }
    return result;
}

static void
disassociate_indexer_from_hot_dbs(DB_INDEXER *indexer) {
    for (int i = 0; i < indexer->i->N; i++) {
        int result = toku_db_set_indexer(indexer->i->dest_dbs[i], NULL);
        lazy_assert(result == 0);
    }
}

/*
 *  free_indexer_resources() frees all of the resources associated with
 *      struct __toku_indexer_internal 
 *  assumes any previously freed items set the field pointer to NULL
 */

static void 
free_indexer_resources(DB_INDEXER *indexer) {
    if ( indexer->i ) {        
        toku_mutex_destroy(&indexer->i->indexer_lock);
        if ( indexer->i->lec ) {
            toku_le_cursor_close(indexer->i->lec);
        }
        if ( indexer->i->fnums ) { 
            toku_free(indexer->i->fnums); 
            indexer->i->fnums = NULL;
        }
        indexer_undo_do_destroy(indexer);
        // indexer->i
        toku_free(indexer->i);
        indexer->i = NULL;
    }
}

static void 
free_indexer(DB_INDEXER *indexer) {
    if ( indexer ) {
        free_indexer_resources(indexer);
        toku_free(indexer);
        indexer = NULL;
    }
}

void
toku_indexer_lock(DB_INDEXER* indexer) {
    toku_mutex_lock(&indexer->i->indexer_lock);
}

void
toku_indexer_unlock(DB_INDEXER* indexer) {
    toku_mutex_unlock(&indexer->i->indexer_lock);
}

// forward declare the test-only wrapper function for undo-do
static int test_indexer_undo_do(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule);

int 
toku_indexer_create_indexer(DB_ENV *env,
                            DB_TXN *txn,
                            DB_INDEXER **indexerp,
                            DB *src_db,
                            int N,
                            DB *dest_dbs[/*N*/],
                            uint32_t db_flags[/*N*/] UU(),
                            uint32_t indexer_flags) 
{
    int rval = 0;
    DB_INDEXER *indexer = 0;   // set later when created

    *indexerp = NULL;
    
    XCALLOC(indexer);      // init to all zeroes (thus initializing the error_callback and poll_func)
    if ( !indexer )    { rval = ENOMEM; goto create_exit; }
    XCALLOC(indexer->i);   // init to all zeroes (thus initializing all pointers to NULL)
    if ( !indexer->i ) { rval = ENOMEM; goto create_exit; }
    
    indexer->i->env                = env;
    indexer->i->txn                = txn;
    indexer->i->src_db             = src_db;
    indexer->i->N                  = N;
    indexer->i->dest_dbs           = dest_dbs;
    indexer->i->indexer_flags      = indexer_flags;
    indexer->i->loop_mod           = 1000; // call poll_func every 1000 rows
    indexer->i->estimated_rows     = 0; 
    indexer->i->undo_do            = test_indexer_undo_do; // TEST export the undo do function
    
    XCALLOC_N(N, indexer->i->fnums);
    if ( !indexer->i->fnums ) { rval = ENOMEM; goto create_exit; }
    for(int i=0;i<indexer->i->N;i++) {
        indexer->i->fnums[i]       = toku_cachefile_filenum(db_struct_i(dest_dbs[i])->ft_handle->ft->cf);
    }
    indexer->i->filenums.num       = N;
    indexer->i->filenums.filenums  = indexer->i->fnums;
    indexer->i->test_only_flags    = 0;  // for test use only
    
    indexer->set_error_callback    = toku_indexer_set_error_callback;
    indexer->set_poll_function     = toku_indexer_set_poll_function;
    indexer->build                 = build_index;
    indexer->close                 = close_indexer;
    indexer->abort                 = abort_indexer;

    toku_mutex_init(&indexer->i->indexer_lock, NULL);

    //
    // create and close a dummy loader to get redirection going for the hot indexer
    // This way, if the hot index aborts, but other transactions have references to the
    // underlying FT, then those transactions can do dummy operations on the FT
    // while the DB gets redirected back to an empty dictionary
    //
    for (int i = 0; i < N; i++) {
        DB_LOADER* loader = NULL;
        int r = env->create_loader(env, txn, &loader, dest_dbs[i], 1, &dest_dbs[i], NULL, NULL, DB_PRELOCKED_WRITE | LOADER_USE_PUTS);
        if (r) {
            goto create_exit;
        }
        r = loader->close(loader);
        if (r) {
            goto create_exit;
        }
    }
    
    // create and initialize the leafentry cursor
    rval = toku_le_cursor_create(&indexer->i->lec, db_struct_i(src_db)->ft_handle, db_txn_struct_i(txn)->tokutxn);
    if ( !indexer->i->lec ) { goto create_exit; }
    
    // 2954: add recovery and rollback entries
    LSN hot_index_lsn; // not used (yet)
    TOKUTXN      ttxn;
    ttxn = db_txn_struct_i(txn)->tokutxn;
    FILENUMS filenums;
    filenums = indexer->i->filenums;
    toku_multi_operation_client_lock();
    rval = toku_ft_hot_index(NULL, ttxn, filenums, 1, &hot_index_lsn);
    toku_multi_operation_client_unlock();
    
    if (rval == 0) {
        rval = associate_indexer_with_hot_dbs(indexer, dest_dbs, N);
    }
create_exit:
    if ( rval == 0 ) {

        indexer_undo_do_init(indexer);
        
        *indexerp = indexer;

        (void) __sync_fetch_and_add(&STATUS_VALUE(INDEXER_CREATE), 1);
        (void) __sync_fetch_and_add(&STATUS_VALUE(INDEXER_CURRENT), 1);
        if ( STATUS_VALUE(INDEXER_CURRENT) > STATUS_VALUE(INDEXER_MAX) )
            STATUS_VALUE(INDEXER_MAX) = STATUS_VALUE(INDEXER_CURRENT);   // NOT WORTH A LOCK TO MAKE THREADSAFE), may be inaccurate
        
    } else {
        (void) __sync_fetch_and_add(&STATUS_VALUE(INDEXER_CREATE_FAIL), 1);
        free_indexer(indexer);
    }

    return rval;
}

int 
toku_indexer_set_poll_function(DB_INDEXER *indexer,
                               int (*poll_func)(void *poll_extra,
                                                float progress),
                               void *poll_extra)
{
    invariant(indexer != NULL);
    indexer->i->poll_func  = poll_func;
    indexer->i->poll_extra = poll_extra;
    return 0;
}

int 
toku_indexer_set_error_callback(DB_INDEXER *indexer,
                                void (*error_cb)(DB *db, int i, int err,
                                                 DBT *key, DBT *val,
                                                 void *error_extra),
                                void *error_extra)
{
    invariant(indexer != NULL);
    indexer->i->error_callback = error_cb;
    indexer->i->error_extra    = error_extra;
    return 0;
}

// a key is to the right of the indexer's cursor if it compares
// greater than the current le cursor position.
bool
toku_indexer_is_key_right_of_le_cursor(DB_INDEXER *indexer, const DBT *key) {
    return toku_le_cursor_is_key_greater(indexer->i->lec, key);
}

// initialize provisional info by allocating enough space to hold provisional 
// ids, states, and txns for each of the provisional entries in the ule. the 
// ule and le remain owned by the caller, not this struct.
static void 
ule_prov_info_init(struct ule_prov_info *prov_info, LEAFENTRY le, ULEHANDLE ule) {
    prov_info->le = le;
    prov_info->ule = ule;
    prov_info->num_provisional = ule_get_num_provisional(ule);
    prov_info->num_committed = ule_get_num_committed(ule);
    uint32_t n = prov_info->num_provisional;
    if (n > 0) {
        XMALLOC_N(n, prov_info->prov_ids);
        XMALLOC_N(n, prov_info->prov_states);
        XMALLOC_N(n, prov_info->prov_txns);
    }
}

// clean up anything possibly created by ule_prov_info_init()
static void
ule_prov_info_destroy(struct ule_prov_info *prov_info) {
    if (prov_info->num_provisional > 0) {
        toku_free(prov_info->prov_ids);
        toku_free(prov_info->prov_states);
        toku_free(prov_info->prov_txns);
    } else {
        // nothing to free if there was nothing provisional
        invariant(prov_info->prov_ids == NULL);
        invariant(prov_info->prov_states == NULL);
        invariant(prov_info->prov_txns == NULL);
    }
}

static void
indexer_fill_prov_info(DB_INDEXER *indexer, struct ule_prov_info *prov_info) {
    ULEHANDLE ule = prov_info->ule;
    uint32_t num_provisional = prov_info->num_provisional;
    uint32_t num_committed = prov_info->num_committed;
    TXNID *prov_ids = prov_info->prov_ids;
    TOKUTXN_STATE *prov_states = prov_info->prov_states;
    TOKUTXN *prov_txns = prov_info->prov_txns;

    // don't both grabbing the txn manager lock if we don't
    // have any provisional txns to record
    if (num_provisional == 0) {
        return;
    }

    // hold the txn manager lock while we inspect txn state
    // and pin some live txns
    DB_ENV *env = indexer->i->env;
    TXN_MANAGER txn_manager = toku_logger_get_txn_manager(env->i->logger);
    toku_txn_manager_suspend(txn_manager); 
    for (uint32_t i = 0; i < num_provisional; i++) {
        UXRHANDLE uxr = ule_get_uxr(ule, num_committed + i);
        prov_ids[i] = uxr_get_txnid(uxr);
        if (indexer->i->test_xid_state) {
            prov_states[i] = indexer->i->test_xid_state(indexer, prov_ids[i]);
            prov_txns[i] = NULL;
        }
        else {
            TOKUTXN txn = NULL;
            toku_txn_manager_id2txn_unlocked(
                txn_manager, 
                prov_ids[i], 
                &txn
                );
            prov_txns[i] = txn;
            if (txn) {
                prov_states[i] = toku_txn_get_state(txn);
                if (prov_states[i] == TOKUTXN_LIVE || prov_states[i] == TOKUTXN_PREPARING) {
                    // pin this live txn so it can't commit or abort until we're done with it
                    toku_txn_manager_pin_live_txn_unlocked(txn_manager, txn);
                }
            }
            else {
                prov_states[i] = TOKUTXN_RETIRED;
            }
        }
    }
    toku_txn_manager_resume(txn_manager);
}
    
struct le_cursor_extra {
    DB_INDEXER *indexer;
    struct ule_prov_info *prov_info;
};

// cursor callback, so its synchronized with other db operations using 
// cachetable pair locks. because no txn can commit on this db, read
// the provisional info for the newly read ule.
static int
le_cursor_callback(ITEMLEN UU(keylen), bytevec UU(key), ITEMLEN UU(vallen), bytevec val, void *extra, bool lock_only) {
    if (lock_only || val == NULL) {
        ; // do nothing if only locking. do nothing if val==NULL, means DB_NOTFOUND
    } else {
        struct le_cursor_extra *CAST_FROM_VOIDP(cursor_extra, extra);
        struct ule_prov_info *prov_info = cursor_extra->prov_info;
        // the val here is a leafentry. ule_create does not copy the entire
        // contents of the leafentry it is given into its own buffers, so we
        // must allocate space for a leafentry and keep it around with the ule.
        LEAFENTRY CAST_FROM_VOIDP(le, toku_xmemdup(val, vallen));
        ULEHANDLE ule = toku_ule_create(le);
        invariant(ule);
        // when we initialize prov info, we also pass in the leafentry and ule
        // pointers so the caller can access them later. it's their job to free
        // them when they're not needed.
        ule_prov_info_init(prov_info, le, ule);
        indexer_fill_prov_info(cursor_extra->indexer, prov_info);
    }
    return 0;
}

// get the next ule and fill out its provisional info in the
// prov_info struct provided. caller is responsible for cleaning
// up the ule info after it's done.
static int
get_next_ule_with_prov_info(DB_INDEXER *indexer, struct ule_prov_info *prov_info) {
    struct le_cursor_extra extra = {
        .indexer = indexer,
        .prov_info = prov_info,
    };
    int r = toku_le_cursor_next(indexer->i->lec, le_cursor_callback, &extra);
    return r; 
}

static int 
build_index(DB_INDEXER *indexer) {
    int result = 0;

    bool done = false;
    for (uint64_t loop_count = 0; !done; loop_count++) {

        toku_indexer_lock(indexer);

        // grab the next leaf entry and get its provisional info. we'll
        // need the provisional info for the undo-do algorithm, and we get
        // it here so it can be read atomically with respect to txn commit
        // and abort. the atomicity comes from the root-to-leaf path pinned
        // by the query and in the getf callback function
        //
        // this allocates space for the prov info, so we have to destroy it
        // when we're done.
        struct ule_prov_info prov_info;
        memset(&prov_info, 0, sizeof(prov_info));
        result = get_next_ule_with_prov_info(indexer, &prov_info);

        if (result != 0) {
            invariant(prov_info.ule == NULL);
            done = true;
            if (result == DB_NOTFOUND) {
                result = 0;  // all done, normal way to exit loop successfully
            }
        }
        else {
            invariant(prov_info.le);
            invariant(prov_info.ule);
            ULEHANDLE ule = prov_info.ule;
            for (int which_db = 0; (which_db < indexer->i->N) && (result == 0); which_db++) {
                DB *db = indexer->i->dest_dbs[which_db];
                result = indexer_undo_do(indexer, db, ule, &prov_info);
                if ((result != 0) && (indexer->i->error_callback != NULL)) {
                    // grab the key and call the error callback
                    DBT key; toku_init_dbt_flags(&key, DB_DBT_REALLOC);
                    toku_dbt_set(ule_get_keylen(ule), ule_get_key(ule), &key, NULL);
                    indexer->i->error_callback(db, which_db, result, &key, NULL, indexer->i->error_extra);
                    toku_destroy_dbt(&key);
                }
            }
            // the leafentry and ule are not owned by the prov_info,
            // and are still our responsibility to free
            toku_free(prov_info.le);
            toku_ule_free(prov_info.ule);
        }

        toku_indexer_unlock(indexer);
        ule_prov_info_destroy(&prov_info);
        
        if (result == 0) {
            result = maybe_call_poll_func(indexer, loop_count);
        }
        if (result != 0) {
            done = true;
        }
    }

    // post index creation cleanup
    //  - optimize?
    //  - garbage collect?
    //  - unique checks?

    if ( result == 0 ) {
        (void) __sync_fetch_and_add(&STATUS_VALUE(INDEXER_BUILD), 1);
    } else {
        (void) __sync_fetch_and_add(&STATUS_VALUE(INDEXER_BUILD_FAIL), 1);
    }

    return result;
}

// Clients must not operate on any of the hot dbs concurrently with close
static int
close_indexer(DB_INDEXER *indexer) {
    int r = 0;
    (void) __sync_fetch_and_sub(&STATUS_VALUE(INDEXER_CURRENT), 1);

    // Mark txn as needing a checkpoint.
    // (This will cause a checkpoint, which is necessary
    //   because these files are not necessarily on disk and all the operations
    //   to create them are not in the recovery log.)
    DB_TXN     *txn = indexer->i->txn;
    TOKUTXN tokutxn = db_txn_struct_i(txn)->tokutxn;
    toku_txn_require_checkpoint_on_commit(tokutxn);

    // Disassociate the indexer from the hot db and free_indexer
    disassociate_indexer_from_hot_dbs(indexer);
    free_indexer(indexer);

    if ( r == 0 ) {
        (void) __sync_fetch_and_add(&STATUS_VALUE(INDEXER_CLOSE), 1);
    } else {
        (void) __sync_fetch_and_add(&STATUS_VALUE(INDEXER_CLOSE_FAIL), 1);
    }
    return r;
}

// Clients must not operate on any of the hot dbs concurrently with abort
static int 
abort_indexer(DB_INDEXER *indexer) {
    (void) __sync_fetch_and_sub(&STATUS_VALUE(INDEXER_CURRENT), 1);
    (void) __sync_fetch_and_add(&STATUS_VALUE(INDEXER_ABORT), 1);
    // Disassociate the indexer from the hot db and free_indexer
    disassociate_indexer_from_hot_dbs(indexer);
    free_indexer(indexer);
    return 0;
}


// derived from ha_tokudb::estimate_num_rows
static int
update_estimated_rows(DB_INDEXER *indexer) {
    DBT key;  toku_init_dbt(&key);
    DBT data; toku_init_dbt(&data);
    DBC* crsr;
    DB_TXN* txn = NULL;
    uint64_t less, equal, greater;
    int is_exact;
    int error;
    DB *db = indexer->i->src_db;
    DB_ENV *db_env = indexer->i->env;

    error = db_env->txn_begin(db_env, 0, &txn, DB_READ_UNCOMMITTED);
    if (error) goto cleanup;

    error = db->cursor(db, txn, &crsr, 0);
    if (error) { goto cleanup; }
    
    error = crsr->c_get(crsr, &key, &data, DB_FIRST);
    if (error == DB_NOTFOUND) {
        indexer->i->estimated_rows = 0;
        error = 0;
        goto cleanup;
    } 
    else if (error) { goto cleanup; }

    error = db->key_range64(db, txn, &key,
                            &less, &equal, &greater,
                            &is_exact);
    if (error) { goto cleanup; }

    indexer->i->estimated_rows = equal + greater;
    error = 0;
cleanup:
    if ( crsr != NULL ) {
        int rr = crsr->c_close(crsr);
        invariant(rr == 0);
        crsr = NULL;
    }
    txn->commit(txn, 0);
    return error;
}

static int 
maybe_call_poll_func(DB_INDEXER *indexer, uint64_t loop_count) {
    int result = 0;
    if ( indexer->i->poll_func != NULL && ( loop_count % indexer->i->loop_mod ) == 0 ) {
        int r __attribute__((unused)) = update_estimated_rows(indexer);
        // what happens if estimate_rows fails?
        //   - currently does not modify estimate, which is probably sufficient
        float progress;
        if ( indexer->i->estimated_rows == 0  || loop_count > indexer->i->estimated_rows)
            progress = 1.0;
        else
            progress = (float)loop_count / (float)indexer->i->estimated_rows;
        result = indexer->i->poll_func(indexer->i->poll_extra, progress);
    }
    return result;
}


// this allows us to force errors under test.  Flags are defined in indexer.h
void
toku_indexer_set_test_only_flags(DB_INDEXER *indexer, int flags) {
    invariant(indexer != NULL);
    indexer->i->test_only_flags = flags;
}

// this allows us to call the undo do function in tests using
// a convenience wrapper that gets and destroys the ule's prov info
static int
test_indexer_undo_do(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule) {
    struct ule_prov_info prov_info;
    memset(&prov_info, 0, sizeof(prov_info));
    // pass null for the leafentry - we don't need it, neither does the info
    ule_prov_info_init(&prov_info, NULL, ule);
    indexer_fill_prov_info(indexer, &prov_info);
    int r = indexer_undo_do(indexer, hotdb, ule, &prov_info);
    ule_prov_info_destroy(&prov_info);
    return r;
}

DB *
toku_indexer_get_src_db(DB_INDEXER *indexer) {
    return indexer->i->src_db;
}


#undef STATUS_VALUE

