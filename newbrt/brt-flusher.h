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
toku_brtnode_cleaner_callback_internal(
    void *brtnode_pv,
    BLOCKNUM blocknum,
    u_int32_t fullhash,
    void *extraargs,
    BRT_STATUS brt_status
    );

/**
 * Puts a workitem on the flusher thread queue, scheduling the node to be
 * flushed by flush_some_child.
 */
void
flush_node_on_background_thread(
    BRT brt,
    BRTNODE parent,
    BRT_STATUS brt_status
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

C_END

#endif // End of header guardian.
