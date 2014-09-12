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

#include <memory.h>
#include <toku_include/toku_portability.h>

#include "rollback_log_node_cache.h"

void rollback_log_node_cache::init (uint32_t max_num_avail_nodes) {
    XMALLOC_N(max_num_avail_nodes, m_avail_blocknums);
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
    toku_mutex_lock(&m_mutex);
    if (m_num_avail > 0) {
        b.b = m_avail_blocknums[m_first].b;
        m_num_avail--;
        if (++m_first >= m_max_num_avail) {
            m_first = 0;
        }
    }
    toku_mutex_unlock(&m_mutex);
    if (b.b != ROLLBACK_NONE.b) {
        toku_get_and_pin_rollback_log(txn, b, log);
        invariant(rollback_log_is_unused(*log));
    } else {
        *log = NULL;
    }
}

