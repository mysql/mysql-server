/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

/* The purpose of this file is to provide access to the ft_msg,
 * which is the ephemeral version of the messages that lives in
 * a message buffer.
 */

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

#include <db.h>

#include "portability/toku_assert.h"
#include "portability/toku_stdint.h"

#include "ft/txn/xids.h"

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// Message Sequence Number (MSN)
typedef struct __toku_msn { uint64_t msn; } MSN;

// dummy used for message construction, to be filled in when msg is applied to tree
static const MSN ZERO_MSN = { .msn = 0 };

// first 2^62 values reserved for messages created before Dr. No (for upgrade)
static const MSN MIN_MSN = { .msn = 1ULL << 62 };
static const MSN MAX_MSN = { .msn = UINT64_MAX };

/* tree command types */
enum ft_msg_type {
    FT_NONE = 0,
    FT_INSERT = 1,
    FT_DELETE_ANY = 2,  // Delete any matching key.  This used to be called FT_DELETE.
    //FT_DELETE_BOTH = 3,
    FT_ABORT_ANY = 4,   // Abort any commands on any matching key.
    //FT_ABORT_BOTH  = 5, // Abort commands that match both the key and the value
    FT_COMMIT_ANY  = 6,
    //FT_COMMIT_BOTH = 7,
    FT_COMMIT_BROADCAST_ALL = 8, // Broadcast to all leafentries, (commit all transactions).
    FT_COMMIT_BROADCAST_TXN = 9, // Broadcast to all leafentries, (commit specific transaction).
    FT_ABORT_BROADCAST_TXN  = 10, // Broadcast to all leafentries, (commit specific transaction).
    FT_INSERT_NO_OVERWRITE = 11,
    FT_OPTIMIZE = 12,             // Broadcast
    FT_OPTIMIZE_FOR_UPGRADE = 13, // same as FT_OPTIMIZE, but record version number in leafnode
    FT_UPDATE = 14,
    FT_UPDATE_BROADCAST_ALL = 15
};

static inline bool
ft_msg_type_applies_once(enum ft_msg_type type)
{
    bool ret_val;
    switch (type) {
    case FT_INSERT_NO_OVERWRITE:
    case FT_INSERT:
    case FT_DELETE_ANY:
    case FT_ABORT_ANY:
    case FT_COMMIT_ANY:
    case FT_UPDATE:
        ret_val = true;
        break;
    case FT_COMMIT_BROADCAST_ALL:
    case FT_COMMIT_BROADCAST_TXN:
    case FT_ABORT_BROADCAST_TXN:
    case FT_OPTIMIZE:
    case FT_OPTIMIZE_FOR_UPGRADE:
    case FT_UPDATE_BROADCAST_ALL:
    case FT_NONE:
        ret_val = false;
        break;
    default:
        assert(false);
    }
    return ret_val;
}

static inline bool
ft_msg_type_applies_all(enum ft_msg_type type)
{
    bool ret_val;
    switch (type) {
    case FT_NONE:
    case FT_INSERT_NO_OVERWRITE:
    case FT_INSERT:
    case FT_DELETE_ANY:
    case FT_ABORT_ANY:
    case FT_COMMIT_ANY:
    case FT_UPDATE:
        ret_val = false;
        break;
    case FT_COMMIT_BROADCAST_ALL:
    case FT_COMMIT_BROADCAST_TXN:
    case FT_ABORT_BROADCAST_TXN:
    case FT_OPTIMIZE:
    case FT_OPTIMIZE_FOR_UPGRADE:
    case FT_UPDATE_BROADCAST_ALL:
        ret_val = true;
        break;
    default:
        assert(false);
    }
    return ret_val;
}

static inline bool
ft_msg_type_does_nothing(enum ft_msg_type type)
{
    return (type == FT_NONE);
}

class ft_msg {
public:
    ft_msg(const DBT *key, const DBT *val, enum ft_msg_type t, MSN m, XIDS x);

    enum ft_msg_type type() const;

    MSN msn() const;

    XIDS xids() const;

    const DBT *kdbt() const;

    const DBT *vdbt() const;

    size_t total_size() const;

    void serialize_to_wbuf(struct wbuf *wb, bool is_fresh) const;

    // deserialization goes through a static factory function so the ft msg
    // API stays completely const and there's no default constructor
    static ft_msg deserialize_from_rbuf(struct rbuf *rb, XIDS *xids, bool *is_fresh);

    // Version 13/14 messages did not have an msn - so `m' is the MSN
    // that will be assigned to the message that gets deserialized.
    static ft_msg deserialize_from_rbuf_v13(struct rbuf *rb, MSN m, XIDS *xids);

private:
    const DBT _key;
    const DBT _val;
    enum ft_msg_type _type;
    MSN _msn;
    XIDS _xids;
};

// For serialize / deserialize

#include "ft/serialize/wbuf.h"

static inline void wbuf_MSN(struct wbuf *wb, MSN msn) {
    wbuf_ulonglong(wb, msn.msn);
}

#include "ft/serialize/rbuf.h"

static inline MSN rbuf_MSN(struct rbuf *rb) {
    MSN msn = { .msn = rbuf_ulonglong(rb) };
    return msn;
}
