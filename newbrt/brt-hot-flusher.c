/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <brt-flusher.h>
#include <brt-flusher-internal.h>
#include <brt-cachetable-wrappers.h>
#include <brt-internal.h>

// Member Descirption:
// 1. highest_pivot_key - this is the key that corresponds to the 
// most recently flushed leaf entry.
// 2. max_current_key - this is the pivot/key that we inherit as
// we descend down the tree.  We use this to set the highest_pivot_key.
// 3. sub_tree_size - this is the percentage of the entire tree that our
// current position (in a sub-tree) encompasses.
// 4. percentage_done - this is the percentage of leaf nodes that have
// been flushed into.
// 5. rightmost_leaf_seen - this is a boolean we use to determine if
// if we have flushed to every leaf node.
struct hot_flusher_extra {
    DBT highest_pivot_key;
    DBT max_current_key;
    float sub_tree_size;
    float percentage_done;
    bool rightmost_leaf_seen;
};

static BRT_HOT_STATUS_S hot_status;

void 
toku_brt_hot_get_status(BRT_HOT_STATUS s) {
    *s = hot_status;
}

// Copies the max current key to the highest pivot key seen.
static void
hot_set_highest_key(struct hot_flusher_extra *flusher)
{
    // The max current key will be NULL if we are traversing in the
    // rightmost subtree of a given parent.  As such, we don't want to
    // allocate memory for this case.
    if (flusher->max_current_key.data == NULL) {
        if (flusher->highest_pivot_key.data) {
            toku_free(flusher->highest_pivot_key.data);
        }
        flusher->highest_pivot_key.data = NULL;
    } else {
        // Otherwise, let's copy all the contents from one key to the other.
        void *source = flusher->max_current_key.data;
        void *destination = flusher->highest_pivot_key.data;
        u_int32_t size = flusher->max_current_key.size;

        destination = toku_xrealloc(destination, size);
        memcpy(destination, source, size);

        // Finish copying all fields from the max current key.
        // Add free here.
        toku_fill_dbt(&(flusher->highest_pivot_key), destination, size);
    }
}

// Copies the pivot key in the parent to the given DBT key, using the
// pivot corresponding to the given child.
static void
hot_set_key(DBT *key, BRTNODE parent, int childnum)
{
    // assert that childnum is less than number of children - 1.
    DBT pivot;
    struct kv_pair *pair;
    pair = parent->childkeys[childnum];
    pivot = kv_pair_key_to_dbt(pair);

    void *data = key->data;
    u_int32_t size = pivot.size;

    data = toku_xrealloc(data, size);
    memcpy(data, pivot.data, size);

    toku_fill_dbt(key, data, size);
}

static int
hot_just_pick_child(struct brt_header *h,
                    BRTNODE parent,
                    struct hot_flusher_extra *flusher)
{
    int childnum = 0;

    // Search through Parents pivots, see which one is greater than
    // the highest_pivot_key seen so far.
    if (flusher->highest_pivot_key.data == NULL)
    {
        // Special case of the first child of the root node.
        // Also known as, NEGATIVE INFINITY....
        childnum = 0;
    } else {
        // Find the pivot boundary.
        childnum = toku_brtnode_hot_next_child(parent,
                                               &flusher->highest_pivot_key,
                                               &h->descriptor,
                                               h->compare_fun);
    }

    return childnum;
}

static void
hot_update_flusher_keys(BRTNODE parent,
                        int childnum,
                        struct hot_flusher_extra *flusher)
{
    // Update maximum current key if the child is NOT the rightmost
    // child node.
    if (childnum < (parent->n_children - 1)) {
        hot_set_key(&flusher->max_current_key, parent, childnum);
    }
}

// Picks which child flush_some_child will use for flushing and
// recursion.
static int
hot_pick_child(struct brt_header *h,
               BRTNODE parent,
               void *extra)
{
    struct hot_flusher_extra *flusher = extra;
    int childnum = hot_just_pick_child(h, parent, flusher);

    // Now we determine the percentage of the tree flushed so far.

    // Whichever subtree we choose to recurse into, it is a fraction
    // of the current parent.
    flusher->sub_tree_size /= parent->n_children;

    // Update the precentage complete, using our new sub tree size AND
    // the number of children we have already flushed.
    flusher->percentage_done += (flusher->sub_tree_size * childnum);

    hot_update_flusher_keys(parent, childnum, flusher);

    return childnum;
}

// Does nothing for now.
static void
hot_update_status(BRTNODE UU(child),
                  int UU(dirtied),
                  void *UU(extra))
{
    return;
}

static int
hot_pick_child_after_split(struct brt_header *h,
                           BRTNODE parent,
                           int childnuma,
                           int childnumb,
                           void *extra)
{
    struct hot_flusher_extra *flusher = extra;
    int childnum = hot_just_pick_child(h, parent, flusher);
    assert(childnum == childnuma || childnum == childnumb);
    hot_update_flusher_keys(parent, childnum, flusher);
    if (parent->height == 1) {
        childnum = -1;
    }
    return childnum;
}

// Basic constructor/initializer for the hot flusher struct.
static void
hot_flusher_init(struct flusher_advice *advice,
                 struct hot_flusher_extra *flusher)
{
    // Initialize the highest pivot key seen to NULL.  This represents
    // NEGATIVE INFINITY and is used to cover the special case of our
    // first traversal of the tree.
    toku_init_dbt(&(flusher->highest_pivot_key));
    toku_init_dbt(&(flusher->max_current_key));
    flusher->rightmost_leaf_seen = 0;
    flusher->sub_tree_size = 1.0;
    flusher->percentage_done = 0.0;
    flusher_advice_init(advice,
                        hot_pick_child,
                        dont_destroy_basement_nodes,
                        always_recursively_flush,
                        default_merge_child,
                        hot_update_status,
                        hot_pick_child_after_split,
                        flusher
                        );
}

// Erases any DBT keys we have copied from a traversal.
static void
hot_flusher_destroy(struct hot_flusher_extra *flusher)
{
    if (flusher->highest_pivot_key.data) {
        toku_free(flusher->highest_pivot_key.data);
    }

    if (flusher->max_current_key.data) {
        toku_free(flusher->max_current_key.data);
    }
}

// Entry point for Hot Optimize Table (HOT).  Note, this function is
// not recursive.  It iterates over root-to-leaf paths.
int
toku_brt_hot_optimize(BRT brt,
                      int (*progress_callback)(void *extra, float progress),
                      void *progress_extra)
{
    int r = 0;
    struct hot_flusher_extra flusher;
    struct flusher_advice advice;

    hot_flusher_init(&advice, &flusher);

    uint64_t loop_count = 0;
    MSN msn_at_start_of_hot = ZERO_MSN;  // capture msn from root at
                                         // start of HOT operation
    (void) __sync_fetch_and_add(&hot_status.num_started, 1);

    // Higher level logic prevents a dictionary from being deleted or truncated 
    // during a hot optimize operation.  Doing so would violate the hot optimize contract.
    do {
        BRTNODE root;
        CACHEKEY *rootp;
        u_int32_t fullhash;

        // Grab YDB Lock.
        toku_cachetable_call_ydb_lock(brt->h->cf);

        // Get root node (the first parent of each successive HOT
        // call.)
        rootp = toku_calculate_root_offset_pointer(brt->h, &fullhash);
        struct brtnode_fetch_extra bfe;
        fill_bfe_for_full_read(&bfe, brt->h);
        toku_pin_brtnode_off_client_thread(brt->h,
                                           (BLOCKNUM) *rootp,
                                           fullhash,
                                           &bfe,
                                           0,
                                           NULL,
                                           &root);

        toku_assert_entire_node_in_memory(root);

        // Prepare HOT diagnostics.
        if (loop_count == 0) {
            // The first time through, capture msn from root and set
            // info in header while holding ydb lock.
            msn_at_start_of_hot = root->max_msn_applied_to_node_on_disk;
            toku_brt_header_note_hot_begin(brt);
        }

        loop_count++;

        if (loop_count > hot_status.max_root_flush_count) {
            // This is threadsafe, since we're holding the ydb lock.
            hot_status.max_root_flush_count = loop_count;
        }

        // Release YDB Lock.
        toku_cachetable_call_ydb_unlock(brt->h->cf);

        // Initialize the maximum current key.  We need to do this for
        // every traversal.
        if (flusher.max_current_key.data) {
            toku_free(flusher.max_current_key.data);
        }
        flusher.max_current_key.data = NULL;

        flusher.sub_tree_size = 1.0;
        flusher.percentage_done = 0.0;

        // This should recurse to the bottom of the tree and then
        // return.
        if (root->height > 0) {
            flush_some_child(brt->h, root, &advice);
        } else {
            // Since there are no children to flush, we should abort
            // the HOT call.
            flusher.rightmost_leaf_seen = 1;
            toku_unpin_brtnode_off_client_thread(brt->h, root);
        }

        // Set the highest pivot key seen here, since the parent may
        // be unlocked and NULL'd later in our caller:
        // flush_some_child().
        hot_set_highest_key(&flusher);

        // This is where we determine if the traversal is finished or
        // not.
        if (flusher.max_current_key.data == NULL) {
            flusher.rightmost_leaf_seen = 1;
        }

        // Update HOT's progress.
        if (progress_callback != NULL) {
            r = progress_callback(progress_extra, flusher.percentage_done);

            // Check if the callback wants us to stop running HOT.
            if (r != 0) {
                flusher.rightmost_leaf_seen = 1;
            }
        }

        // Loop until the max key has been updated to positive
        // infinity.
    } while (!flusher.rightmost_leaf_seen);

    // Cleanup.
    hot_flusher_destroy(&flusher);

    // More diagnostics.
    {
        BOOL success = false;
        if (r == 0) success = true;
        toku_cachetable_call_ydb_lock(brt->h->cf);
        toku_brt_header_note_hot_complete(brt, success, msn_at_start_of_hot);
        toku_cachetable_call_ydb_unlock(brt->h->cf);
        if (success)
            (void) __sync_fetch_and_add(&hot_status.num_completed, 1);
        else
            (void) __sync_fetch_and_add(&hot_status.num_aborted, 1);
    }
    return r;
}
