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

static void
indexer_commit_keys_init(struct indexer_commit_keys *keys) {
    keys->max_keys = keys->current_keys = 0;
    keys->keys = NULL;
}

static void
indexer_commit_keys_destroy(struct indexer_commit_keys *keys) {
    for (int i = 0; i < keys->max_keys; i++)
        toku_destroy_dbt(&keys->keys[i]);
    toku_free(keys->keys);
}

static int
indexer_commit_keys_max_valid(struct indexer_commit_keys *keys) {
    return keys->current_keys;
}

static void
indexer_commit_keys_add(struct indexer_commit_keys *keys, size_t length, void *ptr) {
    if (keys->current_keys >= keys->max_keys) {
        int new_max_keys = keys->max_keys == 0 ? 8 : keys->max_keys * 2;
        keys->keys = (DBT *) toku_realloc(keys->keys, new_max_keys * sizeof (DBT));
        resource_assert(keys->keys);
        for (int i = keys->current_keys; i < new_max_keys; i++)
            toku_init_dbt(&keys->keys[i]);
        keys->max_keys = new_max_keys;
    }
    DBT *key = &keys->keys[keys->current_keys];
    if (key->flags == 0)
        assert(key->data == NULL);
    key->flags = DB_DBT_REALLOC;
    toku_dbt_set(length, ptr, key, NULL);
    keys->current_keys++;
}

static void
indexer_commit_keys_set_empty(struct indexer_commit_keys *keys) {
    keys->current_keys = 0;
}

// internal functions
static int indexer_setup_xids_committed(DB_INDEXER *indexer, TXNID xid, XIDS *xids_result);
static int indexer_setup_xids_provisional(DB_INDEXER *indexer, ULEHANDLE ule, int trindex, TXNID xid, BOOL xid_is_live, XIDS *xids_result);

static int indexer_find_prev_committed(DB_INDEXER *indexer, ULEHANDLE ule, int i);
static int indexer_find_prev_provisional(DB_INDEXER *indexer, ULEHANDLE ule, int i);

static int indexer_generate_hot_key_val(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule, UXRHANDLE uxr, DBT *hotkey, DBT *hotval);
static int indexer_brt_delete_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
static int indexer_brt_delete_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
static int indexer_brt_insert_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids);
static int indexer_brt_insert_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids);
static int indexer_brt_commit(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
static int indexer_lock_key(DB_INDEXER *indexer, DB *hotdb, DBT *key, TXNID outermost_live_xid);
static int indexer_is_xid_live(DB_INDEXER *indexer, TXNID xid);
static int indexer_is_previous_insert_key_valid(DB_INDEXER *indexer);

void
indexer_undo_do_init(DB_INDEXER *indexer) {
    indexer_commit_keys_init(&indexer->i->commit_keys);

    // DBTs for the key and val
    toku_init_dbt(&indexer->i->hotkey); indexer->i->hotkey.flags = DB_DBT_REALLOC;
    toku_init_dbt(&indexer->i->hotval); indexer->i->hotval.flags = DB_DBT_REALLOC;

    // DBT for the previous inserted key
    toku_init_dbt(&indexer->i->previous_insert_key); indexer->i->previous_insert_key.flags = DB_DBT_REALLOC;
}

void
indexer_undo_do_destroy(DB_INDEXER *indexer) {
    toku_destroy_dbt(&indexer->i->hotkey);
    toku_destroy_dbt(&indexer->i->hotval);
    toku_destroy_dbt(&indexer->i->previous_insert_key);
    indexer_commit_keys_destroy(&indexer->i->commit_keys);
}

static int
indexer_undo_do_committed(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule) {
    int result = 0;

    // init the xids to the root xid
    XIDS xids = xids_get_root_xids();

    // scan the committed stack from bottom to top
    int num_committed = ule_get_num_committed(ule);
    for (int trindex = 0; trindex < num_committed; trindex++) {

        indexer_commit_keys_set_empty(&indexer->i->commit_keys);

        // get the transaction record
        UXRHANDLE uxr = ule_get_uxr(ule, trindex);
        invariant(uxr);

        // setup up the xids
        TXNID this_xid = uxr_get_txnid(uxr);
        result = indexer_setup_xids_committed(indexer, this_xid, &xids);
        if (result != 0)
            break;

        // skip placeholders
        if (uxr_is_placeholder(uxr)) {
            invariant(0); // not allowed
            continue;
        }

        // undo
        int prev_trindex = indexer_find_prev_committed(indexer, ule, trindex);
        if (prev_trindex >= 0) {
            UXRHANDLE prevuxr = ule_get_uxr(ule, prev_trindex);
            if (uxr_is_delete(prevuxr)) {
                ; // do nothing
            } else if (uxr_is_insert(prevuxr)) {
                // generate the hot key and val
                result = indexer_generate_hot_key_val(indexer, hotdb, ule, prevuxr, &indexer->i->hotkey, &indexer->i->hotval);
                if (result == 0) {
                    // send the delete message
                    result = indexer_brt_delete_committed(indexer, hotdb, &indexer->i->hotkey, xids);
                    if (result == 0) 
                        indexer_commit_keys_add(&indexer->i->commit_keys, indexer->i->hotkey.size, indexer->i->hotkey.data);
                }
            } else
                invariant(0);
        }
        if (result != 0)
            break;

        // do
        if (uxr_is_delete(uxr)) {
            ; // do nothing
        } else if (uxr_is_insert(uxr)) {
            // generate the hot key and val
            result = indexer_generate_hot_key_val(indexer, hotdb, ule, uxr, &indexer->i->hotkey, &indexer->i->hotval);
            if (result == 0) {
                // send the insert message
                result = indexer_brt_insert_committed(indexer, hotdb, &indexer->i->hotkey, &indexer->i->hotval, xids);
                if (result == 0) {
                    indexer_commit_keys_add(&indexer->i->commit_keys, indexer->i->hotkey.size, indexer->i->hotkey.data);
                    result = toku_dbt_set(indexer->i->hotkey.size, indexer->i->hotkey.data, &indexer->i->previous_insert_key, NULL);
                }
            }
        } else
            invariant(0);

        // send commit messages if needed
        for (int i = 0; result == 0 && i < indexer_commit_keys_max_valid(&indexer->i->commit_keys); i++) 
            result = indexer_brt_commit(indexer, hotdb, &indexer->i->commit_keys.keys[i], xids);

        if (result != 0)
            break;
    }

    xids_destroy(&xids);

    return result;
}

static int
indexer_undo_do_provisional(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule) {
    int result = 0;

    indexer_commit_keys_set_empty(&indexer->i->commit_keys);

    // init the xids to the root xid
    XIDS xids = xids_get_root_xids();

    TXNID outermost_xid = TXNID_NONE;
    BOOL outermost_is_live = FALSE;

    // scan the provisional stack from bottom to top
    int num_committed = ule_get_num_committed(ule);
    int num_provisional = ule_get_num_provisional(ule);
    for (int trindex = num_committed; trindex < num_committed + num_provisional; trindex++) {

        // get the ith transaction record
        UXRHANDLE uxr = ule_get_uxr(ule, trindex);
        invariant(uxr);

        TXNID this_xid = uxr_get_txnid(uxr);
        BOOL this_xid_is_live = indexer_is_xid_live(indexer, this_xid);
        if (trindex == num_committed) {
            outermost_xid = this_xid;
            outermost_is_live = this_xid_is_live;
        }

        // setup up the xids
        result = indexer_setup_xids_provisional(indexer, ule, trindex, this_xid, this_xid_is_live, &xids);
        if (result != 0)
            break;

        // skip placeholders
        if (uxr_is_placeholder(uxr))
            continue;

        // undo
        int prev_trindex = indexer_find_prev_provisional(indexer, ule, trindex);
        if (prev_trindex >= 0) {
            UXRHANDLE prevuxr = ule_get_uxr(ule, prev_trindex);
            if (uxr_is_delete(prevuxr)) {
                ; // do nothing
            } else if (uxr_is_insert(prevuxr)) {
                // generate the hot key and val
                result = indexer_generate_hot_key_val(indexer, hotdb, ule, prevuxr, &indexer->i->hotkey, &indexer->i->hotval);
                if (result == 0) {
                    // send the delete message
                    if (!outermost_is_live) {
                        result = indexer_brt_delete_committed(indexer, hotdb, &indexer->i->hotkey, xids);
                        if (result == 0)
                            indexer_commit_keys_add(&indexer->i->commit_keys, indexer->i->hotkey.size, indexer->i->hotkey.data);
                    } else {
                        result = indexer_brt_delete_provisional(indexer, hotdb, &indexer->i->hotkey, xids);
                        if (result == 0)
                            result = indexer_lock_key(indexer, hotdb, &indexer->i->hotkey, outermost_xid);
                    }
                }
            } else
                invariant(0);
        }
        if (result != 0)
            break;

        // do
        if (uxr_is_delete(uxr)) {
            if (outermost_is_live && indexer_is_previous_insert_key_valid(indexer))
                result = indexer_lock_key(indexer, hotdb, &indexer->i->previous_insert_key, outermost_xid);
        } else if (uxr_is_insert(uxr)) {
            // generate the hot key and val
            result = indexer_generate_hot_key_val(indexer, hotdb, ule, uxr, &indexer->i->hotkey, &indexer->i->hotval);
            if (result == 0) {
                // send the insert message
                if (!outermost_is_live) {
                    result = indexer_brt_insert_committed(indexer, hotdb, &indexer->i->hotkey, &indexer->i->hotval, xids);
                } else {
                    result = indexer_brt_insert_provisional(indexer, hotdb, &indexer->i->hotkey, &indexer->i->hotval, xids);
                    if (result == 0) 
                        result = indexer_lock_key(indexer, hotdb, &indexer->i->hotkey, outermost_xid);
                }
                if (result == 0)
                    result = toku_dbt_set(indexer->i->hotkey.size, indexer->i->hotkey.data, &indexer->i->previous_insert_key, NULL);
            }
        } else
            invariant(0);

        if (result != 0)
            break;
    }

    // send commits if the outermost provisional transaction is committed
    for (int i = 0; result == 0 && i < indexer_commit_keys_max_valid(&indexer->i->commit_keys); i++) 
        result = indexer_brt_commit(indexer, hotdb, &indexer->i->commit_keys.keys[i], xids);

    xids_destroy(&xids);

    return result;
}

int
indexer_undo_do(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule) {
    int result;

    result = indexer_undo_do_committed(indexer, hotdb, ule);
    if (result == 0)
        result = indexer_undo_do_provisional(indexer, hotdb, ule);

    if ( indexer->i->test_only_flags == INDEXER_TEST_ONLY_ERROR_CALLBACK ) 
        result = EINVAL;

    return result;
}

// the committed XIDS always = [this_xid]
static int
indexer_setup_xids_committed(DB_INDEXER *UU(indexer), TXNID this_xid, XIDS *xids_result) {
    int result = 0;
    XIDS old_xids = *xids_result;
    XIDS new_xids = xids_get_root_xids();
    if (this_xid > 0) {
        XIDS child_xids;
        result = xids_create_child(new_xids, &child_xids, this_xid);
        xids_destroy(&new_xids);
        if (result == 0)
            new_xids = child_xids;
    }
    if (result == 0) {
        xids_destroy(&old_xids);
        *xids_result = new_xids;
    }

    return result;
}

// the provisional XIDS = XIDS . [this_xid] when this_xid is live or when XIDS is empty
static int
indexer_setup_xids_provisional(DB_INDEXER *UU(indexer), ULEHANDLE ule, int trindex, TXNID xid, BOOL xid_is_live, XIDS *xids_result) {
    invariant(trindex >= ule_get_num_committed(ule));
    int result = 0;
    XIDS old_xids = *xids_result;
    XIDS new_xids;
    if (xids_get_num_xids(old_xids) == 0) {
        // setup xids = [ root xid, xid ]
        new_xids = xids_get_root_xids();
        if (xid > 0) {
            XIDS child_xids;
            result = xids_create_child(new_xids, &child_xids, xid);
	    xids_destroy(&new_xids);
            if (result == 0) {
		new_xids = child_xids;
                xids_destroy(&old_xids);
                *xids_result = new_xids;
            }
	}
    } else if (xid_is_live) {
        // append xid to xids
        result = xids_create_child(old_xids, &new_xids, xid);
        if (result == 0) {
            xids_destroy(&old_xids);
            *xids_result = new_xids;
        }
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
indexer_lock_key(DB_INDEXER *indexer, DB *hotdb, DBT *key, TXNID outermost_live_xid) {
    int result = 0;
    // TEST
    if (indexer->i->test_lock_key) {
        result = indexer->i->test_lock_key(indexer, outermost_live_xid, hotdb, key);
    } else {
        DB_ENV *env = indexer->i->env;
        TOKUTXN txn = NULL;
        result = toku_txnid2txn(env->i->logger, outermost_live_xid, &txn);
        invariant(result == 0 && txn != NULL);
        result = toku_grab_write_lock(hotdb, key, txn);
    }
    return result;
}

static int 
indexer_find_prev_committed(DB_INDEXER *UU(indexer), ULEHANDLE ule, int i) {
    invariant(i < ule_num_uxrs(ule));
    int previ = i - 1;
    return previ;
}

static int
indexer_find_prev_provisional(DB_INDEXER *UU(indexer), ULEHANDLE ule, int i) {
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

static int
indexer_is_previous_insert_key_valid(DB_INDEXER *indexer) {
    return indexer->i->previous_insert_key.size > 0;
}
