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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_assert.h>

void concurrent_tree::create(const comparator *cmp) {
    // start with an empty root node. we do this instead of
    // setting m_root to null so there's always a root to lock
    m_root.create_root(cmp);
}

void concurrent_tree::destroy(void) {
    m_root.destroy_root();
}

bool concurrent_tree::is_empty(void) {
    return m_root.is_empty();
}

uint64_t concurrent_tree::get_insertion_memory_overhead(void) {
    return sizeof(treenode);
}

void concurrent_tree::locked_keyrange::prepare(concurrent_tree *tree) {
    // the first step in acquiring a locked keyrange is locking the root
    treenode *const root = &tree->m_root;
    m_tree = tree;
    m_subtree = root;
    m_range = keyrange::get_infinite_range();
    root->mutex_lock();
}

void concurrent_tree::locked_keyrange::acquire(const keyrange &range) {
    treenode *const root = &m_tree->m_root;

    treenode *subtree;
    if (root->is_empty() || root->range_overlaps(range)) {
        subtree = root;
    } else {
        // we do not have a precomputed comparison hint, so pass null
        const keyrange::comparison *cmp_hint = nullptr;
        subtree = root->find_node_with_overlapping_child(range, cmp_hint);
    }

    // subtree is locked. it will be unlocked when this is release()'d
    invariant_notnull(subtree);
    m_range = range;
    m_subtree = subtree;
}

void concurrent_tree::locked_keyrange::release(void) {
    m_subtree->mutex_unlock();
}

template <class F>
void concurrent_tree::locked_keyrange::iterate(F *function) const {
    // if the subtree is non-empty, traverse it by calling the given
    // function on each range, txnid pair found that overlaps.
    if (!m_subtree->is_empty()) {
        m_subtree->traverse_overlaps(m_range, function);
    }
}

void concurrent_tree::locked_keyrange::insert(const keyrange &range, TXNID txnid) {
    // empty means no children, and only the root should ever be empty
    if (m_subtree->is_empty()) {
        m_subtree->set_range_and_txnid(range, txnid);
    } else {
        m_subtree->insert(range, txnid);
    }
}

void concurrent_tree::locked_keyrange::remove(const keyrange &range) {
    invariant(!m_subtree->is_empty());
    treenode *new_subtree = m_subtree->remove(range);
    // if removing range changed the root of the subtree,
    // then the subtree must be the root of the entire tree.
    if (new_subtree == nullptr) {
        invariant(m_subtree->is_root());
        invariant(m_subtree->is_empty());
    }
}

void concurrent_tree::locked_keyrange::remove_all(void) {
    m_subtree->recursive_remove();
}
