/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <memory.h>
#include <toku_include/toku_portability.h>

#include "rollback_log_node_cache.h"

void rollback_log_node_cache::init (uint32_t max_num_avail_nodes) {
    XMALLOC_N(max_num_avail_nodes, m_avail_blocknums);
    XMALLOC_N(max_num_avail_nodes, m_hashes);
    m_max_num_avail = max_num_avail_nodes;
    m_first = 0;
    m_num_avail = 0;
    toku_pthread_mutexattr_t attr;
    toku_mutexattr_init(&attr);
    toku_mutexattr_settype(&attr, TOKU_MUTEX_ADAPTIVE);
    toku_mutex_init(&m_mutex, &attr);
    toku_mutexattr_destroy(&attr);
}

void rollback_log_node_cache::destroy() {
    toku_mutex_destroy(&m_mutex);
    toku_free(m_avail_blocknums);
    toku_free(m_hashes);
}

// returns true if rollback log node was successfully added,
// false otherwise
bool rollback_log_node_cache::give_rollback_log_node(TOKUTXN txn, ROLLBACK_LOG_NODE log){
    bool retval = false;
    toku_mutex_lock(&m_mutex);
    if (m_num_avail < m_max_num_avail) {
        retval = true;
        uint32_t index = m_first + m_num_avail;
        if (index >= m_max_num_avail) {
            index -= m_max_num_avail;
        }
        m_avail_blocknums[index].b = log->blocknum.b;
        m_hashes[index] = log->hash;
        m_num_avail++;
    }
    toku_mutex_unlock(&m_mutex);
    //
    // now unpin the rollback log node
    //
    if (retval) {
        make_rollback_log_empty(log);
        toku_rollback_log_unpin(txn, log);
    }
    return retval;
}

// if a rollback log node is available, will set log to it,
// otherwise, will set log to NULL and caller is on his own
// for getting a rollback log node
void rollback_log_node_cache::get_rollback_log_node(TOKUTXN txn, ROLLBACK_LOG_NODE* log){
    BLOCKNUM b = ROLLBACK_NONE;
    uint32_t hash;
    toku_mutex_lock(&m_mutex);
    if (m_num_avail > 0) {
        b.b = m_avail_blocknums[m_first].b;
        hash = m_hashes[m_first];
        m_num_avail--;
        if (++m_first >= m_max_num_avail) {
            m_first = 0;
        }
    }
    toku_mutex_unlock(&m_mutex);
    if (b.b != ROLLBACK_NONE.b) {
        toku_get_and_pin_rollback_log(txn, b, hash, log);
        invariant(rollback_log_is_unused(*log));
    } else {
        *log = NULL;
    }
}

