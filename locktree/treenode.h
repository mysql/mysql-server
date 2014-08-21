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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <string.h>

#include "portability/memory.h"
#include "portability/toku_pthread.h"

#include "ft/comparator.h"
#include "ft/txn/txn.h"
#include "locktree/keyrange.h"

namespace toku {

// a node in a tree with its own mutex
// - range is the "key" of this node
// - txnid is the single txnid associated with this node
// - left and right children may be null
//
// to build a tree on top of this abstraction, the user:
// - provides memory for a root node, initializes it via create_root()
// - performs tree operations on the root node. memory management
//   below the root node is handled by the abstraction, not the user.
// this pattern:
// - guaruntees a root node always exists.
// - does not allow for rebalances on the root node

class treenode {
public:

    // every treenode function has some common requirements:
    // - node is locked and children are never locked
    // - node may be unlocked if no other thread has visibility

    // effect: create the root node
    void create_root(const comparator *cmp);

    // effect: destroys the root node
    void destroy_root(void);

    // effect: sets the txnid and copies the given range for this node
    void set_range_and_txnid(const keyrange &range, TXNID txnid);

    // returns: true iff this node is marked as empty
    bool is_empty(void);

    // returns: true if this is the root node, denoted by a null parent
    bool is_root(void);

    // returns: true if the given range overlaps with this node's range
    bool range_overlaps(const keyrange &range);

    // effect: locks the node
    void mutex_lock(void);

    // effect: unlocks the node
    void mutex_unlock(void);

    // return: node whose child overlaps, or a child that is empty
    //         and would contain range if it existed
    // given: if cmp_hint is non-null, then it is a precomputed
    //        comparison of this node's range to the given range.
    treenode *find_node_with_overlapping_child(const keyrange &range,
            const keyrange::comparison *cmp_hint);

    // effect: performs an in-order traversal of the ranges that overlap the
    //         given range, calling function->fn() on each node that does
    // requires: function signature is: bool fn(const keyrange &range, TXNID txnid)
    // requires: fn returns true to keep iterating, false to stop iterating
    // requires: fn does not attempt to use any ranges read out by value
    //           after removing a node with an overlapping range from the tree.
    template <class F>
    void traverse_overlaps(const keyrange &range, F *function);

    // effect: inserts the given range and txnid into a subtree, recursively
    // requires: range does not overlap with any node below the subtree
    void insert(const keyrange &range, TXNID txnid);

    // effect: removes the given range from the subtree
    // requires: range exists in the subtree
    // returns: the root of the resulting subtree
    treenode *remove(const keyrange &range);

    // effect: removes this node and all of its children, recursively
    // requires: every node at and below this node is unlocked
    void recursive_remove(void);

private:

    // the child_ptr is a light abstraction for the locking of
    // a child and the maintenence of its depth estimate.

    struct child_ptr {
        // set the child pointer
        void set(treenode *node);

        // get and lock this child if it exists
        treenode *get_locked(void);

        treenode *ptr;
        uint32_t depth_est;
    };

    // the balance factor at which a node is considered imbalanced
    static const int32_t IMBALANCE_THRESHOLD = 2;

    // node-level mutex
    toku_mutex_t m_mutex;

    // the range and txnid for this node. the range contains a copy
    // of the keys originally inserted into the tree. nodes may
    // swap ranges. but at the end of the day, when a node is
    // destroyed, it frees the memory associated with whatever range
    // it has at the time of destruction.
    keyrange m_range;
    TXNID m_txnid;

    // two child pointers
    child_ptr m_left_child;
    child_ptr m_right_child;

    // comparator for ranges
    const comparator *m_cmp;

    // marked for the root node. the root node is never free()'d
    // when removed, but instead marked as empty.
    bool m_is_root;

    // marked for an empty node. only valid for the root.
    bool m_is_empty;

    // effect: initializes an empty node with the given comparator
    void init(const comparator *cmp);

    // requires: *parent is initialized to something meaningful.
    // requires: subtree is non-empty
    // returns: the leftmost child of the given subtree
    // returns: a pointer to the parent of said child in *parent, only
    //          if this function recurred, otherwise it is untouched.
    treenode *find_leftmost_child(treenode **parent);

    // requires: *parent is initialized to something meaningful.
    // requires: subtree is non-empty
    // returns: the rightmost child of the given subtree
    // returns: a pointer to the parent of said child in *parent, only
    //          if this function recurred, otherwise it is untouched.
    treenode *find_rightmost_child(treenode **parent);

    // effect: remove the root of this subtree, destroying the old root
    // returns: the new root of the subtree
    treenode *remove_root_of_subtree(void);

    // requires: subtree is non-empty, direction is not 0
    // returns: the child of the subtree at either the left or rightmost extreme
    treenode *find_child_at_extreme(int direction, treenode **parent);

    // effect: retrieves and possibly rebalances the left child
    // returns: a locked left child, if it exists
    treenode *lock_and_rebalance_left(void);

    // effect: retrieves and possibly rebalances the right child
    // returns: a locked right child, if it exists
    treenode *lock_and_rebalance_right(void);

    // returns: the estimated depth of this subtree
    uint32_t get_depth_estimate(void) const;

    // returns: true iff left subtree depth is sufficiently less than the right
    bool left_imbalanced(int threshold) const;

    // returns: true iff right subtree depth is sufficiently greater than the left
    bool right_imbalanced(int threshold) const;

    // effect: performs an O(1) rebalance, which will "heal" an imbalance by at most 1.
    // effect: if the new root is not this node, then this node is unlocked.
    // returns: locked node representing the new root of the rebalanced subtree
    treenode *maybe_rebalance(void);

    // returns: allocated treenode populated with a copy of the range and txnid
    static treenode *alloc(const comparator *cmp, const keyrange &range, TXNID txnid);

    // requires: node is a locked root node, or an unlocked non-root node
    static void free(treenode *node);

    // effect: swaps the range/txnid pairs for node1 and node2.
    static void swap_in_place(treenode *node1, treenode *node2);

    friend class concurrent_tree_unit_test;
};

// include the implementation here so we can use templated member functions
#include "treenode.cc"

} /* namespace toku */
