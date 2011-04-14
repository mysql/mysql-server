/* -*- mode: C; c-basic-offset: 4 -*- */
/*
 * Copyright (c) 2010-2011 Tokutek Inc.  All rights reserved.
 * The technology is licensed by the Massachusetts Institute of Technology, 
 * Rutgers State University of New Jersey, and the Research Foundation of 
 * State University of New York at Stony Brook under United States of America 
 * Serial No. 11/760379 and to the patents and/or patent applications resulting from it.
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

// initialize the commit keys
static void
indexer_commit_keys_init(struct indexer_commit_keys *keys) {
    keys->max_keys = keys->current_keys = 0;
    keys->keys = NULL;
}

// destroy the commit keys
static void
indexer_commit_keys_destroy(struct indexer_commit_keys *keys) {
    for (int i = 0; i < keys->max_keys; i++)
        toku_destroy_dbt(&keys->keys[i]);
    toku_free(keys->keys);
}

// return the number of keys in the ordered set
static int
indexer_commit_keys_valid(struct indexer_commit_keys *keys) {
    return keys->current_keys;
}

// add a key to the commit keys
static void
indexer_commit_keys_add(struct indexer_commit_keys *keys, size_t length, void *ptr) {
    if (keys->current_keys >= keys->max_keys) {
        int new_max_keys = keys->max_keys == 0 ? 256 : keys->max_keys * 2;
        keys->keys = (DBT *) toku_realloc(keys->keys, new_max_keys * sizeof (DBT));
        resource_assert(keys->keys);
        for (int i = keys->current_keys; i < new_max_keys; i++)
            toku_init_dbt_flags(&keys->keys[i], DB_DBT_REALLOC);
        keys->max_keys = new_max_keys;
    }
    DBT *key = &keys->keys[keys->current_keys];
    toku_dbt_set(length, ptr, key, NULL);
    keys->current_keys++;
}

// set the ordered set to empty
static void
indexer_commit_keys_set_empty(struct indexer_commit_keys *keys) {
    keys->current_keys = 0;
}

// internal functions
static int indexer_set_xid(DB_INDEXER *indexer, TXNID xid, XIDS *xids_result);
static int indexer_append_xid(DB_INDEXER *indexer, TXNID xid, XIDS *xids_result);

static BOOL indexer_find_prev_xr(DB_INDEXER *indexer, ULEHANDLE ule, uint64_t xrindex, uint64_t *prev_xrindex);

static int indexer_generate_hot_key_val(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule, UXRHANDLE uxr, DBT *hotkey, DBT *hotval);
static int indexer_brt_delete_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
static int indexer_brt_delete_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
static int indexer_brt_insert_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids);
static int indexer_brt_insert_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids);
static int indexer_brt_commit(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
static int indexer_lock_key(DB_INDEXER *indexer, DB *hotdb, DBT *key, TXNID outermost_live_xid);
static TOKUTXN_STATE indexer_xid_state(DB_INDEXER *indexer, TXNID xid);


// initialize undo globals located in the indexer private object
void
indexer_undo_do_init(DB_INDEXER *indexer) {
    indexer_commit_keys_init(&indexer->i->commit_keys);
    toku_init_dbt_flags(&indexer->i->hotkey, DB_DBT_REALLOC);
    toku_init_dbt_flags(&indexer->i->hotval, DB_DBT_REALLOC);
}

// destroy the undo globals
void
indexer_undo_do_destroy(DB_INDEXER *indexer) {
    indexer_commit_keys_destroy(&indexer->i->commit_keys);
    toku_destroy_dbt(&indexer->i->hotkey);
    toku_destroy_dbt(&indexer->i->hotval);
}

static int
indexer_undo_do_committed(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule) {
    int result = 0;

    // init the xids to the root xid
    XIDS xids = xids_get_root_xids();

    // scan the committed stack from bottom to top
    uint32_t num_committed = ule_get_num_committed(ule);
    for (uint64_t xrindex = 0; xrindex < num_committed; xrindex++) {

        indexer_commit_keys_set_empty(&indexer->i->commit_keys);

        // get the transaction record
        UXRHANDLE uxr = ule_get_uxr(ule, xrindex);

        // setup up the xids
        TXNID this_xid = uxr_get_txnid(uxr);
        result = indexer_set_xid(indexer, this_xid, &xids);
        if (result != 0)
            break;

        // placeholders in the committed stack are not allowed
        if (uxr_is_placeholder(uxr))
            invariant(0);

        // undo
        if (xrindex > 0) {
            uint64_t prev_xrindex = xrindex - 1;
            UXRHANDLE prevuxr = ule_get_uxr(ule, prev_xrindex);
            if (uxr_is_delete(prevuxr)) {
                ; // do nothing
            } else if (uxr_is_insert(prevuxr)) {
                // generate the hot delete key
                result = indexer_generate_hot_key_val(indexer, hotdb, ule, prevuxr, &indexer->i->hotkey, NULL);
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
            // generate the hot insert key and val
            result = indexer_generate_hot_key_val(indexer, hotdb, ule, uxr, &indexer->i->hotkey, &indexer->i->hotval);
            if (result == 0) {
                // send the insert message
                result = indexer_brt_insert_committed(indexer, hotdb, &indexer->i->hotkey, &indexer->i->hotval, xids);
                if (result == 0)
                    indexer_commit_keys_add(&indexer->i->commit_keys, indexer->i->hotkey.size, indexer->i->hotkey.data);
            }
        } else
            invariant(0);

        // send commit messages if needed
        for (int i = 0; result == 0 && i < indexer_commit_keys_valid(&indexer->i->commit_keys); i++) 
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
    TOKUTXN_STATE outermost_xid_state = TOKUTXN_RETIRED;

    // scan the provisional stack from the outermost to the innermost transaction record
    uint32_t num_committed = ule_get_num_committed(ule);
    uint32_t num_provisional = ule_get_num_provisional(ule);
    for (uint64_t xrindex = num_committed; xrindex < num_committed + num_provisional; xrindex++) {

        // get the ith transaction record
        UXRHANDLE uxr = ule_get_uxr(ule, xrindex);

        TXNID this_xid = uxr_get_txnid(uxr);
        TOKUTXN_STATE this_xid_state = indexer_xid_state(indexer, this_xid);

        if (this_xid_state == TOKUTXN_ABORTING)
            break;         // nothing to do once we reach a transaction that is aborting

        if (xrindex == num_committed) { // if this is the outermost xr
            outermost_xid = this_xid;
            outermost_xid_state = this_xid_state;
            result = indexer_set_xid(indexer, this_xid, &xids);    // always add the outermost xid to the XIDS list
        } else if (this_xid_state == TOKUTXN_LIVE)
            result = indexer_append_xid(indexer, this_xid, &xids); // append a live xid to the XIDS list
        if (result != 0)
            break;

        if (outermost_xid_state != TOKUTXN_LIVE && xrindex > num_committed)
            invariant(this_xid_state == TOKUTXN_RETIRED);

        if (uxr_is_placeholder(uxr))
            continue;         // skip placeholders

        // undo
        uint64_t prev_xrindex;
        BOOL prev_xrindex_found = indexer_find_prev_xr(indexer, ule, xrindex, &prev_xrindex);
        if (prev_xrindex_found) {
            UXRHANDLE prevuxr = ule_get_uxr(ule, prev_xrindex);
            if (uxr_is_delete(prevuxr)) {
                ; // do nothing
            } else if (uxr_is_insert(prevuxr)) {
                // generate the hot delete key
                result = indexer_generate_hot_key_val(indexer, hotdb, ule, prevuxr, &indexer->i->hotkey, NULL);
                if (result == 0) {
                    // send the delete message
                    if (outermost_xid_state == TOKUTXN_LIVE) {
                        invariant(this_xid_state != TOKUTXN_ABORTING);
                        result = indexer_brt_delete_provisional(indexer, hotdb, &indexer->i->hotkey, xids);
                        if (result == 0)
                            result = indexer_lock_key(indexer, hotdb, &indexer->i->hotkey, outermost_xid);
                    } else {
                        invariant(outermost_xid_state == TOKUTXN_RETIRED || outermost_xid_state == TOKUTXN_COMMITTING);
                        result = indexer_brt_delete_committed(indexer, hotdb, &indexer->i->hotkey, xids);
                        if (result == 0)
                            indexer_commit_keys_add(&indexer->i->commit_keys, indexer->i->hotkey.size, indexer->i->hotkey.data);
                    }
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
            // generate the hot insert key and val
            result = indexer_generate_hot_key_val(indexer, hotdb, ule, uxr, &indexer->i->hotkey, &indexer->i->hotval);
            if (result == 0) {
                // send the insert message
                if (outermost_xid_state == TOKUTXN_LIVE) {
                    invariant(this_xid_state != TOKUTXN_ABORTING);
                    result = indexer_brt_insert_provisional(indexer, hotdb, &indexer->i->hotkey, &indexer->i->hotval, xids);
                    if (result == 0) 
                        result = indexer_lock_key(indexer, hotdb, &indexer->i->hotkey, outermost_xid);
                } else {
                    invariant(outermost_xid_state == TOKUTXN_RETIRED || outermost_xid_state == TOKUTXN_COMMITTING);
                    result = indexer_brt_insert_committed(indexer, hotdb, &indexer->i->hotkey, &indexer->i->hotval, xids);
#if 0
                    // no need to do this because we do implicit commits on inserts
                    if (result == 0)
                        indexer_commit_keys_add(&indexer->i->commit_keys, indexer->i->hotkey.size, indexer->i->hotkey.data);
#endif
                }
            }
        } else
            invariant(0);

        if (result != 0)
            break;
    }

    // send commits if the outermost provisional transaction is committed
    for (int i = 0; result == 0 && i < indexer_commit_keys_valid(&indexer->i->commit_keys); i++) 
        result = indexer_brt_commit(indexer, hotdb, &indexer->i->commit_keys.keys[i], xids);

    xids_destroy(&xids);

    return result;
}

int
indexer_undo_do(DB_INDEXER *indexer, DB *hotdb, ULEHANDLE ule) {
    int result = indexer_undo_do_committed(indexer, hotdb, ule);
    if (result == 0)
        result = indexer_undo_do_provisional(indexer, hotdb, ule);
        
    if ( indexer->i->test_only_flags == INDEXER_TEST_ONLY_ERROR_CALLBACK ) 
        result = EINVAL;

    return result;
}

// set xids_result = [root_xid, this_xid]
// Note that this could be sped up by adding a new xids constructor that constructs the stack with
// exactly one xid.
static int
indexer_set_xid(DB_INDEXER *UU(indexer), TXNID this_xid, XIDS *xids_result) {
    int result = 0;
    XIDS old_xids = *xids_result;
    XIDS new_xids = xids_get_root_xids();
    if (this_xid != TXNID_NONE) {
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

// append xid to xids_result
static int
indexer_append_xid(DB_INDEXER *UU(indexer), TXNID xid, XIDS *xids_result) {
    XIDS old_xids = *xids_result;
    XIDS new_xids;
    int result = xids_create_child(old_xids, &new_xids, xid);
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

// return the state of a transaction given a transaction id.  if the transaction no longer exists, 
// then return TOKUTXN_RETIRED.
static TOKUTXN_STATE
indexer_xid_state(DB_INDEXER *indexer, TXNID xid) {
    TOKUTXN_STATE result = TOKUTXN_RETIRED;
    // TEST
    if (indexer->i->test_xid_state) {
        int r = indexer->i->test_xid_state(indexer, xid);
        switch (r) {
        case 0: result = TOKUTXN_LIVE; break;
        case 1: result = TOKUTXN_COMMITTING; break;
        case 2: result = TOKUTXN_ABORTING; break;
        case 3: result = TOKUTXN_RETIRED; break;
        default: assert(0); break;
        }
    } else {
        DB_ENV *env = indexer->i->env;
        TOKUTXN txn = NULL;
        int r = toku_txnid2txn(env->i->logger, xid, &txn);
        invariant(r == 0);
        if (txn) 
            result = toku_txn_get_state(txn);
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

// find the index of a non-placeholder transaction record that is previous to the transaction record
// found at xrindex.  return TRUE if one is found and return its index in prev_xrindex.  otherwise,
// return FALSE.
static BOOL
indexer_find_prev_xr(DB_INDEXER *UU(indexer), ULEHANDLE ule, uint64_t xrindex, uint64_t *prev_xrindex) {
    invariant(xrindex < ule_num_uxrs(ule));
    BOOL  prev_found = FALSE;
    while (xrindex > 0) {
        xrindex -= 1;
        UXRHANDLE uxr = ule_get_uxr(ule, xrindex);
        if (!uxr_is_placeholder(uxr)) {
            *prev_xrindex = xrindex;
            prev_found = TRUE;
            break; 
        }
    }
    return prev_found;
}

// get the innermost live txn from the xids stack.  the xid on the top of the xids stack must be live
// when calling this function.  the indexer_append_xid only appends live xid's onto the stack.
static TOKUTXN
indexer_get_innermost_live_txn(DB_INDEXER *indexer, XIDS xids) {
    DB_ENV *env = indexer->i->env;
    uint8_t num_xids = xids_get_num_xids(xids);
    TXNID xid = xids_get_xid(xids, (u_int8_t)(num_xids-1));
    TOKUTXN txn = NULL;
    int result = toku_txnid2txn(env->i->logger, xid, &txn);
    invariant(result == 0);
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
