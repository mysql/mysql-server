#ifndef LOG_INTERNAL_H
#define LOG_INTERNAL_H

#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "brt-internal.h"
#include "log.h"
#include "toku_assert.h"
#include "toku_list.h"
#include "memarena.h"
#include "logfilemgr.h"
#include <stdio.h>
#include <toku_pthread.h>
#include <sys/types.h>
#include <string.h>

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
    enum typ_tag tag; // must be first
    struct mylock  input_lock, output_lock; // acquired in that order
    BOOL is_open;
    BOOL is_panicked;
    BOOL write_log_files;
    BOOL trim_log_files; // for test purposes
    int panic_errno;
    char *directory;  // file system directory
    int fd;
    CACHETABLE ct;
    int lg_max; // The size of the single file in the log.  Default is 100MB in TokuDB

    // To access these, you must have the input lock
    LSN lsn; // the next available lsn
    OMT live_txns; // a sorted tree.  Old comment said should be a hashtable.  Do we still want that?
    struct logbuf inbuf; // data being accumulated for the write

    // To access these, you must have the output lock
    LSN written_lsn; // the last lsn written
    LSN fsynced_lsn; // What is the LSN of the highest fsynced log entry
    LSN checkpoint_lsn;     // What is the LSN of the most recent completed checkpoint.
    long long next_log_file_number;
    struct logbuf outbuf; // data being written to the file
    int  n_in_file; // The amount of data in the current file

    TOKULOGFILEMGR logfilemgr;

    u_int32_t write_block_size;       // How big should the blocks be written to various logs?
    TXNID oldest_living_xid;

    void (*remove_finalize_callback) (int, void*);  // ydb-level callback to be called when a transaction that ...
    void * remove_finalize_callback_extra;     // ... deletes a file is committed or when one that creates a file is aborted.
};

int toku_logger_find_next_unused_log_file(const char *directory, long long *result);
int toku_logger_find_logfiles (const char *directory, char ***resultp, int *n_logfiles);

struct brtcachefile_pair {
    BRT brt;
    CACHEFILE cf;
};

struct tokutxn {
    enum typ_tag tag;
    u_int64_t txnid64; /* this happens to be the first lsn */
    TOKULOGGER logger;
    TOKUTXN    parent;
    LSN        last_lsn; /* Everytime anything is logged, update the LSN.  (We need to atomically record the LSN along with writing into the log.) */
    LSN        first_lsn; /* The first lsn in the transaction. */
    struct roll_entry *oldest_logentry,*newest_logentry; /* Only logentries with rollbacks are here. There is a list going from newest to oldest. */

    MEMARENA   rollentry_arena;

    size_t     rollentry_resident_bytecount; // How many bytes for the rollentries that are stored in main memory.
    char      *rollentry_filename;
    int        rollentry_fd;         // If we spill the roll_entries, we write them into this fd.
    toku_off_t      rollentry_filesize;   // How many bytes are in the rollentry file (this is the uncompressed bytes.  If the file is compressed it may actually be smaller (or even larger with header information))
    u_int64_t  rollentry_raw_count;  // the total count of every byte in the transaction and all its children.
    OMT        open_brts; // a collection of the brts that we touched.  Indexed by filenum.
    XIDS       xids;      //Represents the xid list
    BOOL       force_fsync_on_commit;  //This transaction NEEDS an fsync once (if) it commits.  (commit means root txn)
    BOOL       has_done_work;          //If this transaction has not done work, there is no need to fsync.
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

static inline int toku_logsizeof_BYTESTRING (BYTESTRING bs) {
    return 4+bs.len;
}

static inline char *fixup_fname(BYTESTRING *f) {
    assert(f->len>0);
    char *fname = toku_xmalloc(f->len+1);
    memcpy(fname, f->data, f->len);
    fname[f->len]=0;
    return fname;
}

int toku_read_rollback_backwards(BREAD, struct roll_entry **item, MEMARENA);
#endif
