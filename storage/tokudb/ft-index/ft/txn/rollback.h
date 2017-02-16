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

#include "ft/cachetable/cachetable.h"
#include "ft/serialize/sub_block.h"
#include "ft/txn/txn.h"

#include "util/memarena.h"

typedef struct rollback_log_node *ROLLBACK_LOG_NODE;
typedef struct serialized_rollback_log_node *SERIALIZED_ROLLBACK_LOG_NODE;

void toku_poll_txn_progress_function(TOKUTXN txn, uint8_t is_commit, uint8_t stall_for_checkpoint);

// these functions assert internally that they succeed

// get a rollback node this txn may use for a new entry. if there
// is a current rollback node to use, pin it, otherwise create one.
void toku_get_and_pin_rollback_log_for_new_entry(TOKUTXN txn, ROLLBACK_LOG_NODE *log);

// get a specific rollback by blocknum
void toku_get_and_pin_rollback_log(TOKUTXN txn, BLOCKNUM blocknum, ROLLBACK_LOG_NODE *log);

// unpin a rollback node from the cachetable
void toku_rollback_log_unpin(TOKUTXN txn, ROLLBACK_LOG_NODE log);

// assert that the given log's txnid and sequence match the ones given
void toku_rollback_verify_contents(ROLLBACK_LOG_NODE log, TXNID_PAIR txnid, uint64_t sequence);

// if there is a previous rollback log for the given log node, prefetch it
void toku_maybe_prefetch_previous_rollback_log(TOKUTXN txn, ROLLBACK_LOG_NODE log);

// unpin and rmove a rollback log from the cachetable
void toku_rollback_log_unpin_and_remove(TOKUTXN txn, ROLLBACK_LOG_NODE log);

void *toku_malloc_in_rollback(ROLLBACK_LOG_NODE log, size_t size);
void *toku_memdup_in_rollback(ROLLBACK_LOG_NODE log, const void *v, size_t len);

// given a transaction and a log node, and if the log is too full,
// set the current rollback log to ROLLBACK_NONE and move the current
// node onto the tail of the rollback node chain. further insertions
// into the rollback log for this transaction will force the creation
// of a new rollback log.
//
// this never unpins the rollback log if a spill occurs. the caller
// is responsible for ensuring the given rollback node is unpinned
// if necessary.
void toku_maybe_spill_rollbacks(TOKUTXN txn, ROLLBACK_LOG_NODE log);

void toku_txn_maybe_note_ft (TOKUTXN txn, struct ft *ft);
int toku_logger_txn_rollback_stats(TOKUTXN txn, struct txn_stat *txn_stat);

int toku_find_xid_by_xid (const TXNID &xid, const TXNID &xidfind);

PAIR_ATTR rollback_memory_size(ROLLBACK_LOG_NODE log);

// A high-level rollback log is made up of a chain of rollback log nodes.
// Each rollback log node is represented (separately) in the cachetable by 
// this structure. Each portion of the rollback log chain has a block num
// and a hash to identify it.
struct rollback_log_node {
    int                layout_version;
    int                layout_version_original;
    int                layout_version_read_from_disk;
    uint32_t           build_id;      // build_id (svn rev number) of software that wrote this node to disk
    int                dirty;
    // to which transaction does this node belong?
    TXNID_PAIR         txnid;
    // sequentially, where in the rollback log chain is this node? 
    // the sequence is between 0 and totalnodes-1
    uint64_t           sequence;
    BLOCKNUM           blocknum; // on which block does this node live?
    // which block number is the previous in the chain of rollback nodes 
    // that make up this rollback log?
    BLOCKNUM           previous; 
    struct roll_entry *oldest_logentry;
    struct roll_entry *newest_logentry;
    memarena           rollentry_arena;
    size_t             rollentry_resident_bytecount; // How many bytes for the rollentries that are stored in main memory.
    PAIR               ct_pair;
};

struct serialized_rollback_log_node {
    char *data;
    uint32_t len;
    int n_sub_blocks;
    BLOCKNUM blocknum;
    struct sub_block sub_block[max_sub_blocks];
};
typedef struct serialized_rollback_log_node *SERIALIZED_ROLLBACK_LOG_NODE;

static inline void
toku_static_serialized_rollback_log_destroy(SERIALIZED_ROLLBACK_LOG_NODE log) {
    toku_free(log->data);
}

static inline void
toku_serialized_rollback_log_destroy(SERIALIZED_ROLLBACK_LOG_NODE log) {
    toku_static_serialized_rollback_log_destroy(log);
    toku_free(log);
}

void rollback_empty_log_init(ROLLBACK_LOG_NODE log);
void make_rollback_log_empty(ROLLBACK_LOG_NODE log);

static inline bool rollback_log_is_unused(ROLLBACK_LOG_NODE log) {
    return (log->txnid.parent_id64 == TXNID_NONE);
}
