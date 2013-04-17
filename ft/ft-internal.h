/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FT_INTERNAL_H
#define FT_INTERNAL_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <config.h>

// Symbol TOKUDB_REVISION is not defined by fractal-tree makefiles, so
// BUILD_ID of 1000 indicates development build of main, not a release build.  
#if defined(TOKUDB_REVISION)
#define BUILD_ID TOKUDB_REVISION
#else
#error
#endif

#include "ft_layout_version.h"
#include "toku_assert.h"
#include "block_allocator.h"
#include "cachetable.h"
#include "fifo.h"
#include "ft-ops.h"
#include "toku_list.h"
#include "omt.h"
#include "leafentry.h"
#include "block_table.h"
#include "mempool.h"
#include "compress.h"

// Uncomment the following to use quicklz

#ifndef FT_FANOUT
#define FT_FANOUT 16
#endif
enum { TREE_FANOUT = FT_FANOUT };
enum { KEY_VALUE_OVERHEAD = 8 }; /* Must store the two lengths. */
enum { FT_CMD_OVERHEAD = (2 + sizeof(MSN))     // the type plus freshness plus MSN
};

enum { FT_DEFAULT_NODE_SIZE = 1 << 22 };
enum { FT_DEFAULT_BASEMENT_NODE_SIZE = 128 * 1024 };

struct nodeheader_in_file {
    int n_in_buffer;
};
enum { BUFFER_HEADER_SIZE = (4 // height//
			     + 4 // n_children
			     + TREE_FANOUT * 8 // children
			     ) };

//
// Field in ftnode_fetch_extra that tells the 
// partial fetch callback what piece of the node
// is needed by the ydb
//
enum ftnode_fetch_type {
    ftnode_fetch_none=1, // no partitions needed.  
    ftnode_fetch_subset, // some subset of partitions needed
    ftnode_fetch_prefetch, // this is part of a prefetch call
    ftnode_fetch_all // every partition is needed
};

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
    DBT *range_lock_left_key, *range_lock_right_key;
    BOOL left_is_neg_infty, right_is_pos_infty;
    // states if we should try to aggressively fetch basement nodes 
    // that are not specifically needed for current query, 
    // but may be needed for other cursor operations user is doing
    // For example, if we have not disabled prefetching,
    // and the user is doing a dictionary wide scan, then
    // even though a query may only want one basement node,
    // we fetch all basement nodes in a leaf node.
    BOOL disable_prefetching; 
    // this value will be set during the fetch_callback call by toku_ftnode_fetch_callback or toku_ftnode_pf_req_callback
    // thi callbacks need to evaluate this anyway, so we cache it here so the search code does not reevaluate it
    int child_to_read;
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
toku_fifo_entry_key_msn_heaviside(OMTVALUE v, void *extrap);

struct toku_fifo_entry_key_msn_cmp_extra {
    DESCRIPTOR desc;
    ft_compare_func cmp;
    FIFO fifo;
};

// same thing for qsort_r
int
toku_fifo_entry_key_msn_cmp(void *extrap, const void *ap, const void *bp);

// data of an available partition of a nonleaf ftnode
struct ftnode_nonleaf_childinfo {
    FIFO buffer;
    OMT broadcast_list;
    OMT fresh_message_tree;
    OMT stale_message_tree;
};

unsigned int toku_bnc_nbytesinbuf(NONLEAF_CHILDINFO bnc);
int toku_bnc_n_entries(NONLEAF_CHILDINFO bnc);
long toku_bnc_memory_size(NONLEAF_CHILDINFO bnc);
long toku_bnc_memory_used(NONLEAF_CHILDINFO bnc);
int toku_bnc_insert_msg(NONLEAF_CHILDINFO bnc, const void *key, ITEMLEN keylen, const void *data, ITEMLEN datalen, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, DESCRIPTOR desc, ft_compare_func cmp);
void toku_bnc_empty(NONLEAF_CHILDINFO bnc);
int toku_bnc_flush_to_child(
    FT h,
    NONLEAF_CHILDINFO bnc, 
    FTNODE child
    );
bool
toku_ft_nonleaf_is_gorged(FTNODE node);


enum reactivity get_nonleaf_reactivity (FTNODE node);
enum reactivity get_node_reactivity (FTNODE node);

// data of an available partition of a leaf ftnode
struct ftnode_leaf_basement_node {
    OMT buffer;                     // pointers to individual leaf entries
    struct mempool buffer_mempool;  // storage for all leaf entries
    unsigned int n_bytes_in_buffer; // How many bytes to represent the OMT (including the per-key overheads, ...
                                    // ... but not including the overheads for the node. 
    unsigned int seqinsert;         // number of sequential inserts to this leaf 
    MSN max_msn_applied;            // max message sequence number applied
    bool stale_ancestor_messages_applied;
    STAT64INFO_S stat64_delta;      // change in stat64 counters since basement was last written to disk
};

enum  __attribute__((__packed__)) pt_state {  // declare this to be packed so that when used below it will only take 1 byte.
    PT_INVALID = 0,
    PT_ON_DISK = 1,
    PT_COMPRESSED = 2,
    PT_AVAIL = 3};

enum __attribute__((__packed__)) ftnode_child_tag {
    BCT_INVALID = 0,
    BCT_NULL,
    BCT_SUBBLOCK,
    BCT_LEAF,
    BCT_NONLEAF
};
    
typedef struct __attribute__((__packed__)) ftnode_child_pointer {
    enum ftnode_child_tag tag;
    union {
	struct sub_block *subblock;
	struct ftnode_nonleaf_childinfo *nonleaf;
	struct ftnode_leaf_basement_node *leaf;
    } u;
} FTNODE_CHILD_POINTER;


struct ftnode_disk_data {
    //
    // stores the offset to the beginning of the partition on disk from the ftnode, and the length, needed to read a partition off of disk
    // the value is only meaningful if the node is clean. If the node is dirty, then the value is meaningless
    //  The START is the distance from the end of the compressed node_info data, to the beginning of the compressed partition
    //  The SIZE is the size of the compressed partition.
    // Rationale:  We cannot store the size from the beginning of the node since we don't know how big the header will be.
    //  However, later when we are doing aligned writes, we won't be able to store the size from the end since we want things to align.
    u_int32_t start;
    u_int32_t size;
};
#define BP_START(node_dd,i) ((node_dd)[i].start)
#define BP_SIZE(node_dd,i) ((node_dd)[i].size)


// a ftnode partition, associated with a child of a node
struct   __attribute__((__packed__)) ftnode_partition {
    // the following three variables are used for nonleaf nodes
    // for leaf nodes, they are meaningless
    BLOCKNUM     blocknum; // blocknum of child 

    //
    // at any time, the partitions may be in one of the following three states (stored in pt_state):
    //   PT_INVALID - means that the partition was just initialized
    //   PT_ON_DISK - means that the partition is not in memory and needs to be read from disk. To use, must read off disk and decompress
    //   PT_COMPRESSED - means that the partition is compressed in memory. To use, must decompress
    //   PT_AVAIL - means the partition is decompressed and in memory
    //
    enum pt_state state; // make this an enum to make debugging easier.  
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

    // clock count used to for pe_callback to determine if a node should be evicted or not
    // for now, saturating the count at 1
    u_int8_t clock_count;

    // How many bytes worth of work was performed by messages in each buffer.
    uint64_t     workdone;
};

struct ftnode {
    MSN      max_msn_applied_to_node_on_disk; // max_msn_applied that will be written to disk
    unsigned int nodesize;
    unsigned int flags;
    BLOCKNUM thisnodename;   // Which block number is this node?
    int    layout_version; // What version of the data structure?
    int    layout_version_original;	// different (<) from layout_version if upgraded from a previous version (useful for debugging)
    int    layout_version_read_from_disk;  // transient, not serialized to disk, (useful for debugging)
    uint32_t build_id;       // build_id (svn rev number) of software that wrote this node to disk
    int    height; /* height is always >= 0.  0 for leaf, >0 for nonleaf. */
    int    dirty;
    u_int32_t fullhash;
    int n_children; //for internal nodes, if n_children==TREE_FANOUT+1 then the tree needs to be rebalanced.
                    // for leaf nodes, represents number of basement nodes
    unsigned int    totalchildkeylens;
    DBT *childkeys;   /* Pivot keys.  Child 0's keys are <= childkeys[0].  Child 1's keys are <= childkeys[1].
                                                                        Child 1's keys are > childkeys[0]. */
    // array of size n_children, consisting of ftnode partitions
    // each one is associated with a child
    // for internal nodes, the ith partition corresponds to the ith message buffer
    // for leaf nodes, the ith partition corresponds to the ith basement node
    struct ftnode_partition *bp;
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
    assert(0<=i && i<node->n_children);
    node->bp[i].ptr.tag = BCT_NULL;
}
static inline bool is_BNULL (FTNODE node, int i) {
    assert(0<=i && i<node->n_children);
    return node->bp[i].ptr.tag == BCT_NULL;
}
static inline NONLEAF_CHILDINFO BNC(FTNODE node, int i) {
    assert(0<=i && i<node->n_children);
    FTNODE_CHILD_POINTER p = node->bp[i].ptr;
    assert(p.tag==BCT_NONLEAF);
    return p.u.nonleaf;
}
static inline void set_BNC(FTNODE node, int i, NONLEAF_CHILDINFO nl) {
    assert(0<=i && i<node->n_children);
    FTNODE_CHILD_POINTER *p = &node->bp[i].ptr;
    p->tag = BCT_NONLEAF;
    p->u.nonleaf = nl;
}
static inline BASEMENTNODE BLB(FTNODE node, int i) {
    assert(0<=i && i<node->n_children);
    FTNODE_CHILD_POINTER p = node->bp[i].ptr;
    assert(p.tag==BCT_LEAF);
    return p.u.leaf;
}
static inline void set_BLB(FTNODE node, int i, BASEMENTNODE bn) {
    assert(0<=i && i<node->n_children);
    FTNODE_CHILD_POINTER *p = &node->bp[i].ptr;
    p->tag = BCT_LEAF;
    p->u.leaf = bn;
}

static inline SUB_BLOCK BSB(FTNODE node, int i) {
    assert(0<=i && i<node->n_children);
    FTNODE_CHILD_POINTER p = node->bp[i].ptr;
    assert(p.tag==BCT_SUBBLOCK);
    return p.u.subblock;
}
static inline void set_BSB(FTNODE node, int i, SUB_BLOCK sb) {
    assert(0<=i && i<node->n_children);
    FTNODE_CHILD_POINTER *p = &node->bp[i].ptr;
    p->tag = BCT_SUBBLOCK;
    p->u.subblock = sb;
}

// ftnode leaf basementnode macros, 
#define BLB_MAX_MSN_APPLIED(node,i) (BLB(node,i)->max_msn_applied)
#define BLB_MAX_DSN_APPLIED(node,i) (BLB(node,i)->max_dsn_applied)
#define BLB_BUFFER(node,i) (BLB(node,i)->buffer)
#define BLB_BUFFER_MEMPOOL(node,i) (BLB(node,i)->buffer_mempool)
#define BLB_NBYTESINBUF(node,i) (BLB(node,i)->n_bytes_in_buffer)
#define BLB_SEQINSERT(node,i) (BLB(node,i)->seqinsert)

/* pivot flags  (must fit in 8 bits) */
enum {
    FT_PIVOT_TRUNC = 4,
    FT_PIVOT_FRONT_COMPRESS = 8,
};

u_int32_t compute_child_fullhash (CACHEFILE cf, FTNODE node, int childnum);

// The brt_header is not managed by the cachetable.  Instead, it hangs off the cachefile as userdata.

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

    // see brt_layout_version.h.  maybe don't need this if we assume
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

    // this field is protected by tree_lock, see comment for tree_lock
    BLOCKNUM root_blocknum;

    const unsigned int flags;
    const unsigned int nodesize;
    const unsigned int basementnodesize;
    const enum toku_compression_method compression_method;

    // Current Minimum MSN to be used when upgrading pre-MSN BRT's.
    // This is decremented from our currnt MIN_MSN so as not to clash
    // with any existing 'normal' MSN's.
    MSN highest_unused_msn_for_upgrade;

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

// brt_header is always the current version.
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

    // lock used by a thread to pin the root node to start a descent into 
    // the tree. This lock protects the blocknum of the root node (root_blocknum). Any 
    // thread that wants to descend down the tree starting at the root 
    // must grab this lock before pinning the root.
    toku_mutex_t tree_lock;

    // protected by blocktable lock
    BLOCK_TABLE blocktable;

    // protected by atomic builtins
    STAT64INFO_S in_memory_stats;

    // transient, not serialized to disk.  updated when we do write to
    // disk.  tells us whether we can do partial eviction (we can't if
    // the on-disk layout version is from before basement nodes)
    int layout_version_read_from_disk;

    // If a transaction created this BRT, which one?
    // If a transaction locked the BRT when it was empty, which transaction?  (Only the latest one matters)
    // 0 if no such transaction
    // only one thread can write to these at once, this is enforced by
    // the lock tree
    TXNID txnid_that_created_or_locked_when_empty;
    TXNID txnid_that_suppressed_recovery_logs;

    // protects modifying live_ft_handles, txns, and pinned_by_checkpoint
    toku_mutex_t ft_ref_lock;
    struct toku_list live_ft_handles;
    // transactions that are using this header.  you should only be able
    // to modify this if you have a valid handle in the list of live brts
    OMT txns;
    // Keep this header around for checkpoint, like a transaction
    bool pinned_by_checkpoint;

    // If nonzero there was a write error.  Don't write any more, because it probably only gets worse.  This is the error code.
    int panic;
    // A malloced string that can indicate what went wrong.
    char *panic_string;
};

// Copy the descriptor into a temporary variable, and tell DRD that subsequent code happens after reading that pointer.
// In combination with the annotation in toku_update_descriptor, this seems to be enough to convince test_4015 that all is well.
// Otherwise, drd complains that the newly malloc'd descriptor string is touched later by some comparison operation.
static const struct __toku_db zero_db; // it's static, so it's all zeros.  icc needs this to be a global
static inline void setup_fake_db (DB *fake_db, DESCRIPTOR orig_desc) {
    *fake_db = zero_db;
    fake_db->cmp_descriptor = orig_desc;
}
#define FAKE_DB(db, desc) struct __toku_db db; setup_fake_db(&db, (desc))

struct ft_options {
    unsigned int nodesize;
    unsigned int basementnodesize;
    enum toku_compression_method compression_method;
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
    BOOL did_set_flags;

    struct ft_options options;
};

// FIXME needs toku prefix
long ftnode_memory_size (FTNODE node);
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
                                      BOOL do_rebalancing,
                                      BOOL in_parallel,
                              /*out*/ size_t *n_bytes_to_write,
                              /*out*/ char  **bytes_to_write);
int toku_serialize_ftnode_to(int fd, BLOCKNUM, FTNODE node, FTNODE_DISK_DATA* ndd, BOOL do_rebalancing, FT h, int n_workitems, int n_threads, BOOL for_checkpoint);
int toku_serialize_rollback_log_to (int fd, BLOCKNUM blocknum, ROLLBACK_LOG_NODE log,
                                    FT h, int n_workitems, int n_threads,
                                    BOOL for_checkpoint);
int toku_deserialize_rollback_log_from (int fd, BLOCKNUM blocknum, u_int32_t fullhash, ROLLBACK_LOG_NODE *logp, FT h);
enum deserialize_error_code toku_deserialize_bp_from_disk(FTNODE node, FTNODE_DISK_DATA ndd, int childnum, int fd, struct ftnode_fetch_extra* bfe);
enum deserialize_error_code toku_deserialize_bp_from_compressed(FTNODE node, int childnum, DESCRIPTOR desc, ft_compare_func cmp);
enum deserialize_error_code toku_deserialize_ftnode_from (int fd, BLOCKNUM off, u_int32_t /*fullhash*/, FTNODE *ftnode, FTNODE_DISK_DATA* ndd, struct ftnode_fetch_extra* bfe);
unsigned int toku_serialize_ftnode_size(FTNODE node); /* How much space will it take? */
int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len);

void toku_verify_or_set_counts(FTNODE);

int toku_serialize_ft_size (FT_HEADER h);
int toku_serialize_ft_to (int fd, FT_HEADER h, BLOCK_TABLE blocktable, CACHEFILE cf);
int toku_serialize_ft_to_wbuf (
    struct wbuf *wbuf, 
    FT_HEADER h, 
    DISKOFF translation_location_on_disk, 
    DISKOFF translation_size_on_disk
    );
enum deserialize_error_code toku_deserialize_ft_from (int fd, LSN max_acceptable_lsn, FT *ft);
int toku_serialize_descriptor_contents_to_fd(int fd, const DESCRIPTOR desc, DISKOFF offset);
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
// FIXME needs toku prefix
void bring_node_fully_into_memory(FTNODE node, FT h);

// append a child node to a parent node
void toku_ft_nonleaf_append_child(FTNODE node, FTNODE child, const DBT *pivotkey);

// append a cmd to a nonleaf node child buffer
void toku_ft_append_to_child_buffer(ft_compare_func compare_fun, DESCRIPTOR desc, FTNODE node, int childnum, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, const DBT *key, const DBT *val);

STAT64INFO_S toku_get_and_clear_basement_stats(FTNODE leafnode);


#if 1
#define DEADBEEF ((void*)0xDEADBEEF)
#else
#define DEADBEEF ((void*)0xDEADBEEFDEADBEEF)
#endif

//#define SLOW
#ifdef SLOW
#define VERIFY_NODE(t,n) (toku_verify_or_set_counts(n), toku_verify_estimates(t,n))
#else
#define VERIFY_NODE(t,n) ((void)0)
#endif

//#define FT_TRACE
#ifdef FT_TRACE
#define WHEN_FTTRACE(x) x
#else
#define WHEN_FTTRACE(x) ((void)0)
#endif

struct ftenv {
    CACHETABLE ct;
    TOKULOGGER logger;
    long long checksum_number;
};

void toku_evict_bn_from_memory(FTNODE node, int childnum, FT h);
void toku_ft_status_update_pivot_fetch_reason(struct ftnode_fetch_extra *bfe);
extern void toku_ftnode_clone_callback(void* value_data, void** cloned_value_data, PAIR_ATTR* new_attr, BOOL for_checkpoint, void* write_extraargs);
extern void toku_ftnode_flush_callback (CACHEFILE cachefile, int fd, BLOCKNUM nodename, void *ftnode_v, void** UU(disk_data), void *extraargs, PAIR_ATTR size, PAIR_ATTR* new_size, BOOL write_me, BOOL keep_me, BOOL for_checkpoint, BOOL is_clone);
extern int toku_ftnode_fetch_callback (CACHEFILE cachefile, int fd, BLOCKNUM nodename, u_int32_t fullhash, void **ftnode_pv, void** UU(disk_data), PAIR_ATTR *sizep, int*dirty, void*extraargs);
extern void toku_ftnode_pe_est_callback(void* ftnode_pv, void* disk_data, long* bytes_freed_estimate, enum partial_eviction_cost *cost, void* write_extraargs);
extern int toku_ftnode_pe_callback (void *ftnode_pv, PAIR_ATTR old_attr, PAIR_ATTR* new_attr, void *extraargs);
extern BOOL toku_ftnode_pf_req_callback(void* ftnode_pv, void* read_extraargs);
int toku_ftnode_pf_callback(void* ftnode_pv, void* UU(disk_data), void* read_extraargs, int fd, PAIR_ATTR* sizep);
extern int toku_ftnode_cleaner_callback( void *ftnode_pv, BLOCKNUM blocknum, u_int32_t fullhash, void *extraargs);

static inline CACHETABLE_WRITE_CALLBACK get_write_callbacks_for_node(FT h) {
    CACHETABLE_WRITE_CALLBACK wc;
    wc.flush_callback = toku_ftnode_flush_callback;
    wc.pe_est_callback = toku_ftnode_pe_est_callback;
    wc.pe_callback = toku_ftnode_pe_callback;
    wc.cleaner_callback = toku_ftnode_cleaner_callback;
    wc.clone_callback = toku_ftnode_clone_callback;
    wc.write_extraargs = h;
    return wc;
}

static const FTNODE null_ftnode=0;

// Values to be used to update ftcursor if a search is successful.
struct ft_cursor_leaf_info_to_be {
    u_int32_t index;
    OMT       omt;
};

// Values to be used to pin a leaf for shortcut searches
struct ft_cursor_leaf_info {
    struct ft_cursor_leaf_info_to_be  to_be;
};

/* a brt cursor is represented as a kv pair in a tree */
struct ft_cursor {
    struct toku_list cursors_link;
    FT_HANDLE ft_handle;
    BOOL prefetching;
    DBT key, val;             // The key-value pair that the cursor currently points to
    DBT range_lock_left_key, range_lock_right_key;
    BOOL left_is_neg_infty, right_is_pos_infty;
    BOOL is_snapshot_read; // true if query is read_committed, false otherwise
    BOOL is_leaf_mode;
    BOOL disable_prefetching;
    BOOL is_temporary;
    TOKUTXN ttxn;
    struct ft_cursor_leaf_info  leaf_info;
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
    bfe->range_lock_left_key = NULL;
    bfe->range_lock_right_key = NULL;
    bfe->left_is_neg_infty = FALSE;
    bfe->right_is_pos_infty = FALSE;
    bfe->child_to_read = -1;
    bfe->disable_prefetching = FALSE;
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
    DBT *left,
    DBT *right,
    BOOL left_is_neg_infty,
    BOOL right_is_pos_infty,
    BOOL disable_prefetching
    )
{
    invariant(h->h->type == FT_CURRENT);
    bfe->type = ftnode_fetch_subset;
    bfe->h = h;
    bfe->search = search;
    bfe->range_lock_left_key = (left->data ? left : NULL);
    bfe->range_lock_right_key = (right->data ? right : NULL);
    bfe->left_is_neg_infty = left_is_neg_infty;
    bfe->right_is_pos_infty = right_is_pos_infty;
    bfe->child_to_read = -1;
    bfe->disable_prefetching = disable_prefetching;
}

//
// Helper function to fill a ftnode_fetch_extra with data
// that will tell the fetch callback that no partitions are
// necessary, only the pivots and/or subtree estimates.
// Currently used for stat64.
//
static inline void fill_bfe_for_min_read(struct ftnode_fetch_extra *bfe, FT h) {
    invariant(h->h->type == FT_CURRENT);
    bfe->type = ftnode_fetch_none;
    bfe->h = h;
    bfe->search = NULL;
    bfe->range_lock_left_key = NULL;
    bfe->range_lock_right_key = NULL;
    bfe->left_is_neg_infty = FALSE;
    bfe->right_is_pos_infty = FALSE;
    bfe->child_to_read = -1;
    bfe->disable_prefetching = FALSE;
}

static inline void destroy_bfe_for_prefetch(struct ftnode_fetch_extra *bfe) {
    assert(bfe->type == ftnode_fetch_prefetch);
    if (bfe->range_lock_left_key != NULL) {
        toku_free(bfe->range_lock_left_key->data);
        toku_destroy_dbt(bfe->range_lock_left_key);
        toku_free(bfe->range_lock_left_key);
        bfe->range_lock_left_key = NULL;
    }
    if (bfe->range_lock_right_key != NULL) {
        toku_free(bfe->range_lock_right_key->data);
        toku_destroy_dbt(bfe->range_lock_right_key);
        toku_free(bfe->range_lock_right_key);
        bfe->range_lock_right_key = NULL;
    }
}

// this is in a strange place because it needs the cursor struct to be defined
static inline void fill_bfe_for_prefetch(struct ftnode_fetch_extra *bfe,
                                         FT h,
                                         FT_CURSOR c) {
    invariant(h->h->type == FT_CURRENT);
    bfe->type = ftnode_fetch_prefetch;
    bfe->h = h;
    bfe->search = NULL;
    {
        const DBT *left = &c->range_lock_left_key;
        const DBT *right = &c->range_lock_right_key;
	if (left->data) {
            MALLOC(bfe->range_lock_left_key); resource_assert(bfe->range_lock_left_key);
            toku_fill_dbt(bfe->range_lock_left_key, toku_xmemdup(left->data, left->size), left->size);
        } else {
            bfe->range_lock_left_key = NULL;
        }
        if (right->data) {
            MALLOC(bfe->range_lock_right_key); resource_assert(bfe->range_lock_right_key);
            toku_fill_dbt(bfe->range_lock_right_key, toku_xmemdup(right->data, right->size), right->size);
        } else {
            bfe->range_lock_right_key = NULL;
        }
    }
    bfe->left_is_neg_infty = c->left_is_neg_infty;
    bfe->right_is_pos_infty = c->right_is_pos_infty;
    bfe->child_to_read = -1;
    bfe->disable_prefetching = c->disable_prefetching;
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

// FIXME needs toku prefix
void maybe_apply_ancestors_messages_to_node (FT_HANDLE t, FTNODE node, ANCESTORS ancestors, struct pivot_bounds const * const bounds, BOOL* msgs_applied);

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
                                    int layout_version, unsigned int nodesize, unsigned int flags);

unsigned int toku_ftnode_which_child(FTNODE node, const DBT *k,
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
unsigned int toku_ftnode_hot_next_child(FTNODE node,
                                         const DBT *k,
                                         DESCRIPTOR desc,
                                         ft_compare_func cmp);

/* Stuff for testing */
// toku_testsetup_initialize() must be called before any other test_setup_xxx() functions are called.
void toku_testsetup_initialize(void);
int toku_testsetup_leaf(FT_HANDLE brt, BLOCKNUM *blocknum, int n_children, char **keys, int *keylens);
int toku_testsetup_nonleaf (FT_HANDLE brt, int height, BLOCKNUM *diskoff, int n_children, BLOCKNUM *children, char **keys, int *keylens);
int toku_testsetup_root(FT_HANDLE brt, BLOCKNUM);
int toku_testsetup_get_sersize(FT_HANDLE brt, BLOCKNUM); // Return the size on disk.
int toku_testsetup_insert_to_leaf (FT_HANDLE brt, BLOCKNUM, char *key, int keylen, char *val, int vallen);
int toku_testsetup_insert_to_nonleaf (FT_HANDLE brt, BLOCKNUM, enum ft_msg_type, char *key, int keylen, char *val, int vallen);
void toku_pin_node_with_min_bfe(FTNODE* node, BLOCKNUM b, FT_HANDLE t);

// These two go together to do lookups in a ftnode using the keys in a command.
struct cmd_leafval_heaviside_extra {
    ft_compare_func compare_fun;
    DESCRIPTOR desc;
    DBT const * const key;
};
int toku_cmd_leafval_heaviside (OMTVALUE leafentry, void *extra)
    __attribute__((__warn_unused_result__));

// toku_ft_root_put_cmd() accepts non-constant cmd because this is where we set the msn
int toku_ft_root_put_cmd(FT h, FT_MSG_S * cmd)
    __attribute__((__warn_unused_result__));

void *mempool_malloc_from_omt(OMT omt, struct mempool *mp, size_t size, void **maybe_free);
// Effect: Allocate a new object of size SIZE in MP.  If MP runs out of space, allocate new a new mempool space, and copy all the items
//  from the OMT (which items refer to items in the old mempool) into the new mempool.
//  If MAYBE_FREE is NULL then free the old mempool's space.
//  Otherwise, store the old mempool's space in maybe_free.
void
toku_get_node_for_verify(
    BLOCKNUM blocknum,
    FT_HANDLE brt,
    FTNODE* nodep
    );

int
toku_verify_ftnode (FT_HANDLE brt,
                     MSN rootmsn, MSN parentmsn,
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
    BOOL initialized;
    TOKU_ENGINE_STATUS_ROW_S status[FT_UPGRADE_STATUS_NUM_ROWS];
} FT_UPGRADE_STATUS_S, *FT_UPGRADE_STATUS;

void toku_ft_upgrade_get_status(FT_UPGRADE_STATUS);

typedef enum {
    LE_MAX_COMMITTED_XR = 0,
    LE_MAX_PROVISIONAL_XR,
    LE_EXPANDED,
    LE_MAX_MEMSIZE,
    LE_STATUS_NUM_ROWS
} le_status_entry;

typedef struct {
    BOOL initialized;
    TOKU_ENGINE_STATUS_ROW_S status[LE_STATUS_NUM_ROWS];
} LE_STATUS_S, *LE_STATUS;

void toku_le_get_status(LE_STATUS);

typedef enum {
    FT_UPDATES = 0,
    FT_UPDATES_BROADCAST,
    FT_DESCRIPTOR_SET,
    FT_PARTIAL_EVICTIONS_NONLEAF,              // number of nonleaf node partial evictions
    FT_PARTIAL_EVICTIONS_LEAF,                 // number of leaf node partial evictions
    FT_MSN_DISCARDS,                           // how many messages were ignored by leaf because of msn
    FT_MAX_WORKDONE,                           // max workdone value of any buffer
    FT_TOTAL_RETRIES,                          // total number of search retries due to TRY_AGAIN
    FT_MAX_SEARCH_EXCESS_RETRIES,              // max number of excess search retries (retries - treeheight) due to TRY_AGAIN
    FT_SEARCH_TRIES_GT_HEIGHT,                 // number of searches that required more tries than the height of the tree
    FT_SEARCH_TRIES_GT_HEIGHTPLUS3,            // number of searches that required more tries than the height of the tree plus three
    FT_DISK_FLUSH_LEAF,                        // number of leaf nodes flushed to disk,    not for checkpoint
    FT_DISK_FLUSH_NONLEAF,                     // number of nonleaf nodes flushed to disk, not for checkpoint
    FT_DISK_FLUSH_LEAF_FOR_CHECKPOINT,         // number of leaf nodes flushed to disk for checkpoint
    FT_DISK_FLUSH_NONLEAF_FOR_CHECKPOINT,      // number of nonleaf nodes flushed to disk for checkpoint
    FT_CREATE_LEAF,                            // number of leaf nodes created
    FT_CREATE_NONLEAF,                         // number of nonleaf nodes created
    FT_DESTROY_LEAF,                           // number of leaf nodes destroyed
    FT_DESTROY_NONLEAF,                        // number of nonleaf nodes destroyed
    FT_MSG_BYTES_IN,                           // how many bytes of messages injected at root (for all trees)
    FT_MSG_BYTES_OUT,                          // how many bytes of messages flushed from h1 nodes to leaves
    FT_MSG_BYTES_CURR,                         // how many bytes of messages currently in trees (estimate)
    FT_MSG_BYTES_MAX,                          // how many bytes of messages currently in trees (estimate)
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
    FT_NUM_PIVOTS_FETCHED_PREFETCH,            // ... for a prefetch
    FT_NUM_PIVOTS_FETCHED_WRITE,               // ... for a write
    FT_NUM_BASEMENTS_FETCHED_NORMAL,           // how many basement nodes were fetched because they were the target of a query
    FT_NUM_BASEMENTS_FETCHED_AGGRESSIVE,       // ... because they were between lc and rc
    FT_NUM_BASEMENTS_FETCHED_PREFETCH,
    FT_NUM_BASEMENTS_FETCHED_WRITE,
    FT_NUM_MSG_BUFFER_FETCHED_NORMAL,          // how many msg buffers were fetched because they were the target of a query
    FT_NUM_MSG_BUFFER_FETCHED_AGGRESSIVE,      // ... because they were between lc and rc
    FT_NUM_MSG_BUFFER_FETCHED_PREFETCH,
    FT_NUM_MSG_BUFFER_FETCHED_WRITE,
    FT_STATUS_NUM_ROWS
} ft_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[FT_STATUS_NUM_ROWS];
} FT_STATUS_S, *FT_STATUS;

void toku_ft_get_status(FT_STATUS);

void
toku_ft_bn_apply_cmd_once (
    BASEMENTNODE bn,
    const FT_MSG cmd,
    u_int32_t idx,
    LEAFENTRY le,
    uint64_t *workdonep,
    STAT64INFO stats_to_update
    );

void
toku_ft_bn_apply_cmd (
    ft_compare_func compare_fun,
    ft_update_func update_fun,
    DESCRIPTOR desc,
    BASEMENTNODE bn,
    FT_MSG cmd,
    uint64_t *workdone,
    STAT64INFO stats_to_update
    );

void
toku_ft_leaf_apply_cmd (
    ft_compare_func compare_fun,
    ft_update_func update_fun,
    DESCRIPTOR desc,
    FTNODE node,
    FT_MSG cmd,
    uint64_t *workdone,
    STAT64INFO stats_to_update
    );

void
toku_ft_node_put_cmd (
    ft_compare_func compare_fun,
    ft_update_func update_fun,
    DESCRIPTOR desc,
    FTNODE node, 
    FT_MSG cmd, 
    bool is_fresh,
    STAT64INFO stats_to_update
    );

void toku_flusher_thread_set_callback(void (*callback_f)(int, void*), void* extra);

enum deserialize_error_code toku_upgrade_subtree_estimates_to_stat64info(int fd, FT h);

#endif
