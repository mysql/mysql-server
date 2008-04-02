#ifndef BRT_INTERNAL_H
#define BRT_INTERNAL_H

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "toku_assert.h"
#include "cachetable.h"
#include "fifo.h"
#include "yerror.h"
#include "gpma.h"
#include "brt.h"
#include "crc.h"
#include "list.h"
#include "mempool.h"
#include "kv-pair.h"

#ifndef BRT_FANOUT
#define BRT_FANOUT 16
#endif
enum { TREE_FANOUT = BRT_FANOUT };
enum { KEY_VALUE_OVERHEAD = 8 }; /* Must store the two lengths. */
enum { PMA_ITEM_OVERHEAD = 4 };
enum { BRT_CMD_OVERHEAD = (1     // the type
			   + 8)  // the xid
};
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
    DISKOFF      diskoff;
    FIFO         buffer;
    unsigned int n_bytes_in_buffer; /* How many bytes are in each buffer (including overheads for the disk-representation) */
};

typedef struct brtnode *BRTNODE;
/* Internal nodes. */
struct brtnode {
    enum typ_tag tag;
    unsigned int nodesize;
    unsigned int flags;
    DISKOFF thisnodename;   // The size of the node allocated on disk.  Not all is necessarily in use.
    //  These two LSNs are used to decide when to make a copy of a node instead of overwriting it.
    //  In the TOKULOGGER is a field called checkpoint_lsn which is the lsn of the most recent checkpoint
    LSN     disk_lsn;       // The LSN as of the most recent version on disk.  (Updated by brt-serialize)  This lsn is saved in the node.
    LSN     log_lsn;        // The LSN of the youngest log entry that affects the current in-memory state.   The log write may not have actually made it to disk.  This lsn is not saved in disk (since the two lsns are the same for any node not in main memory.)
    //  The checkpointing works as follows:
    //      When we unpin a node: if it is dirty and disk_lsn<checkpoint_lsn then we need to make a new copy.
    //      When we checkpoint:  Create a checkpoint record, and cause every dirty node to be written to disk.  The new checkpoint record is *not* incorporated into the disk_lsn of the written nodes.
    //      While we are checkpointing, someone may modify a dirty node that has not yet been written.   In that case, when we unpin the node, we make the new copy (because the disk_lsn<checkpoint_lsn), just as we would usually.
    //
    int     layout_version; // What version of the data structure? (version 2 adds the xid to the brt cmds)
    int    height; /* height is always >= 0.  0 for leaf, >0 for nonleaf. */
    u_int32_t rand4fingerprint;
    u_int32_t local_fingerprint; /* For leaves this is everything in the buffer.  For nonleaves, this is everything in the buffers, but does not include child subtree fingerprints. */
    int    dirty;
    union node {
	struct nonleaf {
	    // Don't actually store the subree fingerprint in the in-memory data structure.
	    int             n_children;  /* if n_children==TREE_FANOUT+1 then the tree needs to be rebalanced. */
	    unsigned int    totalchildkeylens;
	    unsigned int    n_bytes_in_buffers;

	    struct brtnode_nonleaf_childinfo *childinfos; /* One extra so we can grow */

#define BNC_SUBTREE_FINGERPRINT(node,i) ((node)->u.n.childinfos[i].subtree_fingerprint)
#define BNC_DISKOFF(node,i) ((node)->u.n.childinfos[i].diskoff)
#define BNC_BUFFER(node,i) ((node)->u.n.childinfos[i].buffer)
#define BNC_NBYTESINBUF(node,i) ((node)->u.n.childinfos[i].n_bytes_in_buffer)

	    struct kv_pair **childkeys;   /* Pivot keys.  Child 0's keys are <= childkeys[0].  Child 1's keys are <= childkeys[1].
							 Note: It is possible that Child 1's keys are == to child 0's key's, so it is
							 not necessarily true that child 1's keys are > childkeys[0].
						         However, in the absense of duplicate keys, child 1's keys *are* > childkeys[0]. */
        } n;
	struct leaf {
	    GPMA buffer;
	    unsigned int n_bytes_in_buffer; /* How many bytes to represent the PMA (including the per-key overheads, but not including the overheads for the node. */
	    struct mempool buffer_mempool;
	} l;
    } u;
};

/* pivot flags  (must fit in 8 bits) */
enum {
    BRT_PIVOT_TRUNC = 4,
    BRT_PIVOT_FRONT_COMPRESS = 8,
};

struct brt_header {
    int dirty;
    unsigned int nodesize;
    DISKOFF freelist;
    DISKOFF unused_memory;
    DISKOFF unnamed_root;
    int n_named_roots; /* -1 if the only one is unnamed */
    char  **names;
    DISKOFF *roots;
    unsigned int flags;
};

struct brt {
    CACHEFILE cf;
    char *database_name;
    // The header is shared.  It is also ephemeral.
    struct brt_header *h;

    struct list cursors;

    unsigned int nodesize;
    unsigned int flags;
    int (*compare_fun)(DB*,const DBT*,const DBT*);
    int (*dup_compare)(DB*,const DBT*,const DBT*);
    DB *db;           // To pass to the compare fun

    void *skey,*sval; /* Used for DBT return values. */
};

/* serialization code */
void toku_serialize_brtnode_to(int fd, DISKOFF off, DISKOFF size, BRTNODE node);
int toku_deserialize_brtnode_from (int fd, DISKOFF off, BRTNODE *brtnode, unsigned int flags, int nodesize);
unsigned int toku_serialize_brtnode_size(BRTNODE node); /* How much space will it take? */
int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len);

void toku_verify_counts(BRTNODE);

int toku_serialize_brt_header_size (struct brt_header *h);
int toku_serialize_brt_header_to (int fd, struct brt_header *h);
int toku_serialize_brt_header_to_wbuf (struct wbuf *, struct brt_header *h);
int toku_deserialize_brtheader_from (int fd, DISKOFF off, struct brt_header **brth);

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

extern cachetable_flush_func_t toku_brtnode_flush_callback, toku_brtheader_flush_callback;
extern cachetable_fetch_func_t toku_brtnode_fetch_callback, toku_brtheader_fetch_callback;
extern int toku_read_and_pin_brt_header (CACHEFILE cf, struct brt_header **header);
extern int toku_unpin_brt_header (BRT brt);
extern CACHEKEY* toku_calculate_root_offset_pointer (BRT brt);

static const BRTNODE null_brtnode=0;

extern u_int32_t toku_calccrc32_kvpair (const void *key, int keylen, const void *val, int vallen);
extern u_int32_t toku_calccrc32_kvpair_struct (const struct kv_pair *kvp);
extern u_int32_t toku_calccrc32_cmd (int type, TXNID xid, const void *key, int keylen, const void *val, int vallen);
extern u_int32_t toku_calccrc32_cmdstruct (BRT_CMD cmd);

// How long is the pivot key?
unsigned int toku_brt_pivot_key_len (BRT, struct kv_pair *); // Given the tree
unsigned int toku_brtnode_pivot_key_len (BRTNODE, struct kv_pair *); // Given the node

/* a brt cursor is represented as a kv pair in a tree */
struct brt_cursor {
    struct list cursors_link;
    BRT brt;
    DBT key;
    DBT val;
    int is_temporary_cursor;  // If it is a temporary cursor then use the following skey and sval to return tokudb-managed values in dbts.  Otherwise use the brt's skey and skval.
    void *skey, *sval;
};

int toku_create_new_brtnode (BRT t, BRTNODE *result, int height, TOKULOGGER logger);
int toku_unpin_brtnode (BRT brt, BRTNODE node) ;
unsigned int toku_brtnode_which_child (BRTNODE node , DBT *k, DBT *d, BRT t);

/* Stuff for testing */
int toku_testsetup_leaf(BRT brt, DISKOFF *diskoff);
int toku_testsetup_nonleaf (BRT brt, int height, DISKOFF *diskoff, int n_children, DISKOFF *children, u_int32_t *subtree_fingerprints, char **keys, int *keylens);
int toku_testsetup_root(BRT brt, DISKOFF diskoff);
int toku_testsetup_get_sersize(BRT brt, DISKOFF diskoff); // Return the size on disk.
int toku_testsetup_insert_to_leaf (BRT brt, DISKOFF diskoff, char *key, int keylen, char *val, int vallen, u_int32_t *leaf_fingerprint);
int toku_testsetup_insert_to_nonleaf (BRT brt, DISKOFF diskoff, enum brt_cmd_type, char *key, int keylen, char *val, int vallen, u_int32_t *subtree_fingerprint);

int toku_set_func_fsync (int (*fsync_function)(int));

/* allocate a kv pair from a kv memory pool */
//static inline struct kv_pair *kv_pair_malloc_mempool(const void *key, int keylen, const void *val, int vallen, struct mempool *mp) {
//    struct kv_pair *kv = toku_mempool_malloc(mp, sizeof (struct kv_pair) + keylen + vallen, 4);
//    if (kv)
//        kv_pair_init(kv, key, keylen, val, vallen);
//    return kv;
//}

int toku_brtnode_compress_kvspace (GPMA pma, struct mempool *mp);

static inline struct kv_pair *brtnode_malloc_kv_pair (GPMA pma, struct mempool *mp, const void *key, unsigned int keylen, const void *val, unsigned int vallen) {
    struct kv_pair *kv = toku_mempool_malloc(mp, sizeof (struct kv_pair) + keylen + vallen, 4);
    if (kv == 0) {
	if (0 == toku_brtnode_compress_kvspace (pma, mp)) {
	    kv = toku_mempool_malloc(mp, sizeof (struct kv_pair) + keylen + vallen, 4);
	    toku_verify_gpma(pma);
	    assert(kv);
	}
    }
    kv_pair_init(kv, key, keylen, val, vallen);
    return kv;
}

// used for the leaf compare fun
struct lc_pair {
    BRT t;
    int compare_both; // compare_both is set if it is a DUPSORT database and both keys are needed (e.g, for DB_DELETE_ANY)
};
int toku_brtleaf_compare_fun (u_int32_t alen __attribute__((__unused__)), void *aval, u_int32_t blen __attribute__((__unused__)), void *bval, void *lc /*this is (struct lc_pair *) cast to (void*). */) ;

#endif
