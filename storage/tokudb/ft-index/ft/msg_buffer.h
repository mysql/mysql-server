/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:

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
  Copyright (C) 2014 Tokutek, Inc.

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

#include "ft/msg.h"
#include "ft/txn/xids.h"
#include "util/dbt.h"

class message_buffer {
public:
    void create();

    void clone(message_buffer *dst);

    void destroy();

    // effect: deserializes a message buffer from the given rbuf
    // returns: *fresh_offsets (etc) malloc'd to be num_entries large and
    //          populated with *nfresh (etc) offsets in the message buffer
    // requires: if fresh_offsets (etc) != nullptr, then nfresh != nullptr
    void deserialize_from_rbuf(struct rbuf *rb,
                               int32_t **fresh_offsets, int32_t *nfresh,
                               int32_t **stale_offsets, int32_t *nstale,
                               int32_t **broadcast_offsets, int32_t *nbroadcast);

    // effect: deserializes a message buffer whose messages are at version 13/14
    // returns: similar to deserialize_from_rbuf(), excpet there are no stale messages
    //          and each message is assigned a sequential value from *highest_unused_msn_for_upgrade,
    //          which is modified as needed using toku_sync_fech_and_sub()
    // returns: the highest MSN assigned to any message in this buffer
    // requires: similar to deserialize_from_rbuf(), and highest_unused_msn_for_upgrade != nullptr
    MSN deserialize_from_rbuf_v13(struct rbuf *rb,
                                  MSN *highest_unused_msn_for_upgrade,
                                  int32_t **fresh_offsets, int32_t *nfresh,
                                  int32_t **broadcast_offsets, int32_t *nbroadcast);

    void enqueue(const ft_msg &msg, bool is_fresh, int32_t *offset);

    void set_freshness(int32_t offset, bool is_fresh);

    bool get_freshness(int32_t offset) const;

    ft_msg get_message(int32_t offset, DBT *keydbt, DBT *valdbt) const;

    void get_message_key_msn(int32_t offset, DBT *key, MSN *msn) const;

    int num_entries() const;

    size_t buffer_size_in_use() const;

    size_t memory_size_in_use() const;

    size_t memory_footprint() const;

    template <typename F>
    int iterate(F &fn) const {
        for (int32_t offset = 0; offset < _memory_used; ) {
            DBT k, v;
            const ft_msg msg = get_message(offset, &k, &v);
            bool is_fresh = get_freshness(offset);
            int r = fn(msg, is_fresh);
            if (r != 0) {
                return r;
            }
            offset += msg_memsize_in_buffer(msg);
        }
        return 0;
    }

    bool equals(message_buffer *other) const;

    void serialize_to_wbuf(struct wbuf *wb) const;

    static size_t msg_memsize_in_buffer(const ft_msg &msg);

private:
    void _resize(size_t new_size);

    // If this isn't packged, the compiler aligns the xids array and we waste a lot of space
    struct __attribute__((__packed__)) buffer_entry {
        unsigned int  keylen;
        unsigned int  vallen;
        unsigned char type;
        bool          is_fresh;
        MSN           msn;
        XIDS_S        xids_s;
    };

    struct buffer_entry *get_buffer_entry(int32_t offset) const;

    int   _num_entries;
    char *_memory;       // An array of bytes into which buffer entries are embedded.
    int   _memory_size;  // How big is _memory
    int   _memory_used;  // How many bytes are in use?
};
