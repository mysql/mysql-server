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

#include <util/mempool.h>
#include "wbuf.h"
#include <util/dmt.h>
#include "leafentry.h"

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
    uint32_t le_offset;  //Offset of leafentry (in leafentry mempool)
    uint8_t key_le[0]; // key, followed by le
};

static constexpr uint32_t keylen_from_klpair_len(const uint32_t klpair_len) {
    return klpair_len - __builtin_offsetof(klpair_struct, key_le);
}

typedef struct klpair_struct KLPAIR_S, *KLPAIR;

static_assert(__builtin_offsetof(klpair_struct, key_le) == 1*sizeof(uint32_t), "klpair alignment issues");
static_assert(__builtin_offsetof(klpair_struct, key_le) == sizeof(klpair_struct), "klpair size issues");

template<typename dmtcmp_t,
         int (*h)(const DBT &, const dmtcmp_t &)>
static int wrappy_fun_find(const uint32_t klpair_len, const klpair_struct &klpair, const dmtcmp_t &extra) {
    DBT kdbt;
    kdbt.data = const_cast<void*>(reinterpret_cast<const void*>(klpair.key_le));
    kdbt.size = keylen_from_klpair_len(klpair_len);
    return h(kdbt, extra);
}

template<typename inner_iterate_extra_t>
struct wrapped_iterate_extra_t {
    public:
    inner_iterate_extra_t *inner;
    const class bn_data * bd;
};

template<typename iterate_extra_t,
         int (*h)(const void * key, const uint32_t keylen, const LEAFENTRY &, const uint32_t idx, iterate_extra_t *const)>
static int wrappy_fun_iterate(const uint32_t klpair_len, const klpair_struct &klpair, const uint32_t idx, wrapped_iterate_extra_t<iterate_extra_t> *const extra) {
    const void* key = &klpair.key_le;
    LEAFENTRY le = extra->bd->get_le_from_klpair(&klpair);
    return h(key, keylen_from_klpair_len(klpair_len), le, idx, extra->inner);
}


namespace toku {
template<>
class dmt_functor<klpair_struct> {
    public:
        size_t get_dmtdatain_t_size(void) const {
            return sizeof(klpair_struct) + this->keylen;
        }
        void write_dmtdata_t_to(klpair_struct *const dest) const {
            dest->le_offset = this->le_offset;
            memcpy(dest->key_le, this->keyp, this->keylen);
        }

        dmt_functor(uint32_t _keylen, uint32_t _le_offset, const void* _keyp)
            : keylen(_keylen), le_offset(_le_offset), keyp(_keyp) {}
        dmt_functor(const uint32_t klpair_len, klpair_struct *const src)
            : keylen(keylen_from_klpair_len(klpair_len)), le_offset(src->le_offset), keyp(src->key_le) {}
    private:
        const uint32_t keylen;
        const uint32_t le_offset;
        const void* keyp;
};
}

typedef toku::dmt<KLPAIR_S, KLPAIR> klpair_dmt_t;
// This class stores the data associated with a basement node
class bn_data {
public:
    void init_zero(void);
    void initialize_empty(void);
    void initialize_from_data(uint32_t num_entries, struct rbuf *rb, uint32_t data_size, uint32_t version);
    // globals
    uint64_t get_memory_size(void);
    uint64_t get_disk_size(void);
    void verify_mempool(void);

    // Interact with "dmt"
    uint32_t omt_size(void) const;

    template<typename iterate_extra_t,
             int (*f)(const void * key, const uint32_t keylen, const LEAFENTRY &, const uint32_t, iterate_extra_t *const)>
    int omt_iterate(iterate_extra_t *const iterate_extra) const {
        return omt_iterate_on_range<iterate_extra_t, f>(0, omt_size(), iterate_extra);
    }

    template<typename iterate_extra_t,
             int (*f)(const void * key, const uint32_t keylen, const LEAFENTRY &, const uint32_t, iterate_extra_t *const)>
    int omt_iterate_on_range(const uint32_t left, const uint32_t right, iterate_extra_t *const iterate_extra) const {
        wrapped_iterate_extra_t<iterate_extra_t> wrapped_extra = { iterate_extra, this };
        return m_buffer.iterate_on_range< wrapped_iterate_extra_t<iterate_extra_t>, wrappy_fun_iterate<iterate_extra_t, f> >(left, right, &wrapped_extra);
    }

    template<typename dmtcmp_t,
             int (*h)(const DBT &, const dmtcmp_t &)>
    int find_zero(const dmtcmp_t &extra, LEAFENTRY *const value, void** key, uint32_t* keylen, uint32_t *const idxp) const {
        KLPAIR klpair = NULL;
        uint32_t klpair_len;
        int r = m_buffer.find_zero< dmtcmp_t, wrappy_fun_find<dmtcmp_t, h> >(extra, &klpair_len, &klpair, idxp);
        if (r == 0) {
            if (value) {
                *value = get_le_from_klpair(klpair);
            }
            if (key) {
                paranoid_invariant(keylen != NULL);
                *key = klpair->key_le;
                *keylen = keylen_from_klpair_len(klpair_len);
            }
            else {
                paranoid_invariant_null(keylen);
            }
        }
        return r;
    }

    template<typename dmtcmp_t,
             int (*h)(const DBT &, const dmtcmp_t &)>
    int find(const dmtcmp_t &extra, int direction, LEAFENTRY *const value, void** key, uint32_t* keylen, uint32_t *const idxp) const {
        KLPAIR klpair = NULL;
        uint32_t klpair_len;
        int r = m_buffer.find< dmtcmp_t, wrappy_fun_find<dmtcmp_t, h> >(extra, direction, &klpair_len, &klpair, idxp);
        if (r == 0) {
            if (value) {
                *value = get_le_from_klpair(klpair);
            }
            if (key) {
                paranoid_invariant(keylen != NULL);
                *key = klpair->key_le;
                *keylen = keylen_from_klpair_len(klpair_len);
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
        size_t total_key_size,
        size_t total_le_size
        );

    void clone(bn_data* orig_bn_data);
    void delete_leafentry (
        uint32_t idx,
        uint32_t keylen,
        uint32_t old_le_size
        );
    void get_space_for_overwrite(uint32_t idx, const void* keyp, uint32_t keylen, uint32_t old_size, uint32_t new_size, LEAFENTRY* new_le_space);
    void get_space_for_insert(uint32_t idx, const void* keyp, uint32_t keylen, size_t size, LEAFENTRY* new_le_space);

    LEAFENTRY get_le_from_klpair(const klpair_struct *klpair) const;

    void prepare_to_serialize(void);
    void serialize_header(struct wbuf *wb) const;
    void serialize_rest(struct wbuf *wb) const;
    bool need_to_serialize_each_leafentry_with_key(void) const;

    static const uint32_t HEADER_LENGTH = 0
        + sizeof(uint32_t) // key_data_size
        + sizeof(uint32_t) // val_data_size
        + sizeof(uint32_t) // fixed_key_length
        + sizeof(uint8_t) // all_keys_same_length
        + sizeof(uint8_t) // keys_vals_separate
        + 0;
private:

    // Private functions
    LEAFENTRY mempool_malloc_and_update_omt(size_t size, void **maybe_free);
    void omt_compress_kvspace(size_t added_size, void **maybe_free, bool force_compress);
    void add_key(uint32_t keylen);
    void add_keys(uint32_t n_keys, uint32_t combined_keylen);
    void remove_key(uint32_t keylen);

    klpair_dmt_t m_buffer;                     // pointers to individual leaf entries
    struct mempool m_buffer_mempool;  // storage for all leaf entries

    friend class bndata_bugfix_test;
    uint32_t klpair_disksize(const uint32_t klpair_len, const klpair_struct *klpair) const;
    size_t m_disksize_of_keys;

    void initialize_from_separate_keys_and_vals(uint32_t num_entries, struct rbuf *rb, uint32_t data_size, uint32_t version,
                                                uint32_t key_data_size, uint32_t val_data_size, bool all_keys_same_length,
                                                uint32_t fixed_key_length);
};

