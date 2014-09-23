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
  Copyright (C) 2007-2014 Tokutek, Inc.

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

#ident "Copyright (c) 2007-2014 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <portability/toku_portability.h>

#include <db.h>

#include <util/status.h>

enum context_id {
    CTX_INVALID = -1,
    CTX_DEFAULT = 0,          // default context for when no context is set
    CTX_SEARCH,               // searching for a key at the bottom of the tree
    CTX_PROMO,                // promoting a message down the tree
    CTX_FULL_FETCH,           // performing full fetch (pivots + some partial fetch)
    CTX_PARTIAL_FETCH,        // performing partial fetch
    CTX_FULL_EVICTION,        // running partial eviction
    CTX_PARTIAL_EVICTION,     // running partial eviction
    CTX_MESSAGE_INJECTION,    // injecting a message into a buffer
    CTX_MESSAGE_APPLICATION,  // applying ancestor's messages to a basement node
    CTX_FLUSH,                // flushing a buffer
    CTX_CLEANER               // doing work as the cleaner thread
};

// Note a contention event in engine status
void toku_context_note_frwlock_contention(const context_id blocking, const context_id blocked);

namespace toku {

    // class for tracking what a thread is doing
    //
    // usage:
    //
    // // automatically tag and document what you're doing
    // void my_interesting_function(void) {
    //     toku::context ctx("doing something interesting", INTERESTING_FN_1);
    //     ...
    //     {
    //         toku::context inner_ctx("doing something expensive", EXPENSIVE_FN_1);
    //         my_rwlock.wrlock();
    //         expensive();
    //         my_rwlock.wrunlock();
    //     }
    //     ...
    // }
    //
    // // ... so later you can write code like this.
    // // here, we save some info to help determine why a lock could not be acquired
    // void my_rwlock::wrlock() {
    //     r = try_acquire_write_lock();
    //     if (r == 0) {
    //         m_write_locked_context_id = get_thread_local_context()->get_id();
    //         ...
    //     } else {
    //         if (m_write_locked_context_id == EXPENSIVE_FN_1) {
    //             status.blocked_because_of_expensive_fn_1++;
    //         } else if (...) {
    //            ...
    //         }
    //         ...
    //     }
    // }
    class context {
    public:
        context(const context_id id); 

        ~context();

        context_id get_id() const {
            return m_id;
        }

    private:
        // each thread has a stack of contexts, rooted at the trivial "root context"
        const context *m_old_ctx;
        const context_id m_id;
    };

} // namespace toku

// Get the current context of this thread
const toku::context *toku_thread_get_context();

enum context_status_entry {
    CTX_SEARCH_BLOCKED_BY_FULL_FETCH = 0,
    CTX_SEARCH_BLOCKED_BY_PARTIAL_FETCH,
    CTX_SEARCH_BLOCKED_BY_FULL_EVICTION,
    CTX_SEARCH_BLOCKED_BY_PARTIAL_EVICTION,
    CTX_SEARCH_BLOCKED_BY_MESSAGE_INJECTION,
    CTX_SEARCH_BLOCKED_BY_MESSAGE_APPLICATION,
    CTX_SEARCH_BLOCKED_BY_FLUSH,
    CTX_SEARCH_BLOCKED_BY_CLEANER,
    CTX_SEARCH_BLOCKED_OTHER,
    CTX_PROMO_BLOCKED_BY_FULL_FETCH,
    CTX_PROMO_BLOCKED_BY_PARTIAL_FETCH,
    CTX_PROMO_BLOCKED_BY_FULL_EVICTION,
    CTX_PROMO_BLOCKED_BY_PARTIAL_EVICTION,
    CTX_PROMO_BLOCKED_BY_MESSAGE_INJECTION,
    CTX_PROMO_BLOCKED_BY_MESSAGE_APPLICATION,
    CTX_PROMO_BLOCKED_BY_FLUSH,
    CTX_PROMO_BLOCKED_BY_CLEANER,
    CTX_PROMO_BLOCKED_OTHER,
    CTX_BLOCKED_OTHER,
    CTX_STATUS_NUM_ROWS
};

struct context_status {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[CTX_STATUS_NUM_ROWS];
};

void toku_context_get_status(struct context_status *status);

void toku_context_status_destroy(void);
