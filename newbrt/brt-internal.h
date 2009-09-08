/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRT_INTERNAL_H
#define BRT_INTERNAL_H

#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "toku_assert.h"
#include "block_allocator.h"
#include "cachetable.h"
#include "fifo.h"
#include "brt.h"
#include "list.h"
#include "mempool.h"
#include "kv-pair.h"
#include "omt.h"
#include "leafentry.h"
#include "block_table.h"
#include "leaflock.h"

#ifndef BRT_FANOUT
#define BRT_FANOUT 16
#endif
enum { TREE_FANOUT = BRT_FANOUT };
enum { KEY_VALUE_OVERHEAD = 8 }; /* Must store the two lengths. */
enum { OMT_ITEM_OVERHEAD = 0 }; /* No overhead for the OMT item.  The PMA needed to know the idx, but the OMT doesn't. */
enum { BRT_CMD_OVERHEAD = (1)     // the type
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
    u_int64_t nkeys;  // number of distinct keys.
    u_int64_t ndata; // number of key-data pairs (previously leafentry_estimate)
    u_int64_t dsize;  // total size of leafentries
    BOOL      exact;  // are the estimates exact?
};

static struct subtree_estimates const zero_estimates __attribute__((__unused__)) = {0,0,0,TRUE};

static inline void __attribute__((__unused__))
subtract_estimates (struct subtree_estimates *a, struct subtree_estimates *b) {
    if (a->nkeys >= b->nkeys) a->nkeys -= b->nkeys; else a->nkeys=0;
    if (a->ndata >= b->ndata) a->ndata -= b->ndata; else a->ndata=0;
    if (a->dsize >= b->dsize) a->dsize -= b->dsize; else a->dsize=0;
}

static inline void __attribute__((__unused__))
add_estimates (struct subtree_estimates *a, struct subtree_estimates *b) {
    a->nkeys += b->nkeys;
    a->ndata += b->ndata;
    a->dsize += b->dsize;
}


struct brtnode_nonleaf_childinfo {
    u_int32_t    subtree_fingerprint;
    struct subtree_estimates subtree_estimates;
    BLOCKNUM     blocknum;
    BOOL         have_fullhash;     // do we have the full hash?
    u_int32_t    fullhash;          // the fullhash of the child
    FIFO         buffer;
    unsigned int n_bytes_in_buffer; /* How many bytes are in each buffer (including overheads for the disk-representation) */
};

typedef struct brtnode *BRTNODE;
/* Internal nodes. */
struct brtnode {
    enum typ_tag tag;
    struct descriptor *desc;
    unsigned int nodesize;
    int ever_been_written;
    unsigned int flags;
    BLOCKNUM thisnodename;   // Which block number is this node?
    //  These two LSNs are used to decide when to make a copy of a node instead of overwriting it.
    //  In the TOKULOGGER is a field called checkpoint_lsn which is the lsn of the most recent checkpoint
    LSN     disk_lsn;       // The LSN as of the most recent version on disk.  (Updated by brt-serialize)  This lsn is saved in the node.
    LSN     log_lsn;        // The LSN of the youngest log entry that affects the current in-memory state.   The log write may not have actually made it to disk.  This lsn is not saved in disk (since the two lsns are the same for any node not in main memory.)
    //  The checkpointing works as follows:
    //      When we unpin a node: if it is dirty and disk_lsn<checkpoint_lsn then we need to make a new copy.
    //      When we checkpoint:  Create a checkpoint record, and cause every dirty node to be written to disk.  The new checkpoint record is *not* incorporated into the disk_lsn of the written nodes.
    //      While we are checkpointing, someone may modify a dirty node that has not yet been written.   In that case, when we unpin the node, we make the new copy (because the disk_lsn<checkpoint_lsn), just as we would usually.
    //
    int    layout_version; // What version of the data structure?
    int    height; /* height is always >= 0.  0 for leaf, >0 for nonleaf. */
    u_int32_t rand4fingerprint;
    u_int32_t local_fingerprint; /* For leaves this is everything in the buffer.  For nonleaves, this is everything in the buffers, but does not include child subtree fingerprints. */
    int    dirty;
    u_int32_t fullhash;
    union node {
	struct nonleaf {
	    // Don't actually store the subree fingerprint in the in-memory data structure.
	    int             n_children;  /* if n_children==TREE_FANOUT+1 then the tree needs to be rebalanced. */
	    unsigned int    totalchildkeylens;
	    unsigned int    n_bytes_in_buffers;

	    struct brtnode_nonleaf_childinfo *childinfos; /* One extra so we can grow */

#define BNC_SUBTREE_FINGERPRINT(node,i) ((node)->u.n.childinfos[i].subtree_fingerprint)
#define BNC_SUBTREE_ESTIMATES(node,i) ((node)->u.n.childinfos[i].subtree_estimates)
#define BNC_BLOCKNUM(node,i) ((node)->u.n.childinfos[i].blocknum)
#define BNC_BUFFER(node,i) ((node)->u.n.childinfos[i].buffer)
#define BNC_NBYTESINBUF(node,i) ((node)->u.n.childinfos[i].n_bytes_in_buffer)
#define BNC_HAVE_FULLHASH(node,i) ((node)->u.n.childinfos[i].have_fullhash)
#define BNC_FULLHASH(node,i) ((node)->u.n.childinfos[i].fullhash)

	    struct kv_pair **childkeys;   /* Pivot keys.  Child 0's keys are <= childkeys[0].  Child 1's keys are <= childkeys[1].
							 Note: It is possible that Child 1's keys are == to child 0's key's, so it is
							 not necessarily true that child 1's keys are > childkeys[0].
						         However, in the absense of duplicate keys, child 1's keys *are* > childkeys[0]. */
        } n;
	struct leaf {
	    struct subtree_estimates leaf_stats; // actually it is exact.
	    OMT buffer;
	    LEAFLOCK leaflock;
	    unsigned int n_bytes_in_buffer; /* How many bytes to represent the OMT (including the per-key overheads, but not including the overheads for the node. */
            unsigned int seqinsert;         /* number of sequential inserts to this leaf */
	    struct mempool buffer_mempool;
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
    int panic; // If nonzero there was a write error.  Don't write any more, because it probably only gets worse.  This is the error code.
    char *panic_string; // A malloced string that can indicate what went wrong.
    int layout_version;
    unsigned int nodesize;
    BLOCKNUM root;            // roots of the dictionary
    struct remembered_hash root_hash;     // hash of the root offset.
    unsigned int flags;
    struct descriptor descriptor;

    u_int64_t root_put_counter; // the generation number of the brt

    BLOCK_TABLE blocktable;
    // If a transaction created this BRT, which one?
    // If a transaction locked the BRT when it was empty, which transaction?  (Only the latest one matters)
    // 0 if no such transaction
    TXNID txnid_that_created_or_locked_when_empty;
    struct list live_brts;
    struct list zombie_brts;
};

struct brt {
    CACHEFILE cf;
    char *fname; // the filename
    // The header is shared.  It is also ephemeral.
    struct brt_header *h;

    struct list cursors;

    unsigned int nodesize;
    unsigned int flags;
    BOOL did_set_flags;
    BOOL did_set_descriptor;
    BOOL did_set_filenum;
    FILENUM filenum;
    struct descriptor temp_descriptor;
    toku_dbt_upgradef dbt_userformat_upgrade;
    int (*compare_fun)(DB*,const DBT*,const DBT*);
    int (*dup_compare)(DB*,const DBT*,const DBT*);
    DB *db;           // To pass to the compare fun, and close once transactions are done.

    OMT txns; // transactions that are using this OMT (note that the transaction checks the cf also)

    int was_closed; //True when this brt was closed, but is being kept around for transactions.
    int (*close_db)(DB*, u_int32_t);
    u_int32_t close_flags;

    struct list live_brt_link;
    struct list zombie_brt_link;
};

/* serialization code */
int toku_serialize_brtnode_to(int fd, BLOCKNUM, BRTNODE node, struct brt_header *h, int n_workitems, int n_threads, BOOL for_checkpoint);
int toku_deserialize_brtnode_from (int fd, BLOCKNUM off, u_int32_t /*fullhash*/, BRTNODE *brtnode, struct brt_header *h);
unsigned int toku_serialize_brtnode_size(BRTNODE node); /* How much space will it take? */
int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len);

void toku_verify_counts(BRTNODE);

int toku_serialize_brt_header_size (struct brt_header *h);
int toku_serialize_brt_header_to (int fd, struct brt_header *h);
int toku_serialize_brt_header_to_wbuf (struct wbuf *, struct brt_header *h, int64_t address_translation, int64_t size_translation);
int toku_deserialize_brtheader_from (int fd, struct brt_header **brth);
int toku_serialize_descriptor_contents_to_fd(int fd, struct descriptor *desc, DISKOFF offset);

void toku_brtnode_free (BRTNODE *node);

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

extern void toku_brtnode_flush_callback (CACHEFILE cachefile, BLOCKNUM nodename, void *brtnode_v, void *extraargs, long size, BOOL write_me, BOOL keep_me, BOOL for_checkpoint);
extern int toku_brtnode_fetch_callback (CACHEFILE cachefile, BLOCKNUM nodename, u_int32_t fullhash, void **brtnode_pv, long *sizep, void*extraargs, LSN *written_lsn);
extern int toku_brt_alloc_init_header(BRT t);
extern int toku_read_brt_header_and_store_in_cachefile (CACHEFILE cf, struct brt_header **header);
extern CACHEKEY* toku_calculate_root_offset_pointer (BRT brt, u_int32_t *root_hash);

static const BRTNODE null_brtnode=0;

//extern u_int32_t toku_calccrc32_kvpair (const void *key, int keylen, const void *val, int vallen);
//extern u_int32_t toku_calccrc32_kvpair_struct (const struct kv_pair *kvp);
extern u_int32_t toku_calc_fingerprint_cmd (u_int32_t type, XIDS xids, const void *key, u_int32_t keylen, const void *val, u_int32_t vallen);
extern u_int32_t toku_calc_fingerprint_cmdstruct (BRT_MSG cmd);

// How long is the pivot key?
unsigned int toku_brt_pivot_key_len (BRT, struct kv_pair *); // Given the tree
unsigned int toku_brtnode_pivot_key_len (BRTNODE, struct kv_pair *); // Given the node

// Values to be used to update brtcursor if a search is successful.
struct brt_cursor_leaf_info_to_be {
    u_int32_t index;
    OMT       omt;
};

// Values to be used to pin a leaf for shortcut searches
// and to access the leaflock.
struct brt_cursor_leaf_info {
#if TOKU_MULTIPLE_MAIN_THREADS
    BLOCKNUM  blocknumber;
    u_int32_t fullhash;
#endif
    BRTNODE   node;
    LEAFLOCK  leaflock;
    struct brt_cursor_leaf_info_to_be  to_be;
};

/* a brt cursor is represented as a kv pair in a tree */
struct brt_cursor {
    struct list cursors_link;
    BRT brt;
    BOOL current_in_omt;
    BOOL prefetching;
    DBT key, val;             // The key-value pair that the cursor currently points to
    OMTCURSOR omtcursor;
    u_int64_t  root_put_counter; // what was the count on the BRT when we validated the cursor?
    TXNID      oldest_living_xid;// what was the oldest live txnid when we created the cursor?
    struct brt_cursor_leaf_info  leaf_info;
};

// logs the memory allocation, but not the creation of the new node
int toku_create_new_brtnode (BRT t, BRTNODE *result, int height, size_t mpsize);
int toku_unpin_brtnode (BRT brt, BRTNODE node);
unsigned int toku_brtnode_which_child (BRTNODE node , DBT *k, DBT *d, BRT t);

/* Stuff for testing */
int toku_testsetup_leaf(BRT brt, BLOCKNUM *);
int toku_testsetup_nonleaf (BRT brt, int height, BLOCKNUM *diskoff, int n_children, BLOCKNUM *children, u_int32_t *subtree_fingerprints, char **keys, int *keylens);
int toku_testsetup_root(BRT brt, BLOCKNUM);
int toku_testsetup_get_sersize(BRT brt, BLOCKNUM); // Return the size on disk.
int toku_testsetup_insert_to_leaf (BRT brt, BLOCKNUM, char *key, int keylen, char *val, int vallen, u_int32_t *leaf_fingerprint);
int toku_testsetup_insert_to_nonleaf (BRT brt, BLOCKNUM, enum brt_msg_type, char *key, int keylen, char *val, int vallen, u_int32_t *subtree_fingerprint);

// These two go together to do lookups in a brtnode using the keys in a command.
struct cmd_leafval_heaviside_extra {
    BRT t;
    BRT_MSG cmd;
    int compare_both_keys; // Set to 1 for DUPSORT databases that are not doing a DELETE_BOTH
};
int toku_cmd_leafval_heaviside (OMTVALUE leafentry, void *extra);

int toku_brt_root_put_cmd(BRT brt, BRT_MSG cmd);

void *mempool_malloc_from_omt(OMT omt, struct mempool *mp, size_t size, void **maybe_free);
// Effect: Allocate a new object of size SIZE in MP.  If MP runs out of space, allocate new a new mempool space, and copy all the items
//  from the OMT (which items refer to items in the old mempool) into the new mempool.
//  If MAYBE_FREE is NULL then free the old mempool's space.
//  Otherwise, store the old mempool's space in maybe_free.

void mempool_release(struct mempool *); // release anything that was not released when the ..._norelease function was called.

void toku_verify_all_in_mempool(BRTNODE node);

int toku_verify_brtnode (BRT brt, BLOCKNUM blocknum, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen, int recurse) ;

enum brt_layout_version_e {
    BRT_LAYOUT_VERSION_5 = 5,
    BRT_LAYOUT_VERSION_6 = 6,   // Diff from 5 to 6:  Add leafentry_estimate
    BRT_LAYOUT_VERSION_7 = 7,   // Diff from 6 to 7:  Add exact-bit to leafentry_estimate #818, add magic to header #22, add per-subdatase flags #333
    BRT_LAYOUT_VERSION_8 = 8,   // Diff from 7 to 8:  Use murmur instead of crc32.  We are going to make a simplification and stop supporting version 7 and before.  Current As of Beta 1.0.6
    BRT_LAYOUT_VERSION_9 = 9,   // Diff from 8 to 9:  Variable-sized blocks and compression.
    BRT_LAYOUT_VERSION_10 = 10, // Diff from 9 to 10: Variable number of compressed sub-blocks per block, disk byte order == intel byte order, Subtree estimates instead of just leafentry estimates, translation table, dictionary descriptors, checksum in header, subdb support removed from brt layer
    BRT_LAYOUT_VERSION_11 = 11, // Diff from 10 to 11: Nested transaction leafentries (completely redesigned).  BRT_CMDs on disk now support XIDS (multiple txnids) instead of exactly one.
    BRT_NEXT_VERSION,           // the version after the current version
    BRT_LAYOUT_VERSION   = BRT_NEXT_VERSION-1, // A hack so I don't have to change this line.
    BRT_LAYOUT_MIN_SUPPORTED_VERSION = BRT_LAYOUT_VERSION_10 // Minimum version supported for transparent upgrade.
};

void toku_brtheader_free (struct brt_header *h);
int toku_brtheader_close (CACHEFILE cachefile, void *header_v, char **error_string, LSN);
int toku_brtheader_begin_checkpoint (CACHEFILE cachefile, LSN checkpoint_lsn, void *header_v);
int toku_brtheader_checkpoint (CACHEFILE cachefile, void *header_v);
int toku_brtheader_end_checkpoint (CACHEFILE cachefile, void *header_v);

int toku_db_badformat(void);

#endif
