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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#pragma once

#include "util/dmt.h"
#include "util/mempool.h"

#include "ft/leafentry.h"
#include "ft/serialize/wbuf.h"

// Key/leafentry pair stored in a dmt.  The key is inlined, the offset (in leafentry mempool) is stored for the leafentry.
struct klpair_struct {
    uint32_t le_offset;  //Offset of leafentry (in leafentry mempool)
    uint8_t key[0]; // key, followed by le
};

static constexpr uint32_t keylen_from_klpair_len(const uint32_t klpair_len) {
    return klpair_len - __builtin_offsetof(klpair_struct, key);
}


static_assert(__builtin_offsetof(klpair_struct, key) == 1*sizeof(uint32_t), "klpair alignment issues");
static_assert(__builtin_offsetof(klpair_struct, key) == sizeof(klpair_struct), "klpair size issues");

// A wrapper for the heaviside function provided to dmt->find*.
// Needed because the heaviside functions provided to bndata do not know about the internal types.
// Alternative to this wrapper is to expose accessor functions and rewrite all the external heaviside functions.
template<typename dmtcmp_t,
         int (*h)(const DBT &, const dmtcmp_t &)>
static int klpair_find_wrapper(const uint32_t klpair_len, const klpair_struct &klpair, const dmtcmp_t &extra) {
    DBT kdbt;
    kdbt.data = const_cast<void*>(reinterpret_cast<const void*>(klpair.key));
    kdbt.size = keylen_from_klpair_len(klpair_len);
    return h(kdbt, extra);
}

template<typename inner_iterate_extra_t>
struct klpair_iterate_extra {
    public:
    inner_iterate_extra_t *inner;
    const class bn_data * bd;
};

// A wrapper for the high-order function provided to dmt->iterate*
// Needed because the heaviside functions provided to bndata do not know about the internal types.
// Alternative to this wrapper is to expose accessor functions and rewrite all the external heaviside functions.
template<typename iterate_extra_t,
         int (*f)(const void * key, const uint32_t keylen, const LEAFENTRY &, const uint32_t idx, iterate_extra_t *const)>
static int klpair_iterate_wrapper(const uint32_t klpair_len, const klpair_struct &klpair, const uint32_t idx, klpair_iterate_extra<iterate_extra_t> *const extra) {
    const void* key = &klpair.key;
    LEAFENTRY le = extra->bd->get_le_from_klpair(&klpair);
    return f(key, keylen_from_klpair_len(klpair_len), le, idx, extra->inner);
}


namespace toku {
// dmt writer for klpair_struct
class klpair_dmtwriter {
    public:
        // Return the size needed for the klpair_struct that this dmtwriter represents
        size_t get_size(void) const {
            return sizeof(klpair_struct) + this->keylen;
        }
        // Write the klpair_struct this dmtwriter represents to a destination
        void write_to(klpair_struct *const dest) const {
            dest->le_offset = this->le_offset;
            memcpy(dest->key, this->keyp, this->keylen);
        }

        klpair_dmtwriter(uint32_t _keylen, uint32_t _le_offset, const void* _keyp)
            : keylen(_keylen), le_offset(_le_offset), keyp(_keyp) {}
        klpair_dmtwriter(const uint32_t klpair_len, klpair_struct *const src)
            : keylen(keylen_from_klpair_len(klpair_len)), le_offset(src->le_offset), keyp(src->key) {}
    private:
        const uint32_t keylen;
        const uint32_t le_offset;
        const void* keyp;
};
}

typedef toku::dmt<klpair_struct, klpair_struct*, toku::klpair_dmtwriter> klpair_dmt_t;
// This class stores the data associated with a basement node
class bn_data {
public:
    // Initialize an empty bn_data _without_ a dmt backing.
    // Externally only used for deserialization.
    void init_zero(void);

    // Initialize an empty bn_data _with_ a dmt
    void initialize_empty(void);

    // Deserialize a bn_data from rbuf.
    // This is the entry point for deserialization.
    void deserialize_from_rbuf(uint32_t num_entries, struct rbuf *rb, uint32_t data_size, uint32_t version);

    // Retrieve the memory footprint of this basement node.
    // May over or under count: see Tokutek/ft-index#136
    // Also see dmt's implementation.
    uint64_t get_memory_size(void);

    // Get the serialized size of this basement node.
    uint64_t get_disk_size(void);

    // Perform (paranoid) verification that all leafentries are fully contained within the mempool
    void verify_mempool(void);

    // size() of key dmt
    uint32_t num_klpairs(void) const;

    // iterate() on key dmt (and associated leafentries)
    template<typename iterate_extra_t,
             int (*f)(const void * key, const uint32_t keylen, const LEAFENTRY &, const uint32_t, iterate_extra_t *const)>
    int iterate(iterate_extra_t *const iterate_extra) const {
        return iterate_on_range<iterate_extra_t, f>(0, num_klpairs(), iterate_extra);
    }

    // iterate_on_range() on key dmt (and associated leafentries)
    template<typename iterate_extra_t,
             int (*f)(const void * key, const uint32_t keylen, const LEAFENTRY &, const uint32_t, iterate_extra_t *const)>
    int iterate_on_range(const uint32_t left, const uint32_t right, iterate_extra_t *const iterate_extra) const {
        klpair_iterate_extra<iterate_extra_t> klpair_extra = { iterate_extra, this };
        return m_buffer.iterate_on_range< klpair_iterate_extra<iterate_extra_t>, klpair_iterate_wrapper<iterate_extra_t, f> >(left, right, &klpair_extra);
    }

    // find_zero() on key dmt
    template<typename dmtcmp_t,
             int (*h)(const DBT &, const dmtcmp_t &)>
    int find_zero(const dmtcmp_t &extra, LEAFENTRY *const value, void** key, uint32_t* keylen, uint32_t *const idxp) const {
        klpair_struct* klpair = nullptr;
        uint32_t klpair_len;
        int r = m_buffer.find_zero< dmtcmp_t, klpair_find_wrapper<dmtcmp_t, h> >(extra, &klpair_len, &klpair, idxp);
        if (r == 0) {
            if (value) {
                *value = get_le_from_klpair(klpair);
            }
            if (key) {
                paranoid_invariant_notnull(keylen);
                *key = klpair->key;
                *keylen = keylen_from_klpair_len(klpair_len);
            }
            else {
                paranoid_invariant_null(keylen);
            }
        }
        return r;
    }

    // find() on key dmt (and associated leafentries)
    template<typename dmtcmp_t,
             int (*h)(const DBT &, const dmtcmp_t &)>
    int find(const dmtcmp_t &extra, int direction, LEAFENTRY *const value, void** key, uint32_t* keylen, uint32_t *const idxp) const {
        klpair_struct* klpair = nullptr;
        uint32_t klpair_len;
        int r = m_buffer.find< dmtcmp_t, klpair_find_wrapper<dmtcmp_t, h> >(extra, direction, &klpair_len, &klpair, idxp);
        if (r == 0) {
            if (value) {
                *value = get_le_from_klpair(klpair);
            }
            if (key) {
                paranoid_invariant_notnull(keylen);
                *key = klpair->key;
                *keylen = keylen_from_klpair_len(klpair_len);
            }
            else {
                paranoid_invariant_null(keylen);
            }
        }
        return r;
    }

    // Fetch leafentry by index
    __attribute__((__nonnull__))
    int fetch_le(uint32_t idx, LEAFENTRY *le);
    // Fetch (leafentry, key, keylen) by index
    __attribute__((__nonnull__))
    int fetch_klpair(uint32_t idx, LEAFENTRY *le, uint32_t *len, void** key);
    // Fetch (serialized size of leafentry, key, and keylen) by index
    __attribute__((__nonnull__))
    int fetch_klpair_disksize(uint32_t idx, size_t *size);
    // Fetch (key, keylen) by index
    __attribute__((__nonnull__))
    int fetch_key_and_len(uint32_t idx, uint32_t *len, void** key);

    // Move leafentries (and associated key/keylens) from this basement node to dest_bd
    // Moves indexes [lbi-ube)
    __attribute__((__nonnull__))
    void split_klpairs(bn_data* dest_bd, uint32_t first_index_for_dest);

    // Destroy this basement node and free memory.
    void destroy(void);

    // Uses sorted array as input for this basement node.
    // Expects this to be a basement node just initialized with initialize_empty()
    void set_contents_as_clone_of_sorted_array(
        uint32_t num_les,
        const void** old_key_ptrs,
        uint32_t* old_keylens,
        LEAFENTRY* old_les,
        size_t *le_sizes,
        size_t total_key_size,
        size_t total_le_size
        );

    // Make this basement node a clone of orig_bn_data.
    // orig_bn_data still owns all its memory (dmt, mempool)
    // this basement node will have a new dmt, mempool containing same data.
    void clone(bn_data* orig_bn_data);

    // Delete klpair index idx with provided keylen and old leafentry with size old_le_size
    void delete_leafentry (
        uint32_t idx,
        uint32_t keylen,
        uint32_t old_le_size
        );

    // Allocates space in the mempool to store a new leafentry.
    // This may require reorganizing the mempool and updating the dmt.
    __attribute__((__nonnull__))
    void get_space_for_overwrite(uint32_t idx, const void* keyp, uint32_t keylen, uint32_t old_keylen, uint32_t old_size,
                                 uint32_t new_size, LEAFENTRY* new_le_space, void **const maybe_free);

    // Allocates space in the mempool to store a new leafentry
    // and inserts a new key into the dmt
    // This may require reorganizing the mempool and updating the dmt.
    __attribute__((__nonnull__))
    void get_space_for_insert(uint32_t idx, const void* keyp, uint32_t keylen, size_t size, LEAFENTRY* new_le_space, void **const maybe_free);

    // Gets a leafentry given a klpair from this basement node.
    LEAFENTRY get_le_from_klpair(const klpair_struct *klpair) const;

    void serialize_to_wbuf(struct wbuf *const wb);

    // Prepares this basement node for serialization.
    // Must be called before serializing this basement node.
    // Between calling prepare_to_serialize and actually serializing, the basement node may not be modified
    void prepare_to_serialize(void);

    // Serialize the basement node header to a wbuf
    // Requires prepare_to_serialize() to have been called first.
    void serialize_header(struct wbuf *wb) const;

    // Serialize all keys and leafentries to a wbuf
    // Requires prepare_to_serialize() (and serialize_header()) has been called first.
    // Currently only supported when all keys are fixed-length.
    void serialize_rest(struct wbuf *wb) const;

    static const uint32_t HEADER_LENGTH = 0
        + sizeof(uint32_t) // key_data_size
        + sizeof(uint32_t) // val_data_size
        + sizeof(uint32_t) // fixed_key_length
        + sizeof(uint8_t) // all_keys_same_length
        + sizeof(uint8_t) // keys_vals_separate
        + 0;
private:

    // split_klpairs_extra should be a local class in split_klpairs, but
    // the dmt template parameter for iterate needs linkage, so it has to be a
    // separate class, but we want it to be able to call e.g. add_key
    friend class split_klpairs_extra;

    // Allocates space in the mempool.
    // If there is insufficient space, the mempool is enlarged and leafentries may be shuffled to reduce fragmentation.
    // If shuffling happens, the offsets stored in the dmt are updated.
    LEAFENTRY mempool_malloc_and_update_dmt(size_t size, void **maybe_free);

    // Change the size of the mempool to support what is already in it, plus added_size.
    // possibly "compress" by shuffling leafentries around to reduce fragmentation to 0.
    // If fragmentation is already 0 and force_compress is not true, shuffling may be skipped.
    // If shuffling happens, leafentries will be stored in the mempool in sorted order.
    void dmt_compress_kvspace(size_t added_size, void **maybe_free, bool force_compress);

    // Note that a key was added (for maintaining disk-size of this basement node)
    void add_key(uint32_t keylen);

    // Note that multiple keys were added (for maintaining disk-size of this basement node)
    void add_keys(uint32_t n_keys, uint32_t combined_klpair_len);

    // Note that a key was removed (for maintaining disk-size of this basement node)
    void remove_key(uint32_t keylen);

    klpair_dmt_t m_buffer;                     // pointers to individual leaf entries
    struct mempool m_buffer_mempool;  // storage for all leaf entries

    friend class bndata_bugfix_test;

    // Get the serialized size of a klpair.
    // As of Jan 14, 2014, serialized size of a klpair is independent of whether this basement node has fixed-length keys.
    uint32_t klpair_disksize(const uint32_t klpair_len, const klpair_struct *klpair) const;

    // The disk/memory size of all keys.  (Note that the size of memory for the leafentries is maintained by m_buffer_mempool)
    size_t m_disksize_of_keys;

    // Deserialize this basement node from rbuf
    // all keys will be first followed by all leafentries (both in sorted order)
    void initialize_from_separate_keys_and_vals(uint32_t num_entries, struct rbuf *rb, uint32_t data_size, uint32_t version,
                                                uint32_t key_data_size, uint32_t val_data_size, bool all_keys_same_length,
                                                uint32_t fixed_klpair_length);
};
