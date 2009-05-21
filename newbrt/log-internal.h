#ifndef LOG_INTERNAL_H
#define LOG_INTERNAL_H

#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "brt-internal.h"
#include "log.h"
#include "toku_assert.h"
#include "list.h"
#include "memarena.h"
#include <stdio.h>
#include <toku_pthread.h>
#include <sys/types.h>
#include <string.h>

// Locking for the logger
//  For most purposes we use the big ydb lock.
// To log: grab the buf lock
//  If the buf would overflow, then grab the file lock, swap file&buf, release buf lock, write the file, write the entry, release the file lock
//  else append to buf & release lock

#define LOGGER_BUF_SIZE (1<<24)

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


struct tokulogger {
    enum typ_tag tag; // must be first
    struct mylock  input_lock, output_lock; // acquired in that order
    int is_open;
    int is_panicked;
    int write_log_files;
    int panic_errno;
    char *directory;
    int fd;
    CACHETABLE ct;
    int lg_max; // The size of the single file in the log.  Default is 100MB in TokuDB

    // To access these, you must have the input lock
    struct logbytes *head,*tail;
    LSN lsn; // the next available lsn
    struct list live_txns; // just a linked list.  Should be a hashtable.
    int n_in_buf;

    // To access these, you must have the output lock
    LSN written_lsn; // the last lsn written
    LSN fsynced_lsn; // What is the LSN of the highest fsynced log entry
    LSN checkpoint_lsn;     // What is the LSN of the most recent completed checkpoint.
    long long next_log_file_number;
    char buf[LOGGER_BUF_SIZE]; // used to marshall logbytes so we can use only a single write
    int n_in_file;

    u_int32_t write_block_size;       // How big should the blocks be written to various logs?
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
    struct list live_txns_link;

    MEMARENA   rollentry_arena;

    size_t     rollentry_resident_bytecount; // How many bytes for the rollentries that are stored in main memory.
    char      *rollentry_filename;
    int        rollentry_fd;         // If we spill the roll_entries, we write them into this fd.
    toku_off_t      rollentry_filesize;   // How many bytes are in the rollentry file (this is the uncompressed bytes.  If the file is compressed it may actually be smaller (or even larger with header information))
    u_int64_t  rollentry_raw_count;  // the total count of every byte in the transaction and all its children.
    OMT        open_brts; // a collection of the brts that we touched.  Indexed by filenum.
};

int toku_logger_finish (TOKULOGGER logger, struct logbytes *logbytes, struct wbuf *wbuf, int do_fsync);

static inline int toku_logsizeof_u_int8_t (u_int32_t v __attribute__((__unused__))) {
    return 1;
}

static inline int toku_logsizeof_u_int32_t (u_int32_t v __attribute__((__unused__))) {
    return 4;
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

static inline int toku_logsizeof_LOGGEDBRTHEADER (LOGGEDBRTHEADER bs) {
    int in_both = 4+4+4+8+8+4+8;
    in_both += 8; // for the number of block headers
    assert(bs.btt_size.b< 30000000); // otherwise we in trouble on the next line
    in_both += 12*bs.btt_size.b;
    return in_both+8;
}

static inline int toku_logsizeof_INTPAIRARRAY (INTPAIRARRAY pa) {
    return 4+(4+4)*pa.size;
}

static inline char *fixup_fname(BYTESTRING *f) {
    assert(f->len>0);
    char *fname = toku_malloc(f->len+1);
    memcpy(fname, f->data, f->len);
    fname[f->len]=0;
    return fname;
}

int toku_read_rollback_backwards(BREAD, struct roll_entry **item, MEMARENA);
#endif
