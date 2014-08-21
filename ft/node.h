/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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

#include "ft/bndata.h"
#include "ft/comparator.h"
#include "ft/ft.h"
#include "ft/msg_buffer.h"

/* Pivot keys.
 * Child 0's keys are <= pivotkeys[0]. 
 * Child 1's keys are <= pivotkeys[1]. 
 * Child 1's keys are > pivotkeys[0].
 * etc
 */
class ftnode_pivot_keys {
public:
    // effect: create an empty set of pivot keys
    void create_empty();

    // effect: create pivot keys by copying the given DBT array
    void create_from_dbts(const DBT *keys, int n);

    // effect: create pivot keys as a clone of an existing set of pivotkeys
    void create_from_pivot_keys(const ftnode_pivot_keys &pivotkeys);

    void destroy();

    // effect: deserialize pivot keys previously serialized by serialize_to_wbuf()
    void deserialize_from_rbuf(struct rbuf *rb, int n);

    // returns: unowned DBT representing the i'th pivot key
    DBT get_pivot(int i) const;

    // effect: fills a DBT with the i'th pivot key
    // returns: the given dbt
    DBT *fill_pivot(int i, DBT *dbt) const;

    // effect: insert a pivot into the i'th position, shifting others to the right
    void insert_at(const DBT *key, int i);

    // effect: append pivotkeys to the end of our own pivot keys
    void append(const ftnode_pivot_keys &pivotkeys);

    // effect: replace the pivot at the i'th position
    void replace_at(const DBT *key, int i);

    // effect: removes the i'th pivot key, shifting others to the left
    void delete_at(int i);

    // effect: split the pivot keys, removing all pivots at position greater
    //         than or equal to `i' and storing them in *other
    // requires: *other is empty (size == 0)
    void split_at(int i, ftnode_pivot_keys *other);

    // effect: serialize pivot keys to a wbuf
    // requires: wbuf has at least ftnode_pivot_keys::total_size() bytes available
    void serialize_to_wbuf(struct wbuf *wb) const;

    int num_pivots() const;

    // return: the total size of this data structure
    size_t total_size() const;

    // return: the sum of the keys sizes of each pivot (for serialization)
    size_t serialized_size() const;

private:
    inline size_t _align4(size_t x) const {
        return roundup_to_multiple(4, x);
    }

    // effect: create pivot keys, in fixed key format, by copying the given key array
    void _create_from_fixed_keys(const char *fixedkeys, size_t fixed_keylen, int n);

    char *_fixed_key(int i) const {
        return &_fixed_keys[i * _fixed_keylen_aligned];
    }

    bool _fixed_format() const {
        return _fixed_keys != nullptr;
    }

    void sanity_check() const;

    void _insert_at_dbt(const DBT *key, int i);
    void _append_dbt(const ftnode_pivot_keys &pivotkeys);
    void _replace_at_dbt(const DBT *key, int i);
    void _delete_at_dbt(int i);
    void _split_at_dbt(int i, ftnode_pivot_keys *other);

    void _insert_at_fixed(const DBT *key, int i);
    void _append_fixed(const ftnode_pivot_keys &pivotkeys);
    void _replace_at_fixed(const DBT *key, int i);
    void _delete_at_fixed(int i);
    void _split_at_fixed(int i, ftnode_pivot_keys *other);

    // adds/destroys keys at a certain index (in dbt format),
    // maintaining _total_size, but not _num_pivots
    void _add_key_dbt(const DBT *key, int i);
    void _destroy_key_dbt(int i);

    // conversions to and from packed key array format
    void _convert_to_dbt_format();
    void _convert_to_fixed_format();

    // If every key is _fixed_keylen long, then _fixed_key is a
    // packed array of keys..
    char *_fixed_keys;
    // The actual length of the fixed key
    size_t _fixed_keylen;
    // The aligned length that we use for fixed key storage
    size_t _fixed_keylen_aligned;

    // ..otherwise _fixed_keys is null and we store an array of dbts,
    // each representing a key. this is simpler but less cache-efficient.
    DBT *_dbt_keys;

    int _num_pivots;
    size_t _total_size;
};

// TODO: class me up
struct ftnode {
    MSN      max_msn_applied_to_node_on_disk; // max_msn_applied that will be written to disk
    unsigned int flags;
    BLOCKNUM blocknum;   // Which block number is this node?
    int    layout_version; // What version of the data structure?
    int    layout_version_original;	// different (<) from layout_version if upgraded from a previous version (useful for debugging)
    int    layout_version_read_from_disk;  // transient, not serialized to disk, (useful for debugging)
    uint32_t build_id;       // build_id (svn rev number) of software that wrote this node to disk
    int    height; /* height is always >= 0.  0 for leaf, >0 for nonleaf. */
    int    dirty;
    uint32_t fullhash;

    // for internal nodes, if n_children==fanout+1 then the tree needs to be rebalanced.
    // for leaf nodes, represents number of basement nodes
    int n_children;
    ftnode_pivot_keys pivotkeys;

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
    struct ctpair *ct_pair;
};
typedef struct ftnode *FTNODE;

// data of an available partition of a leaf ftnode
struct ftnode_leaf_basement_node {
    bn_data data_buffer;
    unsigned int seqinsert;         // number of sequential inserts to this leaf 
    MSN max_msn_applied;            // max message sequence number applied
    bool stale_ancestor_messages_applied;
    STAT64INFO_S stat64_delta;      // change in stat64 counters since basement was last written to disk
};
typedef struct ftnode_leaf_basement_node *BASEMENTNODE;

enum pt_state {  // declare this to be packed so that when used below it will only take 1 byte.
    PT_INVALID = 0,
    PT_ON_DISK = 1,
    PT_COMPRESSED = 2,
    PT_AVAIL = 3};

enum ftnode_child_tag {
    BCT_INVALID = 0,
    BCT_NULL,
    BCT_SUBBLOCK,
    BCT_LEAF,
    BCT_NONLEAF
};

typedef toku::omt<int32_t> off_omt_t;
typedef toku::omt<int32_t, int32_t, true> marked_off_omt_t;

// data of an available partition of a nonleaf ftnode
struct ftnode_nonleaf_childinfo {
    message_buffer msg_buffer;
    off_omt_t broadcast_list;
    marked_off_omt_t fresh_message_tree;
    off_omt_t stale_message_tree;
    uint64_t flow[2];  // current and last checkpoint
};
typedef struct ftnode_nonleaf_childinfo *NONLEAF_CHILDINFO;
    
typedef struct ftnode_child_pointer {
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
typedef struct ftnode_disk_data *FTNODE_DISK_DATA;

// TODO: Turn these into functions instead of macros
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

//
// TODO: Fix all these names
//       Organize declarations
//       Fix widespread parameter ordering inconsistencies
//
BASEMENTNODE toku_create_empty_bn(void);
BASEMENTNODE toku_create_empty_bn_no_buffer(void); // create a basement node with a null buffer.
NONLEAF_CHILDINFO toku_clone_nl(NONLEAF_CHILDINFO orig_childinfo);
BASEMENTNODE toku_clone_bn(BASEMENTNODE orig_bn);
NONLEAF_CHILDINFO toku_create_empty_nl(void);
void destroy_basement_node (BASEMENTNODE bn);
void destroy_nonleaf_childinfo (NONLEAF_CHILDINFO nl);
void toku_destroy_ftnode_internals(FTNODE node);
void toku_ftnode_free (FTNODE *node);
bool toku_ftnode_fully_in_memory(FTNODE node);
void toku_ftnode_assert_fully_in_memory(FTNODE node);
void toku_evict_bn_from_memory(FTNODE node, int childnum, FT ft);
BASEMENTNODE toku_detach_bn(FTNODE node, int childnum);
void toku_ftnode_update_disk_stats(FTNODE ftnode, FT ft, bool for_checkpoint);
void toku_ftnode_clone_partitions(FTNODE node, FTNODE cloned_node);

void toku_initialize_empty_ftnode(FTNODE node, BLOCKNUM blocknum, int height, int num_children, 
                                  int layout_version, unsigned int flags);

int toku_ftnode_which_child(FTNODE node, const DBT *k, const toku::comparator &cmp);
void toku_ftnode_save_ct_pair(CACHEKEY key, void *value_data, PAIR p);

//
// TODO: put the heaviside functions into their respective 'struct .*extra;' namespaces
//
struct toku_msg_buffer_key_msn_heaviside_extra {
    const toku::comparator &cmp;
    message_buffer *msg_buffer;
    const DBT *key;
    MSN msn;
    toku_msg_buffer_key_msn_heaviside_extra(const toku::comparator &c, message_buffer *mb, const DBT *k, MSN m) :
        cmp(c), msg_buffer(mb), key(k), msn(m) {
    }
};
int toku_msg_buffer_key_msn_heaviside(const int32_t &v, const struct toku_msg_buffer_key_msn_heaviside_extra &extra);

struct toku_msg_buffer_key_msn_cmp_extra {
    const toku::comparator &cmp;
    message_buffer *msg_buffer;
    toku_msg_buffer_key_msn_cmp_extra(const toku::comparator &c, message_buffer *mb) :
        cmp(c), msg_buffer(mb) {
    }
};
int toku_msg_buffer_key_msn_cmp(const struct toku_msg_buffer_key_msn_cmp_extra &extrap, const int &a, const int &b);

struct toku_msg_leafval_heaviside_extra {
    const toku::comparator &cmp;
    DBT const *const key;
    toku_msg_leafval_heaviside_extra(const toku::comparator &c, const DBT *k) :
        cmp(c), key(k) {
    }
};
int toku_msg_leafval_heaviside(DBT const &kdbt, const struct toku_msg_leafval_heaviside_extra &be);

unsigned int toku_bnc_nbytesinbuf(NONLEAF_CHILDINFO bnc);
int toku_bnc_n_entries(NONLEAF_CHILDINFO bnc);
long toku_bnc_memory_size(NONLEAF_CHILDINFO bnc);
long toku_bnc_memory_used(NONLEAF_CHILDINFO bnc);
void toku_bnc_insert_msg(NONLEAF_CHILDINFO bnc, const void *key, uint32_t keylen, const void *data, uint32_t datalen, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, const toku::comparator &cmp);
void toku_bnc_empty(NONLEAF_CHILDINFO bnc);
void toku_bnc_flush_to_child(FT ft, NONLEAF_CHILDINFO bnc, FTNODE child, TXNID parent_oldest_referenced_xid_known);
bool toku_bnc_should_promote(FT ft, NONLEAF_CHILDINFO bnc) __attribute__((const, nonnull));

bool toku_ftnode_nonleaf_is_gorged(FTNODE node, uint32_t nodesize);
uint32_t toku_ftnode_leaf_num_entries(FTNODE node);
void toku_ftnode_leaf_rebalance(FTNODE node, unsigned int basementnodesize);

void toku_ftnode_leaf_run_gc(FT ft, FTNODE node);

enum reactivity {
    RE_STABLE,
    RE_FUSIBLE,
    RE_FISSIBLE
};

enum reactivity toku_ftnode_get_reactivity(FT ft, FTNODE node);
enum reactivity toku_ftnode_get_nonleaf_reactivity(FTNODE node, unsigned int fanout);
enum reactivity toku_ftnode_get_leaf_reactivity(FTNODE node, uint32_t nodesize);

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
int toku_ftnode_hot_next_child(FTNODE node, const DBT *k, const toku::comparator &cmp);

void toku_ftnode_put_msg(const toku::comparator &cmp, ft_update_func update_fun,
                         FTNODE node, int target_childnum,
                         const ft_msg &msg, bool is_fresh, txn_gc_info *gc_info,
                         size_t flow_deltas[], STAT64INFO stats_to_update);

void toku_ft_bn_apply_msg_once(BASEMENTNODE bn, const ft_msg &msg, uint32_t idx,
                               uint32_t le_keylen, LEAFENTRY le, txn_gc_info *gc_info,
                               uint64_t *workdonep, STAT64INFO stats_to_update);

void toku_ft_bn_apply_msg(const toku::comparator &cmp, ft_update_func update_fun,
                          BASEMENTNODE bn, const ft_msg &msg, txn_gc_info *gc_info,
                          uint64_t *workdone, STAT64INFO stats_to_update);

void toku_ft_leaf_apply_msg(const toku::comparator &cmp, ft_update_func update_fun,
                            FTNODE node, int target_childnum,
                            const ft_msg &msg, txn_gc_info *gc_info,
                            uint64_t *workdone, STAT64INFO stats_to_update);

//
// Message management for orthopush
//

struct ancestors {
    // This is the root node if next is NULL (since the root has no ancestors)
    FTNODE node;
    // Which buffer holds messages destined to the node whose ancestors this list represents.
    int childnum;
    struct ancestors *next;
};
typedef struct ancestors *ANCESTORS;

void toku_ft_bnc_move_messages_to_stale(FT ft, NONLEAF_CHILDINFO bnc);

void toku_move_ftnode_messages_to_stale(FT ft, FTNODE node);

// TODO: Should ft_handle just be FT?
class pivot_bounds;
void toku_apply_ancestors_messages_to_node(FT_HANDLE t, FTNODE node, ANCESTORS ancestors,
                                           const pivot_bounds &bounds,
                                           bool *msgs_applied, int child_to_read);

bool toku_ft_leaf_needs_ancestors_messages(FT ft, FTNODE node, ANCESTORS ancestors,
                                           const pivot_bounds &bounds,
                                           MSN *const max_msn_in_path, int child_to_read);

void toku_ft_bn_update_max_msn(FTNODE node, MSN max_msn_applied, int child_to_read);

struct ft_search;
int toku_ft_search_which_child(const toku::comparator &cmp, FTNODE node, ft_search *search);

//
// internal node inline functions
// TODO: Turn the macros into real functions
//

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

static inline struct sub_block *BSB(FTNODE node, int i) {
    paranoid_invariant(i >= 0);
    paranoid_invariant(i < node->n_children);
    FTNODE_CHILD_POINTER p = node->bp[i].ptr;
    paranoid_invariant(p.tag==BCT_SUBBLOCK);
    return p.u.subblock;
}

static inline void set_BSB(FTNODE node, int i, struct sub_block *sb) {
    paranoid_invariant(i >= 0);
    paranoid_invariant(i < node->n_children);
    FTNODE_CHILD_POINTER *p = &node->bp[i].ptr;
    p->tag = BCT_SUBBLOCK;
    p->u.subblock = sb;
}

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

// ftnode leaf basementnode macros, 
#define BLB_MAX_MSN_APPLIED(node,i) (BLB(node,i)->max_msn_applied)
#define BLB_MAX_DSN_APPLIED(node,i) (BLB(node,i)->max_dsn_applied)
#define BLB_DATA(node,i) (&(BLB(node,i)->data_buffer))
#define BLB_NBYTESINDATA(node,i) (BLB_DATA(node,i)->get_disk_size())
#define BLB_SEQINSERT(node,i) (BLB(node,i)->seqinsert)
