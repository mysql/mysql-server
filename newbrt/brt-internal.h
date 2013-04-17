/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRT_INTERNAL_H
#define BRT_INTERNAL_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// Symbol TOKUDB_REVISION is not defined by fractal-tree makefiles, so
// BUILD_ID of 1000 indicates development build of main, not a release build.  
#if defined(TOKUDB_REVISION)
#define BUILD_ID TOKUDB_REVISION
#else
#error
#endif

#include "brt_layout_version.h"
#include "toku_assert.h"
#include "block_allocator.h"
#include "cachetable.h"
#include "fifo.h"
#include "brt.h"
#include "toku_list.h"
#include "kv-pair.h"
#include "omt.h"
#include "leafentry.h"
#include "block_table.h"
#include "c_dialects.h"

// Uncomment the following to use quicklz

C_BEGIN

#ifndef BRT_FANOUT
#define BRT_FANOUT 16
#endif
enum { TREE_FANOUT = BRT_FANOUT };
enum { KEY_VALUE_OVERHEAD = 8 }; /* Must store the two lengths. */
enum { BRT_CMD_OVERHEAD = (2 + sizeof(MSN))     // the type plus freshness plus MSN
};

enum { BRT_DEFAULT_NODE_SIZE = 1 << 22 };
enum { BRT_DEFAULT_BASEMENT_NODE_SIZE = 128 * 1024 };

struct nodeheader_in_file {
    int n_in_buffer;
};
enum { BUFFER_HEADER_SIZE = (4 // height//
			     + 4 // n_children
			     + TREE_FANOUT * 8 // children
			     ) };

//
// Field in brtnode_fetch_extra that tells the 
// partial fetch callback what piece of the node
// is needed by the ydb
//
enum brtnode_fetch_type {
    brtnode_fetch_none=1, // no partitions needed.  
    brtnode_fetch_subset, // some subset of partitions needed
    brtnode_fetch_prefetch, // this is part of a prefetch call
    brtnode_fetch_all // every partition is needed
};

//
// An extra parameter passed to cachetable functions 
// That is used in all types of fetch callbacks.
// The contents help the partial fetch and fetch
// callbacks retrieve the pieces of a node necessary
// for the ensuing operation (flush, query, ...)
//
struct brtnode_fetch_extra {
    enum brtnode_fetch_type type;
    // needed for reading a node off disk
    struct brt_header *h;
    // used in the case where type == brtnode_fetch_subset
    // parameters needed to find out which child needs to be decompressed (so it can be read)
    brt_search_t* search;
    DBT *range_lock_left_key, *range_lock_right_key;
    BOOL left_is_neg_infty, right_is_pos_infty;
    // this value will be set during the fetch_callback call by toku_brtnode_fetch_callback or toku_brtnode_pf_req_callback
    // thi callbacks need to evaluate this anyway, so we cache it here so the search code does not reevaluate it
    int child_to_read;
};

struct toku_fifo_entry_key_msn_heaviside_extra {
    DESCRIPTOR desc;
    brt_compare_func cmp;
    FIFO fifo;
    bytevec key;
    ITEMLEN keylen;
    MSN msn;
};

// comparison function for inserting messages into a
// brtnode_nonleaf_childinfo's message_tree
int
toku_fifo_entry_key_msn_heaviside(OMTVALUE v, void *extrap);

struct toku_fifo_entry_key_msn_cmp_extra {
    DESCRIPTOR desc;
    brt_compare_func cmp;
    FIFO fifo;
};

// same thing for qsort_r
int
toku_fifo_entry_key_msn_cmp(void *extrap, const void *ap, const void *bp);

// data of an available partition of a nonleaf brtnode
struct brtnode_nonleaf_childinfo {
    FIFO buffer;
    OMT broadcast_list;
    OMT fresh_message_tree;
    OMT stale_message_tree;
    unsigned int n_bytes_in_buffer; /* How many bytes are in each buffer (including overheads for the disk-representation) */
};

unsigned int toku_bnc_nbytesinbuf(NONLEAF_CHILDINFO bnc);
int toku_bnc_n_entries(NONLEAF_CHILDINFO bnc);
long toku_bnc_memory_size(NONLEAF_CHILDINFO bnc);
int toku_bnc_insert_msg(NONLEAF_CHILDINFO bnc, const void *key, ITEMLEN keylen, const void *data, ITEMLEN datalen, int type, MSN msn, XIDS xids, bool is_fresh, DESCRIPTOR desc, brt_compare_func cmp);
void toku_bnc_empty(NONLEAF_CHILDINFO bnc);
int toku_bnc_flush_to_child(
    brt_compare_func compare_fun, 
    brt_update_func update_fun, 
    DESCRIPTOR desc, 
    CACHEFILE cf,
    NONLEAF_CHILDINFO bnc, 
    BRTNODE child
    );

// data of an available partition of a leaf brtnode
struct brtnode_leaf_basement_node {
    OMT buffer;
    unsigned int n_bytes_in_buffer; /* How many bytes to represent the OMT (including the per-key overheads, but not including the overheads for the node. */
    unsigned int seqinsert;         /* number of sequential inserts to this leaf */
    MSN max_msn_applied; // max message sequence number applied
    bool stale_ancestor_messages_applied;
};

enum  __attribute__((__packed__)) pt_state {  // declare this to be packed so that when used below it will only take 1 byte.
    PT_INVALID = 0,
    PT_ON_DISK = 1,
    PT_COMPRESSED = 2,
    PT_AVAIL = 3};

enum __attribute__((__packed__)) brtnode_child_tag {
    BCT_INVALID = 0,
    BCT_NULL,
    BCT_SUBBLOCK,
    BCT_LEAF,
    BCT_NONLEAF
};
    
typedef struct __attribute__((__packed__)) brtnode_child_pointer {
    enum brtnode_child_tag tag;
    union {
	struct sub_block *subblock;
	struct brtnode_nonleaf_childinfo *nonleaf;
	struct brtnode_leaf_basement_node *leaf;
    } u;
} BRTNODE_CHILD_POINTER;

// a brtnode partition, associated with a child of a node
struct   __attribute__((__packed__)) brtnode_partition {
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
    // stores the offset to the beginning of the partition on disk from the brtnode, and the length, needed to read a partition off of disk
    // the value is only meaningful if the node is clean. If the node is dirty, then the value is meaningless
    //  The START is the distance from the end of the compressed node_info data, to the beginning of the compressed partition
    //  The SIZE is the size of the compressed partition.
    // Rationale:  We cannot store the size from the beginning of the node since we don't know how big the header will be.
    //  However, later when we are doing aligned writes, we won't be able to store the size from the end since we want things to align.
    u_int32_t start,size;
    //
    // pointer to the partition. Depending on the state, they may be different things
    // if state == PT_INVALID, then the node was just initialized and ptr == NULL
    // if state == PT_ON_DISK, then ptr == NULL
    // if state == PT_COMPRESSED, then ptr points to a struct sub_block*
    // if state == PT_AVAIL, then ptr is:
    //         a struct brtnode_nonleaf_childinfo for internal nodes, 
    //         a struct brtnode_leaf_basement_node for leaf nodes
    //
    struct brtnode_child_pointer ptr;

    // clock count used to for pe_callback to determine if a node should be evicted or not
    // for now, saturating the count at 1
    u_int8_t clock_count;

    // How many bytes worth of work was performed by messages in each buffer.
    uint64_t     workdone;
};

struct brtnode {
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
    uint32_t optimized_for_upgrade;   // version number to which this leaf has been optimized, zero if never optimized for upgrade
    int n_children; //for internal nodes, if n_children==TREE_FANOUT+1 then the tree needs to be rebalanced.
                    // for leaf nodes, represents number of basement nodes
    unsigned int    totalchildkeylens;
    struct kv_pair **childkeys;   /* Pivot keys.  Child 0's keys are <= childkeys[0].  Child 1's keys are <= childkeys[1].
                                                                        Child 1's keys are > childkeys[0]. */
    // array of size n_children, consisting of brtnode partitions
    // each one is associated with a child
    // for internal nodes, the ith partition corresponds to the ith message buffer
    // for leaf nodes, the ith partition corresponds to the ith basement node
    struct brtnode_partition *bp;
};

// brtnode partition macros
// BP stands for brtnode_partition
#define BP_BLOCKNUM(node,i) ((node)->bp[i].blocknum)
#define BP_HAVE_FULLHASH(node,i) ((node)->bp[i].have_fullhash)
#define BP_FULLHASH(node,i) ((node)->bp[i].fullhash)
#define BP_STATE(node,i) ((node)->bp[i].state)
#define BP_START(node,i) ((node)->bp[i].start)
#define BP_SIZE(node,i) ((node)->bp[i].size)
#define BP_WORKDONE(node, i)((node)->bp[i].workdone)

//
// macros for managing a node's clock
// Should be managed by brt.c, NOT by serialize/deserialize
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
static inline void set_BNULL(BRTNODE node, int i) {
    assert(0<=i && i<node->n_children);
    node->bp[i].ptr.tag = BCT_NULL;
}
static inline bool is_BNULL (BRTNODE node, int i) {
    assert(0<=i && i<node->n_children);
    return node->bp[i].ptr.tag == BCT_NULL;
}
static inline NONLEAF_CHILDINFO BNC(BRTNODE node, int i) {
    assert(0<=i && i<node->n_children);
    BRTNODE_CHILD_POINTER p = node->bp[i].ptr;
    assert(p.tag==BCT_NONLEAF);
    return p.u.nonleaf;
}
static inline void set_BNC(BRTNODE node, int i, NONLEAF_CHILDINFO nl) {
    assert(0<=i && i<node->n_children);
    BRTNODE_CHILD_POINTER *p = &node->bp[i].ptr;
    p->tag = BCT_NONLEAF;
    p->u.nonleaf = nl;
}
static inline BASEMENTNODE BLB(BRTNODE node, int i) {
    assert(0<=i && i<node->n_children);
    BRTNODE_CHILD_POINTER p = node->bp[i].ptr;
    assert(p.tag==BCT_LEAF);
    return p.u.leaf;
}
static inline void set_BLB(BRTNODE node, int i, BASEMENTNODE bn) {
    assert(0<=i && i<node->n_children);
    BRTNODE_CHILD_POINTER *p = &node->bp[i].ptr;
    p->tag = BCT_LEAF;
    p->u.leaf = bn;
}

static inline SUB_BLOCK BSB(BRTNODE node, int i) {
    assert(0<=i && i<node->n_children);
    BRTNODE_CHILD_POINTER p = node->bp[i].ptr;
    assert(p.tag==BCT_SUBBLOCK);
    return p.u.subblock;
}
static inline void set_BSB(BRTNODE node, int i, SUB_BLOCK sb) {
    assert(0<=i && i<node->n_children);
    BRTNODE_CHILD_POINTER *p = &node->bp[i].ptr;
    p->tag = BCT_SUBBLOCK;
    p->u.subblock = sb;
}

// brtnode leaf basementnode macros, 
#define BLB_MAX_MSN_APPLIED(node,i) (BLB(node,i)->max_msn_applied)
#define BLB_MAX_DSN_APPLIED(node,i) (BLB(node,i)->max_dsn_applied)
#define BLB_BUFFER(node,i) (BLB(node,i)->buffer)
#define BLB_NBYTESINBUF(node,i) (BLB(node,i)->n_bytes_in_buffer)
#define BLB_SEQINSERT(node,i) (BLB(node,i)->seqinsert)

/* pivot flags  (must fit in 8 bits) */
enum {
    BRT_PIVOT_TRUNC = 4,
    BRT_PIVOT_FRONT_COMPRESS = 8,
};

struct remembered_hash {
    BOOL    valid;      // set to FALSE if the fullhash is invalid
    FILENUM fnum;
    BLOCKNUM root;
    u_int32_t fullhash; // fullhash is the hashed value of fnum and root.
};

// The brt_header is not managed by the cachetable.  Instead, it hangs off the cachefile as userdata.

enum brtheader_type {BRTHEADER_CURRENT=1, BRTHEADER_CHECKPOINT_INPROGRESS};

// brt_header is always the current version.
struct brt_header {
    enum brtheader_type type;
    struct brt_header * checkpoint_header;
    CACHEFILE cf;
    u_int64_t checkpoint_count; // Free-running counter incremented once per checkpoint (toggling LSB).
                                // LSB indicates which header location is used on disk so this
                                // counter is effectively a boolean which alternates with each checkpoint.
    LSN checkpoint_lsn;         // LSN of creation of "checkpoint-begin" record in log.  
    int dirty;
    BOOL dictionary_opened;     // True once this header has been associated with a dictionary (a brt fully opened)
    DICTIONARY_ID dict_id;      // unique id for dictionary
    int panic;                  // If nonzero there was a write error.  Don't write any more, because it probably only gets worse.  This is the error code.
    char *panic_string;         // A malloced string that can indicate what went wrong.
    int layout_version;
    int layout_version_original;	// different (<) from layout_version if upgraded from a previous version (useful for debugging)
    int layout_version_read_from_disk;  // transient, not serialized to disk
    uint32_t build_id;                  // build_id (svn rev number) of software that wrote this node to disk
    uint32_t build_id_original;         // build_id of software that created this tree (read from disk, overwritten when written to disk)
    uint64_t time_of_creation;          // time this tree was created
    uint64_t time_of_last_modification; // last time this header was serialized to disk (read from disk, overwritten when written to disk)
    uint64_t time_of_last_verification; // last time that this tree was verified
    BOOL upgrade_brt_performed;         // initially FALSE, set TRUE when brt has been fully updated (even though nodes may not have been)
    int64_t num_blocks_to_upgrade_13;   // Number of v13 blocks still not newest version. 
    int64_t num_blocks_to_upgrade_14;   // Number of v14 blocks still not newest version. 
    unsigned int nodesize;
    unsigned int basementnodesize;
    BLOCKNUM root;            // roots of the dictionary
    struct remembered_hash root_hash;     // hash of the root offset.
    unsigned int flags;
    DESCRIPTOR_S descriptor;


    BLOCK_TABLE blocktable;
    // If a transaction created this BRT, which one?
    // If a transaction locked the BRT when it was empty, which transaction?  (Only the latest one matters)
    // 0 if no such transaction
    TXNID txnid_that_created_or_locked_when_empty;
    TXNID root_that_created_or_locked_when_empty;
    TXNID txnid_that_suppressed_recovery_logs;
    TXNID root_xid_that_created;
    struct toku_list live_brts;
    struct toku_list zombie_brts;
    struct toku_list checkpoint_before_commit_link;

    brt_compare_func compare_fun;
    brt_update_func update_fun;
};

struct brt {
    CACHEFILE cf;
    // The header is shared.  It is also ephemeral.
    struct brt_header *h;

    struct toku_list cursors;

    unsigned int nodesize;
    unsigned int basementnodesize;
    unsigned int flags;
    BOOL did_set_flags;
    brt_compare_func compare_fun;
    brt_update_func update_fun;
    DB *db;           // To pass to the compare fun, and close once transactions are done.

    OMT txns; // transactions that are using this OMT (note that the transaction checks the cf also)
    int pinned_by_checkpoint;  //Keep this brt around for checkpoint, like a transaction

    int was_closed; //True when this brt was closed, but is being kept around for transactions (or checkpoint).
    int (*close_db)(DB*, u_int32_t);
    u_int32_t close_flags;

    struct toku_list live_brt_link;
    struct toku_list zombie_brt_link;
};

long brtnode_memory_size (BRTNODE node);
PAIR_ATTR make_brtnode_pair_attr(BRTNODE node);

/* serialization code */
void
toku_create_compressed_partition_from_available(
    BRTNODE node, 
    int childnum, 
    SUB_BLOCK sb
    );
int toku_serialize_brtnode_to_memory (BRTNODE node,
                                      unsigned int basementnodesize,
                              /*out*/ size_t *n_bytes_to_write,
                              /*out*/ char  **bytes_to_write);
int toku_serialize_brtnode_to(int fd, BLOCKNUM, BRTNODE node, struct brt_header *h, int n_workitems, int n_threads, BOOL for_checkpoint);
int toku_serialize_rollback_log_to (int fd, BLOCKNUM blocknum, ROLLBACK_LOG_NODE log,
                                    struct brt_header *h, int n_workitems, int n_threads,
                                    BOOL for_checkpoint);
int toku_deserialize_rollback_log_from (int fd, BLOCKNUM blocknum, u_int32_t fullhash, ROLLBACK_LOG_NODE *logp, struct brt_header *h);
void toku_deserialize_bp_from_disk(BRTNODE node, int childnum, int fd, struct brtnode_fetch_extra* bfe);
void toku_deserialize_bp_from_compressed(BRTNODE node, int childnum, DESCRIPTOR desc, brt_compare_func cmp);
int toku_deserialize_brtnode_from (int fd, BLOCKNUM off, u_int32_t /*fullhash*/, BRTNODE *brtnode, struct brtnode_fetch_extra* bfe);
unsigned int toku_serialize_brtnode_size(BRTNODE node); /* How much space will it take? */
int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len);

void toku_verify_or_set_counts(BRTNODE);

int toku_serialize_brt_header_size (struct brt_header *h);
int toku_serialize_brt_header_to (int fd, struct brt_header *h);
int toku_serialize_brt_header_to_wbuf (struct wbuf *, struct brt_header *h, int64_t address_translation, int64_t size_translation);
int toku_deserialize_brtheader_from (int fd, LSN max_acceptable_lsn, struct brt_header **brth);
int toku_serialize_descriptor_contents_to_fd(int fd, const DESCRIPTOR desc, DISKOFF offset);
void toku_serialize_descriptor_contents_to_wbuf(struct wbuf *wb, const DESCRIPTOR desc);
BASEMENTNODE toku_create_empty_bn(void);
BASEMENTNODE toku_create_empty_bn_no_buffer(void); // create a basement node with a null buffer.
NONLEAF_CHILDINFO toku_create_empty_nl(void);
void destroy_basement_node (BASEMENTNODE bn);
void destroy_nonleaf_childinfo (NONLEAF_CHILDINFO nl);
void toku_destroy_brtnode_internals(BRTNODE node);
void toku_brtnode_free (BRTNODE *node);
void toku_assert_entire_node_in_memory(BRTNODE node);

// append a child node to a parent node
void toku_brt_nonleaf_append_child(BRTNODE node, BRTNODE child, struct kv_pair *pivotkey, size_t pivotkeysize);

// append a cmd to a nonleaf node child buffer
void toku_brt_append_to_child_buffer(brt_compare_func compare_fun, DESCRIPTOR desc, BRTNODE node, int childnum, int type, MSN msn, XIDS xids, bool is_fresh, const DBT *key, const DBT *val);

#if 1
#define DEADBEEF ((void*)0xDEADBEEF)
#else
#define DEADBEEF ((void*)0xDEADBEEFDEADBEEF)
#endif

struct brtenv {
    CACHETABLE ct;
    TOKULOGGER logger;
    long long checksum_number;
};

extern void toku_brtnode_flush_callback (CACHEFILE cachefile, int fd, BLOCKNUM nodename, void *brtnode_v, void *extraargs, PAIR_ATTR size, PAIR_ATTR* new_size, BOOL write_me, BOOL keep_me, BOOL for_checkpoint);
extern int toku_brtnode_fetch_callback (CACHEFILE cachefile, int fd, BLOCKNUM nodename, u_int32_t fullhash, void **brtnode_pv, PAIR_ATTR *sizep, int*dirty, void*extraargs);
extern void toku_brtnode_pe_est_callback(void* brtnode_pv, long* bytes_freed_estimate, enum partial_eviction_cost *cost, void* write_extraargs);
extern int toku_brtnode_pe_callback (void *brtnode_pv, PAIR_ATTR old_attr, PAIR_ATTR* new_attr, void *extraargs);
extern BOOL toku_brtnode_pf_req_callback(void* brtnode_pv, void* read_extraargs);
int toku_brtnode_pf_callback(void* brtnode_pv, void* read_extraargs, int fd, PAIR_ATTR* sizep);
extern int toku_brtnode_cleaner_callback (void* brtnode_pv, BLOCKNUM blocknum, u_int32_t fullhash, void* extraargs);
extern int toku_brt_alloc_init_header(BRT t, TOKUTXN txn);
extern int toku_read_brt_header_and_store_in_cachefile (BRT brt, CACHEFILE cf, LSN max_acceptable_lsn, struct brt_header **header, BOOL* was_open);
extern CACHEKEY* toku_calculate_root_offset_pointer (BRT brt, u_int32_t *root_hash);

static const BRTNODE null_brtnode=0;

// How long is the pivot key?
unsigned int toku_brt_pivot_key_len (struct kv_pair *);

// Values to be used to update brtcursor if a search is successful.
struct brt_cursor_leaf_info_to_be {
    u_int32_t index;
    OMT       omt;
};

// Values to be used to pin a leaf for shortcut searches
struct brt_cursor_leaf_info {
    struct brt_cursor_leaf_info_to_be  to_be;
};

/* a brt cursor is represented as a kv pair in a tree */
struct brt_cursor {
    struct toku_list cursors_link;
    BRT brt;
    BOOL prefetching;
    DBT key, val;             // The key-value pair that the cursor currently points to
    DBT range_lock_left_key, range_lock_right_key;
    BOOL left_is_neg_infty, right_is_pos_infty;
    BOOL is_snapshot_read; // true if query is read_committed, false otherwise
    BOOL is_leaf_mode;
    TOKUTXN ttxn;
    struct brt_cursor_leaf_info  leaf_info;
};

//
// Helper function to fill a brtnode_fetch_extra with data
// that will tell the fetch callback that the entire node is
// necessary. Used in cases where the entire node
// is required, such as for flushes.
//
static inline void fill_bfe_for_full_read(struct brtnode_fetch_extra *bfe, struct brt_header *h) {
    bfe->type = brtnode_fetch_all;
    bfe->h = h;
    bfe->search = NULL;
    bfe->range_lock_left_key = NULL;
    bfe->range_lock_right_key = NULL;
    bfe->left_is_neg_infty = FALSE;
    bfe->right_is_pos_infty = FALSE;
    bfe->child_to_read = -1;
}

//
// Helper function to fill a brtnode_fetch_extra with data
// that will tell the fetch callback that some subset of the node
// necessary. Used in cases where some of the node is required
// such as for a point query.
//
static inline void fill_bfe_for_subset_read(
    struct brtnode_fetch_extra *bfe,
    struct brt_header *h,
    brt_search_t* search,
    DBT *left,
    DBT *right,
    BOOL left_is_neg_infty,
    BOOL right_is_pos_infty
    )
{
    bfe->type = brtnode_fetch_subset;
    bfe->h = h;
    bfe->search = search;
    bfe->range_lock_left_key = (left->data ? left : NULL);
    bfe->range_lock_right_key = (right->data ? right : NULL);
    bfe->left_is_neg_infty = left_is_neg_infty;
    bfe->right_is_pos_infty = right_is_pos_infty;
    bfe->child_to_read = -1;
}

//
// Helper function to fill a brtnode_fetch_extra with data
// that will tell the fetch callback that no partitions are
// necessary, only the pivots and/or subtree estimates.
// Currently used for stat64.
//
static inline void fill_bfe_for_min_read(struct brtnode_fetch_extra *bfe, struct brt_header *h) {
    bfe->type = brtnode_fetch_none;
    bfe->h = h;
    bfe->search = NULL;
    bfe->range_lock_left_key = NULL;
    bfe->range_lock_right_key = NULL;
    bfe->left_is_neg_infty = FALSE;
    bfe->right_is_pos_infty = FALSE;
    bfe->child_to_read = -1;
}

static inline void destroy_bfe_for_prefetch(struct brtnode_fetch_extra *bfe) {
    assert(bfe->type == brtnode_fetch_prefetch);
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
static inline void fill_bfe_for_prefetch(struct brtnode_fetch_extra *bfe,
                                         struct brt_header *h,
                                         BRT_CURSOR c) {
    bfe->type = brtnode_fetch_prefetch;
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
}

typedef struct ancestors *ANCESTORS;
struct ancestors {
    BRTNODE   node;     // This is the root node if next is NULL.
    int       childnum; // which buffer holds messages destined to the node whose ancestors this list represents.
    ANCESTORS next;     // Parent of this node (so next->node.(next->childnum) refers to this node).
};
struct pivot_bounds {
    struct kv_pair const * const lower_bound_exclusive;
    struct kv_pair const * const upper_bound_inclusive; // NULL to indicate negative or positive infinity (which are in practice exclusive since there are now transfinite keys in messages).
};

void maybe_apply_ancestors_messages_to_node (BRT t, BRTNODE node, ANCESTORS ancestors, struct pivot_bounds const * const bounds);

int
toku_brt_search_which_child(
    DESCRIPTOR desc,
    brt_compare_func cmp,
    BRTNODE node,
    brt_search_t *search
    );

bool
toku_bfe_wants_child_available (struct brtnode_fetch_extra* bfe, int childnum);

int
toku_bfe_leftmost_child_wanted(struct brtnode_fetch_extra *bfe, BRTNODE node);
int
toku_bfe_rightmost_child_wanted(struct brtnode_fetch_extra *bfe, BRTNODE node);

// allocate a block number
// allocate and initialize a brtnode
// put the brtnode into the cache table
void toku_create_new_brtnode (BRT t, BRTNODE *result, int height, int n_children);

// Effect: Fill in N as an empty brtnode.
void toku_initialize_empty_brtnode (BRTNODE n, BLOCKNUM nodename, int height, int num_children, 
                                    int layout_version, unsigned int nodesize, unsigned int flags);

int toku_pin_brtnode_if_clean(
    BRT brt, BLOCKNUM blocknum, u_int32_t fullhash,
    ANCESTORS ancestors, struct pivot_bounds const * const bounds,
    BRTNODE *node_p
    ); 
int toku_pin_brtnode (BRT brt, BLOCKNUM blocknum, u_int32_t fullhash,
		      UNLOCKERS unlockers,
		      ANCESTORS ancestors, struct pivot_bounds const * const pbounds,
                      struct brtnode_fetch_extra *bfe,
		      BRTNODE *node_p)
    __attribute__((__warn_unused_result__));
void toku_pin_brtnode_holding_lock (BRT brt, BLOCKNUM blocknum, u_int32_t fullhash,
				   ANCESTORS ancestors, struct pivot_bounds const * const pbounds,
                                   struct brtnode_fetch_extra *bfe, BOOL apply_ancestor_messages,
				   BRTNODE *node_p);
void toku_unpin_brtnode (BRT brt, BRTNODE node);
unsigned int toku_brtnode_which_child(BRTNODE node, const DBT *k,
                                      DESCRIPTOR desc, brt_compare_func cmp)
    __attribute__((__warn_unused_result__));

/* Stuff for testing */
// toku_testsetup_initialize() must be called before any other test_setup_xxx() functions are called.
void toku_testsetup_initialize(void);
int toku_testsetup_leaf(BRT brt, BLOCKNUM *);
int toku_testsetup_nonleaf (BRT brt, int height, BLOCKNUM *diskoff, int n_children, BLOCKNUM *children, char **keys, int *keylens);
int toku_testsetup_root(BRT brt, BLOCKNUM);
int toku_testsetup_get_sersize(BRT brt, BLOCKNUM); // Return the size on disk.
int toku_testsetup_insert_to_leaf (BRT brt, BLOCKNUM, char *key, int keylen, char *val, int vallen);
int toku_testsetup_insert_to_nonleaf (BRT brt, BLOCKNUM, enum brt_msg_type, char *key, int keylen, char *val, int vallen);

// These two go together to do lookups in a brtnode using the keys in a command.
struct cmd_leafval_heaviside_extra {
    brt_compare_func compare_fun;
    DESCRIPTOR desc;
    DBT const * const key;
};
int toku_cmd_leafval_heaviside (OMTVALUE leafentry, void *extra)
    __attribute__((__warn_unused_result__));

// toku_brt_root_put_cmd() accepts non-constant cmd because this is where we set the msn
int toku_brt_root_put_cmd(BRT brt, BRT_MSG_S * cmd)
    __attribute__((__warn_unused_result__));

int toku_verify_brtnode (BRT brt, MSN rootmsn, MSN parentmsn,
                         BLOCKNUM blocknum, int height, struct kv_pair *lesser_pivot, struct kv_pair *greatereq_pivot, 
                         int (*progress_callback)(void *extra, float progress), void *extra,
                         int recurse, int verbose, int keep_on_going)
    __attribute__ ((warn_unused_result));

void toku_brtheader_free (struct brt_header *h);
int toku_brtheader_close (CACHEFILE cachefile, int fd, void *header_v, char **error_string, BOOL oplsn_valid, LSN oplsn) __attribute__((__warn_unused_result__));
int toku_brtheader_begin_checkpoint (CACHEFILE cachefile, int fd, LSN checkpoint_lsn, void *header_v) __attribute__((__warn_unused_result__));
int toku_brtheader_checkpoint (CACHEFILE cachefile, int fd, void *header_v) __attribute__((__warn_unused_result__));
int toku_brtheader_end_checkpoint (CACHEFILE cachefile, int fd, void *header_v) __attribute__((__warn_unused_result__));
int toku_maybe_upgrade_brt(BRT t) __attribute__((__warn_unused_result__));
int toku_db_badformat(void) __attribute__((__warn_unused_result__));

int toku_brt_remove_on_commit(TOKUTXN child, DBT* iname_dbt_p) __attribute__((__warn_unused_result__));
int toku_brt_remove_now(CACHETABLE ct, DBT* iname_dbt_p) __attribute__((__warn_unused_result__));


typedef struct brt_upgrade_status {
    u_int64_t header_13;    // how many headers were upgraded from version 13
    u_int64_t nonleaf_13;
    u_int64_t leaf_13;
    u_int64_t optimized_for_upgrade; // how many optimize_for_upgrade messages were sent
} BRT_UPGRADE_STATUS_S, *BRT_UPGRADE_STATUS;

void toku_brt_get_upgrade_status(BRT_UPGRADE_STATUS);


typedef struct le_status {
    u_int64_t max_committed_xr;
    u_int64_t max_provisional_xr;
    u_int64_t expanded;
    u_int64_t max_memsize;
} LE_STATUS_S, *LE_STATUS;

void toku_le_get_status(LE_STATUS);

typedef struct brt_status {
    u_int64_t updates;
    u_int64_t updates_broadcast;
    u_int64_t descriptor_set;
    u_int64_t partial_fetch_hit;           // node partition is present
    u_int64_t partial_fetch_miss;          // node is present but partition is absent
    u_int64_t partial_fetch_compressed;    // node partition is present but compressed
    u_int64_t msn_discards;                // how many messages were ignored by leaf because of msn
    u_int64_t max_workdone;                // max workdone value of any buffer
    u_int64_t dsn_gap;                     // dsn has detected a gap in continuity of root-to-leaf path (internal node was evicted and re-read)
    uint64_t  total_searches;              // total number of searches
    uint64_t  total_retries;               // total number of search retries due to TRY_AGAIN
    uint64_t  max_search_excess_retries;   // max number of excess search retries (retries - treeheight) due to TRY_AGAIN
    uint64_t  max_search_root_tries;       // max number of times root node was fetched in a single search
    uint64_t  search_root_retries;         // number of searches that required the root node to be fetched more than once
    uint64_t  search_tries_gt_height;      // number of searches that required more tries than the height of the tree
    uint64_t  search_tries_gt_heightplus3; // number of searches that required more tries than the height of the tree plus three
} BRT_STATUS_S, *BRT_STATUS;

void toku_brt_get_status(BRT_STATUS);

void
brtleaf_split (struct brt_header* h, BRTNODE node, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, BOOL create_new_node, u_int32_t num_dependent_nodes, BRTNODE* dependent_nodes);

void
brt_leaf_apply_cmd_once (
    BASEMENTNODE bn,
    const BRT_MSG cmd,
    u_int32_t idx,
    LEAFENTRY le,
    OMT snapshot_txnids,
    OMT live_list_reverse,
    uint64_t *workdonep
    );

void
brt_leaf_put_cmd (
    brt_compare_func compare_fun,
    brt_update_func update_fun,
    DESCRIPTOR desc,
    BASEMENTNODE bn, 
    BRT_MSG cmd, 
    bool* made_change,
    uint64_t *workdone,
    OMT snapshot_txnids,
    OMT live_list_reverse
    );

void toku_apply_cmd_to_leaf(
    brt_compare_func compare_fun, 
    brt_update_func update_fun, 
    DESCRIPTOR desc, 
    BRTNODE node, 
    BRT_MSG cmd, 
    bool *made_change, 
    uint64_t *workdone, 
    OMT snapshot_txnids, 
    OMT live_list_reverse
    );

void toku_reset_root_xid_that_created(BRT brt, TXNID new_root_xid_that_created);
// Reset the root_xid_that_created field to the given value.  
// This redefines which xid created the dictionary.


void toku_flusher_thread_set_callback(void (*callback_f)(int, void*), void* extra);

C_END

#endif
