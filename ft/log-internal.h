/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef LOG_INTERNAL_H
#define LOG_INTERNAL_H

#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "ft-internal.h"
#include "log.h"
#include "toku_assert.h"
#include "toku_list.h"
#include "memarena.h"
#include "logfilemgr.h"
#include "txn.h"
#include <stdio.h>
#include <toku_pthread.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include "txn_manager.h"

// Locking for the logger
//  For most purposes we use the big ydb lock.
// To log: grab the buf lock
//  If the buf would overflow, then grab the file lock, swap file&buf, release buf lock, write the file, write the entry, release the file lock
//  else append to buf & release lock

#define LOGGER_MIN_BUF_SIZE (1<<24)

struct mylock {
    toku_mutex_t lock;
};

static inline void ml_init(struct mylock *l) {
    toku_mutex_init(&l->lock, 0);
}
static inline void ml_lock(struct mylock *l) {
    toku_mutex_lock(&l->lock);
}
static inline void ml_unlock(struct mylock *l) {
    toku_mutex_unlock(&l->lock);
}
static inline void ml_destroy(struct mylock *l) {
    toku_mutex_destroy(&l->lock);
}

struct logbuf {
    int n_in_buf;
    int buf_size;
    char *buf;
    LSN  max_lsn_in_buf;
};

struct tokulogger {
    struct mylock  input_lock;

    toku_mutex_t output_condition_lock; // if you need both this lock and input_lock, acquire the output_lock first, then input_lock. More typical is to get the output_is_available condition to be false, and then acquire the input_lock.
    toku_cond_t  output_condition;      //
    BOOL output_is_available;           // this is part of the predicate for the output condition.  It's true if no thread is modifying the output (either doing an fsync or otherwise fiddling with the output).

    BOOL is_open;
    BOOL is_panicked;
    BOOL write_log_files;
    BOOL trim_log_files; // for test purposes
    int panic_errno;
    char *directory;  // file system directory
    DIR *dir; // descriptor for directory
    int fd;
    CACHETABLE ct;
    int lg_max; // The size of the single file in the log.  Default is 100MB in TokuDB

    // To access these, you must have the input lock
    LSN lsn; // the next available lsn
    struct logbuf inbuf; // data being accumulated for the write

    // To access these, you must have the output condition lock.
    LSN written_lsn; // the last lsn written
    LSN fsynced_lsn; // What is the LSN of the highest fsynced log entry  (accessed only while holding the output lock, and updated only when the output lock and output permission are held)
    LSN last_completed_checkpoint_lsn;     // What is the LSN of the most recent completed checkpoint.
    long long next_log_file_number;
    struct logbuf outbuf; // data being written to the file
    int  n_in_file; // The amount of data in the current file

    // To access the logfilemgr you must have the output condition lock.
    TOKULOGFILEMGR logfilemgr;

    u_int32_t write_block_size;       // How big should the blocks be written to various logs?

    u_int64_t input_lock_ctr;             // how many times has input_lock been taken and released
    u_int64_t output_condition_lock_ctr;  // how many times has output_condition_lock been taken and released
    u_int64_t swap_ctr;                   // how many times have input/output log buffers been swapped
    void (*remove_finalize_callback) (DICTIONARY_ID, void*);  // ydb-level callback to be called when a transaction that ...
    void * remove_finalize_callback_extra;                    // ... deletes a file is committed or when one that creates a file is aborted.
    CACHEFILE rollback_cachefile;
    TXN_MANAGER txn_manager;
};

int toku_logger_find_next_unused_log_file(const char *directory, long long *result);
int toku_logger_find_logfiles (const char *directory, char ***resultp, int *n_logfiles);

struct txn_roll_info {
    // these are number of rollback nodes and rollback entries for this txn.
    //
    // the current rollback node below has sequence number num_rollback_nodes - 1
    // (because they are numbered 0...num-1). often, the current rollback is
    // already set to this block num, which means it exists and is available to
    // log some entries. if the current rollback is NONE and the number of
    // rollback nodes for this transaction is non-zero, then we will use
    // the number of rollback nodes to know which sequence number to assign
    // to a new one we create
    uint64_t num_rollback_nodes;
    uint64_t num_rollentries;
    uint64_t num_rollentries_processed;
    uint64_t rollentry_raw_count;  // the total count of every byte in the transaction and all its children.

    // spilled rollback nodes are rollback nodes that were gorged by this
    // transaction, retired, and saved in a list.

    // the spilled rollback head is the block number of the first rollback node
    // that makes up the rollback log chain
    BLOCKNUM spilled_rollback_head;
    uint32_t spilled_rollback_head_hash;
    // the spilled rollback is the block number of the last rollback node that
    // makes up the rollback log chain. 
    BLOCKNUM spilled_rollback_tail;
    uint32_t spilled_rollback_tail_hash;
    // the current rollback node block number we may use. if this is ROLLBACK_NONE,
    // then we need to create one and set it here before using it.
    BLOCKNUM current_rollback; 
    uint32_t current_rollback_hash;
};

struct tokutxn {
    // These don't change after create:
    const time_t starttime; // timestamp in seconds of transaction start
    const u_int64_t txnid64; // this happens to be the first lsn
    const u_int64_t ancestor_txnid64; // this is the lsn of root transaction
    const u_int64_t snapshot_txnid64; // this is the lsn of the snapshot
    const TXN_SNAPSHOT_TYPE snapshot_type;
    const BOOL recovered_from_checkpoint;
    const TOKULOGGER logger;
    const TOKUTXN parent;
    // These don't either but they're created in a way that's hard to make
    // strictly const.
    DB_TXN *container_db_txn; // reference to DB_TXN that contains this tokutxn
    xid_omt_t *live_root_txn_list; // the root txns live when the root ancestor (self if a root) started.
    XIDS xids; // Represents the xid list

    bool begin_was_logged;
    // These are not read until a commit, prepare, or abort starts, and
    // they're "monotonic" (only go false->true) during operation:
    BOOL checkpoint_needed_before_commit;
    BOOL do_fsync;
    BOOL force_fsync_on_commit;  //This transaction NEEDS an fsync once (if) it commits.  (commit means root txn)

    // Not used until commit, prepare, or abort starts:
    LSN do_fsync_lsn;
    TOKU_XA_XID xa_xid; // for prepared transactions
    TXN_PROGRESS_POLL_FUNCTION progress_poll_fun;
    void *progress_poll_fun_extra;

    toku_mutex_t txn_lock;
    // Protected by the txn lock:
    OMT open_fts; // a collection of the fts that we touched.  Indexed by filenum.
    struct txn_roll_info roll_info; // Info used to manage rollback entries

    // Protected by the txn manager lock:
    TOKUTXN_STATE state;
    struct toku_list prepared_txns_link; // list of prepared transactions
    uint32_t num_pin; // number of threads (all hot indexes) that want this
                      // txn to not transition to commit or abort
};

static inline int
txn_has_current_rollback_log(TOKUTXN txn) {
    return txn->roll_info.current_rollback.b != ROLLBACK_NONE.b;
}

static inline int
txn_has_spilled_rollback_logs(TOKUTXN txn) {
    return txn->roll_info.spilled_rollback_tail.b != ROLLBACK_NONE.b;
}

struct txninfo {
    uint64_t   rollentry_raw_count;  // the total count of every byte in the transaction and all its children.
    uint32_t   num_fts;
    FT *open_fts;
    BOOL       force_fsync_on_commit;  //This transaction NEEDS an fsync once (if) it commits.  (commit means root txn)
    uint64_t   num_rollback_nodes;
    uint64_t   num_rollentries;
    BLOCKNUM   spilled_rollback_head;
    BLOCKNUM   spilled_rollback_tail;
    BLOCKNUM   current_rollback;
};

static inline int toku_logsizeof_u_int8_t (u_int32_t v __attribute__((__unused__))) {
    return 1;
}

static inline int toku_logsizeof_u_int32_t (u_int32_t v __attribute__((__unused__))) {
    return 4;
}

static inline int toku_logsizeof_u_int64_t (u_int32_t v __attribute__((__unused__))) {
    return 8;
}

static inline int toku_logsizeof_BOOL (u_int32_t v __attribute__((__unused__))) {
    return 1;
}

static inline int toku_logsizeof_FILENUM (FILENUM v __attribute__((__unused__))) {
    return 4;
}

static inline int toku_logsizeof_DISKOFF (DISKOFF v __attribute__((__unused__))) {
    return 8;
}
static inline int toku_logsizeof_BLOCKNUM (BLOCKNUM v __attribute__((__unused__))) {
    return 8;
}

static inline int toku_logsizeof_LSN (LSN lsn __attribute__((__unused__))) {
    return 8;
}

static inline int toku_logsizeof_TXNID (TXNID txnid __attribute__((__unused__))) {
    return 8;
}

static inline int toku_logsizeof_XIDP (XIDP xid) {
    assert(0<=xid->gtrid_length && xid->gtrid_length<=64);
    assert(0<=xid->bqual_length && xid->bqual_length<=64);
    return xid->gtrid_length
	+ xid->bqual_length
	+ 4  // formatID
	+ 1  // gtrid_length
	+ 1; // bqual_length
}

static inline int toku_logsizeof_FILENUMS (FILENUMS fs) {
    static const FILENUM f = {0}; //fs could have .num==0 and then we cannot dereference
    return 4 + fs.num * toku_logsizeof_FILENUM(f);
}

static inline int toku_logsizeof_BYTESTRING (BYTESTRING bs) {
    return 4+bs.len;
}

static inline char *fixup_fname(BYTESTRING *f) {
    assert(f->len>0);
    char *fname = (char*)toku_xmalloc(f->len+1);
    memcpy(fname, f->data, f->len);
    fname[f->len]=0;
    return fname;
}

#endif
