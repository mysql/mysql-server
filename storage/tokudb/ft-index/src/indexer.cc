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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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

/*
 *   The indexer
 */
#include <config.h>

#include <stdio.h>
#include <string.h>
#include <toku_portability.h>
#include "toku_assert.h"
#include "ydb-internal.h"
#include <ft/le-cursor.h>
#include "indexer.h"
#include <ft/ft-ops.h>
#include <ft/leafentry.h>
#include <ft/ule.h>
#include <ft/txn/xids.h>
#include <ft/logger/log-internal.h>
#include <ft/cachetable/checkpoint.h>
#include <portability/toku_atomic.h>
#include "loader.h"
#include <util/status.h>

///////////////////////////////////////////////////////////////////////////////////
// Engine status
//
// Status is intended for display to humans to help understand system behavior.
// It does not need to be perfectly thread-safe.

static INDEXER_STATUS_S indexer_status;

#define STATUS_INIT(k,c,t,l,inc) TOKUFT_STATUS_INIT(indexer_status, k, c, t, "indexer: " l, inc)

static void
status_init(void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    STATUS_INIT(INDEXER_CREATE,      nullptr, UINT64, "number of indexers successfully created", TOKU_ENGINE_STATUS);
    STATUS_INIT(INDEXER_CREATE_FAIL, nullptr, UINT64, "number of calls to toku_indexer_create_indexer() that failed", TOKU_ENGINE_STATUS);
    STATUS_INIT(INDEXER_BUILD,       nullptr, UINT64, "number of calls to indexer->build() succeeded", TOKU_ENGINE_STATUS);
    STATUS_INIT(INDEXER_BUILD_FAIL,  nullptr, UINT64, "number of calls to indexer->build() failed", TOKU_ENGINE_STATUS);
    STATUS_INIT(INDEXER_CLOSE,       nullptr, UINT64, "number of calls to indexer->close() that succeeded", TOKU_ENGINE_STATUS);
    STATUS_INIT(INDEXER_CLOSE_FAIL,  nullptr, UINT64, "number of calls to indexer->close() that failed", TOKU_ENGINE_STATUS);
    STATUS_INIT(INDEXER_ABORT,       nullptr, UINT64, "number of calls to indexer->abort()", TOKU_ENGINE_STATUS);
    STATUS_INIT(INDEXER_CURRENT,     nullptr, UINT64, "number of indexers currently in existence", TOKU_ENGINE_STATUS);
    STATUS_INIT(INDEXER_MAX,         nullptr, UINT64, "max number of indexers that ever existed simultaneously", TOKU_ENGINE_STATUS);
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
        toku_mutex_destroy(&indexer->i->indexer_estimate_lock);
        toku_destroy_dbt(&indexer->i->position_estimate);
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

// a shortcut call
//
// a cheap(er) call to see if a key must be inserted
// into the DB. If true, then we know we have to insert.
// If false, then we don't know, and have to check again
// after grabbing the indexer lock
bool
toku_indexer_may_insert(DB_INDEXER* indexer, const DBT* key) {
    bool may_insert = false;
    toku_mutex_lock(&indexer->i->indexer_estimate_lock);

    // if we have no position estimate, we can't tell, so return false
    if (indexer->i->position_estimate.data == nullptr) {
        may_insert = false;
    } else {
        DB *db = indexer->i->src_db;
        const toku::comparator &cmp = toku_ft_get_comparator(db->i->ft_handle);
        int c = cmp(&indexer->i->position_estimate, key);

        // if key > position_estimate, then we know the indexer cursor
        // is past key, and we can safely say that associated values of 
        // key must be inserted into the indexer's db
        may_insert = c < 0;
    }

    toku_mutex_unlock(&indexer->i->indexer_estimate_lock);
    return may_insert;
}

void
toku_indexer_update_estimate(DB_INDEXER* indexer) {
    toku_mutex_lock(&indexer->i->indexer_estimate_lock);
    toku_le_cursor_update_estimate(indexer->i->lec, &indexer->i->position_estimate);
    toku_mutex_unlock(&indexer->i->indexer_estimate_lock);
}

// forward declare the test-only wrapper function for undo-do
static int test_indexer_undo_do(DB_INDEXER *indexer, DB *hotdb, DBT* key, ULEHANDLE ule);

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
    int rval;
    DB_INDEXER *indexer = 0;   // set later when created
    HANDLE_READ_ONLY_TXN(txn);

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
    toku_mutex_init(&indexer->i->indexer_estimate_lock, NULL);
    toku_init_dbt(&indexer->i->position_estimate);

    //
    // create and close a dummy loader to get redirection going for the hot indexer
    // This way, if the hot index aborts, but other transactions have references to the
    // underlying FT, then those transactions can do dummy operations on the FT
    // while the DB gets redirected back to an empty dictionary
    //
    {
        DB_LOADER* loader = NULL;
        rval = toku_loader_create_loader(env, txn, &loader, NULL, N, &dest_dbs[0], NULL, NULL, DB_PRELOCKED_WRITE | LOADER_DISALLOW_PUTS, true);
        if (rval) {
            goto create_exit;
        }
        rval = loader->close(loader);
        if (rval) {
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
    toku_ft_hot_index(NULL, ttxn, filenums, 1, &hot_index_lsn);
    toku_multi_operation_client_unlock();

    if (rval == 0) {
        rval = associate_indexer_with_hot_dbs(indexer, dest_dbs, N);
    }
create_exit:
    if ( rval == 0 ) {

        indexer_undo_do_init(indexer);

        *indexerp = indexer;

        (void) toku_sync_fetch_and_add(&STATUS_VALUE(INDEXER_CREATE), 1);
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(INDEXER_CURRENT), 1);
        if ( STATUS_VALUE(INDEXER_CURRENT) > STATUS_VALUE(INDEXER_MAX) )
            STATUS_VALUE(INDEXER_MAX) = STATUS_VALUE(INDEXER_CURRENT);   // NOT WORTH A LOCK TO MAKE THREADSAFE), may be inaccurate

    } else {
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(INDEXER_CREATE_FAIL), 1);
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
toku_indexer_should_insert_key(DB_INDEXER *indexer, const DBT *key) {
    // the hot indexer runs from the end to the beginning, it gets the largest keys first
    //
    // if key is less than indexer's position, then we should NOT insert it because
    // the indexer will get to it. If it is greater or equal, that means the indexer
    // has already processed the key, and will not get to it, therefore, we need
    // to handle it
    return toku_le_cursor_is_key_greater_or_equal(indexer->i->lec, key);
}

// initialize provisional info by allocating enough space to hold provisional 
// ids, states, and txns for each of the provisional entries in the ule. the 
// ule and le remain owned by the caller, not this struct.
static void 
ule_prov_info_init(struct ule_prov_info *prov_info, const void* key, uint32_t keylen, LEAFENTRY le, ULEHANDLE ule) {
    prov_info->le = le;
    prov_info->ule = ule;
    prov_info->keylen = keylen;
    prov_info->key = toku_xmalloc(keylen);
    memcpy(prov_info->key, key, keylen);
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

    // handle test case first
    if (indexer->i->test_xid_state) {
        for (uint32_t i = 0; i < num_provisional; i++) {
            UXRHANDLE uxr = ule_get_uxr(ule, num_committed + i);
            prov_ids[i] = uxr_get_txnid(uxr);
            prov_states[i] = indexer->i->test_xid_state(indexer, prov_ids[i]);
            prov_txns[i] = NULL;
        }
        return;
    }
    
    // hold the txn manager lock while we inspect txn state
    // and pin some live txns
    DB_ENV *env = indexer->i->env;
    TXN_MANAGER txn_manager = toku_logger_get_txn_manager(env->i->logger);
    TXNID parent_xid = uxr_get_txnid(ule_get_uxr(ule, num_committed));

    // let's first initialize things to defaults
    for (uint32_t i = 0; i < num_provisional; i++) {
        UXRHANDLE uxr = ule_get_uxr(ule, num_committed + i);
        prov_ids[i] = uxr_get_txnid(uxr);
        prov_txns[i] = NULL;
        prov_states[i] = TOKUTXN_RETIRED;
    }

    toku_txn_manager_suspend(txn_manager); 
    TXNID_PAIR root_xid_pair = {.parent_id64=parent_xid, .child_id64 = TXNID_NONE};
    TOKUTXN root_txn = NULL;
    toku_txn_manager_id2txn_unlocked(
        txn_manager, 
        root_xid_pair, 
        &root_txn
        );
    if (root_txn == NULL) {
        toku_txn_manager_resume(txn_manager);
        return; //everything is retired in this case, the default
    }
    prov_txns[0] = root_txn;
    prov_states[0] = toku_txn_get_state(root_txn);
    toku_txn_lock_state(root_txn);
    prov_states[0] = toku_txn_get_state(root_txn);
    if (prov_states[0] == TOKUTXN_LIVE || prov_states[0] == TOKUTXN_PREPARING) {
        // pin this live txn so it can't commit or abort until we're done with it
        toku_txn_pin_live_txn_unlocked(root_txn);
    }
    toku_txn_unlock_state(root_txn);

    root_txn->child_manager->suspend();
    for (uint32_t i = 1; i < num_provisional; i++) {
        UXRHANDLE uxr = ule_get_uxr(ule, num_committed + i);
        TXNID child_id = uxr_get_txnid(uxr);
        TOKUTXN txn = NULL;

        TXNID_PAIR txnid_pair = {.parent_id64 = parent_xid, .child_id64 = child_id};
        root_txn->child_manager->find_tokutxn_by_xid_unlocked(txnid_pair, &txn);
        prov_txns[i] = txn;
        if (txn) {
            toku_txn_lock_state(txn);
            prov_states[i] = toku_txn_get_state(txn);
            if (prov_states[i] == TOKUTXN_LIVE || prov_states[i] == TOKUTXN_PREPARING) {
                // pin this live txn so it can't commit or abort until we're done with it
                toku_txn_pin_live_txn_unlocked(txn);
            }
            toku_txn_unlock_state(txn);
        }
        else {
            prov_states[i] = TOKUTXN_RETIRED;
        }
    }
    root_txn->child_manager->resume();
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
le_cursor_callback(uint32_t keylen, const void *key, uint32_t UU(vallen), const void *val, void *extra, bool lock_only) {
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
        ule_prov_info_init(prov_info, key, keylen, le, ule);
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
        // grab the multi operation lock because we will be injecting messages
        // grab it here because we must hold it before
        // trying to pin any live transactions, as discovered by #5775
        toku_multi_operation_client_lock();

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
            for (int which_db = 0; (which_db < indexer->i->N) && (result == 0); which_db++) {
                DB *db = indexer->i->dest_dbs[which_db];
                DBT_ARRAY *hot_keys = &indexer->i->hot_keys[which_db];
                DBT_ARRAY *hot_vals = &indexer->i->hot_vals[which_db];
                result = indexer_undo_do(indexer, db, &prov_info, hot_keys, hot_vals);
                if ((result != 0) && (indexer->i->error_callback != NULL)) {
                    // grab the key and call the error callback
                    DBT key; toku_init_dbt_flags(&key, DB_DBT_REALLOC);
                    toku_dbt_set(prov_info.keylen, prov_info.key, &key, NULL);
                    indexer->i->error_callback(db, which_db, result, &key, NULL, indexer->i->error_extra);
                    toku_destroy_dbt(&key);
                }
            }
            // the leafentry and ule are not owned by the prov_info,
            // and are still our responsibility to free
            toku_free(prov_info.le);
            toku_free(prov_info.key);
            toku_ule_free(prov_info.ule);
        }

        toku_multi_operation_client_unlock();
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
        // Perform a checkpoint so that all of the indexing makes it to disk before continuing.
        // Otherwise indexing would not be crash-safe becasue none of the undo-do messages are in the recovery log.
        DB_ENV *env = indexer->i->env;
        CHECKPOINTER cp = toku_cachetable_get_checkpointer(env->i->cachetable);
        toku_checkpoint(cp, env->i->logger, NULL, NULL, NULL, NULL, INDEXER_CHECKPOINT);
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(INDEXER_BUILD), 1);
    } else {
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(INDEXER_BUILD_FAIL), 1);
    }

    return result;
}

// Clients must not operate on any of the hot dbs concurrently with close
static int
close_indexer(DB_INDEXER *indexer) {
    int r = 0;
    (void) toku_sync_fetch_and_sub(&STATUS_VALUE(INDEXER_CURRENT), 1);

    // Disassociate the indexer from the hot db and free_indexer
    disassociate_indexer_from_hot_dbs(indexer);
    free_indexer(indexer);

    if ( r == 0 ) {
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(INDEXER_CLOSE), 1);
    } else {
        (void) toku_sync_fetch_and_add(&STATUS_VALUE(INDEXER_CLOSE_FAIL), 1);
    }
    return r;
}

// Clients must not operate on any of the hot dbs concurrently with abort
static int 
abort_indexer(DB_INDEXER *indexer) {
    (void) toku_sync_fetch_and_sub(&STATUS_VALUE(INDEXER_CURRENT), 1);
    (void) toku_sync_fetch_and_add(&STATUS_VALUE(INDEXER_ABORT), 1);
    // Disassociate the indexer from the hot db and free_indexer
    disassociate_indexer_from_hot_dbs(indexer);
    free_indexer(indexer);
    return 0;
}


// derived from the handlerton's estimate_num_rows()
static int
update_estimated_rows(DB_INDEXER *indexer) {
    int error;
    DB_TXN *txn = NULL;
    DB_ENV *db_env = indexer->i->env;
    error = db_env->txn_begin(db_env, 0, &txn, DB_READ_UNCOMMITTED);
    if (error == 0) {
        DB_BTREE_STAT64 stats;
        DB *db = indexer->i->src_db;
        error = db->stat64(db, txn, &stats);
        if (error == 0) {
            indexer->i->estimated_rows = stats.bt_ndata;
        }
        txn->commit(txn, 0);
    }
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
test_indexer_undo_do(DB_INDEXER *indexer, DB *hotdb, DBT* key, ULEHANDLE ule) {
    int which_db;
    for (which_db = 0; which_db < indexer->i->N; which_db++) {
        if (indexer->i->dest_dbs[which_db] == hotdb) {
            break;
        }
    }
    if (which_db == indexer->i->N) {
        return EINVAL;
    }
    struct ule_prov_info prov_info;
    memset(&prov_info, 0, sizeof(prov_info));
    // pass null for the leafentry - we don't need it, neither does the info
    ule_prov_info_init(&prov_info, key->data, key->size, NULL, ule); // mallocs prov_info->key, owned by this function
    indexer_fill_prov_info(indexer, &prov_info);
    DBT_ARRAY *hot_keys = &indexer->i->hot_keys[which_db];
    DBT_ARRAY *hot_vals = &indexer->i->hot_vals[which_db];
    int r = indexer_undo_do(indexer, hotdb, &prov_info, hot_keys, hot_vals);
    toku_free(prov_info.key);
    ule_prov_info_destroy(&prov_info);
    return r;
}

DB *
toku_indexer_get_src_db(DB_INDEXER *indexer) {
    return indexer->i->src_db;
}


#undef STATUS_VALUE

