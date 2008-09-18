#ifndef BRT_INTERNAL_H
#define BRT_INTERNAL_H

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "toku_assert.h"
#include "block_allocator.h"
#include "cachetable.h"
#include "fifo.h"
#include "brt.h"
#include "list.h"
#include "mempool.h"
#include "kv-pair.h"
#include "leafentry.h"

typedef void *OMTVALUE;
#include "omt.h"

#ifndef BRT_FANOUT
#define BRT_FANOUT 16
#endif
enum { TREE_FANOUT = BRT_FANOUT };
enum { KEY_VALUE_OVERHEAD = 8 }; /* Must store the two lengths. */
enum { OMT_ITEM_OVERHEAD = 0 }; /* No overhead for the OMT item.  The PMA needed to know the idx, but the OMT doesn't. */
enum { BRT_CMD_OVERHEAD = (1     // the type
			   + 8)  // the xid
};
enum { LE_OVERHEAD_BOUND = 9 }; // the type and xid

enum { BRT_DEFAULT_NODE_SIZE = 1 << 20 };

struct nodeheader_in_file {
    int n_in_buffer;
};
enum { BUFFER_HEADER_SIZE = (4 // height//
			     + 4 // n_children
			     + TREE_FANOUT * 8 // children
			     ) };

struct brtnode_nonleaf_childinfo {
    u_int32_t    subtree_fingerprint;
    u_int64_t    leafentry_estimate; // estimate how many leafentries are below us.
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
    int    layout_version; // What version of the data structure? (version 2 adds the xid to the brt cmds)
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
#define BNC_SUBTREE_LEAFENTRY_ESTIMATE(node,i) ((node)->u.n.childinfos[i].leafentry_estimate)
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
	    OMT buffer;
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

struct block_translation_pair {
    DISKOFF diskoff;
    DISKOFF size;
};

// The brt_header is not managed by the cachetable.  Instead, it hangs off the cachefile as userdata.

struct brt_header {
    int refcount;
    int dirty;
    int layout_version;
    unsigned int nodesize;
    int n_named_roots; /* -1 if the only one is unnamed */
    char  **names;             // an array of names.  NULL if subdatabases are not allowed.
    BLOCKNUM *roots;            // An array of the roots of the various dictionaries.  Element 0 holds the element if no subdatabases allowed.
    struct remembered_hash *root_hashes;     // an array of hashes of the root offsets.
    unsigned int *flags_array; // an array of flags.  Element 0 holds the element if no subdatabases allowed.
    
    FIFO fifo; // all the abort and commit commands.  If the header gets flushed to disk, we write the fifo contents beyond the unused_memory.

    // This is the map from block numbers to offsets
    //int n_blocks, n_blocks_array_size;
    //struct block_descriptor *blocks;
    BLOCKNUM free_blocks; // free list for blocks.  Use -1 to indicate that there are no free blocks
    BLOCKNUM unused_blocks; // first unused block

    u_int64_t translated_blocknum_limit;
    struct block_translation_pair *block_translation;

    // Where and how big is the block translation vector stored on disk.
    // The size of the on_disk buffer may no longer match the max_blocknum_translated field, since blocks may have been allocated or freed.
    // We need to remember this old information so we can free it properly.
    u_int64_t block_translation_size_on_disk;    // the size of the block containing the translation (i.e. 8 times the number of entries)
    u_int64_t block_translation_address_on_disk; // 0 if there is no memory allocated
    
    // The in-memory data structure  for block allocation
    BLOCK_ALLOCATOR block_allocator;
};

struct brt {
    CACHEFILE cf;
    char *fname; // the filename
    char *database_name;
    // The header is shared.  It is also ephemeral.
    struct brt_header *h;

    struct list cursors;

    unsigned int nodesize;
    unsigned int flags;
    unsigned int did_set_flags;
    int (*compare_fun)(DB*,const DBT*,const DBT*);
    int (*dup_compare)(DB*,const DBT*,const DBT*);
    DB *db;           // To pass to the compare fun

    void *skey,*sval; /* Used for DBT return values. */

    OMT txns; // transactions that are using this OMT (note that the transaction checks the cf also)
    u_int64_t txn_that_created; // which txn created it.  Use  0 if no such txn.
    u_int64_t root_put_counter;

};

/* serialization code */
void toku_serialize_brtnode_to(int fd, BLOCKNUM, BRTNODE node, BRT brt);
int toku_deserialize_brtnode_from (int fd, BLOCKNUM off, u_int32_t /*fullhash*/, BRTNODE *brtnode, struct brt_header *h);
unsigned int toku_serialize_brtnode_size(BRTNODE node); /* How much space will it take? */
int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len);

void toku_verify_counts(BRTNODE);

int toku_serialize_brt_header_size (struct brt_header *h);
int toku_serialize_brt_header_to (int fd, struct brt_header *h);
int toku_serialize_brt_header_to_wbuf (struct wbuf *, struct brt_header *h);
int toku_deserialize_brtheader_from (int fd, BLOCKNUM off, struct brt_header **brth);

int toku_serialize_fifo_at (int fd, off_t freeoff, FIFO fifo); // Write a fifo into a disk, without worrying about fitting it into a block.  This write is done at the end of the file.

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
//    SPINLOCK  checkpointing;
};

extern void toku_brtnode_flush_callback (CACHEFILE cachefile, BLOCKNUM nodename, void *brtnode_v, void *extraargs, long size, BOOL write_me, BOOL keep_me, LSN modified_lsn, BOOL rename_p);
extern int toku_brtnode_fetch_callback (CACHEFILE cachefile, BLOCKNUM nodename, u_int32_t fullhash, void **brtnode_pv, long *sizep, void*extraargs, LSN *written_lsn);
extern int toku_read_brt_header_and_store_in_cachefile (CACHEFILE cf, struct brt_header **header);
extern CACHEKEY* toku_calculate_root_offset_pointer (BRT brt, u_int32_t *root_hash);

static const BRTNODE null_brtnode=0;

//extern u_int32_t toku_calccrc32_kvpair (const void *key, int keylen, const void *val, int vallen);
//extern u_int32_t toku_calccrc32_kvpair_struct (const struct kv_pair *kvp);
extern u_int32_t toku_calc_fingerprint_cmd (u_int32_t type, TXNID xid, const void *key, u_int32_t keylen, const void *val, u_int32_t vallen);
extern u_int32_t toku_calc_fingerprint_cmdstruct (BRT_CMD cmd);

// How long is the pivot key?
unsigned int toku_brt_pivot_key_len (BRT, struct kv_pair *); // Given the tree
unsigned int toku_brtnode_pivot_key_len (BRTNODE, struct kv_pair *); // Given the node

/* a brt cursor is represented as a kv pair in a tree */
struct brt_cursor {
    struct list cursors_link;
    BRT brt;
    BOOL current_in_omt, prev_in_omt;
    DBT key, val;             // The key-value pair that the cursor currently points to
    DBT prevkey, prevval;     // The key-value pair that the cursor pointed to previously.  (E.g., when we do a DB_NEXT)
    int is_temporary_cursor;  // If it is a temporary cursor then use the following skey and sval to return tokudb-managed values in dbts.  Otherwise use the brt's skey and skval.
    void *skey, *sval;
    OMTCURSOR omtcursor;
    u_int64_t  root_put_counter; // what was the count on the BRT when we validated the cursor?
};

// logs the memory allocation, but not the creation of the new node
int toku_create_new_brtnode (BRT t, BRTNODE *result, int height, TOKULOGGER logger);
int toku_unpin_brtnode (BRT brt, BRTNODE node);
unsigned int toku_brtnode_which_child (BRTNODE node , DBT *k, DBT *d, BRT t);

/* Stuff for testing */
int toku_testsetup_leaf(BRT brt, BLOCKNUM *);
int toku_testsetup_nonleaf (BRT brt, int height, BLOCKNUM *diskoff, int n_children, BLOCKNUM *children, u_int32_t *subtree_fingerprints, char **keys, int *keylens);
int toku_testsetup_root(BRT brt, BLOCKNUM);
int toku_testsetup_get_sersize(BRT brt, BLOCKNUM); // Return the size on disk.
int toku_testsetup_insert_to_leaf (BRT brt, BLOCKNUM, char *key, int keylen, char *val, int vallen, u_int32_t *leaf_fingerprint);
int toku_testsetup_insert_to_nonleaf (BRT brt, BLOCKNUM, enum brt_cmd_type, char *key, int keylen, char *val, int vallen, u_int32_t *subtree_fingerprint);

int toku_set_func_fsync (int (*fsync_function)(int));

// These two go together to do lookups in a brtnode using the keys in a command.
struct cmd_leafval_bessel_extra {
    BRT t;
    BRT_CMD cmd;
    int compare_both_keys; // Set to 1 for DUPSORT databases that are not doing a DELETE_BOTH
};
int toku_cmd_leafval_bessel (OMTVALUE leafentry, void *extra);

int toku_brt_root_put_cmd(BRT brt, BRT_CMD cmd, TOKULOGGER logger);
int toku_cachefile_root_put_cmd (CACHEFILE cf, BRT_CMD cmd, TOKULOGGER logger);

void *mempool_malloc_from_omt(OMT omt, struct mempool *mp, size_t size);

void toku_verify_all_in_mempool(BRTNODE node);

int toku_verify_brtnode (BRT brt, BLOCKNUM blocknum, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen, int recurse) ;

enum brt_layout_version_e {
    BRT_LAYOUT_VERSION_5 = 5,
    BRT_LAYOUT_VERSION_6 = 6, // Diff from 5 to 6:  Add leafentry_estimate
    BRT_LAYOUT_VERSION_7 = 7, // Diff from 6 to 7:  Add exact-bit to leafentry_estimate #818, add magic to header #22, add per-subdatase flags #333
    BRT_LAYOUT_VERSION_8 = 8, // Diff from 7 to 8:  Use murmur instead of crc32.  We are going to make a simplification and stop supporting version 7 and before.  Current As of Beta 1.0.6
    BRT_LAYOUT_VERSION_9 = 9, // Diff from 8 to 9:  Variable-sized blocks and compression.
    BRT_ANTEULTIMATE_VERSION, // the version after the most recent version
    BRT_LAYOUT_VERSION   = BRT_ANTEULTIMATE_VERSION-1 // A hack so I don't have to change this line.
};

void toku_brtheader_free (struct brt_header *h);
int toku_brtheader_close (CACHEFILE cachefile, void *header_v);

#define BLOCK_ALLOCATOR_ALIGNMENT 4096

#endif
