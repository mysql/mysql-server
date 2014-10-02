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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

#include <config.h>

#include <db.h>

#include <portability/toku_race_tools.h>
#include <portability/toku_atomic.h>

#include <ft/cachetable/checkpoint.h>
#include <ft/log_header.h>
#include <ft/txn/txn_manager.h>


#include "ydb-internal.h"
#include "ydb_txn.h"
#include "ydb_row_lock.h"

static uint64_t toku_txn_id64(DB_TXN * txn) {
    HANDLE_PANICKED_ENV(txn->mgrp);
    return toku_txn_get_root_id(db_txn_struct_i(txn)->tokutxn);
}

static void toku_txn_release_locks(DB_TXN *txn) {
    // Prevent access to the locktree map while releasing.
    // It is possible for lock escalation to attempt to
    // modify this data structure while the txn commits.
    toku_mutex_lock(&db_txn_struct_i(txn)->txn_mutex);

    size_t num_ranges = db_txn_struct_i(txn)->lt_map.size();
    for (size_t i = 0; i < num_ranges; i++) {
        txn_lt_key_ranges ranges;
        int r = db_txn_struct_i(txn)->lt_map.fetch(i, &ranges);
        invariant_zero(r);
        toku_db_release_lt_key_ranges(txn, &ranges);
    }

    toku_mutex_unlock(&db_txn_struct_i(txn)->txn_mutex);
}

static void toku_txn_destroy(DB_TXN *txn) {
    db_txn_struct_i(txn)->lt_map.destroy();
    toku_txn_destroy_txn(db_txn_struct_i(txn)->tokutxn);
    toku_mutex_destroy(&db_txn_struct_i(txn)->txn_mutex);
    toku_free(txn);
}

static int toku_txn_commit(DB_TXN * txn, uint32_t flags,
                           TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra,
                           bool release_mo_lock, bool low_priority) {
    HANDLE_PANICKED_ENV(txn->mgrp);
    //Recursively kill off children
    if (db_txn_struct_i(txn)->child) {
        //commit of child sets the child pointer to NULL
        int r_child = toku_txn_commit(db_txn_struct_i(txn)->child, flags, NULL, NULL, false, false);
        if (r_child !=0 && !toku_env_is_panicked(txn->mgrp)) {
            env_panic(txn->mgrp, r_child, "Recursive child commit failed during parent commit.\n");
        }
        //In a panicked env, the child may not be removed from the list.
        HANDLE_PANICKED_ENV(txn->mgrp);
    }
    assert(!db_txn_struct_i(txn)->child);
    //Remove from parent
    if (txn->parent) {
        assert(db_txn_struct_i(txn->parent)->child == txn);
        db_txn_struct_i(txn->parent)->child=NULL;
    }
    if (flags & DB_TXN_SYNC) {
        toku_txn_force_fsync_on_commit(db_txn_struct_i(txn)->tokutxn);
        flags &= ~DB_TXN_SYNC;
    }
    int nosync = (flags & DB_TXN_NOSYNC)!=0 || (db_txn_struct_i(txn)->flags&DB_TXN_NOSYNC);
    flags &= ~DB_TXN_NOSYNC;

    int r;
    if (flags!=0) {
        // frees the tokutxn
        r = toku_txn_abort_txn(db_txn_struct_i(txn)->tokutxn, poll, poll_extra);
    } else {
        // frees the tokutxn
        r = toku_txn_commit_txn(db_txn_struct_i(txn)->tokutxn, nosync,
                                poll, poll_extra);
    }
    if (r!=0 && !toku_env_is_panicked(txn->mgrp)) {
        env_panic(txn->mgrp, r, "Error during commit.\n");
    }
    //If panicked, we're done.
    HANDLE_PANICKED_ENV(txn->mgrp);
    assert_zero(r);

    TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
    TOKULOGGER logger = txn->mgrp->i->logger;
    LSN do_fsync_lsn;
    bool do_fsync;
    toku_txn_get_fsync_info(ttxn, &do_fsync, &do_fsync_lsn);
    // remove the txn from the list of live transactions, and then
    // release the lock tree locks. MVCC requires that toku_txn_complete_txn
    // get called first, otherwise we have bugs, such as #4145 and #4153
    toku_txn_complete_txn(ttxn);
    toku_txn_release_locks(txn);
    // this lock must be released after toku_txn_complete_txn and toku_txn_release_locks because
    // this lock must be held until the references to the open FTs is released
    // begin checkpoint logs these associations, so we must be protect
    // the changing of these associations with checkpointing
    if (release_mo_lock) {
        if (low_priority) {
            toku_low_priority_multi_operation_client_unlock();
        } else {
            toku_multi_operation_client_unlock();
        }
    }
    toku_txn_maybe_fsync_log(logger, do_fsync_lsn, do_fsync);
    if (flags!=0) {
        r = EINVAL;
        goto cleanup;
    }
cleanup:
    toku_txn_destroy(txn);
    return r;
}

static int toku_txn_abort(DB_TXN * txn,
                          TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra) {
    HANDLE_PANICKED_ENV(txn->mgrp);
    //Recursively kill off children (abort or commit are both correct, commit is cheaper)
    if (db_txn_struct_i(txn)->child) {
        //commit of child sets the child pointer to NULL
        int r_child = toku_txn_commit(db_txn_struct_i(txn)->child, DB_TXN_NOSYNC, NULL, NULL, false, false);
        if (r_child !=0 && !toku_env_is_panicked(txn->mgrp)) {
            env_panic(txn->mgrp, r_child, "Recursive child commit failed during parent abort.\n");
        }
        //In a panicked env, the child may not be removed from the list.
        HANDLE_PANICKED_ENV(txn->mgrp);
    }
    assert(!db_txn_struct_i(txn)->child);
    //Remove from parent
    if (txn->parent) {
        assert(db_txn_struct_i(txn->parent)->child == txn);
        db_txn_struct_i(txn->parent)->child=NULL;
    }

    int r = toku_txn_abort_txn(db_txn_struct_i(txn)->tokutxn, poll, poll_extra);
    if (r!=0 && !toku_env_is_panicked(txn->mgrp)) {
        env_panic(txn->mgrp, r, "Error during abort.\n");
    }
    HANDLE_PANICKED_ENV(txn->mgrp);
    assert_zero(r);
    toku_txn_complete_txn(db_txn_struct_i(txn)->tokutxn);
    toku_txn_release_locks(txn);
    toku_txn_destroy(txn);
    return r;
}

static int toku_txn_xa_prepare (DB_TXN *txn, TOKU_XA_XID *xid) {
    int r = 0;
    if (!txn) {
        r = EINVAL;
        goto exit;
    }
    if (txn->parent) {
        r = 0; // make this a NO-OP, MySQL calls this
        goto exit;
    }
    HANDLE_PANICKED_ENV(txn->mgrp);
    // Take the mo lock as soon as a non-readonly txn is found
    bool holds_mo_lock;
    holds_mo_lock = false;
    if (!toku_txn_is_read_only(db_txn_struct_i(txn)->tokutxn)) {
        // A readonly transaction does no logging, and therefore does not
        // need the MO lock.
        toku_multi_operation_client_lock();
        holds_mo_lock = true;
    }
    //Recursively commit any children.
    if (db_txn_struct_i(txn)->child) {
        //commit of child sets the child pointer to NULL

        // toku_txn_commit will take the mo_lock if not held and a non-readonly txn is found.
        int r_child = toku_txn_commit(db_txn_struct_i(txn)->child, 0, NULL, NULL, false, false);
        if (r_child !=0 && !toku_env_is_panicked(txn->mgrp)) {
            env_panic(txn->mgrp, r_child, "Recursive child commit failed during parent commit.\n");
        }
        //In a panicked env, the child may not be removed from the list.
        HANDLE_PANICKED_ENV(txn->mgrp);
    }
    assert(!db_txn_struct_i(txn)->child);
    TOKUTXN ttxn;
    ttxn = db_txn_struct_i(txn)->tokutxn;
    toku_txn_prepare_txn(ttxn, xid);
    TOKULOGGER logger;
    logger = txn->mgrp->i->logger;
    LSN do_fsync_lsn;
    bool do_fsync;
    toku_txn_get_fsync_info(ttxn, &do_fsync, &do_fsync_lsn);
    // release the multi operation lock before fsyncing the log
    if (holds_mo_lock) {
        toku_multi_operation_client_unlock();
    }
    toku_txn_maybe_fsync_log(logger, do_fsync_lsn, do_fsync);
exit:
    return r;
}

// requires: must hold the multi operation lock. it is
//           released in toku_txn_xa_prepare before the fsync.
static int toku_txn_prepare (DB_TXN *txn, uint8_t gid[DB_GID_SIZE]) {
    TOKU_XA_XID xid;
    TOKU_ANNOTATE_NEW_MEMORY(&xid, sizeof(xid));
    xid.formatID=0x756b6f54; // "Toku"
    xid.gtrid_length=DB_GID_SIZE/2;  // The maximum allowed gtrid length is 64.  See the XA spec in source:/import/opengroup.org/C193.pdf page 20.
    xid.bqual_length=DB_GID_SIZE/2; // The maximum allowed bqual length is 64.
    memcpy(xid.data, gid, DB_GID_SIZE);
    return toku_txn_xa_prepare(txn, &xid);
}

static int toku_txn_txn_stat (DB_TXN *txn, struct txn_stat **txn_stat) {
    XMALLOC(*txn_stat);
    return toku_logger_txn_rollback_stats(db_txn_struct_i(txn)->tokutxn, *txn_stat);
}

static int locked_txn_txn_stat (DB_TXN *txn, struct txn_stat **txn_stat) {
    int r = toku_txn_txn_stat(txn, txn_stat);
    return r;
}

static int locked_txn_commit_with_progress(DB_TXN *txn, uint32_t flags,
                                           TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra) {
    bool holds_mo_lock = false;
    bool low_priority = false;
    TOKUTXN tokutxn = db_txn_struct_i(txn)->tokutxn;
    if (!toku_txn_is_read_only(tokutxn)) {
        // A readonly transaction does no logging, and therefore does not need the MO lock.
        holds_mo_lock = true;
        if (toku_is_big_tokutxn(tokutxn)) {
            low_priority = true;
            toku_low_priority_multi_operation_client_lock();
        } else {
            toku_multi_operation_client_lock();
        }
    }
    // cannot begin a checkpoint.
    // the multi operation lock is taken the first time we
    // see a non-readonly txn in the recursive commit.
    // But released in the first-level toku_txn_commit (if taken),
    // this way, we don't hold it while we fsync the log.
    int r = toku_txn_commit(txn, flags, poll, poll_extra, holds_mo_lock, low_priority);
    return r;
}

static int locked_txn_abort_with_progress(DB_TXN *txn,
                                          TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra) {
    // cannot begin a checkpoint
    // the multi operation lock is taken the first time we
    // see a non-readonly txn in the abort (or recursive commit).
    // But released here so we don't have to hold additional state.
    bool holds_mo_lock = false;
    bool low_priority = false;
    TOKUTXN tokutxn = db_txn_struct_i(txn)->tokutxn;
    if (!toku_txn_is_read_only(tokutxn)) {
        // A readonly transaction does no logging, and therefore does not need the MO lock.
        holds_mo_lock = true;
        if (toku_is_big_tokutxn(tokutxn)) {
            low_priority = true;
            toku_low_priority_multi_operation_client_lock();
        } else {
            toku_multi_operation_client_lock();
        }
    }
    int r = toku_txn_abort(txn, poll, poll_extra);
    if (holds_mo_lock) {
        if (low_priority) {
            toku_low_priority_multi_operation_client_unlock();
        } else {
            toku_multi_operation_client_unlock();
        }
    }
    return r;
}

int locked_txn_commit(DB_TXN *txn, uint32_t flags) {
    int r = locked_txn_commit_with_progress(txn, flags, NULL, NULL);
    return r;
}

int locked_txn_abort(DB_TXN *txn) {
    int r = locked_txn_abort_with_progress(txn, NULL, NULL);
    return r;
}

static void locked_txn_set_client_id(DB_TXN *txn, uint64_t client_id) {
    toku_txn_set_client_id(db_txn_struct_i(txn)->tokutxn, client_id);
}

static uint64_t locked_txn_get_client_id(DB_TXN *txn) {
    return toku_txn_get_client_id(db_txn_struct_i(txn)->tokutxn);
}

static int toku_txn_discard(DB_TXN *txn, uint32_t flags) {
    // check parameters
    if (flags != 0)
        return EINVAL;
    TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
    if (toku_txn_get_state(ttxn) != TOKUTXN_PREPARING)
        return EINVAL;

    bool low_priority;
    if (toku_is_big_tokutxn(ttxn)) {
        low_priority = true;
        toku_low_priority_multi_operation_client_lock();
    } else {
        low_priority = false;
        toku_multi_operation_client_lock();
    }

    // discard
    toku_txn_discard_txn(ttxn);

    // complete
    toku_txn_complete_txn(ttxn);

    // release locks
    toku_txn_release_locks(txn);

    if (low_priority) {
        toku_low_priority_multi_operation_client_unlock();
    } else {
        toku_multi_operation_client_unlock();
    }

    // destroy
    toku_txn_destroy(txn);

    return 0;
}

static inline void txn_func_init(DB_TXN *txn) {
#define STXN(name) txn->name = locked_txn_ ## name
    STXN(abort);
    STXN(commit);
    STXN(abort_with_progress);
    STXN(commit_with_progress);
    STXN(txn_stat);
    STXN(set_client_id);
    STXN(get_client_id);
#undef STXN
#define SUTXN(name) txn->name = toku_txn_ ## name
    SUTXN(prepare);
    SUTXN(xa_prepare);
    SUTXN(discard);
#undef SUTXN
    txn->id64 = toku_txn_id64;
}

//
// Creates a transaction for the user
// In our system, as far as the user is concerned, the rules are as follows:
//  - one cannot operate on a transaction if a child exists, with the exception of commit/abort
//  - one cannot operate on a transaction simultaneously in two separate threads
//     (the reason for this is that some operations may create a child transaction
//     as part of the function, such as env->dbremove and env->dbrename, and if
//     transactions could be operated on simulatenously in different threads, the first
//     rule above is violated)
//  - if a parent transaction is committed/aborted, the child transactions are recursively
//     committed
//
int toku_txn_begin(DB_ENV *env, DB_TXN * stxn, DB_TXN ** txn, uint32_t flags) {
    HANDLE_PANICKED_ENV(env);
    HANDLE_ILLEGAL_WORKING_PARENT_TXN(env, stxn); //Cannot create child while child already exists.
    if (!toku_logger_is_open(env->i->logger)) 
        return toku_ydb_do_error(env, EINVAL, "Environment does not have logging enabled\n");
    if (!(env->i->open_flags & DB_INIT_TXN))  
        return toku_ydb_do_error(env, EINVAL, "Environment does not have transactions enabled\n");

    uint32_t txn_flags = 0;
    txn_flags |= DB_TXN_NOWAIT; //We do not support blocking locks. RFP remove this?

    // handle whether txn is declared as read only
    bool parent_txn_declared_read_only = 
        stxn && 
        (db_txn_struct_i(stxn)->flags & DB_TXN_READ_ONLY);    
    bool txn_declared_read_only = false;
    if (flags & DB_TXN_READ_ONLY) {
        txn_declared_read_only = true;
        txn_flags |=  DB_TXN_READ_ONLY;
        flags &= ~(DB_TXN_READ_ONLY);
    }
    if (txn_declared_read_only && stxn &&
        !parent_txn_declared_read_only
        )
    {
        return toku_ydb_do_error(
            env, 
            EINVAL, 
            "Current transaction set as read only, but parent transaction is not\n"
            );
    }
    if (parent_txn_declared_read_only) 
    {
        // don't require child transaction to also set transaction as read only
        // if parent has already done so
        txn_flags |=  DB_TXN_READ_ONLY;
        txn_declared_read_only = true;
    }

    
    TOKU_ISOLATION child_isolation = TOKU_ISO_SERIALIZABLE;
    uint32_t iso_flags = flags & DB_ISOLATION_FLAGS;
    if (!(iso_flags == 0 || 
          iso_flags == DB_TXN_SNAPSHOT || 
          iso_flags == DB_READ_COMMITTED || 
          iso_flags == DB_READ_UNCOMMITTED || 
          iso_flags == DB_SERIALIZABLE || 
          iso_flags == DB_INHERIT_ISOLATION)
       ) 
    {
        return toku_ydb_do_error(
            env, 
            EINVAL, 
            "Invalid isolation flags set\n"
            );
    }
    flags &= ~iso_flags;

    switch (iso_flags) {
        case (DB_INHERIT_ISOLATION):
            if (stxn) {
                child_isolation = db_txn_struct_i(stxn)->iso;
            }
            else {
                return toku_ydb_do_error(
                    env, 
                    EINVAL, 
                    "Cannot set DB_INHERIT_ISOLATION when no parent exists\n"
                    );                    
            }
            break;
        case (DB_READ_COMMITTED):
            child_isolation = TOKU_ISO_READ_COMMITTED;
            break;
        case (DB_READ_UNCOMMITTED):
            child_isolation = TOKU_ISO_READ_UNCOMMITTED;
            break;
        case (DB_TXN_SNAPSHOT):
            child_isolation = TOKU_ISO_SNAPSHOT;
            break;
        case (DB_SERIALIZABLE):
            child_isolation = TOKU_ISO_SERIALIZABLE;
            break;
        case (0):
            child_isolation = stxn ? db_txn_struct_i(stxn)->iso : TOKU_ISO_SERIALIZABLE;
            break;
        default:
            assert(false); // error path is above, so this should not happen
            break;
    }
    if (stxn && child_isolation != db_txn_struct_i(stxn)->iso) {
        return toku_ydb_do_error(
            env, 
            EINVAL, 
            "Cannot set isolation level of transaction to something different \
                isolation level\n"
            );   
    }

    if (flags&DB_TXN_NOWAIT) {
        txn_flags |=  DB_TXN_NOWAIT;
        flags     &= ~DB_TXN_NOWAIT;
    }
    if (flags&DB_TXN_NOSYNC) {
        txn_flags |=  DB_TXN_NOSYNC;
        flags     &= ~DB_TXN_NOSYNC;
    }
    if (flags!=0) return toku_ydb_do_error(env, EINVAL, "Invalid flags passed to DB_ENV->txn_begin\n");

    struct __toku_db_txn_external *XCALLOC(eresult); // so the internal stuff is stuck on the end.
    DB_TXN *result = &eresult->external_part;

    result->mgrp = env;
    txn_func_init(result);

    result->parent = stxn;
    db_txn_struct_i(result)->flags = txn_flags;
    db_txn_struct_i(result)->iso = child_isolation;
    db_txn_struct_i(result)->lt_map.create_no_array();

    toku_mutex_init(&db_txn_struct_i(result)->txn_mutex, NULL);

    TXN_SNAPSHOT_TYPE snapshot_type;
    switch(db_txn_struct_i(result)->iso){
        case(TOKU_ISO_SNAPSHOT):
        {
            snapshot_type = TXN_SNAPSHOT_ROOT;
            break;
        }
        case(TOKU_ISO_READ_COMMITTED):
        {
            snapshot_type = TXN_SNAPSHOT_CHILD;
            break;
        }
        default:
        {
            snapshot_type = TXN_SNAPSHOT_NONE;
            break;
        }
    }
    int r = toku_txn_begin_with_xid(
        stxn ? db_txn_struct_i(stxn)->tokutxn : 0,
        &db_txn_struct_i(result)->tokutxn,
        env->i->logger,
        TXNID_PAIR_NONE,
        snapshot_type,
        result,
        false, // for_recovery
        txn_declared_read_only // read_only
        );
    if (r != 0) {
        toku_free(result);
        return r;
    }

    //Add to the list of children for the parent.
    if (result->parent) {
        assert(!db_txn_struct_i(result->parent)->child);
        db_txn_struct_i(result->parent)->child = result;
    }

    *txn = result;
    return 0;
}

void toku_keep_prepared_txn_callback (DB_ENV *env, TOKUTXN tokutxn) {
    struct __toku_db_txn_external *XCALLOC(eresult);
    DB_TXN *result = &eresult->external_part;
    result->mgrp = env;
    txn_func_init(result);
    
    result->parent = NULL;

    db_txn_struct_i(result)->tokutxn = tokutxn;
    db_txn_struct_i(result)->lt_map.create();

    toku_txn_set_container_db_txn(tokutxn, result);

    toku_mutex_init(&db_txn_struct_i(result)->txn_mutex, NULL);
}

// Test-only function
void toku_increase_last_xid(DB_ENV *env, uint64_t increment) {
    toku_txn_manager_increase_last_xid(toku_logger_get_txn_manager(env->i->logger), increment);
}

bool toku_is_big_txn(DB_TXN *txn) {
    return toku_is_big_tokutxn(db_txn_struct_i(txn)->tokutxn);
}

bool toku_is_big_tokutxn(TOKUTXN tokutxn) {
    return toku_txn_has_spilled_rollback(tokutxn);
}
