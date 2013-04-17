/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRT_FLUSHER
#define BRT_FLUSHER
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// This must be first to make the 64-bit file mode work right in Linux
#include <brttypes.h>
#include <c_dialects.h>

C_BEGIN

typedef struct brt_flusher_status {
    uint64_t  cleaner_total_nodes;         // total number of nodes whose buffers are potentially flushed by cleaner thread
    uint64_t  cleaner_h1_nodes;            // number of nodes of height one whose message buffers are flushed by cleaner thread
    uint64_t  cleaner_hgt1_nodes;          // number of nodes of height > 1 whose message buffers are flushed by cleaner thread
    uint64_t  cleaner_empty_nodes;         // number of nodes that are selected by cleaner, but whose buffers are empty
    uint64_t  cleaner_nodes_dirtied;       // number of nodes that are made dirty by the cleaner thread
    uint64_t  cleaner_max_buffer_size;     // max number of bytes in message buffer flushed by cleaner thread
    uint64_t  cleaner_min_buffer_size;
    uint64_t  cleaner_total_buffer_size;
    uint64_t  cleaner_max_buffer_workdone; // max workdone value of any message buffer flushed by cleaner thread
    uint64_t  cleaner_min_buffer_workdone;
    uint64_t  cleaner_total_buffer_workdone;
    uint64_t  cleaner_num_leaf_merges_started;     // number of times cleaner thread tries to merge a leaf
    uint64_t  cleaner_num_leaf_merges_running;     // number of cleaner thread leaf merges in progress
    uint64_t  cleaner_num_leaf_merges_completed;   // number of times cleaner thread successfully merges a leaf
    uint64_t  cleaner_num_dirtied_for_leaf_merge;  // nodes dirtied by the "flush from root" process to merge a leaf node
    uint64_t  flush_total;                 // total number of flushes done by flusher threads or cleaner threads
    uint64_t  flush_in_memory;             // number of in memory flushes
    uint64_t  flush_needed_io;             // number of flushes that had to read a child (or part) off disk
    uint64_t  flush_cascades;              // number of flushes that triggered another flush in the child
    uint64_t  flush_cascades_1;            // number of flushes that triggered 1 cascading flush
    uint64_t  flush_cascades_2;            // number of flushes that triggered 2 cascading flushes
    uint64_t  flush_cascades_3;            // number of flushes that triggered 3 cascading flushes
    uint64_t  flush_cascades_4;            // number of flushes that triggered 4 cascading flushes
    uint64_t  flush_cascades_5;            // number of flushes that triggered 5 cascading flushes
    uint64_t  flush_cascades_gt_5;         // number of flushes that triggered more than 5 cascading flushes
    uint64_t  split_leaf;                  // number of leaf nodes split
    uint64_t  split_nonleaf;               // number of nonleaf nodes split
    uint64_t  merge_leaf;                  // number of times leaf nodes are merged
    uint64_t  merge_nonleaf;               // number of times nonleaf nodes are merged    
    uint64_t  balance_leaf;                // number of times a leaf node is balanced inside brt
} BRT_FLUSHER_STATUS_S, *BRT_FLUSHER_STATUS;

void toku_brt_flusher_status_init(void) __attribute__((__constructor__));
void toku_brt_flusher_get_status(BRT_FLUSHER_STATUS);

/**
 * Only for testing, not for production.
 *
 * Set a callback the flusher thread will use to signal various points
 * during its execution.
 */
void
toku_flusher_thread_set_callback(
    void (*callback_f)(int, void*),
    void* extra
    );

/**
 * Brings the node into memory and flushes the fullest buffer.  If the
 * heaviest child is empty, does nothing, otherwise, executes
 * flush_some_child to do the flush.
 *
 * Wrapped by toku_brtnode_cleaner_callback to provide access to
 * brt_status which currently just lives in brt.c.
 */
int
toku_brtnode_cleaner_callback(
    void *brtnode_pv,
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    void *extraargs
    );

/**
 * Puts a workitem on the flusher thread queue, scheduling the node to be
 * flushed by flush_some_child.
 */
void
flush_node_on_background_thread(
    BRT brt,
    BRTNODE parent
    );

/**
 * Effect: Split a leaf node.
 * Argument "node" is node to be split.
 * Upon return:
 *   nodea and nodeb point to new nodes that result from split of "node"
 *   nodea is the left node that results from the split
 *   splitk is the right-most key of nodea
 */
void
brtleaf_split(
    struct brt_header* h,
    BRTNODE node,
    BRTNODE *nodea,
    BRTNODE *nodeb,
    DBT *splitk,
    BOOL create_new_node,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes
    );

/**
 * Effect: node must be a node-leaf node.  It is split into two nodes, and
 *         the fanout is split between them.
 *    Sets splitk->data pointer to a malloc'd value
 *    Sets nodea, and nodeb to the two new nodes.
 *    The caller must replace the old node with the two new nodes.
 *    This function will definitely reduce the number of children for the node,
 *    but it does not guarantee that the resulting nodes are smaller than nodesize.
 */
void
brt_nonleaf_split(
    struct brt_header* h,
    BRTNODE node,
    BRTNODE *nodea,
    BRTNODE *nodeb,
    DBT *splitk,
    u_int32_t num_dependent_nodes,
    BRTNODE* dependent_nodes
    );



/************************************************************************
 * HOT optimize, should perhaps be factored out to its own header file  *
 ************************************************************************
 */

typedef struct brt_hot_status {
    uint64_t  num_started;          // number of HOT operations that have begun
    uint64_t  num_completed;        // number of HOT operations that have successfully completed
    uint64_t  num_aborted;          // number of HOT operations that have been aborted
    uint64_t  max_root_flush_count; // max number of flushes from root ever required to optimize a tree
} BRT_HOT_STATUS_S, *BRT_HOT_STATUS;

void toku_brt_hot_status_init(void) __attribute__((__constructor__));
void toku_brt_hot_get_status(BRT_HOT_STATUS);

/**
 * Takes given BRT and pushes all pending messages to the leaf nodes.
 */
int
toku_brt_hot_optimize(BRT brt,
                      int (*progress_callback)(void *extra, float progress),
                      void *progress_extra);

C_END

#endif // End of header guardian.
