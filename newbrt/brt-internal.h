/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRT_INTERNAL_H
#define BRT_INTERNAL_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "brt_layout_version.h"
#include "toku_assert.h"
#include "block_allocator.h"
#include "cachetable.h"
#include "fifo.h"
#include "brt.h"
#include "toku_list.h"
#include "mempool.h"
#include "kv-pair.h"
#include "omt.h"
#include "leafentry.h"
#include "block_table.h"
#include "leaflock.h"
#include "c_dialects.h"

C_BEGIN

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
    u_int64_t nkeys;  // number of distinct keys (obsolete with removal of dupsort, but not worth removing)
    u_int64_t ndata; // number of key-data pairs (previously leafentry_estimate)
    u_int64_t dsize;  // total size of leafentries
    BOOL      exact;  // are the estimates exact?
};

static struct subtree_estimates const zero_estimates __attribute__((__unused__)) = {0,0,0,TRUE};

static inline struct subtree_estimates __attribute__((__unused__))
make_subtree_estimates (u_int64_t nkeys, u_int64_t ndata, u_int64_t dsize, BOOL exact) {
    return (struct subtree_estimates){nkeys, ndata, dsize, exact};
}

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

/* Internal nodes. */
struct brtnode {
    unsigned int nodesize;
    int ever_been_written;
    unsigned int flags;
    BLOCKNUM thisnodename;   // Which block number is this node?
    int    layout_version; // What version of the data structure?
    int    layout_version_original;	// different (<) from layout_version if upgraded from a previous version (useful for debugging)
    int    layout_version_read_from_disk;  // transient, not serialized to disk, (useful for debugging)
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
						         However, in the absence of duplicate keys, child 1's keys *are* > childkeys[0]. */
        } n;
	struct leaf {
	    struct subtree_estimates leaf_stats; // actually it is exact.
            uint32_t optimized_for_upgrade;   // version number to which this leaf has been optimized, zero if never optimized for upgrade
	    OMT buffer;
            LEAFLOCK_POOL leaflock_pool;
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
    BOOL dictionary_opened;     // True once this header has been associated with a dictionary (a brt fully opened)
    DICTIONARY_ID dict_id;      // unique id for dictionary
    int panic; // If nonzero there was a write error.  Don't write any more, because it probably only gets worse.  This is the error code.
    char *panic_string; // A malloced string that can indicate what went wrong.
    int layout_version;
    int layout_version_original;	// different (<) from layout_version if upgraded from a previous version (useful for debugging)
    int layout_version_read_from_disk;  // transient, not serialized to disk
    BOOL upgrade_brt_performed;         // initially FALSE, set TRUE when brt has been fully updated (even though nodes may not have been)
    int64_t num_blocks_to_upgrade;      // Number of v12 blocks still not newest version. When we release layout 14 we may need to turn this to an array or add more variables.  
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
    BOOL did_set_descriptor;
    DESCRIPTOR_S temp_descriptor;
    int (*compare_fun)(DB*,const DBT*,const DBT*);

    // When an upsert message arrives it contains a key, a value (upserted_val), and an extra DBT (upserted_extra).
    // If there is no such key in the database, then the upserted value is inserted.
    // If there is such a key in the database, then there is associated with that key another value (prev_val).
    // The system calls upsert_fun(DB, upserted_value, upserted_extra, prev_val, set_val, set_extra)
    // where set_val and set_extra are provided by the system.
    // The upsert_fun can look at the DBTs and the DB (to get the db descriptor) and perform one of the following actions:
    //  a) It can return DB_DELETE_UPSERT (which is defined in db.h).  In this case, the system deletes the key-value pair.
    //  b) OR it can return call set_val(new_val, set_extra),
    //     where new_val is the dbt that was passed in.  The set_val function will copy anything it needs out of new_val, so the memory pointed
    //     to by new_val may be stack allocated by upsert_fun (or it may be malloced, in which case upsert_fun should free that memory).
    // Notes: 
    //   1) The DBTs passed to upsert_fun may point to memory that will be freed after the upsert_fun returns.
    //   2) Furtheremore, there is likely to be some sort of lock held when upsert_fun is called.
    // Those notes should not matter, since the upsert_fun should essentially be a pure function of the DBTs and DB descriptor passed in.
    int (*upsert_fun)(DB*, const DBT*key, const DBT *upserted_val, const DBT *upserted_extra, const DBT *prev_val,
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
int toku_serialize_brtnode_to_memory (BRTNODE node, int n_workitems, int n_threads,
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

void toku_verify_or_set_counts(BRTNODE, BOOL);

int toku_serialize_brt_header_size (struct brt_header *h);
int toku_serialize_brt_header_to (int fd, struct brt_header *h);
int toku_serialize_brt_header_to_wbuf (struct wbuf *, struct brt_header *h, int64_t address_translation, int64_t size_translation);
int toku_deserialize_brtheader_from (int fd, LSN max_acceptable_lsn, struct brt_header **brth);
int toku_serialize_descriptor_contents_to_fd(int fd, const DESCRIPTOR desc, DISKOFF offset);
void toku_serialize_descriptor_contents_to_wbuf(struct wbuf *wb, const DESCRIPTOR desc);

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

extern void toku_brtnode_flush_callback (CACHEFILE cachefile, int fd, BLOCKNUM nodename, void *brtnode_v, void *extraargs, long size, BOOL write_me, BOOL keep_me, BOOL for_checkpoint);
extern int toku_brtnode_fetch_callback (CACHEFILE cachefile, int fd, BLOCKNUM nodename, u_int32_t fullhash, void **brtnode_pv, long *sizep, int*dirty, void*extraargs);
extern int toku_brt_alloc_init_header(BRT t, TOKUTXN txn);
extern int toku_read_brt_header_and_store_in_cachefile (CACHEFILE cf, LSN max_acceptable_lsn, struct brt_header **header, BOOL* was_open);
extern CACHEKEY* toku_calculate_root_offset_pointer (BRT brt, u_int32_t *root_hash);

static const BRTNODE null_brtnode=0;

//extern u_int32_t toku_calccrc32_kvpair (const void *key, int keylen, const void *val, int vallen);
//extern u_int32_t toku_calccrc32_kvpair_struct (const struct kv_pair *kvp);
extern u_int32_t toku_calc_fingerprint_cmd (u_int32_t type, XIDS xids, const void *key, u_int32_t keylen, const void *val, u_int32_t vallen);
extern u_int32_t toku_calc_fingerprint_cmdstruct (BRT_MSG cmd);

// How long is the pivot key?
unsigned int toku_brt_pivot_key_len (struct kv_pair *);

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

// logs the memory allocation, but not the creation of the new node
int toku_create_new_brtnode (BRT t, BRTNODE *result, int height, size_t mpsize);
int toku_unpin_brtnode (BRT brt, BRTNODE node);
unsigned int toku_brtnode_which_child (BRTNODE node , DBT *k, BRT t);

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

int toku_verify_brtnode (BRT brt, BLOCKNUM blocknum, int height, struct kv_pair *lesser_pivot, struct kv_pair *greatereq_pivot, int recurse)
    __attribute__ ((warn_unused_result));

void toku_brtheader_free (struct brt_header *h);
int toku_brtheader_close (CACHEFILE cachefile, int fd, void *header_v, char **error_string, BOOL oplsn_valid, LSN oplsn);
int toku_brtheader_begin_checkpoint (CACHEFILE cachefile, int fd, LSN checkpoint_lsn, void *header_v);
int toku_brtheader_checkpoint (CACHEFILE cachefile, int fd, void *header_v);
int toku_brtheader_end_checkpoint (CACHEFILE cachefile, int fd, void *header_v);
int toku_maybe_upgrade_brt(BRT t);
int toku_db_badformat(void);

int toku_brt_remove_on_commit(TOKUTXN child, DBT* iname_dbt_p);
int toku_brt_remove_now(CACHETABLE ct, DBT* iname_dbt_p);


typedef struct brt_upgrade_status {
    u_int64_t header_12;    // how many headers upgrade from version 12
    u_int64_t nonleaf_12;
    u_int64_t leaf_12;
    u_int64_t optimized_for_upgrade_12; // how many optimize_for_upgrade messages sent
} BRT_UPGRADE_STATUS_S, *BRT_UPGRADE_STATUS;

void toku_brt_get_upgrade_status(BRT_UPGRADE_STATUS);


typedef struct le_status {
    u_int64_t max_committed_xr;
    u_int64_t max_provisional_xr;
    u_int64_t expanded;
    u_int64_t max_memsize;
} LE_STATUS_S, *LE_STATUS;

void toku_le_get_status(LE_STATUS);


C_END

#endif
