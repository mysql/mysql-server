/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "portability/toku_stdint.h"

#include "ft/txn/txn_state.h"
#include "ft/serialize/block_table.h"
#include "util/omt.h"

typedef uint64_t TXNID;

typedef struct tokutxn *TOKUTXN;

#define TXNID_NONE_LIVING ((TXNID)0)
#define TXNID_NONE        ((TXNID)0)
#define TXNID_MAX         ((TXNID)-1)

typedef struct txnid_pair_s {
    TXNID parent_id64;
    TXNID child_id64;
} TXNID_PAIR;

static const TXNID_PAIR TXNID_PAIR_NONE = { .parent_id64 = TXNID_NONE, .child_id64 = TXNID_NONE };

// We include the child manager here beacuse it uses the TXNID / TOKUTXN types
#include "ft/txn/txn_child_manager.h"

/* Log Sequence Number (LSN)
 * Make the LSN be a struct instead of an integer so that we get better type checking. */
typedef struct __toku_lsn { uint64_t lsn; } LSN;
static const LSN ZERO_LSN = { .lsn = 0 };
static const LSN MAX_LSN = { .lsn = UINT64_MAX };

//
// Types of snapshots that can be taken by a tokutxn
//  - TXN_SNAPSHOT_NONE: means that there is no snapshot. Reads do not use snapshot reads.
//                       used for SERIALIZABLE and READ UNCOMMITTED
//  - TXN_SNAPSHOT_ROOT: means that all tokutxns use their root transaction's snapshot
//                       used for REPEATABLE READ
//  - TXN_SNAPSHOT_CHILD: means that each child tokutxn creates its own snapshot
//                        used for READ COMMITTED
//

typedef enum __TXN_SNAPSHOT_TYPE { 
    TXN_SNAPSHOT_NONE=0,
    TXN_SNAPSHOT_ROOT=1,
    TXN_SNAPSHOT_CHILD=2
} TXN_SNAPSHOT_TYPE;

typedef toku::omt<struct tokutxn *> txn_omt_t;
typedef toku::omt<TXNID> xid_omt_t;
typedef toku::omt<struct referenced_xid_tuple, struct referenced_xid_tuple *> rx_omt_t;

inline bool txn_pair_is_none(TXNID_PAIR txnid) {
    return txnid.parent_id64 == TXNID_NONE && txnid.child_id64 == TXNID_NONE;
}

inline bool txn_needs_snapshot(TXN_SNAPSHOT_TYPE snapshot_type, struct tokutxn *parent) {
    // we need a snapshot if the snapshot type is a child or
    // if the snapshot type is root and we have no parent.
    // Cases that we don't need a snapshot: when snapshot type is NONE
    //  or when it is ROOT and we have a parent
    return (snapshot_type != TXN_SNAPSHOT_NONE && (parent==NULL || snapshot_type == TXN_SNAPSHOT_CHILD));
}

struct tokulogger;

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
    struct tokulogger *const logger;
    struct tokutxn *const parent;
    // The child txn is protected by the child_txn_manager lock
    // and by the user contract. The user contract states (and is
    // enforced at the ydb layer) that a child txn should not be created
    // while another child exists. The txn_child_manager will protect
    // other threads from trying to read this value while another
    // thread commits/aborts the child
    struct tokutxn *child;

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
    struct XIDS_S *xids; // Represents the xid list

    struct tokutxn *snapshot_next;
    struct tokutxn *snapshot_prev;

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
    toku::omt<struct ft*> open_fts; // a collection of the fts that we touched.  Indexed by filenum.
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
    time_t start_time;
};
typedef struct tokutxn *TOKUTXN;

void toku_txn_lock(struct tokutxn *txn);
void toku_txn_unlock(struct tokutxn *txn);

uint64_t toku_txn_get_root_id(struct tokutxn *txn);
bool txn_declared_read_only(struct tokutxn *txn);

int toku_txn_begin_txn (
    DB_TXN  *container_db_txn,
    struct tokutxn *parent_tokutxn, 
    struct tokutxn **tokutxn, 
    struct tokulogger *logger,
    TXN_SNAPSHOT_TYPE snapshot_type,
    bool read_only
    );

DB_TXN * toku_txn_get_container_db_txn (struct tokutxn *tokutxn);
void toku_txn_set_container_db_txn(struct tokutxn *txn, DB_TXN *db_txn);

// toku_txn_begin_with_xid is called from recovery and has no containing DB_TXN 
int toku_txn_begin_with_xid (
    struct tokutxn *parent_tokutxn, 
    struct tokutxn **tokutxn, 
    struct tokulogger *logger, 
    TXNID_PAIR xid, 
    TXN_SNAPSHOT_TYPE snapshot_type,
    DB_TXN *container_db_txn,
    bool for_recovery,
    bool read_only
    );

void toku_txn_update_xids_in_txn(struct tokutxn *txn, TXNID xid);

int toku_txn_load_txninfo (struct tokutxn *txn, struct txninfo *info);

int toku_txn_commit_txn (struct tokutxn *txn, int nosync,
                         TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);
int toku_txn_commit_with_lsn(struct tokutxn *txn, int nosync, LSN oplsn,
                             TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);

int toku_txn_abort_txn(struct tokutxn *txn,
                       TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);
int toku_txn_abort_with_lsn(struct tokutxn *txn, LSN oplsn,
                            TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);

int toku_txn_discard_txn(struct tokutxn *txn);

void toku_txn_prepare_txn (struct tokutxn *txn, TOKU_XA_XID *xid, int nosync);
// Effect: Do the internal work of preparing a transaction (does not log the prepare record).

void toku_txn_get_prepared_xa_xid(struct tokutxn *txn, TOKU_XA_XID *xa_xid);
// Effect: Fill in the XID information for a transaction.  The caller allocates the XID and the function fills in values.

void toku_txn_maybe_fsync_log(struct tokulogger *logger, LSN do_fsync_lsn, bool do_fsync);

void toku_txn_get_fsync_info(struct tokutxn *ttxn, bool* do_fsync, LSN* do_fsync_lsn);

// Complete and destroy a txn
void toku_txn_close_txn(struct tokutxn *txn);

// Remove a txn from any live txn lists
void toku_txn_complete_txn(struct tokutxn *txn);

// Free the memory of a txn
void toku_txn_destroy_txn(struct tokutxn *txn);

struct XIDS_S *toku_txn_get_xids(struct tokutxn *txn);

// Force fsync on commit
void toku_txn_force_fsync_on_commit(struct tokutxn *txn);

typedef enum {
    TXN_BEGIN,             // total number of transactions begun (does not include recovered txns)
    TXN_READ_BEGIN,        // total number of read only transactions begun (does not include recovered txns)
    TXN_COMMIT,            // successful commits
    TXN_ABORT,
    TXN_STATUS_NUM_ROWS
} txn_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[TXN_STATUS_NUM_ROWS];
} TXN_STATUS_S, *TXN_STATUS;

void toku_txn_get_status(TXN_STATUS s);

bool toku_is_txn_in_live_root_txn_list(const xid_omt_t &live_root_txn_list, TXNID xid);

TXNID toku_get_oldest_in_live_root_txn_list(struct tokutxn *txn);

TOKUTXN_STATE toku_txn_get_state(struct tokutxn *txn);

struct tokulogger_preplist {
    TOKU_XA_XID xid;
    DB_TXN *txn;
};
int toku_logger_recover_txn (struct tokulogger *logger, struct tokulogger_preplist preplist[/*count*/], long count, /*out*/ long *retp, uint32_t flags);

void toku_maybe_log_begin_txn_for_write_operation(struct tokutxn *txn);

// Return whether txn (or it's descendents) have done no work.
bool toku_txn_is_read_only(struct tokutxn *txn);

void toku_txn_lock_state(struct tokutxn *txn);
void toku_txn_unlock_state(struct tokutxn *txn);
void toku_txn_pin_live_txn_unlocked(struct tokutxn *txn);
void toku_txn_unpin_live_txn(struct tokutxn *txn);

bool toku_txn_has_spilled_rollback(struct tokutxn *txn);

uint64_t toku_txn_get_client_id(struct tokutxn *txn);
void toku_txn_set_client_id(struct tokutxn *txn, uint64_t client_id);

time_t toku_txn_get_start_time(struct tokutxn *txn);

//
// This function is used by the leafentry iterators.
// returns TOKUDB_ACCEPT if live transaction context is allowed to read a value
// that is written by transaction with LSN of id
// live transaction context may read value if either id is the root ancestor of context, or if
// id was committed before context's snapshot was taken.
// For id to be committed before context's snapshot was taken, the following must be true:
//  - id < context->snapshot_txnid64 AND id is not in context's live root transaction list
// For the above to NOT be true:
//  - id > context->snapshot_txnid64 OR id is in context's live root transaction list
//
int toku_txn_reads_txnid(TXNID txnid, struct tokutxn *txn);

void txn_status_init(void);

void txn_status_destroy(void);

// For serialize / deserialize

#include "ft/serialize/wbuf.h"

static inline void wbuf_TXNID(struct wbuf *wb, TXNID txnid) {
    wbuf_ulonglong(wb, txnid);
}

static inline void wbuf_nocrc_TXNID(struct wbuf *wb, TXNID txnid) {
    wbuf_nocrc_ulonglong(wb, txnid);
}

static inline void wbuf_nocrc_TXNID_PAIR(struct wbuf *wb, TXNID_PAIR txnid) {
    wbuf_nocrc_ulonglong(wb, txnid.parent_id64);
    wbuf_nocrc_ulonglong(wb, txnid.child_id64);
}

static inline void wbuf_nocrc_LSN(struct wbuf *wb, LSN lsn) {
    wbuf_nocrc_ulonglong(wb, lsn.lsn);
}

static inline void wbuf_LSN(struct wbuf *wb, LSN lsn) {
    wbuf_ulonglong(wb, lsn.lsn);
}

#include "ft/serialize/rbuf.h"

static inline void rbuf_TXNID(struct rbuf *rb, TXNID *txnid) {
    *txnid = rbuf_ulonglong(rb);
}

static inline void rbuf_TXNID_PAIR(struct rbuf *rb, TXNID_PAIR *txnid) {
    txnid->parent_id64 = rbuf_ulonglong(rb);
    txnid->child_id64 = rbuf_ulonglong(rb);
}

static inline void rbuf_ma_TXNID(struct rbuf *rb, memarena *UU(ma), TXNID *txnid) {
    rbuf_TXNID(rb, txnid);
}

static inline void rbuf_ma_TXNID_PAIR (struct rbuf *r, memarena *ma __attribute__((__unused__)), TXNID_PAIR *txnid) {
    rbuf_TXNID_PAIR(r, txnid);
}

static inline LSN rbuf_LSN(struct rbuf *rb) {
    LSN lsn = { .lsn = rbuf_ulonglong(rb) };
    return lsn;
}
