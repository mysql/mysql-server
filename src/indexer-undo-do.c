/* -*- mode: C; c-basic-offset: 4 -*- */
/*
 * Copyright (c) 2010 Tokutek Inc.  All rights reserved." 
 * The technology is licensed by the Massachusetts Institute of Technology, 
 * Rutgers State University of New Jersey, and the Research Foundation of 
 * State University of New York at Stony Brook under United States of America 
 * Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
 */

#include <stdio.h>
#include <string.h>
#include <toku_portability.h>
#include "toku_assert.h"
#include "ydb-internal.h"
#include "le-cursor.h"
#include "indexer.h"
#include "brt-internal.h"
#include "toku_atomic.h"
#include "tokuconst.h"
#include "brt.h"
#include "mempool.h"
#include "leafentry.h"
#include "ule.h"
#include "xids.h"

#include "indexer-internal.h"

// internal functions
static int indexer_setup_xids(DB_INDEXER *indexer, ULEHANDLE ule, int i, TXNID xid, XIDS *xids_result);
static int indexer_find_prev(DB_INDEXER *indexer, ULEHANDLE ule, int i);
static int indexer_generate_hot_key_val(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule, UXRHANDLE uxr, DBT *hotkey, DBT *hotval);
static int indexer_brt_delete_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
static int indexer_brt_delete_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
static int indexer_brt_insert_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids);
static int indexer_brt_insert_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids);
static int indexer_brt_commit(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
static int indexer_maybe_lock_provisional_key(DB_INDEXER *indexer, ULEHANDLE ule, int i, DB *hotdb, DBT *key, XIDS xids);
static int indexer_is_xid_live(DB_INDEXER *indexer, TXNID xid);

int
indexer_undo_do(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule) {
    int result = 0;

    // init the xids to the root xid
    XIDS xids = xids_get_root_xids();

    // DBTs for the key and val
    DBT hotkey; toku_init_dbt(&hotkey); hotkey.flags = DB_DBT_REALLOC;
    DBT hotval; toku_init_dbt(&hotval); hotval.flags = DB_DBT_REALLOC;

    // DBT for the previous inserted key
    DBT previous_insert_key; toku_init_dbt(&previous_insert_key); previous_insert_key.flags = DB_DBT_REALLOC;

    // DBTs for the commit keys
    DBT delete_key; toku_init_dbt(&delete_key); delete_key.flags = DB_DBT_REALLOC;
    DBT insert_key; toku_init_dbt(&insert_key); insert_key.flags = DB_DBT_REALLOC;

    // scan the transaction stack from bottom to top
    int num_uxrs = ule_get_num_committed(ule) + ule_get_num_provisional(ule);
    for (int i = 0; i < num_uxrs; i++) {
        // get the ith transaction record
        UXRHANDLE uxr = ule_get_uxr(ule, i);
        invariant(uxr);

        // setup up the xids
        result = indexer_setup_xids(indexer, ule, i, uxr_get_txnid(uxr), &xids);
        if (result != 0)
            break;

        // skip placeholders
        if (uxr_is_placeholder(uxr)) {
            invariant(ule_is_provisional(ule, i));
            continue;
        }

        // init delete and insert commits
        BOOL do_delete_commit = FALSE, do_insert_commit = FALSE;

        // undo
        int previ = indexer_find_prev(indexer, ule, i);
        if (previ >= 0) {
            UXRHANDLE prevuxr = ule_get_uxr(ule, previ);
            if (uxr_is_delete(prevuxr)) {
                ; // do nothing
            } else if (uxr_is_insert(prevuxr)) {
                // generate the hot key and val
                result = indexer_generate_hot_key_val(indexer, hotdb, ule, prevuxr, &hotkey, &hotval);
                if (result == 0) {
                    // send the delete message
                    if (ule_is_committed(ule, i) || !indexer_is_xid_live(indexer, xids_get_outermost_xid(xids))) { 
                        result = indexer_brt_delete_committed(indexer, hotdb, &hotkey, xids);
                        if (result == 0) {
                            do_delete_commit = TRUE;
                            result = toku_dbt_set(hotkey.size, hotkey.data, &delete_key, NULL);
                        }
                    } else {
                        result = indexer_brt_delete_provisional(indexer, hotdb, &hotkey, xids);
                    }
                    if (result == 0)
                        result = indexer_maybe_lock_provisional_key(indexer, ule, i, hotdb, &hotkey, xids);
                }
            } else
                invariant(0);
        }
        if (result != 0)
            break;

        // do
        if (uxr_is_delete(uxr)) {
            result = indexer_maybe_lock_provisional_key(indexer, ule, i, hotdb, &previous_insert_key, xids);
        } else if (uxr_is_insert(uxr)) {
            // generate the hot key and val
            result = indexer_generate_hot_key_val(indexer, hotdb, ule, uxr, &hotkey, &hotval);
            if (result == 0) {
                // send the insert message
                if (ule_is_committed(ule, i) || !indexer_is_xid_live(indexer, xids_get_outermost_xid(xids))) { 
                    result = indexer_brt_insert_committed(indexer, hotdb, &hotkey, &hotval, xids);
                    if (result == 0) {
                        do_insert_commit = TRUE;
                        result = toku_dbt_set(hotkey.size, hotkey.data, &insert_key, NULL);
                    }
                } else {
                    result = indexer_brt_insert_provisional(indexer, hotdb, &hotkey, &hotval, xids);
                }
                if (result == 0) {
                    result = indexer_maybe_lock_provisional_key(indexer, ule, i, hotdb, &hotkey, xids);
                    if (result == 0)
                        result = toku_dbt_set(hotkey.size, hotkey.data, &previous_insert_key, NULL);
                }
            }
        } else
            invariant(0);

        // send commit messages if needed
        if (result == 0 && do_delete_commit)
            result = indexer_brt_commit(indexer, hotdb, &delete_key, xids);
        if (result == 0 && do_insert_commit) 
            result = indexer_brt_commit(indexer, hotdb, &insert_key, xids);

        if (result != 0)
            break;
    }

    // cleanup
    toku_destroy_dbt(&hotkey);
    toku_destroy_dbt(&hotval);
    toku_destroy_dbt(&delete_key);
    toku_destroy_dbt(&insert_key);
    toku_destroy_dbt(&previous_insert_key);
    xids_destroy(&xids);

    if ( indexer->i->test_only_flags == INDEXER_TEST_ONLY_ERROR_CALLBACK ) 
        result = EINVAL;

    return result;
}

static int
indexer_setup_xids(DB_INDEXER *indexer, ULEHANDLE ule, int i, TXNID this_xid, XIDS *xids_result) {
    indexer = indexer;
    int result = 0;
    XIDS old_xids = *xids_result;
    XIDS new_xids;
    if (i <= ule_get_num_committed(ule) || xids_get_num_xids(old_xids) == 0) {
        // setup xids = [ root xid, this xid ]
        new_xids = xids_get_root_xids();
        if (this_xid > 0) {
            XIDS child_xids;
            result = xids_create_child(new_xids, &child_xids, this_xid);
	    xids_destroy(&new_xids);
            if (result == 0)
		new_xids = child_xids;
	}
    } else {
        // append this_xid to xids
        result = xids_create_child(old_xids, &new_xids, this_xid);
    }

    if (result == 0) {
        xids_destroy(&old_xids);
        *xids_result = new_xids;
    }

    return result;
}

static int
indexer_generate_hot_key_val(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule, UXRHANDLE uxr, DBT *hotkey, DBT *hotval) {
    int result = 0;

    // setup the source key
    DBT srckey;
    toku_fill_dbt(&srckey, ule_get_key(ule), ule_get_keylen(ule));

    // setup the source val
    DBT srcval;
    toku_fill_dbt(&srcval, uxr_get_val(uxr), uxr_get_vallen(uxr));

    // generate the secondary row
    DB_ENV *env = indexer->i->env;
    result = env->i->generate_row_for_put(hotdb, indexer->i->src_db, hotkey, hotval, &srckey, &srcval);

    toku_destroy_dbt(&srckey);
    toku_destroy_dbt(&srcval);

    return result;
}

// looks up the TOKUTXN by TXNID.  if it does not exist then the transaction is committed.
// returns TRUE if the xid is committed.  otherwise returns FALSE.
static int
indexer_is_xid_live(DB_INDEXER *indexer, TXNID xid) {
    int result = 0;
    // TEST
    if (indexer->i->test_is_xid_live) {
        result = indexer->i->test_is_xid_live(indexer, xid);
    } else {
        DB_ENV *env = indexer->i->env;
        TOKUTXN txn = NULL;
        int r = toku_txnid2txn(env->i->logger, xid, &txn);
        invariant(r == 0);
        result = txn != NULL;
    }
    return result;
}

// Take a write lock on the given key for the outermost xid in the xids list.
static int 
indexer_maybe_lock_provisional_key(DB_INDEXER *indexer, ULEHANDLE ule, int i, DB *hotdb, DBT *key, XIDS xids) {
    int result = 0;
    if (ule_is_provisional(ule, i)) {
	TXNID outermost_live_xid = xids_get_outermost_xid(xids);
        // TEST
        if (indexer->i->test_maybe_lock_provisional_key) {
            result = indexer->i->test_maybe_lock_provisional_key(indexer, outermost_live_xid, hotdb, key);
        } else {
            DB_ENV *env = indexer->i->env;
            TOKUTXN txn = NULL;
            result = toku_txnid2txn(env->i->logger, outermost_live_xid, &txn);
            invariant(result == 0);
            if (txn != NULL) {
                result = toku_grab_write_lock(hotdb, key, txn);
                lazy_assert(result == 0);
            }
        }
    }
    return result;
}

static int 
indexer_find_prev_committed(ULEHANDLE ule, int i) {
    invariant(i < ule_num_uxrs(ule));
    int previ = i - 1;
    return previ;
}

static int
indexer_find_prev_provisional(ULEHANDLE ule, int i) {
    invariant(i < ule_num_uxrs(ule));
    int previ = i - 1;
    while (previ >= 0) {
        UXRHANDLE uxr = ule_get_uxr(ule, previ);
        if (!uxr_is_placeholder(uxr))
            break;
        previ -= 1;
    }
    return previ;
}

// find the index of the previous transaction record before the ith position in the stack
static int
indexer_find_prev(DB_INDEXER *UU(indexer), ULEHANDLE ule, int i) {
    int result;
    if (ule_is_committed(ule, i))
        result = indexer_find_prev_committed(ule, i);
    else
        result = indexer_find_prev_provisional(ule, i);
    return result;
}

static TOKUTXN
indexer_get_innermost_live_txn(DB_INDEXER *indexer, XIDS xids) {
    DB_ENV *env = indexer->i->env;
    TOKUTXN txn = NULL;
    for (uint8_t num_xids = xids_get_num_xids(xids); txn == NULL && num_xids > 0; num_xids--) {
        TXNID xid = xids_get_xid(xids, num_xids-1);
        int result = toku_txnid2txn(env->i->logger, xid, &txn);
        invariant(result == 0);
    }
    return txn;
}

// inject "delete" message into brt with logging in recovery and rollback logs,
// and making assocation between txn and brt
static int 
indexer_brt_delete_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids) {
    int result = 0;
    // TEST
    if (indexer->i->test_delete_provisional) {
        result = indexer->i->test_delete_provisional(indexer, hotdb, hotkey, xids);
    } else {
        result = toku_ydb_check_avail_fs_space(indexer->i->env);
        if (result == 0) {
            TOKUTXN txn = indexer_get_innermost_live_txn(indexer, xids);
            invariant(txn != NULL);
            result = toku_brt_maybe_delete (hotdb->i->brt, hotkey, txn, FALSE, ZERO_LSN, TRUE);
        }
    }
    return result;	
}

// send a delete message into the tree without rollback or recovery logging
static int 
indexer_brt_delete_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids) {
    int result = 0;
    // TEST
    if (indexer->i->test_delete_committed) {
        result = indexer->i->test_delete_committed(indexer, hotdb, hotkey, xids);
    } else {
        result = toku_ydb_check_avail_fs_space(indexer->i->env);
        if (result == 0) 
            result = toku_brt_send_delete(db_struct_i(hotdb)->brt, hotkey, xids);
    }
    return result;
}

// inject "insert" message into brt with logging in recovery and rollback logs,
// and making assocation between txn and brt
static int 
indexer_brt_insert_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids) {
    int result = 0;
    // TEST
    if (indexer->i->test_insert_provisional) {
        result = indexer->i->test_insert_provisional(indexer, hotdb, hotkey, hotval, xids);
    } else {
        result = toku_ydb_check_avail_fs_space(indexer->i->env);
        if (result == 0) {
            TOKUTXN txn = indexer_get_innermost_live_txn(indexer, xids);
            invariant(txn != NULL);
            result = toku_brt_maybe_insert (hotdb->i->brt, hotkey, hotval, txn, FALSE, ZERO_LSN, TRUE, BRT_INSERT);
        }
    }
    return result;	
}

// send an insert message into the tree without rollback or recovery logging
// and without associating the txn and the brt
static int 
indexer_brt_insert_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids) {
    int result = 0;
    // TEST
    if (indexer->i->test_insert_committed) {
        result = indexer->i->test_insert_committed(indexer, hotdb, hotkey, hotval, xids);
    } else {
        result = toku_ydb_check_avail_fs_space(indexer->i->env);
        if (result == 0)
            result  = toku_brt_send_insert(db_struct_i(hotdb)->brt, hotkey, hotval, xids, BRT_INSERT);
    }
    return result;
}

// send a commit message into the tree
// Note: If the xid is zero, then the leafentry will already have a committed transaction
//       record and no commit message is needed.  (A commit message with xid of zero is
//       illegal anyway.)
static int 
indexer_brt_commit(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids) {
    int result = 0;
    if (xids_get_num_xids(xids) > 0) {// send commit only when not the root xid
        // TEST
        if (indexer->i->test_commit_any) {
            result = indexer->i->test_commit_any(indexer, hotdb, hotkey, xids);
        } else {
            result = toku_ydb_check_avail_fs_space(indexer->i->env);
            if (result == 0)
                result = toku_brt_send_commit_any(db_struct_i(hotdb)->brt, hotkey, xids);
        }
    }
    return result;
}
