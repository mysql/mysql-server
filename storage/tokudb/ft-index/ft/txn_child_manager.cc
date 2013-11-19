/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: rollback.cc 49033 2012-10-17 18:48:30Z zardosht $"
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

#include "log-internal.h"
#include "txn_child_manager.h"

//
// initialized a txn_child_manager,
// when called, root->txnid.parent_id64 may not yet be set
//
void txn_child_manager::init(TOKUTXN root) {
    invariant(root->txnid.child_id64 == TXNID_NONE);
    invariant(root->parent == NULL);
    m_root = root;
    m_last_xid = TXNID_NONE;
    ZERO_STRUCT(m_mutex);

    toku_pthread_mutexattr_t attr;
    toku_mutexattr_init(&attr);
    toku_mutexattr_settype(&attr, TOKU_MUTEX_ADAPTIVE);
    toku_mutex_init(&m_mutex, &attr);
    toku_mutexattr_destroy(&attr);
}

void txn_child_manager::destroy() {
    toku_mutex_destroy(&m_mutex);
}

void txn_child_manager::start_child_txn_for_recovery(TOKUTXN child, TOKUTXN parent, TXNID_PAIR txnid) {
    invariant(parent->txnid.parent_id64 == m_root->txnid.parent_id64);
    invariant(txnid.parent_id64 == m_root->txnid.parent_id64);

    child->txnid = txnid;
    toku_mutex_lock(&m_mutex);
    if (txnid.child_id64 > m_last_xid) {
        m_last_xid = txnid.child_id64;
    }
    parent->child = child;
    toku_mutex_unlock(&m_mutex);
}

void txn_child_manager::start_child_txn(TOKUTXN child, TOKUTXN parent) {
    invariant(parent->txnid.parent_id64 == m_root->txnid.parent_id64);
    child->txnid.parent_id64 = m_root->txnid.parent_id64;
    toku_mutex_lock(&m_mutex);
    
    ++m_last_xid;
    // Here we ensure that the child_id64 is never equal to the parent_id64
    // We do this to make this feature work more easily with the XIDs
    // struct and message application. The XIDs struct stores the parent id
    // as the first TXNID, and subsequent TXNIDs store child ids. So, if we
    // have a case where the parent id is the same as the child id, we will
    // have to do some tricky maneuvering in the message application code
    // in ule.cc. So, to lessen the probability of bugs, we ensure that the
    // parent id is not the same as the child id.
    if (m_last_xid == m_root->txnid.parent_id64) {
        ++m_last_xid;
    }
    child->txnid.child_id64 = m_last_xid;

    parent->child = child;
    toku_mutex_unlock(&m_mutex);
}

void txn_child_manager::finish_child_txn(TOKUTXN child) {
    invariant(child->txnid.parent_id64 == m_root->txnid.parent_id64);
    toku_mutex_lock(&m_mutex);
    child->parent->child = NULL;
    toku_mutex_unlock(&m_mutex);
}

void txn_child_manager::suspend() {
    toku_mutex_lock(&m_mutex);
}

void txn_child_manager::resume() {
    toku_mutex_unlock(&m_mutex);
}

void txn_child_manager::find_tokutxn_by_xid_unlocked(TXNID_PAIR xid, TOKUTXN* result) {
    invariant(xid.parent_id64 == m_root->txnid.parent_id64);
    TOKUTXN curr_txn = m_root;
    while (curr_txn != NULL) {
        if (xid.child_id64 == curr_txn->txnid.child_id64) {
            *result = curr_txn;
            break;
        }
        curr_txn = curr_txn->child;
    }
}

int txn_child_manager::iterate(txn_mgr_iter_callback cb, void* extra) { 
    TOKUTXN curr_txn = m_root; 
    int ret = 0; 
    toku_mutex_lock(&m_mutex); 
    while (curr_txn != NULL) { 
        ret = cb(curr_txn, extra); 
        if (ret != 0) { 
            break; 
        } 
        curr_txn = curr_txn->child; 
    } 
    toku_mutex_unlock(&m_mutex); 
    return ret; 
} 

