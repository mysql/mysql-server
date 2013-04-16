/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_assert.h>

void concurrent_tree::create(comparator *cmp) {
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
