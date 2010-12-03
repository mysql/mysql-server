#ifndef LOG_INTERNAL_H
#define LOG_INTERNAL_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "brt-internal.h"
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

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// Locking for the logger
//  For most purposes we use the big ydb lock.
// To log: grab the buf lock
//  If the buf would overflow, then grab the file lock, swap file&buf, release buf lock, write the file, write the entry, release the file lock
//  else append to buf & release lock

#define LOGGER_MIN_BUF_SIZE (1<<24)

struct mylock {
    toku_pthread_mutex_t lock;
    int is_locked;
};

static inline int ml_init(struct mylock *l) {
    l->is_locked=0;
    memset(&l->lock, 0, sizeof(l->lock));
    return toku_pthread_mutex_init(&l->lock, 0);
}
static inline int ml_lock(struct mylock *l) {
    int r = toku_pthread_mutex_lock(&l->lock);
    assert(l->is_locked==0);
    l->is_locked=1;
    return r;
}
static inline int ml_unlock(struct mylock *l) {
    assert(l->is_locked==1);
    l->is_locked=0;
    return toku_pthread_mutex_unlock(&l->lock);
}
static inline int ml_destroy(struct mylock *l) {
    assert(l->is_locked==0);
    return toku_pthread_mutex_destroy(&l->lock);
}

struct logbuf {
    int n_in_buf;
    int buf_size;
    char *buf;
    LSN  max_lsn_in_buf;
};

struct tokulogger {
    struct mylock  input_lock;

    toku_pthread_mutex_t output_condition_lock; // if you need both this lock and input_lock, acquire the output_lock first, then input_lock. More typical is to get the output_is_available condition to be false, and then acquire the input_lock.
    toku_pthread_cond_t  output_condition;      //
    BOOL output_is_available;                  // this is part of the predicate for the output condition.  It's true if no thread is modifying the output (either doing an fsync or otherwise fiddling with the output).

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
    OMT live_txns; // a sorted tree.  Old comment said should be a hashtable.  Do we still want that?
    OMT live_root_txns; // a sorted tree.
    OMT snapshot_txnids;    //contains TXNID x | x is snapshot txn
    //contains TXNID pairs (x,y) | y is oldest txnid s.t. x is in y's live list
    // every TXNID that is in some snapshot's live list is used as the key for this OMT, x, as described above. 
    // The second half of the pair, y, is the youngest snapshot txnid (that is, has the highest LSN), such that x is in its live list.
    // So, for example, Say T_800 begins, T_800 commits right after snapshot txn T_1100  begins. Then (800,1100) is in 
    // this list
    OMT live_list_reverse;  
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
    TXNID oldest_living_xid;

    u_int64_t input_lock_ctr;             // how many times has input_lock been taken and released
    u_int64_t output_condition_lock_ctr;  // how many times has output_condition_lock been taken and released
    u_int64_t swap_ctr;                   // how many times have input/output log buffers been swapped
    void (*remove_finalize_callback) (DICTIONARY_ID, void*);  // ydb-level callback to be called when a transaction that ...
    void * remove_finalize_callback_extra;                    // ... deletes a file is committed or when one that creates a file is aborted.
    CACHEFILE rollback_cachefile;
};

int toku_logger_find_next_unused_log_file(const char *directory, long long *result);
int toku_logger_find_logfiles (const char *directory, char ***resultp, int *n_logfiles);

struct brtcachefile_pair {
    BRT brt;
    CACHEFILE cf;
};

struct tokutxn {
    u_int64_t txnid64; /* this happens to be the first lsn */
    u_int64_t ancestor_txnid64; /* this is the lsn of root transaction */
    u_int64_t snapshot_txnid64; /* this is the lsn of the snapshot */
    TOKULOGGER logger;
    TOKUTXN    parent;
    DB_TXN*    container_db_txn;  // reference to DB_TXN that contains this tokutxn

    u_int64_t  rollentry_raw_count;  // the total count of every byte in the transaction and all its children.
    OMT        open_brts; // a collection of the brts that we touched.  Indexed by filenum.
    TXN_SNAPSHOT_TYPE snapshot_type;
    OMT        live_root_txn_list; // the root txns live when the root ancestor (self if a root) started
    XIDS       xids;      //Represents the xid list
    BOOL       force_fsync_on_commit;  //This transaction NEEDS an fsync once (if) it commits.  (commit means root txn)
    TXN_PROGRESS_POLL_FUNCTION progress_poll_fun;
    void *                     progress_poll_fun_extra;
    uint64_t   num_rollback_nodes;
    uint64_t   num_rollentries;
    uint64_t   num_rollentries_processed;
    BLOCKNUM   spilled_rollback_head;
    uint32_t   spilled_rollback_head_hash;
    BLOCKNUM   spilled_rollback_tail;
    uint32_t   spilled_rollback_tail_hash;
    BLOCKNUM   current_rollback;
    uint32_t   current_rollback_hash;
    BOOL       recovered_from_checkpoint;
    ROLLBACK_LOG_NODE pinned_inprogress_rollback_log;
    struct toku_list checkpoint_before_commit;
    TXN_IGNORE_S ignore_errors; // 2954
};

struct txninfo {
    uint64_t   rollentry_raw_count;  // the total count of every byte in the transaction and all its children.
    uint32_t   num_brts;
    BRT       *open_brts;
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

static inline int toku_logsizeof_FILENUM (FILENUM v __attribute__((__unused__))) {
    return 4;
}

static inline int toku_logsizeof_DISKOFF (DISKOFF v __attribute__((__unused__))) {
    return 8;
}
static inline int toku_logsizeof_BLOCKNUM (BLOCKNUM v __attribute__((__unused__))) {
    return 8;
}

static inline int toku_logsizeof_TXNID (TXNID txnid __attribute__((__unused__))) {
    return 8;
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


#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
