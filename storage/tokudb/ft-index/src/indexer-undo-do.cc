/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

#include <toku_portability.h>
#include <toku_assert.h>

#include <stdio.h>
#include <string.h>

#include <ft/le-cursor.h>
#include <ft/tokuconst.h>
#include <ft/ft-ops.h>
#include <ft/leafentry.h>
#include <ft/ule.h>
#include <ft/xids.h>
#include <ft/txn_manager.h>
#include <ft/checkpoint.h>

#include "ydb-internal.h"
#include "ydb_row_lock.h"
#include "indexer.h"
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
        keys->keys = (DBT *) toku_xrealloc(keys->keys, new_max_keys * sizeof (DBT));
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

static bool indexer_find_prev_xr(DB_INDEXER *indexer, ULEHANDLE ule, uint64_t xrindex, uint64_t *prev_xrindex);

static int indexer_generate_hot_keys_vals(DB_INDEXER *indexer, DB *hotdb, struct ule_prov_info* prov_info, UXRHANDLE uxr, DBT_ARRAY *hotkeys, DBT_ARRAY *hotvals);
static int indexer_ft_delete_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids, TOKUTXN txn);
static int indexer_ft_delete_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
static int indexer_ft_insert_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids, TOKUTXN txn);
static int indexer_ft_insert_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids);
static int indexer_ft_commit(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids);
static void indexer_lock_key(DB_INDEXER *indexer, DB *hotdb, DBT *key, TXNID outermost_live_xid, TOKUTXN txn);


// initialize undo globals located in the indexer private object
void
indexer_undo_do_init(DB_INDEXER *indexer) {
    indexer_commit_keys_init(&indexer->i->commit_keys);
    XMALLOC_N(indexer->i->N, indexer->i->hot_keys);
    XMALLOC_N(indexer->i->N, indexer->i->hot_vals);
    for (int which = 0; which < indexer->i->N; which++) {
        toku_dbt_array_init(&indexer->i->hot_keys[which], 1);
        toku_dbt_array_init(&indexer->i->hot_vals[which], 1);
    }
}

// destroy the undo globals
void
indexer_undo_do_destroy(DB_INDEXER *indexer) {
    indexer_commit_keys_destroy(&indexer->i->commit_keys);
    if (indexer->i->hot_keys) {
        invariant(indexer->i->hot_vals);
        for (int which = 0; which < indexer->i->N; which++) {
            toku_dbt_array_destroy(&indexer->i->hot_keys[which]);
            toku_dbt_array_destroy(&indexer->i->hot_vals[which]);
        }
        toku_free(indexer->i->hot_keys);
        toku_free(indexer->i->hot_vals);
    }
}

static int
indexer_undo_do_committed(DB_INDEXER *indexer, DB *hotdb, struct ule_prov_info *prov_info, DBT_ARRAY *hot_keys, DBT_ARRAY *hot_vals) {
    int result = 0;
    ULEHANDLE ule = prov_info->ule;

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
        invariant(!uxr_is_placeholder(uxr));

        // undo
        if (xrindex > 0) {
            uint64_t prev_xrindex = xrindex - 1;
            UXRHANDLE prevuxr = ule_get_uxr(ule, prev_xrindex);
            if (uxr_is_delete(prevuxr)) {
                ; // do nothing
            } else if (uxr_is_insert(prevuxr)) {
                // generate the hot delete key
                result = indexer_generate_hot_keys_vals(indexer, hotdb, prov_info, prevuxr, hot_keys, NULL);
                if (result == 0) {
                    paranoid_invariant(hot_keys->size <= hot_keys->capacity);
                    for (uint32_t i = 0; i < hot_keys->size; i++) {
                        DBT *hotkey = &hot_keys->dbts[i];

                        // send the delete message
                        result = indexer_ft_delete_committed(indexer, hotdb, hotkey, xids);
                        if (result == 0) {
                            indexer_commit_keys_add(&indexer->i->commit_keys, hotkey->size, hotkey->data);
                        }
                    }
                }
            } else {
                assert(0);
            }
        }
        if (result != 0) {
            break;
        }

        // do
        if (uxr_is_delete(uxr)) {
            ; // do nothing
        } else if (uxr_is_insert(uxr)) {
            // generate the hot insert key and val
            result = indexer_generate_hot_keys_vals(indexer, hotdb, prov_info, uxr, hot_keys, hot_vals);
            if (result == 0) {
                paranoid_invariant(hot_keys->size == hot_vals->size);
                paranoid_invariant(hot_keys->size <= hot_keys->capacity);
                paranoid_invariant(hot_vals->size <= hot_vals->capacity);
                for (uint32_t i = 0; i < hot_keys->size; i++) {
                    DBT *hotkey = &hot_keys->dbts[i];
                    DBT *hotval = &hot_vals->dbts[i];

                    // send the insert message
                    result = indexer_ft_insert_committed(indexer, hotdb, hotkey, hotval, xids);
                    if (result == 0) {
                        indexer_commit_keys_add(&indexer->i->commit_keys, hotkey->size, hotkey->data);
                    }
                }
            }
        } else
            assert(0);

        // send commit messages if needed
        for (int i = 0; result == 0 && i < indexer_commit_keys_valid(&indexer->i->commit_keys); i++) 
            result = indexer_ft_commit(indexer, hotdb, &indexer->i->commit_keys.keys[i], xids);

        if (result != 0)
            break;
    }

    xids_destroy(&xids);

    return result;
}

static void release_txns(
    ULEHANDLE ule,
    TOKUTXN_STATE* prov_states,
    TOKUTXN* prov_txns,
    DB_INDEXER *indexer
    )
{
    uint32_t num_provisional = ule_get_num_provisional(ule);
    if (indexer->i->test_xid_state) {
        goto exit;
    }
    for (uint32_t i = 0; i < num_provisional; i++) {
        if (prov_states[i] == TOKUTXN_LIVE || prov_states[i] == TOKUTXN_PREPARING) {
            toku_txn_unpin_live_txn(prov_txns[i]);
        }
    }
exit:
    return;
}

static int
indexer_undo_do_provisional(DB_INDEXER *indexer, DB *hotdb, struct ule_prov_info *prov_info, DBT_ARRAY *hot_keys, DBT_ARRAY *hot_vals) {
    int result = 0;
    indexer_commit_keys_set_empty(&indexer->i->commit_keys);
    ULEHANDLE ule = prov_info->ule;

    // init the xids to the root xid
    XIDS xids = xids_get_root_xids();

    uint32_t num_provisional = prov_info->num_provisional;
    uint32_t num_committed = prov_info->num_committed;
    TXNID *prov_ids = prov_info->prov_ids;
    TOKUTXN *prov_txns = prov_info->prov_txns;
    TOKUTXN_STATE *prov_states = prov_info->prov_states;

    // nothing to do if there's nothing provisional
    if (num_provisional == 0) {
        goto exit;
    }

    TXNID outermost_xid_state;
    outermost_xid_state = prov_states[0];
    
    // scan the provisional stack from the outermost to the innermost transaction record
    TOKUTXN curr_txn;
    curr_txn = NULL;
    for (uint64_t xrindex = num_committed; xrindex < num_committed + num_provisional; xrindex++) {

        // get the ith transaction record
        UXRHANDLE uxr = ule_get_uxr(ule, xrindex);

        TXNID this_xid = uxr_get_txnid(uxr);
        TOKUTXN_STATE this_xid_state = prov_states[xrindex - num_committed];

        if (this_xid_state == TOKUTXN_ABORTING) {
            break;         // nothing to do once we reach a transaction that is aborting
        }
        
        if (xrindex == num_committed) { // if this is the outermost xr
            result = indexer_set_xid(indexer, this_xid, &xids);    // always add the outermost xid to the XIDS list
            curr_txn = prov_txns[xrindex - num_committed];
        } else {
            switch (this_xid_state) {
            case TOKUTXN_LIVE:
                result = indexer_append_xid(indexer, this_xid, &xids); // append a live xid to the XIDS list
                curr_txn = prov_txns[xrindex - num_committed];
                if (!indexer->i->test_xid_state) {
                    assert(curr_txn);
                }
                break;
            case TOKUTXN_PREPARING:
                assert(0); // not allowed
            case TOKUTXN_COMMITTING:
            case TOKUTXN_ABORTING:
            case TOKUTXN_RETIRED:
                break; // nothing to do
            }
        }
        if (result != 0)
            break;

        if (outermost_xid_state != TOKUTXN_LIVE && xrindex > num_committed) {
            // if the outermost is not live, then the inner state must be retired.  thats the way that the txn API works.
            assert(this_xid_state == TOKUTXN_RETIRED);
        }

        if (uxr_is_placeholder(uxr)) {
            continue;         // skip placeholders
        }
        // undo
        uint64_t prev_xrindex;
        bool prev_xrindex_found = indexer_find_prev_xr(indexer, ule, xrindex, &prev_xrindex);
        if (prev_xrindex_found) {
            UXRHANDLE prevuxr = ule_get_uxr(ule, prev_xrindex);
            if (uxr_is_delete(prevuxr)) {
                ; // do nothing
            } else if (uxr_is_insert(prevuxr)) {
                // generate the hot delete key
                result = indexer_generate_hot_keys_vals(indexer, hotdb, prov_info, prevuxr, hot_keys, NULL);
                if (result == 0) {
                    paranoid_invariant(hot_keys->size <= hot_keys->capacity);
                    for (uint32_t i = 0; i < hot_keys->size; i++) {
                        DBT *hotkey = &hot_keys->dbts[i];

                        // send the delete message
                        switch (outermost_xid_state) {
                        case TOKUTXN_LIVE:
                        case TOKUTXN_PREPARING:
                            invariant(this_xid_state != TOKUTXN_ABORTING);
                            invariant(!curr_txn || toku_txn_get_state(curr_txn) == TOKUTXN_LIVE || toku_txn_get_state(curr_txn) == TOKUTXN_PREPARING);
                            result = indexer_ft_delete_provisional(indexer, hotdb, hotkey, xids, curr_txn);
                            if (result == 0) {
                                indexer_lock_key(indexer, hotdb, hotkey, prov_ids[0], curr_txn);
                            }
                            break;
                        case TOKUTXN_COMMITTING:
                        case TOKUTXN_RETIRED:
                            result = indexer_ft_delete_committed(indexer, hotdb, hotkey, xids);
                            if (result == 0)
                                indexer_commit_keys_add(&indexer->i->commit_keys, hotkey->size, hotkey->data);
                            break;
                        case TOKUTXN_ABORTING: // can not happen since we stop processing the leaf entry if the outer most xr is aborting
                            assert(0);
                        }
                    }
                }
            } else
                assert(0);
        }
        if (result != 0)
            break;

        // do
        if (uxr_is_delete(uxr)) {
            ; // do nothing
        } else if (uxr_is_insert(uxr)) {
            // generate the hot insert key and val
            result = indexer_generate_hot_keys_vals(indexer, hotdb, prov_info, uxr, hot_keys, hot_vals);
            if (result == 0) {
                paranoid_invariant(hot_keys->size == hot_vals->size);
                paranoid_invariant(hot_keys->size <= hot_keys->capacity);
                paranoid_invariant(hot_vals->size <= hot_vals->capacity);
                for (uint32_t i = 0; i < hot_keys->size; i++) {
                    DBT *hotkey = &hot_keys->dbts[i];
                    DBT *hotval = &hot_vals->dbts[i];

                    // send the insert message
                    switch (outermost_xid_state) {
                    case TOKUTXN_LIVE:
                    case TOKUTXN_PREPARING:
                        assert(this_xid_state != TOKUTXN_ABORTING);
                        invariant(!curr_txn || toku_txn_get_state(curr_txn) == TOKUTXN_LIVE || toku_txn_get_state(curr_txn) == TOKUTXN_PREPARING);
                        result = indexer_ft_insert_provisional(indexer, hotdb, hotkey, hotval, xids, curr_txn);
                        if (result == 0) {
                            indexer_lock_key(indexer, hotdb, hotkey, prov_ids[0], prov_txns[0]);
                        }
                        break;
                    case TOKUTXN_COMMITTING:
                    case TOKUTXN_RETIRED:
                        result = indexer_ft_insert_committed(indexer, hotdb, hotkey, hotval, xids);
                        // no need to do this because we do implicit commits on inserts
                        if (0 && result == 0)
                            indexer_commit_keys_add(&indexer->i->commit_keys, hotkey->size, hotkey->data);
                        break;
                    case TOKUTXN_ABORTING: // can not happen since we stop processing the leaf entry if the outer most xr is aborting
                        assert(0);
                    }
                }
            }
        } else
            assert(0);

        if (result != 0)
            break;
    }

    // send commits if the outermost provisional transaction is committed
    for (int i = 0; result == 0 && i < indexer_commit_keys_valid(&indexer->i->commit_keys); i++) {
        result = indexer_ft_commit(indexer, hotdb, &indexer->i->commit_keys.keys[i], xids);
    }

    // be careful with this in the future. Right now, only exit path
    // is BEFORE we call fill_prov_info, so this happens before exit
    // If in the future we add a way to exit after fill_prov_info,
    // then this will need to be handled below exit
    release_txns(ule, prov_states, prov_txns, indexer);
exit:
    xids_destroy(&xids);
    return result;
}

int
indexer_undo_do(DB_INDEXER *indexer, DB *hotdb, struct ule_prov_info *prov_info, DBT_ARRAY *hot_keys, DBT_ARRAY *hot_vals) {
    int result = indexer_undo_do_committed(indexer, hotdb, prov_info, hot_keys, hot_vals);
    if (result == 0) {
        result = indexer_undo_do_provisional(indexer, hotdb, prov_info, hot_keys, hot_vals);
    }
    if (indexer->i->test_only_flags == INDEXER_TEST_ONLY_ERROR_CALLBACK)  {
        result = EINVAL;
    }

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
indexer_generate_hot_keys_vals(DB_INDEXER *indexer, DB *hotdb, struct ule_prov_info *prov_info, UXRHANDLE uxr, DBT_ARRAY *hotkeys, DBT_ARRAY *hotvals) {
    int result = 0;

    // setup the source key
    DBT srckey;
    toku_fill_dbt(&srckey, prov_info->key, prov_info->keylen);

    // setup the source val
    DBT srcval;
    toku_fill_dbt(&srcval, uxr_get_val(uxr), uxr_get_vallen(uxr));

    // generate the secondary row
    DB_ENV *env = indexer->i->env;
    if (hotvals) {
        result = env->i->generate_row_for_put(hotdb, indexer->i->src_db, hotkeys, hotvals, &srckey, &srcval);
    }
    else {
        result = env->i->generate_row_for_del(hotdb, indexer->i->src_db, hotkeys, &srckey, &srcval);
    }
    toku_destroy_dbt(&srckey);
    toku_destroy_dbt(&srcval);

    return result;
}

// Take a write lock on the given key for the outermost xid in the xids list.
static void 
indexer_lock_key(DB_INDEXER *indexer, DB *hotdb, DBT *key, TXNID outermost_live_xid, TOKUTXN txn) {
    // TEST
    if (indexer->i->test_lock_key) {
        indexer->i->test_lock_key(indexer, outermost_live_xid, hotdb, key);
    } else {
        toku_db_grab_write_lock(hotdb, key, txn);
    }
}

// find the index of a non-placeholder transaction record that is previous to the transaction record
// found at xrindex.  return true if one is found and return its index in prev_xrindex.  otherwise,
// return false.
static bool
indexer_find_prev_xr(DB_INDEXER *UU(indexer), ULEHANDLE ule, uint64_t xrindex, uint64_t *prev_xrindex) {
    assert(xrindex < ule_num_uxrs(ule));
    bool  prev_found = false;
    while (xrindex > 0) {
        xrindex -= 1;
        UXRHANDLE uxr = ule_get_uxr(ule, xrindex);
        if (!uxr_is_placeholder(uxr)) {
            *prev_xrindex = xrindex;
            prev_found = true;
            break; 
        }
    }
    return prev_found;
}

// inject "delete" message into brt with logging in recovery and rollback logs,
// and making assocation between txn and brt
static int 
indexer_ft_delete_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids, TOKUTXN txn) {
    int result = 0;
    // TEST
    if (indexer->i->test_delete_provisional) {
        result = indexer->i->test_delete_provisional(indexer, hotdb, hotkey, xids);
    } else {
        result = toku_ydb_check_avail_fs_space(indexer->i->env);
        if (result == 0) {
            assert(txn != NULL);
            // Not sure if this is really necessary, as
            // the hot index DB should have to be checkpointed
            // upon commit of the hot index transaction, but
            // it is safe to do this
            // this question apples to delete_committed, insert_provisional
            // and insert_committed
            toku_ft_maybe_delete (hotdb->i->ft_handle, hotkey, txn, false, ZERO_LSN, true);
        }
    }
    return result;	
}

// send a delete message into the tree without rollback or recovery logging
static int 
indexer_ft_delete_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids) {
    int result = 0;
    // TEST
    if (indexer->i->test_delete_committed) {
        result = indexer->i->test_delete_committed(indexer, hotdb, hotkey, xids);
    } else {
        result = toku_ydb_check_avail_fs_space(indexer->i->env);
        if (result == 0) {
            toku_ft_send_delete(db_struct_i(hotdb)->ft_handle, hotkey, xids, TXNID_NONE, make_gc_info(true));
        }
    }
    return result;
}

// inject "insert" message into brt with logging in recovery and rollback logs,
// and making assocation between txn and brt
static int 
indexer_ft_insert_provisional(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids, TOKUTXN txn) {
    int result = 0;
    // TEST
    if (indexer->i->test_insert_provisional) {
        result = indexer->i->test_insert_provisional(indexer, hotdb, hotkey, hotval, xids);
    } else {
        result = toku_ydb_check_avail_fs_space(indexer->i->env);
        if (result == 0) {
            assert(txn != NULL);
            // comment/question in indexer_ft_delete_provisional applies
            toku_ft_maybe_insert (hotdb->i->ft_handle, hotkey, hotval, txn, false, ZERO_LSN, true, FT_INSERT);
        }
    }
    return result;
}

// send an insert message into the tree without rollback or recovery logging
// and without associating the txn and the brt
static int 
indexer_ft_insert_committed(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, DBT *hotval, XIDS xids) {
    int result = 0;
    // TEST
    if (indexer->i->test_insert_committed) {
        result = indexer->i->test_insert_committed(indexer, hotdb, hotkey, hotval, xids);
    } else {
        result = toku_ydb_check_avail_fs_space(indexer->i->env);
        if (result == 0) {
            toku_ft_send_insert(db_struct_i(hotdb)->ft_handle, hotkey, hotval, xids, FT_INSERT, TXNID_NONE, make_gc_info(true));
        }
    }
    return result;
}

// send a commit message into the tree
// Note: If the xid is zero, then the leafentry will already have a committed transaction
//       record and no commit message is needed.  (A commit message with xid of zero is
//       illegal anyway.)
static int 
indexer_ft_commit(DB_INDEXER *indexer, DB *hotdb, DBT *hotkey, XIDS xids) {
    int result = 0;
    if (xids_get_num_xids(xids) > 0) {// send commit only when not the root xid
        // TEST
        if (indexer->i->test_commit_any) {
            result = indexer->i->test_commit_any(indexer, hotdb, hotkey, xids);
        } else {
            result = toku_ydb_check_avail_fs_space(indexer->i->env);
            if (result == 0)
                toku_ft_send_commit_any(db_struct_i(hotdb)->ft_handle, hotkey, xids, TXNID_NONE, make_gc_info(true));
        }
    }
    return result;
}
