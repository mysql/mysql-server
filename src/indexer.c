/* -*- mode: C; c-basic-offset: 4 -*- */
/*
 * Copyright (c) 2010 Tokutek Inc.  All rights reserved." 
 * The technology is licensed by the Massachusetts Institute of Technology, 
 * Rutgers State University of New Jersey, and the Research Foundation of 
 * State University of New York at Stony Brook under United States of America 
 * Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
 */

/*
 *   The indexer
 */
#include <stdio.h>
#include <string.h>
#include <toku_portability.h>
#include "toku_assert.h"
#include "ydb-internal.h"
#include "le-cursor.h"
#include "indexer.h"
#include "toku_atomic.h"
#include "tokuconst.h"
#include "brt.h"
#include "mempool.h"
#include "leafentry.h"
#include "ule.h"
#include "xids.h"
#include "log-internal.h"

// for now 
static INDEXER_STATUS_S status;

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

static void
indexer_add_refs(DB_INDEXER *indexer) {
    toku_db_add_ref(indexer->i->src_db);
    for (int i = 0; i < indexer->i->N; i++)
        toku_db_add_ref(indexer->i->dest_dbs[i]);
}

static void
indexer_release_refs(DB_INDEXER *indexer) {
    toku_db_release_ref(indexer->i->src_db);
    for (int i = 0; i < indexer->i->N; i++)
        toku_db_release_ref(indexer->i->dest_dbs[i]);
}

/*
 *  free_indexer_resources() frees all of the resources associated with
 *      struct __toku_indexer_internal 
 *  assumes any previously freed items set the field pointer to NULL
 */

static void 
free_indexer_resources(DB_INDEXER *indexer) {
    if ( indexer->i ) {
        if ( indexer->i->lec )   { le_cursor_close(indexer->i->lec); }
        if ( indexer->i->fnums ) { 
            toku_free(indexer->i->fnums); 
            indexer->i->fnums = NULL;
        }
        indexer_undo_do_destroy(indexer);
        indexer_release_refs(indexer);
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

int 
toku_indexer_create_indexer(DB_ENV *env,
                            DB_TXN *txn,
                            DB_INDEXER **indexerp,
                            DB *src_db,
                            int N,
                            DB *dest_dbs[N],
                            uint32_t db_flags[N] UU(),
                            uint32_t indexer_flags) 
{
    int rval = 0;
    DB_INDEXER *indexer = 0;   // set later when created

    *indexerp = NULL;
	
    rval = toku_grab_read_lock_on_directory (src_db, txn);
    if (rval == 0) {
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
        indexer->i->undo_do            = indexer_undo_do; // TEST export the undo do function
	
	XCALLOC_N(N, indexer->i->fnums);
	if ( !indexer->i->fnums ) { rval = ENOMEM; goto create_exit; }
	for(int i=0;i<indexer->i->N;i++) {
	    indexer->i->fnums[i]       = toku_cachefile_filenum(db_struct_i(dest_dbs[i])->brt->cf);
	}
	indexer->i->filenums.num       = N;
	indexer->i->filenums.filenums  = indexer->i->fnums;
        indexer->i->test_only_flags    = 0;  // for test use only
	
	indexer->set_error_callback    = toku_indexer_set_error_callback;
	indexer->set_poll_function     = toku_indexer_set_poll_function;
	indexer->build                 = build_index;
	indexer->close                 = close_indexer;
	indexer->abort                 = abort_indexer;
	
	// create and initialize the leafentry cursor
	rval = le_cursor_create(&indexer->i->lec, db_struct_i(src_db)->brt, db_txn_struct_i(txn)->tokutxn);
	if ( !indexer->i->lec ) { goto create_exit; }
	
	// 2954: add recovery and rollback entries
	LSN hot_index_lsn; // not used (yet)
	TOKUTXN      ttxn = db_txn_struct_i(txn)->tokutxn;
	FILENUMS filenums = indexer->i->filenums;
	rval = toku_brt_hot_index(NULL, ttxn, filenums, 1, &hot_index_lsn);
    }

    if (rval == 0)
	rval = associate_indexer_with_hot_dbs(indexer, dest_dbs, N);

create_exit:
    if ( rval == 0 ) {

        indexer_undo_do_init(indexer);
        indexer_add_refs(indexer);
        
        *indexerp = indexer;

	(void) toku_sync_fetch_and_increment_uint64(&status.create);
	(void) toku_sync_fetch_and_increment_uint32(&status.current);
	if ( status.current > status.max )
	    status.max = status.current;   // not worth a lock to make threadsafe, may be inaccurate

    } else {
	(void) toku_sync_fetch_and_increment_uint64(&status.create_fail);
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

BOOL
toku_indexer_is_key_right_of_le_cursor(DB_INDEXER *indexer, DB *db, const DBT *key) {
    return is_key_right_of_le_cursor(indexer->i->lec, key, db);
}

static int 
build_index(DB_INDEXER *indexer) {
    int result = 0;

    DBT key; toku_init_dbt_flags(&key, DB_DBT_REALLOC);
    DBT le; toku_init_dbt_flags(&le, DB_DBT_REALLOC);

    BOOL done = FALSE;
    for (uint64_t loop_count = 0; !done; loop_count++) {

        toku_ydb_lock();

        result = le_cursor_next(indexer->i->lec, &le);
	if (result != 0) {
	    done = TRUE;
	    if (result == DB_NOTFOUND)
		result = 0;  // all done, normal way to exit loop successfully
	}
        else {
            // this code may be faster ule malloc/free is not done every time
            ULEHANDLE ule = toku_ule_create(le.data);
            for (int which_db = 0; (which_db < indexer->i->N) && (result == 0); which_db++) {
                DB *db = indexer->i->dest_dbs[which_db];
                result = indexer_undo_do(indexer, db, ule);
                if ( (result != 0) && (indexer->i->error_callback != NULL)) {
                    toku_dbt_set(ule_get_keylen(ule), ule_get_key(ule), &key, NULL);
                    indexer->i->error_callback(db, which_db, result, &key, NULL, indexer->i->error_extra);
                }
            }
            toku_ule_free(ule);
        }

        // if there is lock contention, then sleep for 1 millisecond after the unlock
        // note: the value 1000 was empirically determined to provide good query performance
        //         during hotindexing
        toku_ydb_unlock_and_yield(1000); 
        
        if (result == 0) 
            result = maybe_call_poll_func(indexer, loop_count);
	if (result != 0)
	    done = TRUE;
    }

    toku_destroy_dbt(&key);
    toku_destroy_dbt(&le);

    // post index creation cleanup
    //  - optimize?
    //  - garbage collect?
    //  - unique checks?

    if ( result == 0 ) {
        (void) toku_sync_fetch_and_increment_uint64(&status.build);
    } else {
	(void) toku_sync_fetch_and_increment_uint64(&status.build_fail);
    }


    return result;
}

static int 
close_indexer(DB_INDEXER *indexer) {
    int r = 0;
    (void) toku_sync_fetch_and_decrement_uint32(&status.current);

    toku_ydb_lock();
    {
        // Add all created dbs to the transaction's checkpoint_before_commit list.  
        // (This will cause a local checkpoint of created index files, which is necessary 
        //   because these files are not necessarily on disk and all the operations 
        //   to create them are not in the recovery log.)
        DB_TXN     *txn = indexer->i->txn;
        TOKUTXN tokutxn = db_txn_struct_i(txn)->tokutxn;
        BRT brt;
        DB *db;
        for (int which_db = 0; which_db < indexer->i->N ; which_db++) {
            db = indexer->i->dest_dbs[which_db];
            brt = db_struct_i(db)->brt;
            toku_brt_require_local_checkpoint(brt, tokutxn);
        }

        // Disassociate the indexer from the hot db and free_indexer
        disassociate_indexer_from_hot_dbs(indexer);
        free_indexer(indexer);
    }
    toku_ydb_unlock();

    if ( r == 0 ) {
        (void) toku_sync_fetch_and_increment_uint64(&status.close);
    } else {
	(void) toku_sync_fetch_and_increment_uint64(&status.close_fail);
    }
    return r;
}

static int 
abort_indexer(DB_INDEXER *indexer) {
    (void) toku_sync_fetch_and_decrement_uint32(&status.current);
    (void) toku_sync_fetch_and_increment_uint64(&status.abort);
    
    toku_ydb_lock();
    {
        // Disassociate the indexer from the hot db and free_indexer
        disassociate_indexer_from_hot_dbs(indexer);
        free_indexer(indexer);
    }
    toku_ydb_unlock();
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
        if ( indexer->i->estimated_rows == 0 ) {
            progress = 1.0;
        } 
        else {
            progress = (float)loop_count / (float)indexer->i->estimated_rows;
        }
        result = indexer->i->poll_func(indexer->i->poll_extra, progress);
    }
    return result;
}

void 
toku_indexer_get_status(INDEXER_STATUS s) {
    *s = status;
}

// this allows us to force errors under test.  Flags are defined in indexer.h
void
toku_indexer_set_test_only_flags(DB_INDEXER *indexer, int flags) {
    invariant(indexer != NULL);
    indexer->i->test_only_flags = flags;
}

DB *
toku_indexer_get_src_db(DB_INDEXER *indexer) {
    return indexer->i->src_db;
}
