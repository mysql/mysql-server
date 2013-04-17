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
enum { OMT_ITEM_OVERHEAD = 0 }; /* No overhead for the OMT item.  The PMA needed to know the idx, but the OMT doesn't. */
enum { BRT_CMD_OVERHEAD = (1 + sizeof(MSN))     // the type plus MSN
};

enum { BRT_DEFAULT_NODE_SIZE = 1 << 22 };

struct nodeheader_in_file {
    int n_in_buffer;
};
enum { BUFFER_HEADER_SIZE = (4 // height//
			     + 4 // n_children
			     + TREE_FANOUT * 8 // children
			     ) };

struct subtree_estimates {
    // estimate number of rows in the tree by counting the number of rows
    // in the leaves.  The stuff in the internal nodes is likely to be off O(1).
    u_int64_t nkeys;  // number of distinct keys (obsolete with removal of dupsort, but not worth removing)
    u_int64_t ndata; // number of key-data pairs (previously leafentry_estimate)
    u_int64_t dsize;  // total size of leafentries
    BOOL      exact;  // are the estimates exact?
};

static struct subtree_estimates const zero_estimates = {0,0,0,TRUE};

static inline struct subtree_estimates __attribute__((__unused__))
make_subtree_estimates (u_int64_t nkeys, u_int64_t ndata, u_int64_t dsize, BOOL exact) {
    return (struct subtree_estimates){nkeys, ndata, dsize, exact};
}

static inline void
subtract_estimates (struct subtree_estimates *a, struct subtree_estimates *b) {
    if (a->nkeys >= b->nkeys) a->nkeys -= b->nkeys; else a->nkeys=0;
    if (a->ndata >= b->ndata) a->ndata -= b->ndata; else a->ndata=0;
    if (a->dsize >= b->dsize) a->dsize -= b->dsize; else a->dsize=0;
}

static inline void
add_estimates (struct subtree_estimates *a, struct subtree_estimates *b) {
    a->nkeys += b->nkeys;
    a->ndata += b->ndata;
    a->dsize += b->dsize;
}


struct brtnode_nonleaf_childinfo {
    BLOCKNUM     blocknum;
    BOOL         have_fullhash;     // do we have the full hash?
    u_int32_t    fullhash;          // the fullhash of the child
    FIFO         buffer;
    unsigned int n_bytes_in_buffer; /* How many bytes are in each buffer (including overheads for the disk-representation) */
};

struct brtnode_leaf_basement_node {
    uint32_t optimized_for_upgrade;   // version number to which this leaf has been optimized, zero if never optimized for upgrade
    BOOL soft_copy_is_up_to_date;        // the data in the OMT reflects the softcopy state.
    OMT buffer;
    unsigned int n_bytes_in_buffer; /* How many bytes to represent the OMT (including the per-key overheads, but not including the overheads for the node. */
    unsigned int seqinsert;         /* number of sequential inserts to this leaf */
};

/* Internal nodes. */
struct brtnode {
    MSN      max_msn_applied_to_node;  // max msn that has been applied to this node (for root node, this is max msn for the tree)
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
    struct kv_pair **childkeys;   /* Pivot keys.  Child 0's keys are <= childkeys[0].  Child 1's keys are <= childkeys[1].
                                                                        Child 1's keys are > childkeys[0]. */

    struct subtree_estimates *subtree_estimates; //array of estimates for each child, for leaf nodes, are estimates
                                                 // of basement nodes
    union node {
	struct nonleaf {
	    unsigned int    n_bytes_in_buffers;

	    struct brtnode_nonleaf_childinfo *childinfos; /* One extra so we can grow */

#define BNC_BLOCKNUM(node,i) ((node)->u.n.childinfos[i].blocknum)
#define BNC_BUFFER(node,i) ((node)->u.n.childinfos[i].buffer)
#define BNC_NBYTESINBUF(node,i) ((node)->u.n.childinfos[i].n_bytes_in_buffer)
#define BNC_HAVE_FULLHASH(node,i) ((node)->u.n.childinfos[i].have_fullhash)
#define BNC_FULLHASH(node,i) ((node)->u.n.childinfos[i].fullhash)

        } n;
	struct leaf {
            struct brtnode_leaf_basement_node *bn; // individual basement nodes of a leaf
	} l;
    } u;
};

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
    BOOL upgrade_brt_performed;         // initially FALSE, set TRUE when brt has been fully updated (even though nodes may not have been)
    int64_t num_blocks_to_upgrade;      // Number of v13 blocks still not newest version. When we release layout 15 we may need to turn this to an array or add more variables.  
    unsigned int nodesize;
    BLOCKNUM root;            // roots of the dictionary
    struct remembered_hash root_hash;     // hash of the root offset.
    unsigned int flags;
    DESCRIPTOR_S descriptor;

    u_int64_t root_put_counter; // the generation number of the brt

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
};

struct brt {
    CACHEFILE cf;
    // The header is shared.  It is also ephemeral.
    struct brt_header *h;

    struct toku_list cursors;

    unsigned int nodesize;
    unsigned int flags;
    BOOL did_set_flags;
    int (*compare_fun)(DB*,const DBT*,const DBT*);

    int (*update_fun)(DB*,
		      const DBT*key, const DBT *old_val, const DBT *extra,
		      void (*set_val)(const DBT *new_val, void*set_extra), void *set_extra);

    DB *db;           // To pass to the compare fun, and close once transactions are done.

    OMT txns; // transactions that are using this OMT (note that the transaction checks the cf also)
    int pinned_by_checkpoint;  //Keep this brt around for checkpoint, like a transaction

    int was_closed; //True when this brt was closed, but is being kept around for transactions (or checkpoint).
    int (*close_db)(DB*, u_int32_t);
    u_int32_t close_flags;

    struct toku_list live_brt_link;
    struct toku_list zombie_brt_link;
};

/* serialization code */
int toku_serialize_brtnode_to_memory (BRTNODE node,
                              /*out*/ size_t *n_bytes_to_write,
                              /*out*/ char  **bytes_to_write);
int toku_serialize_brtnode_to(int fd, BLOCKNUM, BRTNODE node, struct brt_header *h, int n_workitems, int n_threads, BOOL for_checkpoint);
int toku_serialize_rollback_log_to (int fd, BLOCKNUM blocknum, ROLLBACK_LOG_NODE log,
                                    struct brt_header *h, int n_workitems, int n_threads,
                                    BOOL for_checkpoint);
int toku_deserialize_rollback_log_from (int fd, BLOCKNUM blocknum, u_int32_t fullhash, ROLLBACK_LOG_NODE *logp, struct brt_header *h);
int toku_deserialize_brtnode_from (int fd, BLOCKNUM off, u_int32_t /*fullhash*/, BRTNODE *brtnode, struct brt_header *h);
unsigned int toku_serialize_brtnode_size(BRTNODE node); /* How much space will it take? */
int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len);

void toku_verify_or_set_counts(BRTNODE);

int toku_serialize_brt_header_size (struct brt_header *h);
int toku_serialize_brt_header_to (int fd, struct brt_header *h);
int toku_serialize_brt_header_to_wbuf (struct wbuf *, struct brt_header *h, int64_t address_translation, int64_t size_translation);
int toku_deserialize_brtheader_from (int fd, LSN max_acceptable_lsn, struct brt_header **brth);
int toku_serialize_descriptor_contents_to_fd(int fd, const DESCRIPTOR desc, DISKOFF offset);
void toku_serialize_descriptor_contents_to_wbuf(struct wbuf *wb, const DESCRIPTOR desc);
void toku_setup_empty_leafnode( BRTNODE n, u_int32_t num_bn);
void toku_destroy_brtnode_internals(BRTNODE node);
void toku_brtnode_free (BRTNODE *node);

// append a child node to a parent node
void toku_brt_nonleaf_append_child(BRTNODE node, BRTNODE child, struct kv_pair *pivotkey, size_t pivotkeysize);

// append a cmd to a nonleaf node child buffer
void toku_brt_append_to_child_buffer(BRTNODE node, int childnum, int type, MSN msn, XIDS xids, const DBT *key, const DBT *val);

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

extern void toku_brtnode_flush_callback (CACHEFILE cachefile, int fd, BLOCKNUM nodename, void *brtnode_v, void *extraargs, long size, BOOL write_me, BOOL keep_me, BOOL for_checkpoint);
extern int toku_brtnode_fetch_callback (CACHEFILE cachefile, int fd, BLOCKNUM nodename, u_int32_t fullhash, void **brtnode_pv, long *sizep, int*dirty, void*extraargs);
extern int toku_brtnode_pe_callback (void *brtnode_pv, long bytes_to_free, long* bytes_freed, void *extraargs);
extern int toku_brt_alloc_init_header(BRT t, TOKUTXN txn);
extern int toku_read_brt_header_and_store_in_cachefile (CACHEFILE cf, LSN max_acceptable_lsn, struct brt_header **header, BOOL* was_open);
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
    BOOL current_in_omt;
    BOOL prefetching;
    DBT key, val;             // The key-value pair that the cursor currently points to
    OMTCURSOR omtcursor;
    u_int64_t  root_put_counter; // what was the count on the BRT when we validated the cursor?
    TXNID      oldest_living_xid;// what was the oldest live txnid when we created the cursor?
    BOOL is_snapshot_read; // true if query is read_committed, false otherwise
    BOOL is_leaf_mode;
    TOKUTXN ttxn;
    struct brt_cursor_leaf_info  leaf_info;
};

typedef struct ancestors *ANCESTORS;
struct ancestors {
    BRTNODE   node;
    int       childnum; // which buffer holds our ancestors.
    ANCESTORS next;
};
struct pivot_bounds {
    struct kv_pair const * const lower_bound_exclusive;
    struct kv_pair const * const upper_bound_inclusive; // NULL to indicate negative or positive infinity (which are in practice exclusive since there are now transfinite keys in messages).
};

// logs the memory allocation, but not the creation of the new node
void toku_create_new_brtnode (BRT t, BRTNODE *result, int height, int n_children);
int toku_pin_brtnode (BRT brt, BLOCKNUM blocknum, u_int32_t fullhash,
		      UNLOCKERS unlockers,
		      ANCESTORS ancestors, struct pivot_bounds const * const pbounds,
		      BRTNODE *node_p)
    __attribute__((__warn_unused_result__));
void toku_pin_brtnode_holding_lock (BRT brt, BLOCKNUM blocknum, u_int32_t fullhash,
				   ANCESTORS ancestors, struct pivot_bounds const * const pbounds,
				   BRTNODE *node_p);
void toku_unpin_brtnode (BRT brt, BRTNODE node);
unsigned int toku_brtnode_which_child (BRTNODE node , const DBT *k, BRT t)
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
    BRT t;
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

typedef struct update_status {
    u_int64_t updates;
    u_int64_t updates_broadcast;
    u_int64_t descriptor_set;
} UPDATE_STATUS_S, *UPDATE_STATUS;

void toku_update_get_status(UPDATE_STATUS);

void
brt_leaf_apply_cmd_once (
    BASEMENTNODE bn, 
    SUBTREE_EST se,
    const BRT_MSG cmd,
    u_int32_t idx, 
    LEAFENTRY le, 
    TOKULOGGER logger
    );

void 
toku_apply_cmd_to_leaf(BRT t, BRTNODE node, BRT_MSG cmd, int *made_change);

void toku_reset_root_xid_that_created(BRT brt, TXNID new_root_xid_that_created);
// Reset the root_xid_that_created field to the given value.  
// This redefines which xid created the dictionary.

C_END

#endif
