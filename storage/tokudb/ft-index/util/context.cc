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

#ident "Copyright (c) 2007-2014 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <string.h>

#include <util/context.h>

namespace toku {

    static const context default_context(CTX_DEFAULT);
    static __thread const context *tl_current_context = &default_context;

    // save the old context, set the current context
    context::context(const context_id id) :
        m_old_ctx(tl_current_context),
        m_id(id) {
        tl_current_context = this;
    }

    // restore the old context
    context::~context() {
        tl_current_context = m_old_ctx;
    }

} // namespace toku

// thread local context

const toku::context *toku_thread_get_context() {
    return toku::tl_current_context;
}

// engine status

static struct context_status context_status;
#define CONTEXT_STATUS_INIT(key, legend) TOKUFT_STATUS_INIT(context_status, key, nullptr, PARCOUNT, "context: " legend, TOKU_ENGINE_STATUS)

static void
context_status_init(void) {
    CONTEXT_STATUS_INIT(CTX_SEARCH_BLOCKED_BY_FULL_FETCH,           "tree traversals blocked by a full fetch");
    CONTEXT_STATUS_INIT(CTX_SEARCH_BLOCKED_BY_PARTIAL_FETCH,        "tree traversals blocked by a partial fetch");
    CONTEXT_STATUS_INIT(CTX_SEARCH_BLOCKED_BY_FULL_EVICTION,        "tree traversals blocked by a full eviction");
    CONTEXT_STATUS_INIT(CTX_SEARCH_BLOCKED_BY_PARTIAL_EVICTION,     "tree traversals blocked by a partial eviction");
    CONTEXT_STATUS_INIT(CTX_SEARCH_BLOCKED_BY_MESSAGE_INJECTION,    "tree traversals blocked by a message injection");
    CONTEXT_STATUS_INIT(CTX_SEARCH_BLOCKED_BY_MESSAGE_APPLICATION,  "tree traversals blocked by a message application");
    CONTEXT_STATUS_INIT(CTX_SEARCH_BLOCKED_BY_FLUSH,                "tree traversals blocked by a flush");
    CONTEXT_STATUS_INIT(CTX_SEARCH_BLOCKED_BY_CLEANER,              "tree traversals blocked by a the cleaner thread");
    CONTEXT_STATUS_INIT(CTX_SEARCH_BLOCKED_OTHER,                   "tree traversals blocked by something uninstrumented");
    CONTEXT_STATUS_INIT(CTX_PROMO_BLOCKED_BY_FULL_FETCH,            "promotion blocked by a full fetch (should never happen)");
    CONTEXT_STATUS_INIT(CTX_PROMO_BLOCKED_BY_PARTIAL_FETCH,         "promotion blocked by a partial fetch (should never happen)");
    CONTEXT_STATUS_INIT(CTX_PROMO_BLOCKED_BY_FULL_EVICTION,         "promotion blocked by a full eviction (should never happen)");
    CONTEXT_STATUS_INIT(CTX_PROMO_BLOCKED_BY_PARTIAL_EVICTION,      "promotion blocked by a partial eviction (should never happen)");
    CONTEXT_STATUS_INIT(CTX_PROMO_BLOCKED_BY_MESSAGE_INJECTION,     "promotion blocked by a message injection");
    CONTEXT_STATUS_INIT(CTX_PROMO_BLOCKED_BY_MESSAGE_APPLICATION,   "promotion blocked by a message application");
    CONTEXT_STATUS_INIT(CTX_PROMO_BLOCKED_BY_FLUSH,                 "promotion blocked by a flush");
    CONTEXT_STATUS_INIT(CTX_PROMO_BLOCKED_BY_CLEANER,               "promotion blocked by the cleaner thread");
    CONTEXT_STATUS_INIT(CTX_PROMO_BLOCKED_OTHER,                    "promotion blocked by something uninstrumented");
    CONTEXT_STATUS_INIT(CTX_BLOCKED_OTHER,                          "something uninstrumented blocked by something uninstrumented");
    context_status.initialized = true;
}
#undef FS_STATUS_INIT

void toku_context_get_status(struct context_status *status) {
    if (!context_status.initialized) {
        context_status_init();
    }
    *status = context_status;
}

#define STATUS_INC(x, d) increment_partitioned_counter(context_status.status[x].value.parcount, d);

void toku_context_note_frwlock_contention(const context_id blocked, const context_id blocking) {
    if (!context_status.initialized) {
        context_status_init();
    }
    if (blocked != CTX_SEARCH && blocked != CTX_PROMO) {
        // Return early if this event is "unknown"
        STATUS_INC(CTX_BLOCKED_OTHER, 1);
        return;
    }
    switch (blocking) {
    case CTX_FULL_FETCH:
        if (blocked == CTX_SEARCH) {
            STATUS_INC(CTX_SEARCH_BLOCKED_BY_FULL_FETCH, 1);
        } else if (blocked == CTX_PROMO) {
            STATUS_INC(CTX_PROMO_BLOCKED_BY_FULL_FETCH, 1);
        }
        break;
    case CTX_PARTIAL_FETCH:
        if (blocked == CTX_SEARCH) {
            STATUS_INC(CTX_SEARCH_BLOCKED_BY_PARTIAL_FETCH, 1);
        } else if (blocked == CTX_PROMO) {
            STATUS_INC(CTX_PROMO_BLOCKED_BY_PARTIAL_FETCH, 1);
        }
        break;
    case CTX_FULL_EVICTION:
        if (blocked == CTX_SEARCH) {
            STATUS_INC(CTX_SEARCH_BLOCKED_BY_FULL_EVICTION, 1);
        } else if (blocked == CTX_PROMO) {
            STATUS_INC(CTX_PROMO_BLOCKED_BY_FULL_EVICTION, 1);
        }
        break;
    case CTX_PARTIAL_EVICTION:
        if (blocked == CTX_SEARCH) {
            STATUS_INC(CTX_SEARCH_BLOCKED_BY_PARTIAL_EVICTION, 1);
        } else if (blocked == CTX_PROMO) {
            STATUS_INC(CTX_PROMO_BLOCKED_BY_PARTIAL_EVICTION, 1);
        }
        break;
    case CTX_MESSAGE_INJECTION:
        if (blocked == CTX_SEARCH) {
            STATUS_INC(CTX_SEARCH_BLOCKED_BY_MESSAGE_INJECTION, 1);
        } else if (blocked == CTX_PROMO) {
            STATUS_INC(CTX_PROMO_BLOCKED_BY_MESSAGE_INJECTION, 1);
        }
        break;
    case CTX_MESSAGE_APPLICATION:
        if (blocked == CTX_SEARCH) {
            STATUS_INC(CTX_SEARCH_BLOCKED_BY_MESSAGE_APPLICATION, 1);
        } else if (blocked == CTX_PROMO) {
            STATUS_INC(CTX_PROMO_BLOCKED_BY_MESSAGE_APPLICATION, 1);
        }
        break;
    case CTX_FLUSH:
        if (blocked == CTX_SEARCH) {
            STATUS_INC(CTX_SEARCH_BLOCKED_BY_FLUSH, 1);
        } else if (blocked == CTX_PROMO) {
            STATUS_INC(CTX_PROMO_BLOCKED_BY_FLUSH, 1);
        }
        break;
    case CTX_CLEANER:
        if (blocked == CTX_SEARCH) {
            STATUS_INC(CTX_SEARCH_BLOCKED_BY_CLEANER, 1);
        } else if (blocked == CTX_PROMO) {
            STATUS_INC(CTX_PROMO_BLOCKED_BY_CLEANER, 1);
        }
        break;
    default:
        if (blocked == CTX_SEARCH) {
            STATUS_INC(CTX_SEARCH_BLOCKED_OTHER, 1);
        } else if (blocked == CTX_PROMO) {
            STATUS_INC(CTX_PROMO_BLOCKED_OTHER, 1);
        }
        break;
    }
}

void toku_context_status_destroy(void) {
    for (int i = 0; i < CTX_STATUS_NUM_ROWS; ++i) {
        if (context_status.status[i].type == PARCOUNT) {
            destroy_partitioned_counter(context_status.status[i].value.parcount);
        }
    }
}
