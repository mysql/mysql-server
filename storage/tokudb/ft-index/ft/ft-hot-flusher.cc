/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <ft-flusher.h>
#include <ft-flusher-internal.h>
#include <ft-cachetable-wrappers.h>
#include <ft-internal.h>
#include <ft.h>
#include <portability/toku_atomic.h>
#include <util/status.h>

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

static FT_HOT_STATUS_S hot_status;

#define STATUS_INIT(k,c,t,l,inc) TOKUDB_STATUS_INIT(hot_status, k, c, t, "hot: " l, inc)

#define STATUS_VALUE(x) hot_status.status[x].value.num

void
toku_ft_hot_status_init(void)
{
    STATUS_INIT(FT_HOT_NUM_STARTED,          nullptr, UINT64, "operations ever started", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_HOT_NUM_COMPLETED,        nullptr, UINT64, "operations successfully completed", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_HOT_NUM_ABORTED,          nullptr, UINT64, "operations aborted", TOKU_ENGINE_STATUS);
    STATUS_INIT(FT_HOT_MAX_ROOT_FLUSH_COUNT, nullptr, UINT64, "max number of flushes from root ever required to optimize a tree", TOKU_ENGINE_STATUS);

    hot_status.initialized = true;
}
#undef STATUS_INIT

void
toku_ft_hot_get_status(FT_HOT_STATUS s) {
    if (!hot_status.initialized) {
        toku_ft_hot_status_init();
    }
    *s = hot_status;
}

// Copies the max current key to the highest pivot key seen.
static void
hot_set_highest_key(struct hot_flusher_extra *flusher)
{
    // The max current key will be NULL if we are traversing in the
    // rightmost subtree of a given parent.  As such, we don't want to
    // allocate memory for this case.
    toku_destroy_dbt(&flusher->highest_pivot_key);
    if (flusher->max_current_key.data != NULL) {
        // Otherwise, let's copy all the contents from one key to the other.
        toku_clone_dbt(&flusher->highest_pivot_key, flusher->max_current_key);
    }
}

static void
hot_set_start_key(struct hot_flusher_extra *flusher, const DBT* start)
{
    toku_destroy_dbt(&flusher->highest_pivot_key);
    if (start != NULL) {
        // Otherwise, let's copy all the contents from one key to the other.
        toku_clone_dbt(&flusher->highest_pivot_key, *start);
    }
}

static int
hot_just_pick_child(FT h,
                    FTNODE parent,
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
        childnum = toku_ftnode_hot_next_child(parent,
                                               &flusher->highest_pivot_key,
                                               &h->cmp_descriptor,
                                               h->compare_fun);
    }

    return childnum;
}

static void
hot_update_flusher_keys(FTNODE parent,
                        int childnum,
                        struct hot_flusher_extra *flusher)
{
    // Update maximum current key if the child is NOT the rightmost
    // child node.
    if (childnum < (parent->n_children - 1)) {
        toku_destroy_dbt(&flusher->max_current_key);
        toku_clone_dbt(&flusher->max_current_key, parent->childkeys[childnum]);
    }
}

// Picks which child toku_ft_flush_some_child will use for flushing and
// recursion.
static int
hot_pick_child(FT h,
               FTNODE parent,
               void *extra)
{
    struct hot_flusher_extra *flusher = (struct hot_flusher_extra *) extra;
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
hot_update_status(FTNODE UU(child),
                  int UU(dirtied),
                  void *UU(extra))
{
    return;
}

// If we've just split a node, HOT needs another chance to decide which
// one to flush into.  This gives it a chance to do that, and update the
// keys it maintains.
static int
hot_pick_child_after_split(FT h,
                           FTNODE parent,
                           int childnuma,
                           int childnumb,
                           void *extra)
{
    struct hot_flusher_extra *flusher = (struct hot_flusher_extra *) extra;
    int childnum = hot_just_pick_child(h, parent, flusher);
    assert(childnum == childnuma || childnum == childnumb);
    hot_update_flusher_keys(parent, childnum, flusher);
    if (parent->height == 1) {
        // We don't want to recurse into a leaf node, but if we return
        // anything valid, ft_split_child will try to go there, so we
        // return -1 to allow ft_split_child to have its default
        // behavior, which will be to stop recursing.
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
    toku_destroy_dbt(&flusher->highest_pivot_key);
    toku_destroy_dbt(&flusher->max_current_key);
}

// Entry point for Hot Optimize Table (HOT).  Note, this function is
// not recursive.  It iterates over root-to-leaf paths.
int
toku_ft_hot_optimize(FT_HANDLE brt, DBT* left, DBT* right,
                      int (*progress_callback)(void *extra, float progress),
                      void *progress_extra)
{
    int r = 0;
    struct hot_flusher_extra flusher;
    struct flusher_advice advice;

    hot_flusher_init(&advice, &flusher);
    hot_set_start_key(&flusher, left);

    uint64_t loop_count = 0;
    MSN msn_at_start_of_hot = ZERO_MSN;  // capture msn from root at
                                         // start of HOT operation
    (void) toku_sync_fetch_and_add(&STATUS_VALUE(FT_HOT_NUM_STARTED), 1);

    toku_ft_note_hot_begin(brt);

    // Higher level logic prevents a dictionary from being deleted or
    // truncated during a hot optimize operation.  Doing so would violate
    // the hot optimize contract.
    do {
        FTNODE root;
        CACHEKEY root_key;
        uint32_t fullhash;

        {
            // Get root node (the first parent of each successive HOT
            // call.)
            toku_calculate_root_offset_pointer(brt->ft, &root_key, &fullhash);
            struct ftnode_fetch_extra bfe;
            fill_bfe_for_full_read(&bfe, brt->ft);
            toku_pin_ftnode_off_client_thread(brt->ft,
                                               (BLOCKNUM) root_key,
                                               fullhash,
                                               &bfe,
                                               PL_WRITE_EXPENSIVE, 
                                               0,
                                               NULL,
                                               &root);
            toku_assert_entire_node_in_memory(root);
        }

        // Prepare HOT diagnostics.
        if (loop_count == 0) {
            // The first time through, capture msn from root
            msn_at_start_of_hot = root->max_msn_applied_to_node_on_disk;
        }

        loop_count++;

        if (loop_count > STATUS_VALUE(FT_HOT_MAX_ROOT_FLUSH_COUNT)) {
            STATUS_VALUE(FT_HOT_MAX_ROOT_FLUSH_COUNT) = loop_count;
        }

        // Initialize the maximum current key.  We need to do this for
        // every traversal.
        toku_destroy_dbt(&flusher.max_current_key);

        flusher.sub_tree_size = 1.0;
        flusher.percentage_done = 0.0;

        // This should recurse to the bottom of the tree and then
        // return.
        if (root->height > 0) {
            toku_ft_flush_some_child(brt->ft, root, &advice);
        } else {
            // Since there are no children to flush, we should abort
            // the HOT call.
            flusher.rightmost_leaf_seen = 1;
            toku_unpin_ftnode_off_client_thread(brt->ft, root);
        }

        // Set the highest pivot key seen here, since the parent may
        // be unlocked and NULL'd later in our caller:
        // toku_ft_flush_some_child().
        hot_set_highest_key(&flusher);

        // This is where we determine if the traversal is finished or
        // not.
        if (flusher.max_current_key.data == NULL) {
            flusher.rightmost_leaf_seen = 1;
        }
        else if (right) {
            // if we have flushed past the bounds set for us,
            // set rightmost_leaf_seen so we exit
            FAKE_DB(db, &brt->ft->cmp_descriptor);
            int cmp = brt->ft->compare_fun(&db, &flusher.max_current_key, right);
            if (cmp > 0) {
                flusher.rightmost_leaf_seen = 1;
            }
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
        bool success = false;
        if (r == 0) { success = true; }

        {
            toku_ft_note_hot_complete(brt, success, msn_at_start_of_hot);
        }

        if (success) {
            (void) toku_sync_fetch_and_add(&STATUS_VALUE(FT_HOT_NUM_COMPLETED), 1);
        } else {
            (void) toku_sync_fetch_and_add(&STATUS_VALUE(FT_HOT_NUM_ABORTED), 1);
        }
    }
    return r;
}

#include <toku_race_tools.h>
void __attribute__((__constructor__)) toku_hot_helgrind_ignore(void);
void
toku_hot_helgrind_ignore(void) {
    // incremented only while lock is held, but read by engine status asynchronously.
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&hot_status, sizeof hot_status);
}


#undef STATUS_VALUE
