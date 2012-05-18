/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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

// Yield the lock so someone else can work, and then reacquire the lock.
// Useful while processing commit or rollback logs, to allow others to access the system.
static void 
ydb_yield (voidfp f, void *fv, void *UU(v)) {
    toku_ydb_unlock(); 
    if (f) 
        f(fv);
    toku_ydb_lock();
}

static void
toku_txn_destroy(DB_TXN *txn) {
    (void) __sync_fetch_and_sub(&txn->mgrp->i->open_txns, 1);
    assert(txn->mgrp->i->open_txns>=0);
    toku_txn_destroy_txn(db_txn_struct_i(txn)->tokutxn);
    toku_mutex_destroy(&db_txn_struct_i(txn)->txn_mutex);
#if !TOKUDB_NATIVE_H
    toku_free(db_txn_struct_i(txn));
#endif
    toku_free(txn);
}

static int 
toku_txn_commit_only(DB_TXN * txn, u_int32_t flags,
                     TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra,
                     bool release_multi_operation_client_lock) {
    if (!txn) return EINVAL;
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
	// Calls ydb_yield(NULL) occasionally
        r = toku_txn_abort_txn(db_txn_struct_i(txn)->tokutxn, ydb_yield, NULL, poll, poll_extra,
			       release_multi_operation_client_lock);
    } else {
	// frees the tokutxn
	// Calls ydb_yield(NULL) occasionally
        r = toku_txn_commit_txn(db_txn_struct_i(txn)->tokutxn, nosync, ydb_yield, NULL,
				poll, poll_extra, release_multi_operation_client_lock);
    }

    if (r!=0 && !toku_env_is_panicked(txn->mgrp)) {
	env_panic(txn->mgrp, r, "Error during commit.\n");
    }
    //If panicked, we're done.
    HANDLE_PANICKED_ENV(txn->mgrp);
    assert_zero(r);

    // Close the logger after releasing the locks
    r = toku_txn_release_locks(txn);
    TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
    TOKULOGGER logger = txn->mgrp->i->logger;
    LSN do_fsync_lsn;
    BOOL do_fsync;
    //
    // quickie fix for 5.2.0, need to extract these variables so that
    // we can do the fsync after the close of txn. We need to do it 
    // after the close because if we do it before, there are race
    // conditions exposed by test_stress1.c (#4145, #4153)
    //
    // Here is what was going on. In Maxwell (5.1.X), we used to 
    // call toku_txn_maybe_fsync_log in between toku_txn_release_locks
    // and toku_txn_close_txn. As a result, the ydb lock was released
    // and retaken in between these two calls. This was wrong, as the 
    // two commands need to be atomic. The problem was that 
    // when the ydb lock was released, the locks that this txn took
    // were released, but the txn was not removed from the list of 
    // live transactions. This allowed the following sequence of events: 
    //  - another txn B comes and writes to some key this transaction wrote to
    //  - txn B successfully commits
    //  - read txn C comes along, sees this transaction in its live list,
    //     but NOT txn B, which came after this transaction.
    //     This is incorrect. When txn C comes across a leafentry that has been
    //     modified by both this transaction and B, it'll read B's value, even
    //     though it cannot read this transaction's value, which comes below
    //     B's value on the leafentry's stack. This behavior is incorrect.
    //  All of this happens while the ydb lock is yielded. This causes a failure
    //  in the test_stress tests.
    //
    toku_txn_get_fsync_info(ttxn, &do_fsync, &do_fsync_lsn);
    toku_txn_complete_txn(ttxn);
    toku_txn_maybe_fsync_log(logger, do_fsync_lsn, do_fsync, ydb_yield, NULL);

    //Promote list to parent (dbs that must close before abort)
    if (txn->parent) {
        //Combine lists.
        while (!toku_list_empty(&db_txn_struct_i(txn)->dbs_that_must_close_before_abort)) {
            struct toku_list *list = toku_list_pop(&db_txn_struct_i(txn)->dbs_that_must_close_before_abort);
            toku_list_push(&db_txn_struct_i(txn->parent)->dbs_that_must_close_before_abort, list);
        }
    }
    else {
        //Empty the list
        while (!toku_list_empty(&db_txn_struct_i(txn)->dbs_that_must_close_before_abort)) {
            toku_list_pop(&db_txn_struct_i(txn)->dbs_that_must_close_before_abort);
        }
    }

    if (flags!=0) return EINVAL;
    return r;
}

int 
toku_txn_commit(DB_TXN * txn, u_int32_t flags,
                TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra,
                bool release_multi_operation_client_lock) {
    int r = toku_txn_commit_only(txn, flags, poll, poll_extra, release_multi_operation_client_lock);
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

static int 
toku_txn_abort_only(DB_TXN * txn,
                    TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra,
                    bool release_multi_operation_client_lock) {
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

    //All dbs that must close before abort, must now be closed
    assert(toku_list_empty(&db_txn_struct_i(txn)->dbs_that_must_close_before_abort));

    int r = toku_txn_abort_txn(db_txn_struct_i(txn)->tokutxn, ydb_yield, NULL, poll, poll_extra, release_multi_operation_client_lock);
    if (r!=0 && !toku_env_is_panicked(txn->mgrp)) {
	env_panic(txn->mgrp, r, "Error during abort.\n");
    }
    HANDLE_PANICKED_ENV(txn->mgrp);
    assert_zero(r);
    r = toku_txn_release_locks(txn);
    toku_txn_complete_txn(db_txn_struct_i(txn)->tokutxn);
    return r;
}

static int
toku_txn_xa_prepare (DB_TXN *txn, TOKU_XA_XID *xid) {
    if (!txn) return EINVAL;
    if (txn->parent) return 0; // make this a NO-OP, MySQL calls this
    HANDLE_PANICKED_ENV(txn->mgrp);
    //Recursively commit any children.
    if (db_txn_struct_i(txn)->child) {
        //commit of child sets the child pointer to NULL
        int r_child = toku_txn_commit(db_txn_struct_i(txn)->child, 0, NULL, NULL, false);
        if (r_child !=0 && !toku_env_is_panicked(txn->mgrp)) {
	    env_panic(txn->mgrp, r_child, "Recursive child commit failed during parent commit.\n");
        }
        //In a panicked env, the child may not be removed from the list.
        HANDLE_PANICKED_ENV(txn->mgrp);
    }
    assert(!db_txn_struct_i(txn)->child);
    TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
    int r = toku_txn_prepare_txn(ttxn, xid);
    TOKULOGGER logger = txn->mgrp->i->logger;
    LSN do_fsync_lsn;
    bool do_fsync;
    toku_txn_get_fsync_info(ttxn, &do_fsync, &do_fsync_lsn);
    toku_txn_maybe_fsync_log(logger, do_fsync_lsn, do_fsync, ydb_yield, NULL);
    return r;
}

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

int 
toku_txn_abort(DB_TXN * txn,
               TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra,
               bool release_multi_operation_client_lock) {
    int r = toku_txn_abort_only(txn, poll, poll_extra, release_multi_operation_client_lock);
    toku_txn_destroy(txn);
    return r;
}   
 
// Create a new transaction.
// Called without holding the ydb lock.
int 
locked_txn_begin(DB_ENV *env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags) {
    int r = toku_txn_begin(env, stxn, txn, flags, 0, false);
    return r;
}

static u_int32_t 
locked_txn_id(DB_TXN *txn) {
    toku_ydb_lock(); 
    u_int32_t r = toku_txn_id(txn); 
    toku_ydb_unlock(); 
    return r;
}

static int 
toku_txn_txn_stat (DB_TXN *txn, struct txn_stat **txn_stat) {
    XMALLOC(*txn_stat);
    return toku_logger_txn_rollback_raw_count(db_txn_struct_i(txn)->tokutxn, &(*txn_stat)->rollback_raw_count);
}

static int 
locked_txn_txn_stat (DB_TXN *txn, struct txn_stat **txn_stat) {
    toku_ydb_lock(); 
    int r = toku_txn_txn_stat(txn, txn_stat); 
    toku_ydb_unlock(); 
    return r;
}

static int
locked_txn_commit_with_progress(DB_TXN *txn, u_int32_t flags,
                                TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra) {
    TOKUTXN ttxn = db_txn_struct_i(txn)->tokutxn;
    if (toku_txn_requires_checkpoint(ttxn)) {
        toku_checkpoint(txn->mgrp->i->cachetable, txn->mgrp->i->logger, NULL, NULL, NULL, NULL, TXN_COMMIT_CHECKPOINT);
    }
    toku_multi_operation_client_lock(); //Cannot checkpoint during a commit.
    toku_ydb_lock();
    int r = toku_txn_commit_only(txn, flags, poll, poll_extra, true); // the final 'true' says to release the multi_operation_client_lock
    toku_ydb_unlock();
    toku_txn_destroy(txn);
    return r;
}

static int 
locked_txn_abort_with_progress(DB_TXN *txn,
                               TXN_PROGRESS_POLL_FUNCTION poll, void* poll_extra) {
    toku_multi_operation_client_lock(); //Cannot checkpoint during an abort.
    toku_ydb_lock();
    int r = toku_txn_abort_only(txn, poll, poll_extra, true); // the final 'true' says to release the multi_operation_client_lock
    toku_ydb_unlock();
    toku_txn_destroy(txn);
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

static int
locked_txn_prepare (DB_TXN *txn, u_int8_t gid[DB_GID_SIZE]) {
    toku_ydb_lock(); int r = toku_txn_prepare (txn, gid); toku_ydb_unlock(); return r;
}

static int
locked_txn_xa_prepare (DB_TXN *txn, TOKU_XA_XID *xid) {
    toku_ydb_lock(); int r = toku_txn_xa_prepare (txn, xid); toku_ydb_unlock(); return r;
}

int 
toku_txn_begin(DB_ENV *env, DB_TXN * stxn, DB_TXN ** txn, u_int32_t flags, bool internal, bool holds_ydb_lock) {
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

    if (internal && stxn) {
        child_isolation = db_txn_struct_i(stxn)->iso;
    }
    else {
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
            case (0):
                child_isolation = TOKU_ISO_SERIALIZABLE;
                break;
            default:
                assert(FALSE); // error path is above, so this should not happen
                break;
        }
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

    struct __toku_db_txn_external *XMALLOC(eresult); // so the internal stuff is stuck on the end.
    memset(eresult, 0, sizeof(*eresult));
    DB_TXN *result = &eresult->external_part;

    //toku_ydb_notef("parent=%p flags=0x%x\n", stxn, flags);
    result->mgrp = env;
#define STXN(name) result->name = locked_txn_ ## name
    STXN(abort);
    STXN(commit);
    STXN(abort_with_progress);
    STXN(commit_with_progress);
    STXN(id);
    STXN(prepare);
    STXN(xa_prepare);
    STXN(txn_stat);
#undef STXN

    result->parent = stxn;
#if !TOKUDB_NATIVE_H
    MALLOC(db_txn_struct_i(result));
    if (!db_txn_struct_i(result)) {
        toku_free(result);
        return ENOMEM;
    }
#endif
    memset(db_txn_struct_i(result), 0, sizeof *db_txn_struct_i(result));
    db_txn_struct_i(result)->flags = txn_flags;
    db_txn_struct_i(result)->iso = child_isolation;
    toku_list_init(&db_txn_struct_i(result)->dbs_that_must_close_before_abort);

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
    r = toku_txn_create_txn(&db_txn_struct_i(result)->tokutxn, 
                            stxn ? db_txn_struct_i(stxn)->tokutxn : 0, 
                            env->i->logger,
                            TXNID_NONE,
                            snapshot_type,
                            result
                            );
    if (r != 0)
        return r;
    if (!holds_ydb_lock) 
        toku_ydb_lock();
    r = toku_txn_start_txn(db_txn_struct_i(result)->tokutxn);
    if (!holds_ydb_lock) 
        toku_ydb_unlock();
    if (r != 0)
        return r;

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
#define STXN(name) result->name = locked_txn_ ## name
    STXN(abort);
    STXN(commit);
    STXN(abort_with_progress);
    STXN(commit_with_progress);
    STXN(id);
    STXN(prepare);
    STXN(txn_stat);
#undef STXN
    
    result->parent = NULL;
    
#if !TOKUDB_NATIVE_H
    MALLOC(db_txn_struct_i(result));
    if (!db_txn_struct_i(result)) {
        toku_free(result);
        return ENOMEM;
    }
#endif
    memset(db_txn_struct_i(result), 0, sizeof *db_txn_struct_i(result));
    toku_list_init(&db_txn_struct_i(result)->dbs_that_must_close_before_abort);

    {
	int r = toku_lth_create(&db_txn_struct_i(result)->lth);
	assert(r==0);
    }

    db_txn_struct_i(result)->tokutxn = tokutxn;

    toku_txn_set_container_db_txn(tokutxn, result);

    (void) __sync_fetch_and_add(&env->i->open_txns, 1);
}
