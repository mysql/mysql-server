/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef LOG_INTERNAL_H
#define LOG_INTERNAL_H

#ident "$Id$"
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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include "ft-internal.h"
#include "log.h"
#include "toku_list.h"
#include "memarena.h"
#include "logfilemgr.h"
#include "txn.h"
#include "txn_manager.h"
#include <portability/toku_pthread.h>
#include <util/omt.h>
#include "rollback_log_node_cache.h"
#include "txn_child_manager.h"

using namespace toku;
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
    bool output_is_available;           // this is part of the predicate for the output condition.  It's true if no thread is modifying the output (either doing an fsync or otherwise fiddling with the output).

    bool is_open;
    bool write_log_files;
    bool trim_log_files; // for test purposes
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

    uint32_t write_block_size;       // How big should the blocks be written to various logs?

    uint64_t num_writes_to_disk;         // how many times did we write to disk?
    uint64_t bytes_written_to_disk;        // how many bytes have been written to disk?
    tokutime_t time_spent_writing_to_disk; // how much tokutime did we spend writing to disk?
    uint64_t num_wait_buf_long;            // how many times we waited >= 100ms for the in buf

    void (*remove_finalize_callback) (DICTIONARY_ID, void*);  // ydb-level callback to be called when a transaction that ...
    void * remove_finalize_callback_extra;                    // ... deletes a file is committed or when one that creates a file is aborted.
    CACHEFILE rollback_cachefile;
    rollback_log_node_cache rollback_cache;
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
    // the spilled rollback is the block number of the last rollback node that
    // makes up the rollback log chain. 
    BLOCKNUM spilled_rollback_tail;
    // the current rollback node block number we may use. if this is ROLLBACK_NONE,
    // then we need to create one and set it here before using it.
    BLOCKNUM current_rollback; 
};

struct tokutxn {
    // These don't change after create:

    TXNID_PAIR txnid;

    uint64_t snapshot_txnid64; // this is the lsn of the snapshot
    const TXN_SNAPSHOT_TYPE snapshot_type;
    const bool for_recovery;
    const TOKULOGGER logger;
    const TOKUTXN parent;
    // The child txn is protected by the child_txn_manager lock
    // and by the user contract. The user contract states (and is
    // enforced at the ydb layer) that a child txn should not be created
    // while another child exists. The txn_child_manager will protect
    // other threads from trying to read this value while another
    // thread commits/aborts the child
    TOKUTXN child;
    // statically allocated child manager, if this 
    // txn is a root txn, this manager will be used and set to 
    // child_manager for this transaction and all of its children
    txn_child_manager child_manager_s;
    // child manager for this transaction, all of its children,
    // and all of its ancestors
    txn_child_manager* child_manager;
    // These don't change but they're created in a way that's hard to make
    // strictly const.
    DB_TXN *container_db_txn; // reference to DB_TXN that contains this tokutxn
    xid_omt_t *live_root_txn_list; // the root txns live when the root ancestor (self if a root) started.
    XIDS xids; // Represents the xid list

    TOKUTXN snapshot_next;
    TOKUTXN snapshot_prev;

    bool begin_was_logged;
    bool declared_read_only; // true if the txn was declared read only when began
    // These are not read until a commit, prepare, or abort starts, and
    // they're "monotonic" (only go false->true) during operation:
    bool do_fsync;
    bool force_fsync_on_commit;  //This transaction NEEDS an fsync once (if) it commits.  (commit means root txn)

    // Not used until commit, prepare, or abort starts:
    LSN do_fsync_lsn;
    TOKU_XA_XID xa_xid; // for prepared transactions
    TXN_PROGRESS_POLL_FUNCTION progress_poll_fun;
    void *progress_poll_fun_extra;

    toku_mutex_t txn_lock;
    // Protected by the txn lock:
    omt<FT> open_fts; // a collection of the fts that we touched.  Indexed by filenum.
    struct txn_roll_info roll_info; // Info used to manage rollback entries

    // mutex that protects the transition of the state variable
    // the rest of the variables are used by the txn code and 
    // hot indexing to ensure that when hot indexing is processing a 
    // leafentry, a TOKUTXN cannot dissappear or change state out from
    // underneath it
    toku_mutex_t state_lock;
    toku_cond_t state_cond;
    TOKUTXN_STATE state;
    uint32_t num_pin; // number of threads (all hot indexes) that want this
                      // txn to not transition to commit or abort
    uint64_t client_id;
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
    bool       force_fsync_on_commit;  //This transaction NEEDS an fsync once (if) it commits.  (commit means root txn)
    uint64_t   num_rollback_nodes;
    uint64_t   num_rollentries;
    BLOCKNUM   spilled_rollback_head;
    BLOCKNUM   spilled_rollback_tail;
    BLOCKNUM   current_rollback;
};

static inline int toku_logsizeof_uint8_t (uint32_t v __attribute__((__unused__))) {
    return 1;
}

static inline int toku_logsizeof_uint32_t (uint32_t v __attribute__((__unused__))) {
    return 4;
}

static inline int toku_logsizeof_uint64_t (uint32_t v __attribute__((__unused__))) {
    return 8;
}

static inline int toku_logsizeof_bool (uint32_t v __attribute__((__unused__))) {
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

static inline int toku_logsizeof_TXNID_PAIR (TXNID_PAIR txnid __attribute__((__unused__))) {
    return 16;
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
