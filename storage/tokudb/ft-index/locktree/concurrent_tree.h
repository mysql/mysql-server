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

#ifndef CONCURRENT_TREE_H
#define CONCURRENT_TREE_H

#include <ft/comparator.h>

#include "treenode.h"
#include "keyrange.h"

namespace toku {

// A concurrent_tree stores non-overlapping ranges.
// Access to disjoint parts of the tree usually occurs concurrently.

class concurrent_tree {
public:

    // A locked_keyrange gives you exclusive access to read and write
    // operations that occur on any keys in that range. You only have
    // the right to operate on keys in that range or keys that were read
    // from the keyrange using iterate()
    //
    // Access model:
    // - user prepares a locked keyrange. all threads serialize behind prepare().
    // - user breaks the serialzation point by acquiring a range, or releasing.
    // - one thread operates on a certain locked_keyrange object at a time.
    // - when the thread is finished, it releases

    class locked_keyrange {
    public:
        // effect: prepare to acquire a locked keyrange over the given
        //         concurrent_tree, preventing other threads from preparing
        //         until this thread either does acquire() or release().
        // note: operations performed on a prepared keyrange are equivalent
        //         to ones performed on an acquired keyrange over -inf, +inf.
        // rationale: this provides the user with a serialization point for descending 
        //            or modifying the the tree. it also proives a convenient way of
        //            doing serializable operations on the tree.
        // There are two valid sequences of calls:
        //  - prepare, acquire, [operations], release
        //  - prepare, [operations],release
        void prepare(concurrent_tree *tree);

        // requires: the locked keyrange was prepare()'d
        // effect: acquire a locked keyrange over the given concurrent_tree.
        //         the locked keyrange represents the range of keys overlapped
        //         by the given range
        void acquire(const keyrange &range);

        // effect: releases a locked keyrange and the mutex it holds
        void release(void);

        // effect: iterate over each range this locked_keyrange represents,
        //         calling function->fn() on each node's keyrange and txnid
        //         until there are no more or the function returns false
        template <class F>
        void iterate(F *function) const;

        // inserts the given range into the tree, with an associated txnid.
        // requires: range does not overlap with anything in this locked_keyrange
        // rationale: caller is responsible for only inserting unique ranges
        void insert(const keyrange &range, TXNID txnid);

        // effect: removes the given range from the tree
        // requires: range exists exactly in this locked_keyrange
        // rationale: caller is responsible for only removing existing ranges
        void remove(const keyrange &range);

        // effect: removes all of the keys represented by this locked keyrange
        // rationale: we'd like a fast way to empty out a tree
        void remove_all(void);

    private:
        // the concurrent tree this locked keyrange is for
        concurrent_tree *m_tree;

        // the range of keys this locked keyrange represents
        keyrange m_range;

        // the subtree under which all overlapping ranges exist
        treenode *m_subtree;

        friend class concurrent_tree_unit_test;
    };

    // effect: initialize the tree to an empty state
    void create(comparator *cmp);

    // effect: destroy the tree.
    // requires: tree is empty
    void destroy(void);

    // returns: true iff the tree is empty
    bool is_empty(void);

    // returns: the memory overhead of a single insertion into the tree
    static uint64_t get_insertion_memory_overhead(void);

private:
    // the root needs to always exist so there's a lock to grab
    // even if the tree is empty. that's why we store a treenode
    // here and not a pointer to one.
    treenode m_root;

    friend class concurrent_tree_unit_test;
};

// include the implementation here so we can use templated member
// functions in locked_keyrange, which are expanded and defined in the
// compilation unit that includes "concurrent_tree.h". if we didn't
// include the source here, then there would be problems with multiple
// definitions of the tree functions
#include "concurrent_tree.cc"

} /* namespace toku */

#endif /* CONCURRENT_TREE_H */
