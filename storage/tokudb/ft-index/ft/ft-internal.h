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

#include "portability/toku_config.h"
#include "portability/toku_list.h"
#include "portability/toku_race_tools.h"

#include "ft/cachetable/cachetable.h"
#include "ft/comparator.h"
#include "ft/ft.h"
#include "ft/ft-ops.h"
#include "ft/node.h"
#include "ft/serialize/block_table.h"
#include "ft/txn/rollback.h"

// Symbol TOKUDB_REVISION is not defined by fractal-tree makefiles, so
// BUILD_ID of 1000 indicates development build of main, not a release build.  
#if defined(TOKUDB_REVISION)
#define BUILD_ID TOKUDB_REVISION
#else
#error
#endif

struct ft_search;

enum { FT_DEFAULT_FANOUT = 16 };
enum { FT_DEFAULT_NODE_SIZE = 4 * 1024 * 1024 };
enum { FT_DEFAULT_BASEMENT_NODE_SIZE = 128 * 1024 };

// We optimize for a sequential insert pattern if 100 consecutive injections
// happen into the rightmost leaf node due to promotion.
enum { FT_SEQINSERT_SCORE_THRESHOLD = 100 };

uint32_t compute_child_fullhash (CACHEFILE cf, FTNODE node, int childnum);

enum ft_type {
    FT_CURRENT = 1,
    FT_CHECKPOINT_INPROGRESS
};

// The ft_header is not managed by the cachetable.  Instead, it hangs off the cachefile as userdata.
struct ft_header {
    enum ft_type type;

    int dirty;

    // Free-running counter incremented once per checkpoint (toggling LSB).
    // LSB indicates which header location is used on disk so this
    // counter is effectively a boolean which alternates with each checkpoint.
    uint64_t checkpoint_count;
    // LSN of creation of "checkpoint-begin" record in log.
    LSN checkpoint_lsn;

    // see serialize/ft_layout_version.h.  maybe don't need this if we assume
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
typedef struct ft_header *FT_HEADER;

// ft_header is always the current version.
struct ft {
    FT_HEADER h;
    FT_HEADER checkpoint_header;

    // These are (mostly) read-only.

    CACHEFILE cf;
    // unique id for dictionary
    DICTIONARY_ID dict_id;

    // protected by locktree
    DESCRIPTOR_S descriptor;

    // protected by locktree and user.
    // User makes sure this is only changed when no activity on tree
    DESCRIPTOR_S cmp_descriptor;
    // contains a pointer to cmp_descriptor (above) - their lifetimes are bound
    toku::comparator cmp;

    // the update function always utilizes the cmp_descriptor, not the regular one
    ft_update_func update_fun;

    // These are not read-only:

    // protected by blocktable lock
    block_table blocktable;

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

    // The blocknum of the rightmost leaf node in the tree. Stays constant through splits
    // and merges using pair-swapping (like the root node, see toku_ftnode_swap_pair_values())
    // 
    // This field only transitions from RESERVED_BLOCKNUM_NULL to non-null, never back.
    // We initialize it when promotion inserts into a non-root leaf node on the right extreme.
    // We use the blocktable lock to protect the initialize transition, though it's not really
    // necessary since all threads should be setting it to the same value. We maintain that invariant
    // on first initialization, see ft_set_or_verify_rightmost_blocknum()
    BLOCKNUM rightmost_blocknum;

    // sequential access pattern heuristic
    // - when promotion pushes a message directly into the rightmost leaf, the score goes up.
    // - if the score is high enough, we optimistically attempt to insert directly into the rightmost leaf
    // - if our attempt fails because the key was not in range of the rightmost leaf, we reset the score back to 0
    uint32_t seqinsert_score;
};

// Allocate a DB struct off the stack and only set its comparison
// descriptor. We don't bother setting any other fields because
// the comparison function doesn't need it, and we would like to
// reduce the CPU work done per comparison.
#define FAKE_DB(db, desc) struct __toku_db db; do { db.cmp_descriptor = const_cast<DESCRIPTOR>(desc); } while (0)

struct ft_options {
    unsigned int nodesize;
    unsigned int basementnodesize;
    enum toku_compression_method compression_method;
    unsigned int fanout;
    unsigned int flags;
    uint8_t memcmp_magic;
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

//
// Field in ftnode_fetch_extra that tells the 
// partial fetch callback what piece of the node
// is needed by the ydb
//
enum ftnode_fetch_type {
    ftnode_fetch_none = 1, // no partitions needed.  
    ftnode_fetch_subset, // some subset of partitions needed
    ftnode_fetch_prefetch, // this is part of a prefetch call
    ftnode_fetch_all, // every partition is needed
    ftnode_fetch_keymatch, // one child is needed if it holds both keys
};

// Info passed to cachetable fetch callbacks to say which parts of a node
// should be fetched (perhaps a subset, perhaps the whole thing, depending
// on operation)
class ftnode_fetch_extra {
public:
    // Used when the whole node must be in memory, such as for flushes.
    void create_for_full_read(FT ft);

    // A subset of children are necessary. Used by point queries.
    void create_for_subset_read(FT ft, ft_search *search, const DBT *left, const DBT *right,
                                bool left_is_neg_infty, bool right_is_pos_infty,
                                bool disable_prefetching, bool read_all_partitions);

    // No partitions are necessary - only pivots and/or subtree estimates.
    // Currently used for stat64.
    void create_for_min_read(FT ft);

    // Used to prefetch partitions that fall within the bounds given by the cursor.
    void create_for_prefetch(FT ft, struct ft_cursor *cursor);

    // Only a portion of the node (within a keyrange) is required.
    // Used by keysrange when the left and right key are in the same basement node.
    void create_for_keymatch(FT ft, const DBT *left, const DBT *right,
                             bool disable_prefetching, bool read_all_partitions);

    void destroy(void);

    // return: true if a specific childnum is required to be in memory
    bool wants_child_available(int childnum) const;

    // return: the childnum of the leftmost child that is required to be in memory
    int leftmost_child_wanted(FTNODE node) const;

    // return: the childnum of the rightmost child that is required to be in memory
    int rightmost_child_wanted(FTNODE node) const;

    // needed for reading a node off disk
    FT ft;

    enum ftnode_fetch_type type;

    // used in the case where type == ftnode_fetch_subset
    // parameters needed to find out which child needs to be decompressed (so it can be read)
    ft_search *search;
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

private:
    void _create_internal(FT ft_);
};

// Only exported for tests.
// Cachetable callbacks for ftnodes.
void toku_ftnode_clone_callback(void* value_data, void** cloned_value_data, long* clone_size, PAIR_ATTR* new_attr, bool for_checkpoint, void* write_extraargs);
void toku_ftnode_checkpoint_complete_callback(void *value_data);
void toku_ftnode_flush_callback (CACHEFILE cachefile, int fd, BLOCKNUM blocknum, void *ftnode_v, void** UU(disk_data), void *extraargs, PAIR_ATTR size, PAIR_ATTR* new_size, bool write_me, bool keep_me, bool for_checkpoint, bool is_clone);
int toku_ftnode_fetch_callback (CACHEFILE cachefile, PAIR p, int fd, BLOCKNUM blocknum, uint32_t fullhash, void **ftnode_pv, void** UU(disk_data), PAIR_ATTR *sizep, int*dirty, void*extraargs);
void toku_ftnode_pe_est_callback(void* ftnode_pv, void* disk_data, long* bytes_freed_estimate, enum partial_eviction_cost *cost, void* write_extraargs);
int toku_ftnode_pe_callback(void *ftnode_pv, PAIR_ATTR old_attr, void *extraargs,
                            void (*finalize)(PAIR_ATTR new_attr, void *extra), void *finalize_extra);
bool toku_ftnode_pf_req_callback(void* ftnode_pv, void* read_extraargs);
int toku_ftnode_pf_callback(void* ftnode_pv, void* UU(disk_data), void* read_extraargs, int fd, PAIR_ATTR* sizep);
int toku_ftnode_cleaner_callback( void *ftnode_pv, BLOCKNUM blocknum, uint32_t fullhash, void *extraargs);

CACHETABLE_WRITE_CALLBACK get_write_callbacks_for_node(FT ft);

// This is only exported for tests.
// append a child node to a parent node
void toku_ft_nonleaf_append_child(FTNODE node, FTNODE child, const DBT *pivotkey);

// This is only exported for tests.
// append a message to a nonleaf node child buffer
void toku_ft_append_to_child_buffer(const toku::comparator &cmp, FTNODE node, int childnum, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, const DBT *key, const DBT *val);

STAT64INFO_S toku_get_and_clear_basement_stats(FTNODE leafnode);

//#define SLOW
#ifdef SLOW
#define VERIFY_NODE(t,n) (toku_verify_or_set_counts(n), toku_verify_estimates(t,n))
#else
#define VERIFY_NODE(t,n) ((void)0)
#endif

void toku_verify_or_set_counts(FTNODE);

// TODO: consider moving this to ft/pivotkeys.cc
class pivot_bounds {
public:
    pivot_bounds(const DBT &lbe_dbt, const DBT &ubi_dbt);

    pivot_bounds next_bounds(FTNODE node, int childnum) const;

    const DBT *lbe() const;
    const DBT *ubi() const;

    static pivot_bounds infinite_bounds();

private:
    DBT _prepivotkey(FTNODE node, int childnum, const DBT &lbe_dbt) const;
    DBT _postpivotkey(FTNODE node, int childnum, const DBT &ubi_dbt) const;

    // if toku_dbt_is_empty() is true for either bound, then it represents
    // negative or positive infinity (which are exclusive in practice)
    const DBT _lower_bound_exclusive;
    const DBT _upper_bound_inclusive;
};

// allocate a block number
// allocate and initialize a ftnode
// put the ftnode into the cache table
void toku_create_new_ftnode(FT_HANDLE ft_handle, FTNODE *result, int height, int n_children);

/* Stuff for testing */
// toku_testsetup_initialize() must be called before any other test_setup_xxx() functions are called.
void toku_testsetup_initialize(void);
int toku_testsetup_leaf(FT_HANDLE ft_h, BLOCKNUM *blocknum, int n_children, char **keys, int *keylens);
int toku_testsetup_nonleaf (FT_HANDLE ft_h, int height, BLOCKNUM *blocknum, int n_children, BLOCKNUM *children, char **keys, int *keylens);
int toku_testsetup_root(FT_HANDLE ft_h, BLOCKNUM);
int toku_testsetup_get_sersize(FT_HANDLE ft_h, BLOCKNUM); // Return the size on disk.
int toku_testsetup_insert_to_leaf (FT_HANDLE ft_h, BLOCKNUM, const char *key, int keylen, const char *val, int vallen);
int toku_testsetup_insert_to_nonleaf (FT_HANDLE ft_h, BLOCKNUM, enum ft_msg_type, const char *key, int keylen, const char *val, int vallen);
void toku_pin_node_with_min_bfe(FTNODE* node, BLOCKNUM b, FT_HANDLE t);

void toku_ft_root_put_msg(FT ft, const ft_msg &msg, txn_gc_info *gc_info);

// TODO: Rename
void toku_get_node_for_verify(BLOCKNUM blocknum, FT_HANDLE ft_h, FTNODE* nodep);

int
toku_verify_ftnode (FT_HANDLE ft_h,
                    MSN rootmsn, MSN parentmsn_with_messages, bool messages_exist_above,
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
    FT_PRO_RIGHTMOST_LEAF_SHORTCUT_SUCCESS,
    FT_PRO_RIGHTMOST_LEAF_SHORTCUT_FAIL_POS,
    FT_PRO_RIGHTMOST_LEAF_SHORTCUT_FAIL_REACTIVE,
    FT_STATUS_NUM_ROWS
} ft_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[FT_STATUS_NUM_ROWS];
} FT_STATUS_S, *FT_STATUS;

void toku_ft_status_update_pivot_fetch_reason(ftnode_fetch_extra *bfe);
void toku_ft_status_update_flush_reason(FTNODE node, uint64_t uncompressed_bytes_flushed, uint64_t bytes_written, tokutime_t write_time, bool for_checkpoint);
void toku_ft_status_update_serialize_times(FTNODE node, tokutime_t serialize_time, tokutime_t compress_time);
void toku_ft_status_update_deserialize_times(FTNODE node, tokutime_t deserialize_time, tokutime_t decompress_time);
void toku_ft_status_note_msn_discard(void);
void toku_ft_status_note_update(bool broadcast);
void toku_ft_status_note_msg_bytes_out(size_t buffsize);
void toku_ft_status_note_ftnode(int height, bool created); // created = false means destroyed

void toku_ft_get_status(FT_STATUS);

void toku_flusher_thread_set_callback(void (*callback_f)(int, void*), void* extra);

// For upgrade
int toku_upgrade_subtree_estimates_to_stat64info(int fd, FT ft) __attribute__((nonnull));
int toku_upgrade_msn_from_root_to_header(int fd, FT ft) __attribute__((nonnull));

// A callback function is invoked with the key, and the data.
// The pointers (to the bytevecs) must not be modified.  The data must be copied out before the callback function returns.
// Note: In the thread-safe version, the ftnode remains locked while the callback function runs.  So return soon, and don't call the ft code from the callback function.
// If the callback function returns a nonzero value (an error code), then that error code is returned from the get function itself.
// The cursor object will have been updated (so that if result==0 the current value is the value being passed)
//  (If r!=0 then the cursor won't have been updated.)
// If r!=0, it's up to the callback function to return that value of r.
// A 'key' pointer of NULL means that element is not found (effectively infinity or
// -infinity depending on direction)
// When lock_only is false, the callback does optional lock tree locking and then processes the key and val.
// When lock_only is true, the callback only does optional lock tree locking.
typedef int (*FT_GET_CALLBACK_FUNCTION)(uint32_t keylen, const void *key, uint32_t vallen, const void *val, void *extra, bool lock_only);

typedef bool (*FT_CHECK_INTERRUPT_CALLBACK)(void *extra);

struct ft_cursor;
int toku_ft_search(FT_HANDLE ft_handle, ft_search *search, FT_GET_CALLBACK_FUNCTION getf, void *getf_v, struct ft_cursor *ftcursor, bool can_bulk_fetch);
