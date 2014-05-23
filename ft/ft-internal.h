/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FT_INTERNAL_H
#define FT_INTERNAL_H

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

#include <portability/toku_config.h>
#include <toku_race_tools.h>

// Symbol TOKUDB_REVISION is not defined by fractal-tree makefiles, so
// BUILD_ID of 1000 indicates development build of main, not a release build.  
#if defined(TOKUDB_REVISION)
#define BUILD_ID TOKUDB_REVISION
#else
#error
#endif

#include "ft_layout_version.h"
#include "block_allocator.h"
#include "cachetable.h"
#include "fifo.h"
#include "ft-ops.h"
#include "toku_list.h"
#include <util/omt.h>
#include "leafentry.h"
#include "block_table.h"
#include "compress.h"
#include <util/mempool.h>
#include <util/omt.h>
#include "bndata.h"

enum { KEY_VALUE_OVERHEAD = 8 }; /* Must store the two lengths. */
enum { FT_MSG_OVERHEAD = (2 + sizeof(MSN)) };   // the type plus freshness plus MSN
enum { FT_DEFAULT_FANOUT = 16 };
enum { FT_DEFAULT_NODE_SIZE = 4 * 1024 * 1024 };
enum { FT_DEFAULT_BASEMENT_NODE_SIZE = 128 * 1024 };

//
// Field in ftnode_fetch_extra that tells the 
// partial fetch callback what piece of the node
// is needed by the ydb
//
enum ftnode_fetch_type {
    ftnode_fetch_none=1, // no partitions needed.  
    ftnode_fetch_subset, // some subset of partitions needed
    ftnode_fetch_prefetch, // this is part of a prefetch call
    ftnode_fetch_all, // every partition is needed
    ftnode_fetch_keymatch, // one child is needed if it holds both keys
};

static bool is_valid_ftnode_fetch_type(enum ftnode_fetch_type type) UU();
static bool is_valid_ftnode_fetch_type(enum ftnode_fetch_type type) {
    switch (type) {
        case ftnode_fetch_none:
        case ftnode_fetch_subset:
        case ftnode_fetch_prefetch:
        case ftnode_fetch_all:
        case ftnode_fetch_keymatch:
            return true;
        default:
            return false;
    }
}

//
// An extra parameter passed to cachetable functions 
// That is used in all types of fetch callbacks.
// The contents help the partial fetch and fetch
// callbacks retrieve the pieces of a node necessary
// for the ensuing operation (flush, query, ...)
//
struct ftnode_fetch_extra {
    enum ftnode_fetch_type type;
    // needed for reading a node off disk
    FT h;
    // used in the case where type == ftnode_fetch_subset
    // parameters needed to find out which child needs to be decompressed (so it can be read)
    ft_search_t* search;
    DBT range_lock_left_key, range_lock_right_key;
    bool left_is_neg_infty, right_is_pos_infty;
    // states if we should try to aggressively fetch basement nodes 
    // that are not specifically needed for current query, 
    // but may be needed for other cursor operations user is doing
    // For example, if we have not disabled prefetching,
    // and the user is doing a dictionary wide scan, then
    // even though a query may only want one basement node,
    // we fetch all basement nodes in a leaf node.
    bool disable_prefetching;
    // this value will be set during the fetch_callback call by toku_ftnode_fetch_callback or toku_ftnode_pf_req_callback
    // thi callbacks need to evaluate this anyway, so we cache it here so the search code does not reevaluate it
    int child_to_read;
    // when we read internal nodes, we want to read all the data off disk in one I/O
    // then we'll treat it as normal and only decompress the needed partitions etc.

    bool read_all_partitions;
    // Accounting: How many bytes were read, and how much time did we spend doing I/O?
    uint64_t bytes_read;
    tokutime_t io_time;
    tokutime_t decompress_time;
    tokutime_t deserialize_time;
};

struct toku_fifo_entry_key_msn_heaviside_extra {
    DESCRIPTOR desc;
    ft_compare_func cmp;
    FIFO fifo;
    const DBT *key;
    MSN msn;
};

// comparison function for inserting messages into a
// ftnode_nonleaf_childinfo's message_tree
int
toku_fifo_entry_key_msn_heaviside(const int32_t &v, const struct toku_fifo_entry_key_msn_heaviside_extra &extra);

struct toku_fifo_entry_key_msn_cmp_extra {
    DESCRIPTOR desc;
    ft_compare_func cmp;
    FIFO fifo;
};

// same thing for qsort_r
int
toku_fifo_entry_key_msn_cmp(const struct toku_fifo_entry_key_msn_cmp_extra &extrap, const int &a, const int &b);

typedef toku::omt<int32_t> off_omt_t;
typedef toku::omt<int32_t, int32_t, true> marked_off_omt_t;

// data of an available partition of a nonleaf ftnode
struct ftnode_nonleaf_childinfo {
    FIFO buffer;
    off_omt_t broadcast_list;
    marked_off_omt_t fresh_message_tree;
    off_omt_t stale_message_tree;
    uint64_t flow[2];  // current and last checkpoint
};

unsigned int toku_bnc_nbytesinbuf(NONLEAF_CHILDINFO bnc);
int toku_bnc_n_entries(NONLEAF_CHILDINFO bnc);
long toku_bnc_memory_size(NONLEAF_CHILDINFO bnc);
long toku_bnc_memory_used(NONLEAF_CHILDINFO bnc);
void toku_bnc_insert_msg(NONLEAF_CHILDINFO bnc, const void *key, ITEMLEN keylen, const void *data, ITEMLEN datalen, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, DESCRIPTOR desc, ft_compare_func cmp);
void toku_bnc_empty(NONLEAF_CHILDINFO bnc);
void toku_bnc_flush_to_child(FT h, NONLEAF_CHILDINFO bnc, FTNODE child, TXNID parent_oldest_referenced_xid_known);
bool toku_bnc_should_promote(FT ft, NONLEAF_CHILDINFO bnc) __attribute__((const, nonnull));
bool toku_ft_nonleaf_is_gorged(FTNODE node, uint32_t nodesize);

enum reactivity get_nonleaf_reactivity(FTNODE node, unsigned int fanout);
enum reactivity get_node_reactivity(FT ft, FTNODE node);
uint32_t get_leaf_num_entries(FTNODE node);

// data of an available partition of a leaf ftnode
struct ftnode_leaf_basement_node {
    bn_data data_buffer;
    unsigned int seqinsert;         // number of sequential inserts to this leaf 
    MSN max_msn_applied;            // max message sequence number applied
    bool stale_ancestor_messages_applied;
    STAT64INFO_S stat64_delta;      // change in stat64 counters since basement was last written to disk
};

enum   pt_state {  // declare this to be packed so that when used below it will only take 1 byte.
    PT_INVALID = 0,
    PT_ON_DISK = 1,
    PT_COMPRESSED = 2,
    PT_AVAIL = 3};

enum  ftnode_child_tag {
    BCT_INVALID = 0,
    BCT_NULL,
    BCT_SUBBLOCK,
    BCT_LEAF,
    BCT_NONLEAF
};
    
typedef struct  ftnode_child_pointer {
    union {
	struct sub_block *subblock;
	struct ftnode_nonleaf_childinfo *nonleaf;
	struct ftnode_leaf_basement_node *leaf;
    } u;
    enum ftnode_child_tag tag;
} FTNODE_CHILD_POINTER;


struct ftnode_disk_data {
    //
    // stores the offset to the beginning of the partition on disk from the ftnode, and the length, needed to read a partition off of disk
    // the value is only meaningful if the node is clean. If the node is dirty, then the value is meaningless
    //  The START is the distance from the end of the compressed node_info data, to the beginning of the compressed partition
    //  The SIZE is the size of the compressed partition.
    // Rationale:  We cannot store the size from the beginning of the node since we don't know how big the header will be.
    //  However, later when we are doing aligned writes, we won't be able to store the size from the end since we want things to align.
    uint32_t start;
    uint32_t size;
};
#define BP_START(node_dd,i) ((node_dd)[i].start)
#define BP_SIZE(node_dd,i) ((node_dd)[i].size)


// a ftnode partition, associated with a child of a node
struct ftnode_partition {
    // the following three variables are used for nonleaf nodes
    // for leaf nodes, they are meaningless
    BLOCKNUM     blocknum; // blocknum of child 

    // How many bytes worth of work was performed by messages in each buffer.
    uint64_t     workdone;

    //
    // pointer to the partition. Depending on the state, they may be different things
    // if state == PT_INVALID, then the node was just initialized and ptr == NULL
    // if state == PT_ON_DISK, then ptr == NULL
    // if state == PT_COMPRESSED, then ptr points to a struct sub_block*
    // if state == PT_AVAIL, then ptr is:
    //         a struct ftnode_nonleaf_childinfo for internal nodes, 
    //         a struct ftnode_leaf_basement_node for leaf nodes
    //
    struct ftnode_child_pointer ptr;
    //
    // at any time, the partitions may be in one of the following three states (stored in pt_state):
    //   PT_INVALID - means that the partition was just initialized
    //   PT_ON_DISK - means that the partition is not in memory and needs to be read from disk. To use, must read off disk and decompress
    //   PT_COMPRESSED - means that the partition is compressed in memory. To use, must decompress
    //   PT_AVAIL - means the partition is decompressed and in memory
    //
    enum pt_state state; // make this an enum to make debugging easier.  

    // clock count used to for pe_callback to determine if a node should be evicted or not
    // for now, saturating the count at 1
    uint8_t clock_count;
};

struct ftnode {
    MSN      max_msn_applied_to_node_on_disk; // max_msn_applied that will be written to disk
    unsigned int flags;
    BLOCKNUM thisnodename;   // Which block number is this node?
    int    layout_version; // What version of the data structure?
    int    layout_version_original;	// different (<) from layout_version if upgraded from a previous version (useful for debugging)
    int    layout_version_read_from_disk;  // transient, not serialized to disk, (useful for debugging)
    uint32_t build_id;       // build_id (svn rev number) of software that wrote this node to disk
    int    height; /* height is always >= 0.  0 for leaf, >0 for nonleaf. */
    int    dirty;
    uint32_t fullhash;
    int n_children; //for internal nodes, if n_children==fanout+1 then the tree needs to be rebalanced.
                    // for leaf nodes, represents number of basement nodes
    unsigned int    totalchildkeylens;
    DBT *childkeys;   /* Pivot keys.  Child 0's keys are <= childkeys[0].  Child 1's keys are <= childkeys[1].
                                                                        Child 1's keys are > childkeys[0]. */

    // What's the oldest referenced xid that this node knows about? The real oldest
    // referenced xid might be younger, but this is our best estimate. We use it
    // as a heuristic to transition provisional mvcc entries from provisional to
    // committed (from implicity committed to really committed).
    //
    // A better heuristic would be the oldest live txnid, but we use this since it
    // still works well most of the time, and its readily available on the inject
    // code path.
    TXNID oldest_referenced_xid_known;

    // array of size n_children, consisting of ftnode partitions
    // each one is associated with a child
    // for internal nodes, the ith partition corresponds to the ith message buffer
    // for leaf nodes, the ith partition corresponds to the ith basement node
    struct ftnode_partition *bp;
    PAIR ct_pair;
};

// ftnode partition macros
// BP stands for ftnode_partition
#define BP_BLOCKNUM(node,i) ((node)->bp[i].blocknum)
#define BP_STATE(node,i) ((node)->bp[i].state)
#define BP_WORKDONE(node, i)((node)->bp[i].workdone)

//
// macros for managing a node's clock
// Should be managed by ft-ops.c, NOT by serialize/deserialize
//

//
// BP_TOUCH_CLOCK uses a compare and swap because multiple threads
// that have a read lock on an internal node may try to touch the clock
// simultaneously
//
#define BP_TOUCH_CLOCK(node, i) ((node)->bp[i].clock_count = 1)
#define BP_SWEEP_CLOCK(node, i) ((node)->bp[i].clock_count = 0)
#define BP_SHOULD_EVICT(node, i) ((node)->bp[i].clock_count == 0)
// not crazy about having these two here, one is for the case where we create new
// nodes, such as in splits and creating new roots, and the other is for when
// we are deserializing a node and not all bp's are touched
#define BP_INIT_TOUCHED_CLOCK(node, i) ((node)->bp[i].clock_count = 1)
#define BP_INIT_UNTOUCHED_CLOCK(node, i) ((node)->bp[i].clock_count = 0)

// internal node macros
static inline void set_BNULL(FTNODE node, int i) {
    paranoid_invariant(i >= 0);
    paranoid_invariant(i < node->n_children);
    node->bp[i].ptr.tag = BCT_NULL;
}
static inline bool is_BNULL (FTNODE node, int i) {
    paranoid_invariant(i >= 0);
    paranoid_invariant(i < node->n_children);
    return node->bp[i].ptr.tag == BCT_NULL;
}
static inline NONLEAF_CHILDINFO BNC(FTNODE node, int i) {
    paranoid_invariant(i >= 0);
    paranoid_invariant(i < node->n_children);
    FTNODE_CHILD_POINTER p = node->bp[i].ptr;
    paranoid_invariant(p.tag==BCT_NONLEAF);
    return p.u.nonleaf;
}
static inline void set_BNC(FTNODE node, int i, NONLEAF_CHILDINFO nl) {
    paranoid_invariant(i >= 0);
    paranoid_invariant(i < node->n_children);
    FTNODE_CHILD_POINTER *p = &node->bp[i].ptr;
    p->tag = BCT_NONLEAF;
    p->u.nonleaf = nl;
}

static inline BASEMENTNODE BLB(FTNODE node, int i) {
    paranoid_invariant(i >= 0);
    // The optimizer really doesn't like it when we compare
    // i to n_children as signed integers. So we assert that
    // n_children is in fact positive before doing a comparison
    // on the values forcibly cast to unsigned ints.
    paranoid_invariant(node->n_children > 0);
    paranoid_invariant((unsigned) i < (unsigned) node->n_children);
    FTNODE_CHILD_POINTER p = node->bp[i].ptr;
    paranoid_invariant(p.tag==BCT_LEAF);
    return p.u.leaf;
}
static inline void set_BLB(FTNODE node, int i, BASEMENTNODE bn) {
    paranoid_invariant(i >= 0);
    paranoid_invariant(i < node->n_children);
    FTNODE_CHILD_POINTER *p = &node->bp[i].ptr;
    p->tag = BCT_LEAF;
    p->u.leaf = bn;
}

static inline SUB_BLOCK BSB(FTNODE node, int i) {
    paranoid_invariant(i >= 0);
    paranoid_invariant(i < node->n_children);
    FTNODE_CHILD_POINTER p = node->bp[i].ptr;
    paranoid_invariant(p.tag==BCT_SUBBLOCK);
    return p.u.subblock;
}
static inline void set_BSB(FTNODE node, int i, SUB_BLOCK sb) {
    paranoid_invariant(i >= 0);
    paranoid_invariant(i < node->n_children);
    FTNODE_CHILD_POINTER *p = &node->bp[i].ptr;
    p->tag = BCT_SUBBLOCK;
    p->u.subblock = sb;
}

// ftnode leaf basementnode macros, 
#define BLB_MAX_MSN_APPLIED(node,i) (BLB(node,i)->max_msn_applied)
#define BLB_MAX_DSN_APPLIED(node,i) (BLB(node,i)->max_dsn_applied)
#define BLB_DATA(node,i) (&(BLB(node,i)->data_buffer))
#define BLB_NBYTESINDATA(node,i) (BLB_DATA(node,i)->get_disk_size())
#define BLB_SEQINSERT(node,i) (BLB(node,i)->seqinsert)

/* pivot flags  (must fit in 8 bits) */
enum {
    FT_PIVOT_TRUNC = 4,
    FT_PIVOT_FRONT_COMPRESS = 8,
};

uint32_t compute_child_fullhash (CACHEFILE cf, FTNODE node, int childnum);

// The ft_header is not managed by the cachetable.  Instead, it hangs off the cachefile as userdata.

enum ft_type {FT_CURRENT=1, FT_CHECKPOINT_INPROGRESS};

struct ft_header {
    enum ft_type type;

    int dirty;

    // Free-running counter incremented once per checkpoint (toggling LSB).
    // LSB indicates which header location is used on disk so this
    // counter is effectively a boolean which alternates with each checkpoint.
    uint64_t checkpoint_count;
    // LSN of creation of "checkpoint-begin" record in log.
    LSN checkpoint_lsn;

    // see ft_layout_version.h.  maybe don't need this if we assume
    // it's always the current version after deserializing
    const int layout_version;
    // different (<) from layout_version if upgraded from a previous
    // version (useful for debugging)
    const int layout_version_original;
    // build_id (svn rev number) of software that wrote this node to
    // disk. (read from disk, overwritten when written to disk, I
    // think).
    const uint32_t build_id;
    // build_id of software that created this tree
    const uint32_t build_id_original;

    // time this tree was created
    const uint64_t time_of_creation;
    // and the root transaction id that created it
    TXNID root_xid_that_created;
    // last time this header was serialized to disk (read from disk,
    // overwritten when written to disk)
    uint64_t time_of_last_modification;
    // last time that this tree was verified
    uint64_t time_of_last_verification;

    // this field is essentially a const
    BLOCKNUM root_blocknum;

    const unsigned int flags;

    //protected by toku_ft_lock
    unsigned int nodesize; 
    unsigned int basementnodesize;
    enum toku_compression_method compression_method;
    unsigned int fanout;

    // Current Minimum MSN to be used when upgrading pre-MSN FT's.
    // This is decremented from our currnt MIN_MSN so as not to clash
    // with any existing 'normal' MSN's.
    MSN highest_unused_msn_for_upgrade;
    // Largest MSN ever injected into the tree.  Used to set the MSN for
    // messages as they get injected.
    MSN max_msn_in_ft;

    // last time that a hot optimize operation was begun
    uint64_t time_of_last_optimize_begin;
    // last time that a hot optimize operation was successfully completed
    uint64_t time_of_last_optimize_end;
    // the number of hot optimize operations currently in progress on this tree
    uint32_t count_of_optimize_in_progress;
    // the number of hot optimize operations in progress on this tree at the time of the last crash  (this field is in-memory only)
    uint32_t count_of_optimize_in_progress_read_from_disk;
    // all messages before this msn have been applied to leaf nodes
    MSN msn_at_start_of_last_completed_optimize;

    STAT64INFO_S on_disk_stats;
};

// ft_header is always the current version.
struct ft {
    FT_HEADER h;
    FT_HEADER checkpoint_header;

    // These are (mostly) read-only.

    CACHEFILE cf;
    // unique id for dictionary
    DICTIONARY_ID dict_id;
    ft_compare_func compare_fun;
    ft_update_func update_fun;

    // protected by locktree
    DESCRIPTOR_S descriptor;
    // protected by locktree and user. User 
    // makes sure this is only changed
    // when no activity on tree
    DESCRIPTOR_S cmp_descriptor;

    // These are not read-only:

    // protected by blocktable lock
    BLOCK_TABLE blocktable;

    // protected by atomic builtins
    STAT64INFO_S in_memory_stats;

    // transient, not serialized to disk.  updated when we do write to
    // disk.  tells us whether we can do partial eviction (we can't if
    // the on-disk layout version is from before basement nodes)
    int layout_version_read_from_disk;

    // Logically the reference count is zero if live_ft_handles is empty, txns is 0, and pinned_by_checkpoint is false.

    // ft_ref_lock protects modifying live_ft_handles, txns, and pinned_by_checkpoint.
    toku_mutex_t ft_ref_lock;
    struct toku_list live_ft_handles;
    // Number of transactions that are using this FT.  you should only be able
    // to modify this if you have a valid handle in live_ft_handles
    uint32_t num_txns;
    // A checkpoint is running.  If true, then keep this header around for checkpoint, like a transaction
    bool pinned_by_checkpoint;

    // is this ft a blackhole? if so, all messages are dropped.
    bool blackhole;
};

// Allocate a DB struct off the stack and only set its comparison
// descriptor. We don't bother setting any other fields because
// the comparison function doesn't need it, and we would like to
// reduce the CPU work done per comparison.
#define FAKE_DB(db, desc) struct __toku_db db; do { db.cmp_descriptor = desc; } while (0)

struct ft_options {
    unsigned int nodesize;
    unsigned int basementnodesize;
    enum toku_compression_method compression_method;
    unsigned int fanout;
    unsigned int flags;
    ft_compare_func compare_fun;
    ft_update_func update_fun;
};

struct ft_handle {
    // The fractal tree.
    FT ft;

    on_redirect_callback redirect_callback;
    void *redirect_callback_extra;
    struct toku_list live_ft_handle_link;
    bool did_set_flags;

    struct ft_options options;
};

PAIR_ATTR make_ftnode_pair_attr(FTNODE node);
PAIR_ATTR make_invalid_pair_attr(void);

/* serialization code */
void
toku_create_compressed_partition_from_available(
    FTNODE node,
    int childnum,
    enum toku_compression_method compression_method,
    SUB_BLOCK sb
    );
void rebalance_ftnode_leaf(FTNODE node, unsigned int basementnodesize);
int toku_serialize_ftnode_to_memory (FTNODE node,
                                      FTNODE_DISK_DATA* ndd,
                                      unsigned int basementnodesize,
                                      enum toku_compression_method compression_method,
                                      bool do_rebalancing,
                                      bool in_parallel,
                              /*out*/ size_t *n_bytes_to_write,
                              /*out*/ size_t *n_uncompressed_bytes,
                              /*out*/ char  **bytes_to_write);
int toku_serialize_ftnode_to(int fd, BLOCKNUM, FTNODE node, FTNODE_DISK_DATA* ndd, bool do_rebalancing, FT h, bool for_checkpoint);
int toku_serialize_rollback_log_to (int fd, ROLLBACK_LOG_NODE log, SERIALIZED_ROLLBACK_LOG_NODE serialized_log, bool is_serialized,
                                    FT h, bool for_checkpoint);
void toku_serialize_rollback_log_to_memory_uncompressed(ROLLBACK_LOG_NODE log, SERIALIZED_ROLLBACK_LOG_NODE serialized);
int toku_deserialize_rollback_log_from (int fd, BLOCKNUM blocknum, ROLLBACK_LOG_NODE *logp, FT h);
int toku_deserialize_bp_from_disk(FTNODE node, FTNODE_DISK_DATA ndd, int childnum, int fd, struct ftnode_fetch_extra* bfe);
int toku_deserialize_bp_from_compressed(FTNODE node, int childnum, struct ftnode_fetch_extra *bfe);
int toku_deserialize_ftnode_from (int fd, BLOCKNUM off, uint32_t /*fullhash*/, FTNODE *ftnode, FTNODE_DISK_DATA* ndd, struct ftnode_fetch_extra* bfe);

// <CER> For verifying old, non-upgraded nodes (versions 13 and 14).
int
decompress_from_raw_block_into_rbuf(uint8_t *raw_block, size_t raw_block_size, struct rbuf *rb, BLOCKNUM blocknum);
// 
    
//////////////// <CER> TODO: Move these function declarations
int
deserialize_ft_from_fd_into_rbuf(int fd,
                                 toku_off_t offset_of_header,
                                 struct rbuf *rb,
                                 uint64_t *checkpoint_count,
                                 LSN *checkpoint_lsn,
                                 uint32_t * version_p);

int
deserialize_ft_versioned(int fd, struct rbuf *rb, FT *ft, uint32_t version);

void read_block_from_fd_into_rbuf(
    int fd, 
    BLOCKNUM blocknum,
    FT h,
    struct rbuf *rb
    );

int
read_compressed_sub_block(struct rbuf *rb, struct sub_block *sb);

int
verify_ftnode_sub_block (struct sub_block *sb);

void
just_decompress_sub_block(struct sub_block *sb);

/* Beginning of ft-node-deserialize.c helper functions. */
void initialize_ftnode(FTNODE node, BLOCKNUM blocknum);
int read_and_check_magic(struct rbuf *rb);
int read_and_check_version(FTNODE node, struct rbuf *rb);
void read_node_info(FTNODE node, struct rbuf *rb, int version);
void allocate_and_read_partition_offsets(FTNODE node, struct rbuf *rb, FTNODE_DISK_DATA *ndd);
int check_node_info_checksum(struct rbuf *rb);
void read_legacy_node_info(FTNODE node, struct rbuf *rb, int version);
int check_legacy_end_checksum(struct rbuf *rb);
/* End of ft-node-deserialization.c helper functions. */

unsigned int toku_serialize_ftnode_size(FTNODE node); /* How much space will it take? */

void toku_verify_or_set_counts(FTNODE);

size_t toku_serialize_ft_size (FT_HEADER h);
void toku_serialize_ft_to (int fd, FT_HEADER h, BLOCK_TABLE blocktable, CACHEFILE cf);
void toku_serialize_ft_to_wbuf (
    struct wbuf *wbuf, 
    FT_HEADER h, 
    DISKOFF translation_location_on_disk, 
    DISKOFF translation_size_on_disk
    );
int toku_deserialize_ft_from (int fd, LSN max_acceptable_lsn, FT *ft);
void toku_serialize_descriptor_contents_to_fd(int fd, const DESCRIPTOR desc, DISKOFF offset);
void toku_serialize_descriptor_contents_to_wbuf(struct wbuf *wb, const DESCRIPTOR desc);
BASEMENTNODE toku_create_empty_bn(void);
BASEMENTNODE toku_create_empty_bn_no_buffer(void); // create a basement node with a null buffer.
NONLEAF_CHILDINFO toku_clone_nl(NONLEAF_CHILDINFO orig_childinfo);
BASEMENTNODE toku_clone_bn(BASEMENTNODE orig_bn);
NONLEAF_CHILDINFO toku_create_empty_nl(void);
// FIXME needs toku prefix
void destroy_basement_node (BASEMENTNODE bn);
// FIXME needs toku prefix
void destroy_nonleaf_childinfo (NONLEAF_CHILDINFO nl);
void toku_destroy_ftnode_internals(FTNODE node);
void toku_ftnode_free (FTNODE *node);
bool is_entire_node_in_memory(FTNODE node);
void toku_assert_entire_node_in_memory(FTNODE node);

// append a child node to a parent node
void toku_ft_nonleaf_append_child(FTNODE node, FTNODE child, const DBT *pivotkey);

// append a message to a nonleaf node child buffer
void toku_ft_append_to_child_buffer(ft_compare_func compare_fun, DESCRIPTOR desc, FTNODE node, int childnum, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, const DBT *key, const DBT *val);

STAT64INFO_S toku_get_and_clear_basement_stats(FTNODE leafnode);

//#define SLOW
#ifdef SLOW
#define VERIFY_NODE(t,n) (toku_verify_or_set_counts(n), toku_verify_estimates(t,n))
#else
#define VERIFY_NODE(t,n) ((void)0)
#endif

void toku_ft_status_update_pivot_fetch_reason(struct ftnode_fetch_extra *bfe);
void toku_ft_status_update_flush_reason(FTNODE node, uint64_t uncompressed_bytes_flushed, uint64_t bytes_written, tokutime_t write_time, bool for_checkpoint);
void toku_ft_status_update_serialize_times(FTNODE node, tokutime_t serialize_time, tokutime_t compress_time);
void toku_ft_status_update_deserialize_times(FTNODE node, tokutime_t deserialize_time, tokutime_t decompress_time);

void toku_ftnode_clone_callback(void* value_data, void** cloned_value_data, long* clone_size, PAIR_ATTR* new_attr, bool for_checkpoint, void* write_extraargs);
void toku_ftnode_checkpoint_complete_callback(void *value_data);
void toku_ftnode_flush_callback (CACHEFILE cachefile, int fd, BLOCKNUM nodename, void *ftnode_v, void** UU(disk_data), void *extraargs, PAIR_ATTR size, PAIR_ATTR* new_size, bool write_me, bool keep_me, bool for_checkpoint, bool is_clone);
int toku_ftnode_fetch_callback (CACHEFILE cachefile, PAIR p, int fd, BLOCKNUM nodename, uint32_t fullhash, void **ftnode_pv, void** UU(disk_data), PAIR_ATTR *sizep, int*dirty, void*extraargs);
void toku_ftnode_pe_est_callback(void* ftnode_pv, void* disk_data, long* bytes_freed_estimate, enum partial_eviction_cost *cost, void* write_extraargs);
int toku_ftnode_pe_callback(void *ftnode_pv, PAIR_ATTR old_attr, void *extraargs,
                            void (*finalize)(PAIR_ATTR new_attr, void *extra), void *finalize_extra);
bool toku_ftnode_pf_req_callback(void* ftnode_pv, void* read_extraargs);
int toku_ftnode_pf_callback(void* ftnode_pv, void* UU(disk_data), void* read_extraargs, int fd, PAIR_ATTR* sizep);
int toku_ftnode_cleaner_callback( void *ftnode_pv, BLOCKNUM blocknum, uint32_t fullhash, void *extraargs);
void toku_evict_bn_from_memory(FTNODE node, int childnum, FT h);
BASEMENTNODE toku_detach_bn(FTNODE node, int childnum);

// Given pinned node and pinned child, split child into two
// and update node with information about its new child.
void toku_ft_split_child(
    FT h,
    FTNODE node,
    int childnum,
    FTNODE child,
    enum split_mode split_mode
    );
// Given pinned node, merge childnum with a neighbor and update node with
// information about the change
void toku_ft_merge_child(
    FT ft,
    FTNODE node,
    int childnum
    );
static inline CACHETABLE_WRITE_CALLBACK get_write_callbacks_for_node(FT h) {
    CACHETABLE_WRITE_CALLBACK wc;
    wc.flush_callback = toku_ftnode_flush_callback;
    wc.pe_est_callback = toku_ftnode_pe_est_callback;
    wc.pe_callback = toku_ftnode_pe_callback;
    wc.cleaner_callback = toku_ftnode_cleaner_callback;
    wc.clone_callback = toku_ftnode_clone_callback;
    wc.checkpoint_complete_callback = toku_ftnode_checkpoint_complete_callback;
    wc.write_extraargs = h;
    return wc;
}

static const FTNODE null_ftnode=0;

/* an ft cursor is represented as a kv pair in a tree */
struct ft_cursor {
    struct toku_list cursors_link;
    FT_HANDLE ft_handle;
    DBT key, val;             // The key-value pair that the cursor currently points to
    DBT range_lock_left_key, range_lock_right_key;
    bool prefetching;
    bool left_is_neg_infty, right_is_pos_infty;
    bool is_snapshot_read; // true if query is read_committed, false otherwise
    bool is_leaf_mode;
    bool disable_prefetching;
    bool is_temporary;
    int out_of_range_error;
    int direction;
    TOKUTXN ttxn;
    FT_CHECK_INTERRUPT_CALLBACK interrupt_cb;
    void *interrupt_cb_extra;
};

//
// Helper function to fill a ftnode_fetch_extra with data
// that will tell the fetch callback that the entire node is
// necessary. Used in cases where the entire node
// is required, such as for flushes.
//
static inline void fill_bfe_for_full_read(struct ftnode_fetch_extra *bfe, FT h) {
    bfe->type = ftnode_fetch_all;
    bfe->h = h;
    bfe->search = NULL;
    toku_init_dbt(&bfe->range_lock_left_key);
    toku_init_dbt(&bfe->range_lock_right_key);
    bfe->left_is_neg_infty = false;
    bfe->right_is_pos_infty = false;
    bfe->child_to_read = -1;
    bfe->disable_prefetching = false;
    bfe->read_all_partitions = false;
    bfe->bytes_read = 0;
    bfe->io_time = 0;
    bfe->deserialize_time = 0;
    bfe->decompress_time = 0;
}

//
// Helper function to fill a ftnode_fetch_extra with data
// that will tell the fetch callback that an explicit range of children is
// necessary. Used in cases where the portion of the node that is required
// is known in advance, e.g. for keysrange when the left and right key
// are in the same basement node.
//
static inline void fill_bfe_for_keymatch(
    struct ftnode_fetch_extra *bfe,
    FT h,
    const DBT *left,
    const DBT *right,
    bool disable_prefetching,
    bool read_all_partitions
    )
{
    paranoid_invariant(h->h->type == FT_CURRENT);
    bfe->type = ftnode_fetch_keymatch;
    bfe->h = h;
    bfe->search = nullptr;
    toku_init_dbt(&bfe->range_lock_left_key);
    toku_init_dbt(&bfe->range_lock_right_key);
    if (left) {
        toku_copyref_dbt(&bfe->range_lock_left_key, *left);
    }

    if (right) {
        toku_copyref_dbt(&bfe->range_lock_right_key, *right);
    }
    bfe->left_is_neg_infty = left == nullptr;
    bfe->right_is_pos_infty = right == nullptr;
    bfe->child_to_read = -1;
    bfe->disable_prefetching = disable_prefetching;
    bfe->read_all_partitions = read_all_partitions;
    bfe->bytes_read = 0;
    bfe->io_time = 0;
    bfe->deserialize_time = 0;
    bfe->decompress_time = 0;
}

//
// Helper function to fill a ftnode_fetch_extra with data
// that will tell the fetch callback that some subset of the node
// necessary. Used in cases where some of the node is required
// such as for a point query.
//
static inline void fill_bfe_for_subset_read(
    struct ftnode_fetch_extra *bfe,
    FT h,
    ft_search_t* search,
    const DBT *left,
    const DBT *right,
    bool left_is_neg_infty,
    bool right_is_pos_infty,
    bool disable_prefetching,
    bool read_all_partitions
    )
{
    paranoid_invariant(h->h->type == FT_CURRENT);
    bfe->type = ftnode_fetch_subset;
    bfe->h = h;
    bfe->search = search;
    toku_init_dbt(&bfe->range_lock_left_key);
    toku_init_dbt(&bfe->range_lock_right_key);
    if (left) {
        toku_copyref_dbt(&bfe->range_lock_left_key, *left);
    }
    if (right) {
        toku_copyref_dbt(&bfe->range_lock_right_key, *right);
    }
    bfe->left_is_neg_infty = left_is_neg_infty;
    bfe->right_is_pos_infty = right_is_pos_infty;
    bfe->child_to_read = -1;
    bfe->disable_prefetching = disable_prefetching;
    bfe->read_all_partitions = read_all_partitions;
    bfe->bytes_read = 0;
    bfe->io_time = 0;
    bfe->deserialize_time = 0;
    bfe->decompress_time = 0;
}

//
// Helper function to fill a ftnode_fetch_extra with data
// that will tell the fetch callback that no partitions are
// necessary, only the pivots and/or subtree estimates.
// Currently used for stat64.
//
static inline void fill_bfe_for_min_read(struct ftnode_fetch_extra *bfe, FT h) {
    paranoid_invariant(h->h->type == FT_CURRENT);
    bfe->type = ftnode_fetch_none;
    bfe->h = h;
    bfe->search = NULL;
    toku_init_dbt(&bfe->range_lock_left_key);
    toku_init_dbt(&bfe->range_lock_right_key);
    bfe->left_is_neg_infty = false;
    bfe->right_is_pos_infty = false;
    bfe->child_to_read = -1;
    bfe->disable_prefetching = false;
    bfe->read_all_partitions = false;
    bfe->bytes_read = 0;
    bfe->io_time = 0;
    bfe->deserialize_time = 0;
    bfe->decompress_time = 0;
}

static inline void destroy_bfe_for_prefetch(struct ftnode_fetch_extra *bfe) {
    paranoid_invariant(bfe->type == ftnode_fetch_prefetch);
    toku_destroy_dbt(&bfe->range_lock_left_key);
    toku_destroy_dbt(&bfe->range_lock_right_key);
}

// this is in a strange place because it needs the cursor struct to be defined
static inline void fill_bfe_for_prefetch(struct ftnode_fetch_extra *bfe,
                                         FT h,
                                         FT_CURSOR c) {
    paranoid_invariant(h->h->type == FT_CURRENT);
    bfe->type = ftnode_fetch_prefetch;
    bfe->h = h;
    bfe->search = NULL;
    toku_init_dbt(&bfe->range_lock_left_key);
    toku_init_dbt(&bfe->range_lock_right_key);
    const DBT *left = &c->range_lock_left_key;
    if (left->data) {
        toku_clone_dbt(&bfe->range_lock_left_key, *left);
    }
    const DBT *right = &c->range_lock_right_key;
    if (right->data) {
        toku_clone_dbt(&bfe->range_lock_right_key, *right);
    }
    bfe->left_is_neg_infty = c->left_is_neg_infty;
    bfe->right_is_pos_infty = c->right_is_pos_infty;
    bfe->child_to_read = -1;
    bfe->disable_prefetching = c->disable_prefetching;
    bfe->read_all_partitions = false;
    bfe->bytes_read = 0;
    bfe->io_time = 0;
    bfe->deserialize_time = 0;
    bfe->decompress_time = 0;
}

struct ancestors {
    FTNODE   node;     // This is the root node if next is NULL.
    int       childnum; // which buffer holds messages destined to the node whose ancestors this list represents.
    ANCESTORS next;     // Parent of this node (so next->node.(next->childnum) refers to this node).
};
struct pivot_bounds {
    const DBT * const lower_bound_exclusive;
    const DBT * const upper_bound_inclusive; // NULL to indicate negative or positive infinity (which are in practice exclusive since there are now transfinite keys in messages).
};

__attribute__((nonnull))
void toku_move_ftnode_messages_to_stale(FT ft, FTNODE node);
void toku_apply_ancestors_messages_to_node (FT_HANDLE t, FTNODE node, ANCESTORS ancestors, struct pivot_bounds const * const bounds, bool* msgs_applied, int child_to_read);
__attribute__((nonnull))
bool toku_ft_leaf_needs_ancestors_messages(FT ft, FTNODE node, ANCESTORS ancestors, struct pivot_bounds const * const bounds, MSN *const max_msn_in_path, int child_to_read);
__attribute__((nonnull))
void toku_ft_bn_update_max_msn(FTNODE node, MSN max_msn_applied, int child_to_read);

__attribute__((const,nonnull))
size_t toku_ft_msg_memsize_in_fifo(FT_MSG msg);

int
toku_ft_search_which_child(
    DESCRIPTOR desc,
    ft_compare_func cmp,
    FTNODE node,
    ft_search_t *search
    );

bool
toku_bfe_wants_child_available (struct ftnode_fetch_extra* bfe, int childnum);

int
toku_bfe_leftmost_child_wanted(struct ftnode_fetch_extra *bfe, FTNODE node);
int
toku_bfe_rightmost_child_wanted(struct ftnode_fetch_extra *bfe, FTNODE node);

// allocate a block number
// allocate and initialize a ftnode
// put the ftnode into the cache table
void toku_create_new_ftnode (FT_HANDLE t, FTNODE *result, int height, int n_children);

// Effect: Fill in N as an empty ftnode.
void toku_initialize_empty_ftnode (FTNODE n, BLOCKNUM nodename, int height, int num_children, 
                                    int layout_version, unsigned int flags);

int toku_ftnode_which_child(FTNODE node, const DBT *k,
                            DESCRIPTOR desc, ft_compare_func cmp)
    __attribute__((__warn_unused_result__));

/**
 * Finds the next child for HOT to flush to, given that everything up to
 * and including k has been flattened.
 *
 * If k falls between pivots in node, then we return the childnum where k
 * lies.
 *
 * If k is equal to some pivot, then we return the next (to the right)
 * childnum.
 */
int toku_ftnode_hot_next_child(FTNODE node,
                               const DBT *k,
                               DESCRIPTOR desc,
                               ft_compare_func cmp);

/* Stuff for testing */
// toku_testsetup_initialize() must be called before any other test_setup_xxx() functions are called.
void toku_testsetup_initialize(void);
int toku_testsetup_leaf(FT_HANDLE ft_h, BLOCKNUM *blocknum, int n_children, char **keys, int *keylens);
int toku_testsetup_nonleaf (FT_HANDLE ft_h, int height, BLOCKNUM *diskoff, int n_children, BLOCKNUM *children, char **keys, int *keylens);
int toku_testsetup_root(FT_HANDLE ft_h, BLOCKNUM);
int toku_testsetup_get_sersize(FT_HANDLE ft_h, BLOCKNUM); // Return the size on disk.
int toku_testsetup_insert_to_leaf (FT_HANDLE ft_h, BLOCKNUM, const char *key, int keylen, const char *val, int vallen);
int toku_testsetup_insert_to_nonleaf (FT_HANDLE ft_h, BLOCKNUM, enum ft_msg_type, const char *key, int keylen, const char *val, int vallen);
void toku_pin_node_with_min_bfe(FTNODE* node, BLOCKNUM b, FT_HANDLE t);

void toku_ft_root_put_msg(FT h, FT_MSG msg, txn_gc_info *gc_info);

void
toku_get_node_for_verify(
    BLOCKNUM blocknum,
    FT_HANDLE ft_h,
    FTNODE* nodep
    );

int
toku_verify_ftnode (FT_HANDLE ft_h,
                    MSN rootmsn, MSN parentmsn, bool messages_exist_above,
                     FTNODE node, int height,
                     const DBT *lesser_pivot,               // Everything in the subtree should be > lesser_pivot.  (lesser_pivot==NULL if there is no lesser pivot.)
                     const DBT *greatereq_pivot,            // Everything in the subtree should be <= lesser_pivot.  (lesser_pivot==NULL if there is no lesser pivot.)
                     int (*progress_callback)(void *extra, float progress), void *progress_extra,
                     int recurse, int verbose, int keep_going_on_failure)
    __attribute__ ((warn_unused_result));

int toku_db_badformat(void) __attribute__((__warn_unused_result__));

typedef enum {
    FT_UPGRADE_FOOTPRINT = 0,
    FT_UPGRADE_STATUS_NUM_ROWS
} ft_upgrade_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[FT_UPGRADE_STATUS_NUM_ROWS];
} FT_UPGRADE_STATUS_S, *FT_UPGRADE_STATUS;

void toku_ft_upgrade_get_status(FT_UPGRADE_STATUS);

typedef enum {
    LE_MAX_COMMITTED_XR = 0,
    LE_MAX_PROVISIONAL_XR,
    LE_EXPANDED,
    LE_MAX_MEMSIZE,
    LE_APPLY_GC_BYTES_IN,
    LE_APPLY_GC_BYTES_OUT,
    LE_NORMAL_GC_BYTES_IN,
    LE_NORMAL_GC_BYTES_OUT,
    LE_STATUS_NUM_ROWS
} le_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[LE_STATUS_NUM_ROWS];
} LE_STATUS_S, *LE_STATUS;

void toku_le_get_status(LE_STATUS);

typedef enum {
    FT_UPDATES = 0,
    FT_UPDATES_BROADCAST,
    FT_DESCRIPTOR_SET,
    FT_MSN_DISCARDS,                           // how many messages were ignored by leaf because of msn
    FT_TOTAL_RETRIES,                          // total number of search retries due to TRY_AGAIN
    FT_SEARCH_TRIES_GT_HEIGHT,                 // number of searches that required more tries than the height of the tree
    FT_SEARCH_TRIES_GT_HEIGHTPLUS3,            // number of searches that required more tries than the height of the tree plus three
    FT_DISK_FLUSH_LEAF,                        // number of leaf nodes flushed to disk,    not for checkpoint
    FT_DISK_FLUSH_LEAF_BYTES,                  // number of leaf nodes flushed to disk,    not for checkpoint
    FT_DISK_FLUSH_LEAF_UNCOMPRESSED_BYTES,                  // number of leaf nodes flushed to disk,    not for checkpoint
    FT_DISK_FLUSH_LEAF_TOKUTIME,               // number of leaf nodes flushed to disk,    not for checkpoint
    FT_DISK_FLUSH_NONLEAF,                     // number of nonleaf nodes flushed to disk, not for checkpoint
    FT_DISK_FLUSH_NONLEAF_BYTES,               // number of nonleaf nodes flushed to disk, not for checkpoint
    FT_DISK_FLUSH_NONLEAF_UNCOMPRESSED_BYTES,               // number of nonleaf nodes flushed to disk, not for checkpoint
    FT_DISK_FLUSH_NONLEAF_TOKUTIME,            // number of nonleaf nodes flushed to disk, not for checkpoint
    FT_DISK_FLUSH_LEAF_FOR_CHECKPOINT,         // number of leaf nodes flushed to disk for checkpoint
    FT_DISK_FLUSH_LEAF_BYTES_FOR_CHECKPOINT,   // number of leaf nodes flushed to disk for checkpoint
    FT_DISK_FLUSH_LEAF_UNCOMPRESSED_BYTES_FOR_CHECKPOINT,// number of leaf nodes flushed to disk for checkpoint
    FT_DISK_FLUSH_LEAF_TOKUTIME_FOR_CHECKPOINT,// number of leaf nodes flushed to disk for checkpoint
    FT_DISK_FLUSH_NONLEAF_FOR_CHECKPOINT,      // number of nonleaf nodes flushed to disk for checkpoint
    FT_DISK_FLUSH_NONLEAF_BYTES_FOR_CHECKPOINT,// number of nonleaf nodes flushed to disk for checkpoint
    FT_DISK_FLUSH_NONLEAF_UNCOMPRESSED_BYTES_FOR_CHECKPOINT,// number of nonleaf nodes flushed to disk for checkpoint
    FT_DISK_FLUSH_NONLEAF_TOKUTIME_FOR_CHECKPOINT,// number of nonleaf nodes flushed to disk for checkpoint
    FT_DISK_FLUSH_LEAF_COMPRESSION_RATIO,      // effective compression ratio for leaf bytes flushed to disk
    FT_DISK_FLUSH_NONLEAF_COMPRESSION_RATIO,   // effective compression ratio for nonleaf bytes flushed to disk
    FT_DISK_FLUSH_OVERALL_COMPRESSION_RATIO,   // effective compression ratio for all bytes flushed to disk
    FT_PARTIAL_EVICTIONS_NONLEAF,              // number of nonleaf node partial evictions
    FT_PARTIAL_EVICTIONS_NONLEAF_BYTES,        // number of nonleaf node partial evictions
    FT_PARTIAL_EVICTIONS_LEAF,                 // number of leaf node partial evictions
    FT_PARTIAL_EVICTIONS_LEAF_BYTES,           // number of leaf node partial evictions
    FT_FULL_EVICTIONS_LEAF,                    // number of full cachetable evictions on leaf nodes
    FT_FULL_EVICTIONS_LEAF_BYTES,              // number of full cachetable evictions on leaf nodes (bytes)
    FT_FULL_EVICTIONS_NONLEAF,                 // number of full cachetable evictions on nonleaf nodes
    FT_FULL_EVICTIONS_NONLEAF_BYTES,           // number of full cachetable evictions on nonleaf nodes (bytes)
    FT_CREATE_LEAF,                            // number of leaf nodes created
    FT_CREATE_NONLEAF,                         // number of nonleaf nodes created
    FT_DESTROY_LEAF,                           // number of leaf nodes destroyed
    FT_DESTROY_NONLEAF,                        // number of nonleaf nodes destroyed
    FT_MSG_BYTES_IN,                           // how many bytes of messages injected at root (for all trees)
    FT_MSG_BYTES_OUT,                          // how many bytes of messages flushed from h1 nodes to leaves
    FT_MSG_BYTES_CURR,                         // how many bytes of messages currently in trees (estimate)
    FT_MSG_NUM,                                // how many messages injected at root
    FT_MSG_NUM_BROADCAST,                      // how many broadcast messages injected at root
    FT_NUM_BASEMENTS_DECOMPRESSED_NORMAL,      // how many basement nodes were decompressed because they were the target of a query
    FT_NUM_BASEMENTS_DECOMPRESSED_AGGRESSIVE,  // ... because they were between lc and rc
    FT_NUM_BASEMENTS_DECOMPRESSED_PREFETCH,
    FT_NUM_BASEMENTS_DECOMPRESSED_WRITE,
    FT_NUM_MSG_BUFFER_DECOMPRESSED_NORMAL,     // how many msg buffers were decompressed because they were the target of a query
    FT_NUM_MSG_BUFFER_DECOMPRESSED_AGGRESSIVE, // ... because they were between lc and rc
    FT_NUM_MSG_BUFFER_DECOMPRESSED_PREFETCH,
    FT_NUM_MSG_BUFFER_DECOMPRESSED_WRITE,
    FT_NUM_PIVOTS_FETCHED_QUERY,               // how many pivots were fetched for a query
    FT_BYTES_PIVOTS_FETCHED_QUERY,               // how many pivots were fetched for a query
    FT_TOKUTIME_PIVOTS_FETCHED_QUERY,               // how many pivots were fetched for a query
    FT_NUM_PIVOTS_FETCHED_PREFETCH,            // ... for a prefetch
    FT_BYTES_PIVOTS_FETCHED_PREFETCH,            // ... for a prefetch
    FT_TOKUTIME_PIVOTS_FETCHED_PREFETCH,            // ... for a prefetch
    FT_NUM_PIVOTS_FETCHED_WRITE,               // ... for a write
    FT_BYTES_PIVOTS_FETCHED_WRITE,               // ... for a write
    FT_TOKUTIME_PIVOTS_FETCHED_WRITE,               // ... for a write
    FT_NUM_BASEMENTS_FETCHED_NORMAL,           // how many basement nodes were fetched because they were the target of a query
    FT_BYTES_BASEMENTS_FETCHED_NORMAL,           // how many basement nodes were fetched because they were the target of a query
    FT_TOKUTIME_BASEMENTS_FETCHED_NORMAL,           // how many basement nodes were fetched because they were the target of a query
    FT_NUM_BASEMENTS_FETCHED_AGGRESSIVE,       // ... because they were between lc and rc
    FT_BYTES_BASEMENTS_FETCHED_AGGRESSIVE,       // ... because they were between lc and rc
    FT_TOKUTIME_BASEMENTS_FETCHED_AGGRESSIVE,       // ... because they were between lc and rc
    FT_NUM_BASEMENTS_FETCHED_PREFETCH,
    FT_BYTES_BASEMENTS_FETCHED_PREFETCH,
    FT_TOKUTIME_BASEMENTS_FETCHED_PREFETCH,
    FT_NUM_BASEMENTS_FETCHED_WRITE,
    FT_BYTES_BASEMENTS_FETCHED_WRITE,
    FT_TOKUTIME_BASEMENTS_FETCHED_WRITE,
    FT_NUM_MSG_BUFFER_FETCHED_NORMAL,          // how many msg buffers were fetched because they were the target of a query
    FT_BYTES_MSG_BUFFER_FETCHED_NORMAL,          // how many msg buffers were fetched because they were the target of a query
    FT_TOKUTIME_MSG_BUFFER_FETCHED_NORMAL,          // how many msg buffers were fetched because they were the target of a query
    FT_NUM_MSG_BUFFER_FETCHED_AGGRESSIVE,      // ... because they were between lc and rc
    FT_BYTES_MSG_BUFFER_FETCHED_AGGRESSIVE,      // ... because they were between lc and rc
    FT_TOKUTIME_MSG_BUFFER_FETCHED_AGGRESSIVE,      // ... because they were between lc and rc
    FT_NUM_MSG_BUFFER_FETCHED_PREFETCH,
    FT_BYTES_MSG_BUFFER_FETCHED_PREFETCH,
    FT_TOKUTIME_MSG_BUFFER_FETCHED_PREFETCH,
    FT_NUM_MSG_BUFFER_FETCHED_WRITE,
    FT_BYTES_MSG_BUFFER_FETCHED_WRITE,
    FT_TOKUTIME_MSG_BUFFER_FETCHED_WRITE,
    FT_LEAF_COMPRESS_TOKUTIME, // seconds spent compressing leaf leaf nodes to memory
    FT_LEAF_SERIALIZE_TOKUTIME, // seconds spent serializing leaf node to memory
    FT_LEAF_DECOMPRESS_TOKUTIME, // seconds spent decompressing leaf nodes to memory
    FT_LEAF_DESERIALIZE_TOKUTIME, // seconds spent deserializing leaf nodes to memory
    FT_NONLEAF_COMPRESS_TOKUTIME, // seconds spent compressing nonleaf nodes to memory
    FT_NONLEAF_SERIALIZE_TOKUTIME, // seconds spent serializing nonleaf nodes to memory
    FT_NONLEAF_DECOMPRESS_TOKUTIME, // seconds spent decompressing nonleaf nodes to memory
    FT_NONLEAF_DESERIALIZE_TOKUTIME, // seconds spent deserializing nonleaf nodes to memory
    FT_PRO_NUM_ROOT_SPLIT,
    FT_PRO_NUM_ROOT_H0_INJECT,
    FT_PRO_NUM_ROOT_H1_INJECT,
    FT_PRO_NUM_INJECT_DEPTH_0,
    FT_PRO_NUM_INJECT_DEPTH_1,
    FT_PRO_NUM_INJECT_DEPTH_2,
    FT_PRO_NUM_INJECT_DEPTH_3,
    FT_PRO_NUM_INJECT_DEPTH_GT3,
    FT_PRO_NUM_STOP_NONEMPTY_BUF,
    FT_PRO_NUM_STOP_H1,
    FT_PRO_NUM_STOP_LOCK_CHILD,
    FT_PRO_NUM_STOP_CHILD_INMEM,
    FT_PRO_NUM_DIDNT_WANT_PROMOTE,
    FT_BASEMENT_DESERIALIZE_FIXED_KEYSIZE, // how many basement nodes were deserialized with a fixed keysize
    FT_BASEMENT_DESERIALIZE_VARIABLE_KEYSIZE, // how many basement nodes were deserialized with a variable keysize
    FT_STATUS_NUM_ROWS
} ft_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[FT_STATUS_NUM_ROWS];
} FT_STATUS_S, *FT_STATUS;

void toku_ft_get_status(FT_STATUS);

void
toku_ft_bn_apply_msg_once(
    BASEMENTNODE bn,
    const FT_MSG msg,
    uint32_t idx,
    LEAFENTRY le,
    txn_gc_info *gc_info,
    uint64_t *workdonep,
    STAT64INFO stats_to_update
    );

void
toku_ft_bn_apply_msg(
    ft_compare_func compare_fun,
    ft_update_func update_fun,
    DESCRIPTOR desc,
    BASEMENTNODE bn,
    FT_MSG msg,
    txn_gc_info *gc_info,
    uint64_t *workdone,
    STAT64INFO stats_to_update
    );

void
toku_ft_leaf_apply_msg(
    ft_compare_func compare_fun,
    ft_update_func update_fun,
    DESCRIPTOR desc,
    FTNODE node,
    int target_childnum,
    FT_MSG msg,
    txn_gc_info *gc_info,
    uint64_t *workdone,
    STAT64INFO stats_to_update
    );

void
toku_ft_node_put_msg(
    ft_compare_func compare_fun,
    ft_update_func update_fun,
    DESCRIPTOR desc,
    FTNODE node,
    int target_childnum,
    FT_MSG msg,
    bool is_fresh,
    txn_gc_info *gc_info,
    size_t flow_deltas[],
    STAT64INFO stats_to_update
    );

void toku_flusher_thread_set_callback(void (*callback_f)(int, void*), void* extra);

int toku_upgrade_subtree_estimates_to_stat64info(int fd, FT h) __attribute__((nonnull));
int toku_upgrade_msn_from_root_to_header(int fd, FT h) __attribute__((nonnull));

#endif
