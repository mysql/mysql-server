/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

#include <db.h>
#include "ydb-internal.h"
#include <ft/checkpoint.h>
#include <ft/log_header.h>
#include "ydb_txn.h"
#include <lock_tree/lth.h>
#include <valgrind/helgrind.h>
#include "ft/txn_manager.h"

static int 
toku_txn_release_locks(DB_TXN* txn) {
    assert(txn);

    toku_lth* lth = db_txn_struct_i(txn)->lth;
    int r = ENOSYS;
    int first_error = 0;
    if (lth) {
        toku_lth_start_scan(lth);
        toku_lock_tree* next = toku_lth_next(lth);
        while (next) {
            r = toku_lt_unlock_txn(next, toku_txn_get_txnid(db_txn_struct_i(txn)->tokutxn));
            if (!first_error && r!=0) { first_error = r; }
            if (r == 0) {
                r = toku_lt_remove_ref(next);
                if (!first_error && r!=0) { first_error = r; }
            }
            next = toku_lth_next(lth);
        }
        toku_lth_close(lth);
        db_txn_struct_i(txn)->lth = NULL;
    }
    r = first_error;

    return r;
}

static void
toku_txn_destroy(DB_TXN *txn) {
    int32_t open_txns = __sync_sub_and_fetch(&txn->mgrp->i->open_txns, 1);
    invariant(open_txns >= 0);
    toku_txn_destroy_txn(db_txn_struct_i(txn)->tokutxn);
    toku_mutex_destroy(&db_txn_struct_i(txn)->txn_mutex);
#if !TOKUDB_NATIVE_H
    toku_free(db_txn_struct_i(txn));
#endif
    toku_free(txn);
}

static int
toku_txn_commit(DB_TXN * txn, u_int32_t flags,
                TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra,
                bool release_mo_lock) {
    HANDLE_PANICKED_ENV(txn->mgrp);
    //Recursively kill off children
    if (db_txn_struct_i(txn)->child) {
        //commit of child sets the child pointer to NULL
        int r_child = toku_txn_commit(db_txn_struct_i(txn)->child, flags, NULL, NULL, false);
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

    //toku_ydb_notef("flags=%d\n", flags);
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
    BOOL do_fsync;
    toku_txn_get_fsync_info(ttxn, &do_fsync, &do_fsync_lsn);
    // remove the txn from the list of live transactions, and then
    // release the lock tree locks. MVCC requires that toku_txn_complete_txn
    // get called first, otherwise we have bugs, such as #4145 and #4153
    toku_txn_complete_txn(ttxn);
    r = toku_txn_release_locks(txn);
    // this lock must be released after toku_txn_complete_txn and toku_txn_release_locks because
    // this lock must be held until the references to the open FTs is released
    // begin checkpoint logs these associations, so we must be protect
    // the changing of these associations with checkpointing
    if (release_mo_lock) {
        toku_multi_operation_client_unlock();
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

static u_int32_t 
toku_txn_id(DB_TXN * txn) {
    HANDLE_PANICKED_ENV(txn->mgrp);
    toku_ydb_barf();
    abort();
    return -1;
}

static u_int64_t 
toku_txn_id64(DB_TXN * txn) {
    HANDLE_PANICKED_ENV(txn->mgrp);
    return toku_txn_get_id(db_txn_struct_i(txn)->tokutxn);
}

static int 
toku_txn_abort(DB_TXN * txn,
               TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra) {
    HANDLE_PANICKED_ENV(txn->mgrp);
    //Recursively kill off children (abort or commit are both correct, commit is cheaper)
    if (db_txn_struct_i(txn)->child) {
        //commit of child sets the child pointer to NULL
        int r_child = toku_txn_commit(db_txn_struct_i(txn)->child, DB_TXN_NOSYNC, NULL, NULL, false);
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
    r = toku_txn_release_locks(txn);
    toku_txn_destroy(txn);
    return r;
}

static int
toku_txn_xa_prepare (DB_TXN *txn, TOKU_XA_XID *xid) {
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
    bool holds_mo_lock = false;
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
        int r_child = toku_txn_commit(db_txn_struct_i(txn)->child, 0, NULL, NULL, false);
        if (r_child !=0 && !toku_env_is_panicked(txn->mgrp)) {
            env_panic(txn->mgrp, r_child, "Recursive child commit failed during parent commit.\n");
        }
        //In a panicked env, the child may not be removed from the list.
        HANDLE_PANICKED_ENV(txn->mgrp);
    }
    assert(!db_txn_struct_i(txn)->child);
    TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
    r = toku_txn_prepare_txn(ttxn, xid);
    TOKULOGGER logger = txn->mgrp->i->logger;
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
static int
toku_txn_prepare (DB_TXN *txn, u_int8_t gid[DB_GID_SIZE]) {
    TOKU_XA_XID xid;
    ANNOTATE_NEW_MEMORY(&xid, sizeof(xid));
    xid.formatID=0x756b6f54; // "Toku"
    xid.gtrid_length=DB_GID_SIZE/2;  // The maximum allowed gtrid length is 64.  See the XA spec in source:/import/opengroup.org/C193.pdf page 20.
    xid.bqual_length=DB_GID_SIZE/2; // The maximum allowed bqual length is 64.
    memcpy(xid.data, gid, DB_GID_SIZE);
    return toku_txn_xa_prepare(txn, &xid);
}

static u_int32_t 
locked_txn_id(DB_TXN *txn) {
    u_int32_t r = toku_txn_id(txn); 
    return r;
}

static int 
toku_txn_txn_stat (DB_TXN *txn, struct txn_stat **txn_stat) {
    XMALLOC(*txn_stat);
    return toku_logger_txn_rollback_raw_count(db_txn_struct_i(txn)->tokutxn, &(*txn_stat)->rollback_raw_count);
}

static int 
locked_txn_txn_stat (DB_TXN *txn, struct txn_stat **txn_stat) {
    int r = toku_txn_txn_stat(txn, txn_stat); 
    return r;
}

static int
locked_txn_commit_with_progress(DB_TXN *txn, u_int32_t flags,
                                TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra) {
    TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
    if (toku_txn_requires_checkpoint(ttxn)) {
        toku_checkpoint(txn->mgrp->i->cachetable, txn->mgrp->i->logger, NULL, NULL, NULL, NULL, TXN_COMMIT_CHECKPOINT);
    }
    bool holds_mo_lock = false;
    if (!toku_txn_is_read_only(db_txn_struct_i(txn)->tokutxn)) {
        // A readonly transaction does no logging, and therefore does not
        // need the MO lock.
        toku_multi_operation_client_lock();
        holds_mo_lock = true;
    }
    // cannot begin a checkpoint.
    // the multi operation lock is taken the first time we
    // see a non-readonly txn in the recursive commit.
    // But released in the first-level toku_txn_commit (if taken),
    // this way, we don't hold it while we fsync the log.
    int r = toku_txn_commit(txn, flags, poll, poll_extra, holds_mo_lock);
    return r;
}

static int 
locked_txn_abort_with_progress(DB_TXN *txn,
                               TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra) {
    // cannot begin a checkpoint
    // the multi operation lock is taken the first time we
    // see a non-readonly txn in the abort (or recursive commit).
    // But released here so we don't have to hold additional state.
    bool holds_mo_lock = false;
    if (!toku_txn_is_read_only(db_txn_struct_i(txn)->tokutxn)) {
        // A readonly transaction does no logging, and therefore does not
        // need the MO lock.
        toku_multi_operation_client_lock();
        holds_mo_lock = true;
    }
    int r = toku_txn_abort(txn, poll, poll_extra);
    if (holds_mo_lock) {
        toku_multi_operation_client_unlock();
    }
    return r;
}

int 
locked_txn_commit(DB_TXN *txn, u_int32_t flags) {
    int r = locked_txn_commit_with_progress(txn, flags, NULL, NULL);
    return r;
}

int 
locked_txn_abort(DB_TXN *txn) {
    int r = locked_txn_abort_with_progress(txn, NULL, NULL);
    return r;
}

static inline void
txn_func_init(DB_TXN *txn) {
#define STXN(name) txn->name = locked_txn_ ## name
    STXN(abort);
    STXN(commit);
    STXN(abort_with_progress);
    STXN(commit_with_progress);
    STXN(id);
    STXN(txn_stat);
#undef STXN
#define SUTXN(name) txn->name = toku_txn_ ## name
    SUTXN(prepare);
    SUTXN(xa_prepare);
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
int 
toku_txn_begin(DB_ENV *env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags) {
    HANDLE_PANICKED_ENV(env);
    HANDLE_ILLEGAL_WORKING_PARENT_TXN(env, stxn); //Cannot create child while child already exists.
    if (!toku_logger_is_open(env->i->logger)) 
        return toku_ydb_do_error(env, EINVAL, "Environment does not have logging enabled\n");
    if (!(env->i->open_flags & DB_INIT_TXN))  
        return toku_ydb_do_error(env, EINVAL, "Environment does not have transactions enabled\n");

    u_int32_t txn_flags = 0;
    txn_flags |= DB_TXN_NOWAIT; //We do not support blocking locks. RFP remove this?
    TOKU_ISOLATION child_isolation = TOKU_ISO_SERIALIZABLE;
    u_int32_t iso_flags = flags & DB_ISOLATION_FLAGS;
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
            assert(FALSE); // error path is above, so this should not happen
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

    //toku_ydb_notef("parent=%p flags=0x%x\n", stxn, flags);
    result->mgrp = env;
    txn_func_init(result);

    result->parent = stxn;
#if !TOKUDB_NATIVE_H
    CALLOC(db_txn_struct_i(result));
    if (!db_txn_struct_i(result)) {
        toku_free(result);
        return ENOMEM;
    }
#endif
    db_txn_struct_i(result)->flags = txn_flags;
    db_txn_struct_i(result)->iso = child_isolation;

    // we used to initialize the transaction's lth here.
    // Now we initialize the lth only if the transaction needs the lth,
    // in toku_txn_add_lt. If this transaction never does anything 
    // that requires using a lock tree, then the lth is never 
    // created.
    int r = 0;
    
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
    r = toku_txn_manager_start_txn(&db_txn_struct_i(result)->tokutxn,
                                   toku_logger_get_txn_manager(env->i->logger),
                                   stxn ? db_txn_struct_i(stxn)->tokutxn : 0,
                                   env->i->logger,
                                   TXNID_NONE,
                                   snapshot_type,
                                   result,
                                   false);
    if (r != 0) {
        toku_free(result);
        return r;
    }

    //Add to the list of children for the parent.
    if (result->parent) {
        assert(!db_txn_struct_i(result->parent)->child);
        db_txn_struct_i(result->parent)->child = result;
    }

    toku_mutex_init(&db_txn_struct_i(result)->txn_mutex, NULL);
    (void) __sync_fetch_and_add(&env->i->open_txns, 1);

    *txn = result;
    return 0;
}

void toku_keep_prepared_txn_callback (DB_ENV *env, TOKUTXN tokutxn) {
    struct __toku_db_txn_external *XMALLOC(eresult);
    memset(eresult, 0, sizeof(*eresult));
    DB_TXN *result = &eresult->external_part;
    result->mgrp = env;
    txn_func_init(result);
    
    result->parent = NULL;
    
#if !TOKUDB_NATIVE_H
    MALLOC(db_txn_struct_i(result));
    if (!db_txn_struct_i(result)) {
        toku_free(result);
        return ENOMEM;
    }
#endif
    memset(db_txn_struct_i(result), 0, sizeof *db_txn_struct_i(result));

    {
        int r = toku_lth_create(&db_txn_struct_i(result)->lth);
        assert(r==0);
    }

    db_txn_struct_i(result)->tokutxn = tokutxn;

    toku_txn_set_container_db_txn(tokutxn, result);

    (void) __sync_fetch_and_add(&env->i->open_txns, 1);
}

// Test-only function
void
toku_increase_last_xid(DB_ENV *env, uint64_t increment) {
    toku_txn_manager_increase_last_xid(toku_logger_get_txn_manager(env->i->logger), increment);
}
