/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
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


#pragma once

#include <util/omt.h>
#include "leafentry.h"
#include <util/mempool.h>

#if 0 //for implementation
static int
UU() verify_in_mempool(OMTVALUE lev, uint32_t UU(idx), void *mpv)
{
    LEAFENTRY CAST_FROM_VOIDP(le, lev);
    struct mempool *CAST_FROM_VOIDP(mp, mpv);
    int r = toku_mempool_inrange(mp, le, leafentry_memsize(le));
    lazy_assert(r);
    return 0;
}
            toku_omt_iterate(bn->buffer, verify_in_mempool, &bn->buffer_mempool);

#endif

struct klpair_struct {
    uint32_t keylen;
    uint8_t key_le[0]; // key, followed by le
};

typedef struct klpair_struct *KLPAIR;

static inline LEAFENTRY get_le_from_klpair(KLPAIR klpair){
    uint32_t keylen = klpair->keylen;
    LEAFENTRY le = (LEAFENTRY)(klpair->key_le + keylen);
    return le;
}

template<typename omtcmp_t,
         int (*h)(const DBT &, const omtcmp_t &)>
static int wrappy_fun_find(const KLPAIR &klpair, const omtcmp_t &extra) {
    //TODO: kill this function when we split, and/or use toku_fill_dbt
    DBT kdbt;
    kdbt.data = klpair->key_le;
    kdbt.size = klpair->keylen;
    return h(kdbt, extra);
}

template<typename iterate_extra_t,
         int (*h)(const void * key, const uint32_t keylen, const LEAFENTRY &, const uint32_t idx, iterate_extra_t *const)>
static int wrappy_fun_iterate(const KLPAIR &klpair, const uint32_t idx, iterate_extra_t *const extra) {
    uint32_t keylen = klpair->keylen;
    void* key = klpair->key_le;
    LEAFENTRY le = get_le_from_klpair(klpair);
    return h(key, keylen, le, idx, extra);
}

typedef toku::omt<KLPAIR> klpair_omt_t;
// This class stores the data associated with a basement node
class bn_data {
public:
    void init_zero(void);
    void initialize_empty(void);
    void initialize_from_data(uint32_t num_entries, unsigned char *buf, uint32_t data_size);
    // globals
    uint64_t get_memory_size(void);
    uint64_t get_disk_size(void);
    void verify_mempool(void);

    // Interact with "omt"
    uint32_t omt_size(void) const;

    template<typename iterate_extra_t,
             int (*f)(const void * key, const uint32_t keylen, const LEAFENTRY &, const uint32_t, iterate_extra_t *const)>
    int omt_iterate(iterate_extra_t *const iterate_extra) const {
        return omt_iterate_on_range<iterate_extra_t, f>(0, omt_size(), iterate_extra);
    }

    template<typename iterate_extra_t,
             int (*f)(const void * key, const uint32_t keylen, const LEAFENTRY &, const uint32_t, iterate_extra_t *const)>
    int omt_iterate_on_range(const uint32_t left, const uint32_t right, iterate_extra_t *const iterate_extra) const {
        return m_buffer.iterate_on_range< iterate_extra_t, wrappy_fun_iterate<iterate_extra_t, f> >(left, right, iterate_extra);
    }

    template<typename omtcmp_t,
             int (*h)(const DBT &, const omtcmp_t &)>
    int find_zero(const omtcmp_t &extra, LEAFENTRY *const value, void** key, uint32_t* keylen, uint32_t *const idxp) const {
        KLPAIR klpair = NULL;
        int r = m_buffer.find_zero< omtcmp_t, wrappy_fun_find<omtcmp_t, h> >(extra, &klpair, idxp);
        if (r == 0) {
            if (value) {
                *value = get_le_from_klpair(klpair);
            }
            if (key) {
                paranoid_invariant(keylen != NULL);
                *key = klpair->key_le;
                *keylen = klpair->keylen;
            }
            else {
                paranoid_invariant(keylen == NULL);
            }
        }
        return r;
    }

    template<typename omtcmp_t,
             int (*h)(const DBT &, const omtcmp_t &)>
    int find(const omtcmp_t &extra, int direction, LEAFENTRY *const value, void** key, uint32_t* keylen, uint32_t *const idxp) const {
        KLPAIR klpair = NULL;
        int r = m_buffer.find< omtcmp_t, wrappy_fun_find<omtcmp_t, h> >(extra, direction, &klpair, idxp);
        if (r == 0) {
            if (value) {
                *value = get_le_from_klpair(klpair);
            }
            if (key) {
                paranoid_invariant(keylen != NULL);
                *key = klpair->key_le;
                *keylen = klpair->keylen;
            }
            else {
                paranoid_invariant(keylen == NULL);
            }
        }
        return r;
    }

    // get info about a single leafentry by index
    int fetch_le(uint32_t idx, LEAFENTRY *le);
    int fetch_klpair(uint32_t idx, LEAFENTRY *le, uint32_t *len, void** key);
    int fetch_klpair_disksize(uint32_t idx, size_t *size);
    int fetch_le_key_and_len(uint32_t idx, uint32_t *len, void** key);

    // Interact with another bn_data
    void move_leafentries_to(BN_DATA dest_bd,
                                      uint32_t lbi, //lower bound inclusive
                                      uint32_t ube //upper bound exclusive
                                      );

    void destroy(void);

    // Replaces contents, into brand new mempool.
    // Returns old mempool base, expects caller to free it.
    void replace_contents_with_clone_of_sorted_array(
        uint32_t num_les,
        const void** old_key_ptrs,
        uint32_t* old_keylens,
        LEAFENTRY* old_les,
        size_t *le_sizes,
        size_t mempool_size
        );

    void clone(bn_data* orig_bn_data);
    void delete_leafentry (
        uint32_t idx,
        uint32_t keylen,
        uint32_t old_le_size
        );
    void get_space_for_overwrite(uint32_t idx, const void* keyp, uint32_t keylen, uint32_t old_size, uint32_t new_size, LEAFENTRY* new_le_space);
    void get_space_for_insert(uint32_t idx, const void* keyp, uint32_t keylen, size_t size, LEAFENTRY* new_le_space);
private:
    // Private functions
    KLPAIR mempool_malloc_from_omt(size_t size, void **maybe_free);
    void omt_compress_kvspace(size_t added_size, void **maybe_free);

    klpair_omt_t m_buffer;                     // pointers to individual leaf entries
    struct mempool m_buffer_mempool;  // storage for all leaf entries

    friend class bndata_bugfix_test;
};

