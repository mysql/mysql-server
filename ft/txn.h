/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKUTXN_H
#define TOKUTXN_H

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

#include "txn_manager.h"

void txn_status_init(void);
void txn_status_destroy(void);


inline bool txn_pair_is_none(TXNID_PAIR txnid) {
    return txnid.parent_id64 == TXNID_NONE && txnid.child_id64 == TXNID_NONE;
}

inline bool txn_needs_snapshot(TXN_SNAPSHOT_TYPE snapshot_type, TOKUTXN parent) {
    // we need a snapshot if the snapshot type is a child or
    // if the snapshot type is root and we have no parent.
    // Cases that we don't need a snapshot: when snapshot type is NONE
    //  or when it is ROOT and we have a parent
    return (snapshot_type != TXN_SNAPSHOT_NONE && (parent==NULL || snapshot_type == TXN_SNAPSHOT_CHILD));
}

void toku_txn_lock(TOKUTXN txn);
void toku_txn_unlock(TOKUTXN txn);

uint64_t toku_txn_get_root_id(TOKUTXN txn);
bool txn_declared_read_only(TOKUTXN txn);

int toku_txn_begin_txn (
    DB_TXN  *container_db_txn,
    TOKUTXN parent_tokutxn, 
    TOKUTXN *tokutxn, 
    TOKULOGGER logger,
    TXN_SNAPSHOT_TYPE snapshot_type,
    bool read_only
    );

DB_TXN * toku_txn_get_container_db_txn (TOKUTXN tokutxn);
void toku_txn_set_container_db_txn (TOKUTXN, DB_TXN*);

// toku_txn_begin_with_xid is called from recovery and has no containing DB_TXN 
int toku_txn_begin_with_xid (
    TOKUTXN parent_tokutxn, 
    TOKUTXN *tokutxn, 
    TOKULOGGER logger, 
    TXNID_PAIR xid, 
    TXN_SNAPSHOT_TYPE snapshot_type,
    DB_TXN *container_db_txn,
    bool for_recovery,
    bool read_only
    );

void toku_txn_update_xids_in_txn(TOKUTXN txn, TXNID xid);

int toku_txn_load_txninfo (TOKUTXN txn, TXNINFO info);

int toku_txn_commit_txn (TOKUTXN txn, int nosync,
                         TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);
int toku_txn_commit_with_lsn(TOKUTXN txn, int nosync, LSN oplsn,
                             TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);

int toku_txn_abort_txn(TOKUTXN txn,
                       TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);
int toku_txn_abort_with_lsn(TOKUTXN txn, LSN oplsn,
                            TXN_PROGRESS_POLL_FUNCTION poll, void *poll_extra);

void toku_txn_prepare_txn (TOKUTXN txn, TOKU_XA_XID *xid);
// Effect: Do the internal work of preparing a transaction (does not log the prepare record).

void toku_txn_get_prepared_xa_xid (TOKUTXN, TOKU_XA_XID *);
// Effect: Fill in the XID information for a transaction.  The caller allocates the XID and the function fills in values.

void toku_txn_maybe_fsync_log(TOKULOGGER logger, LSN do_fsync_lsn, bool do_fsync);

void toku_txn_get_fsync_info(TOKUTXN ttxn, bool* do_fsync, LSN* do_fsync_lsn);

// Complete and destroy a txn
void toku_txn_close_txn(TOKUTXN txn);

// Remove a txn from any live txn lists
void toku_txn_complete_txn(TOKUTXN txn);

// Free the memory of a txn
void toku_txn_destroy_txn(TOKUTXN txn);

XIDS toku_txn_get_xids (TOKUTXN);

// Force fsync on commit
void toku_txn_force_fsync_on_commit(TOKUTXN txn);

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

TXNID toku_get_oldest_in_live_root_txn_list(TOKUTXN txn);

#include "txn_state.h"

TOKUTXN_STATE toku_txn_get_state(TOKUTXN txn);

struct tokulogger_preplist {
    TOKU_XA_XID xid;
    DB_TXN *txn;
};
int toku_logger_recover_txn (TOKULOGGER logger, struct tokulogger_preplist preplist[/*count*/], long count, /*out*/ long *retp, uint32_t flags);

void toku_maybe_log_begin_txn_for_write_operation(TOKUTXN txn);

// Return whether txn (or it's descendents) have done no work.
bool toku_txn_is_read_only(TOKUTXN txn);

void toku_txn_lock_state(TOKUTXN txn);
void toku_txn_unlock_state(TOKUTXN txn);
void toku_txn_pin_live_txn_unlocked(TOKUTXN txn);
void toku_txn_unpin_live_txn(TOKUTXN txn);

bool toku_txn_has_spilled_rollback(TOKUTXN txn);

uint64_t toku_txn_get_client_id(TOKUTXN txn);
void toku_txn_set_client_id(TOKUTXN txn, uint64_t client_id);

#endif //TOKUTXN_H
