/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef BRT_FLUSHER_INTERNAL
#define BRT_FLUSHER_INTERNAL
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <brttypes.h>
#include <c_dialects.h>

C_BEGIN

typedef struct flusher_advice FLUSHER_ADVICE;

/**
 * Choose a child to flush to.  Returns a childnum, or -1 if we should
 * go no further.
 *
 * Flusher threads: pick the heaviest child buffer
 * Cleaner threads: pick the heaviest child buffer
 * Cleaner thread merging leaf nodes: follow down to a key
 * Hot optimize table: follow down to the right of a key
 */
typedef int (*FA_PICK_CHILD)(struct brt_header *h, BRTNODE parent, void* extra);

/**
 * Decide whether to call `flush_some_child` on the child if it is
 * stable and a nonleaf node.
 *
 * Flusher threads: yes if child is gorged
 * Cleaner threads: yes if child is gorged
 * Cleaner thread merging leaf nodes: always yes
 * Hot optimize table: always yes
 */
typedef bool (*FA_SHOULD_RECURSIVELY_FLUSH)(BRTNODE child, void* extra);

/**
 * Called if the child needs merging.  Should do something to get the
 * child out of a fusible state.  Must unpin parent and child.
 *
 * Flusher threads: just do the merge
 * Cleaner threads: if nonleaf, just merge, otherwise start a "cleaner
 *                  thread merge"
 * Cleaner thread merging leaf nodes: just do the merge
 * Hot optimize table: just do the merge
 */
typedef void (*FA_MAYBE_MERGE_CHILD)(struct flusher_advice *fa,
                              struct brt_header *h,
                              BRTNODE parent,
                              int childnum,
                              BRTNODE child,
                              void* extra);

/**
 * Cleaner threads may need to destroy basement nodes which have been
 * brought more up to date than the height 1 node flushing to them.
 * This function is used to determine if we need to check for basement
 * nodes that are too up to date, and then destroy them if we find
 * them.
 *
 * Flusher threads: no
 * Cleaner threads: yes
 * Cleaner thread merging leaf nodes: no
 * Hot optimize table: no
 */
typedef bool (*FA_SHOULD_DESTROY_BN)(void* extra);

/**
 * Update `brt_flusher_status` in whatever way necessary.  Called once
 * by `flush_some_child` right before choosing what to do next (split,
 * merge, recurse), with the number of nodes that were dirtied by this
 * execution of `flush_some_child`.
 */
typedef void (*FA_UPDATE_STATUS)(BRTNODE child, int dirtied, void* extra);

/**
 * Choose whether to go to the left or right child after a split.  Called
 * by `brt_split_child`.  If -1 is returned, `brt_split_child` defaults to
 * the old behavior.
 */
typedef int (*FA_PICK_CHILD_AFTER_SPLIT)(struct brt_header* h,
                                         BRTNODE node,
                                         int childnuma,
                                         int childnumb,
                                         void* extra);

/**
 * A collection of callbacks used by the flushing machinery to make
 * various decisions.  There are implementations of each of these
 * functions for flusher threads (ft_*), cleaner threads (ct_*), , and hot
 * optimize table (hot_*).
 */
struct flusher_advice {
    FA_PICK_CHILD pick_child;
    FA_SHOULD_RECURSIVELY_FLUSH should_recursively_flush;
    FA_MAYBE_MERGE_CHILD maybe_merge_child;
    FA_SHOULD_DESTROY_BN should_destroy_basement_nodes;
    FA_UPDATE_STATUS update_status;
    FA_PICK_CHILD_AFTER_SPLIT pick_child_after_split;
    void* extra; // parameter passed into callbacks
};


void
flusher_advice_init(
    struct flusher_advice *fa,
    FA_PICK_CHILD pick_child,
    FA_SHOULD_DESTROY_BN should_destroy_basement_nodes,
    FA_SHOULD_RECURSIVELY_FLUSH should_recursively_flush,
    FA_MAYBE_MERGE_CHILD maybe_merge_child,
    FA_UPDATE_STATUS update_status,
    FA_PICK_CHILD_AFTER_SPLIT pick_child_after_split,
    void* extra
    );

void
flush_some_child(
    struct brt_header* h,
    BRTNODE parent,
    struct flusher_advice *fa);

bool
always_recursively_flush(BRTNODE child, void* extra);

bool
dont_destroy_basement_nodes(void* extra);

void
default_merge_child(struct flusher_advice *fa,
                    struct brt_header *h,
                    BRTNODE parent,
                    int childnum,
                    BRTNODE child,
                    void* extra);

int
default_pick_child_after_split(struct brt_header *h,
                               BRTNODE parent,
                               int childnuma,
                               int childnumb,
                               void *extra);

C_END

#endif // End of header guardian.
