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

#include <bndata.h>
#include <ft-ops.h>

using namespace toku;
uint32_t bn_data::klpair_disksize(const uint32_t klpair_len, const klpair_struct *klpair) const {
    return sizeof(*klpair) + keylen_from_klpair_len(klpair_len) + leafentry_disksize(get_le_from_klpair(klpair));
}

void bn_data::init_zero() {
    toku_mempool_zero(&m_buffer_mempool);
    m_disksize_of_keys = 0;
}

void bn_data::initialize_empty() {
    init_zero();
    m_buffer.create();
}

void bn_data::add_key(uint32_t keylen) {
    m_disksize_of_keys += sizeof(keylen) + keylen;
}

void bn_data::add_keys(uint32_t n_keys, uint32_t combined_klpair_len) {
    invariant(n_keys * sizeof(uint32_t) <= combined_klpair_len);
    m_disksize_of_keys += combined_klpair_len;
}

void bn_data::remove_key(uint32_t keylen) {
    m_disksize_of_keys -= sizeof(keylen) + keylen;
}

// Deserialize from format optimized for keys being inlined.
// Currently only supports fixed-length keys.
void bn_data::initialize_from_separate_keys_and_vals(uint32_t num_entries, struct rbuf *rb, uint32_t data_size, uint32_t version UU(),
                                                     uint32_t key_data_size, uint32_t val_data_size, bool all_keys_same_length,
                                                     uint32_t fixed_klpair_length) {
    paranoid_invariant(version >= FT_LAYOUT_VERSION_26);  // Support was added @26
    uint32_t ndone_before = rb->ndone;
    init_zero();
    invariant(all_keys_same_length);  // Until otherwise supported.
    bytevec keys_src;
    rbuf_literal_bytes(rb, &keys_src, key_data_size);
    //Generate dmt
    this->m_buffer.create_from_sorted_memory_of_fixed_size_elements(
            keys_src, num_entries, key_data_size, fixed_klpair_length);
    toku_mempool_construct(&this->m_buffer_mempool, val_data_size);

    bytevec vals_src;
    rbuf_literal_bytes(rb, &vals_src, val_data_size);

    if (num_entries > 0) {
        void *vals_dest = toku_mempool_malloc(&this->m_buffer_mempool, val_data_size, 1);
        paranoid_invariant_notnull(vals_dest);
        memcpy(vals_dest, vals_src, val_data_size);
    }

    add_keys(num_entries, num_entries * fixed_klpair_length);

    toku_note_deserialized_basement_node(all_keys_same_length);

    invariant(rb->ndone - ndone_before == data_size);
}

static int
wbufwriteleafentry(const void* key, const uint32_t keylen, const LEAFENTRY &le, const uint32_t UU(idx), struct wbuf * const wb) {
    // need to pack the leafentry as it was in versions
    // where the key was integrated into it (< 26)
    uint32_t begin_spot UU() = wb->ndone;
    uint32_t le_disk_size = leafentry_disksize(le);
    wbuf_nocrc_uint8_t(wb, le->type);
    wbuf_nocrc_uint32_t(wb, keylen);
    if (le->type == LE_CLEAN) {
        wbuf_nocrc_uint32_t(wb, le->u.clean.vallen);
        wbuf_nocrc_literal_bytes(wb, key, keylen);
        wbuf_nocrc_literal_bytes(wb, le->u.clean.val, le->u.clean.vallen);
    }
    else {
        paranoid_invariant(le->type == LE_MVCC);
        wbuf_nocrc_uint32_t(wb, le->u.mvcc.num_cxrs);
        wbuf_nocrc_uint8_t(wb, le->u.mvcc.num_pxrs);
        wbuf_nocrc_literal_bytes(wb, key, keylen);
        wbuf_nocrc_literal_bytes(wb, le->u.mvcc.xrs, le_disk_size - (1 + 4 + 1));
    }
    uint32_t end_spot UU() = wb->ndone;
    paranoid_invariant((end_spot - begin_spot) == keylen + sizeof(keylen) + le_disk_size);
    return 0;
}

void bn_data::serialize_to_wbuf(struct wbuf *const wb) {
    prepare_to_serialize();
    serialize_header(wb);
    if (m_buffer.value_length_is_fixed()) {
        serialize_rest(wb);
    } else {
        //
        // iterate over leafentries and place them into the buffer
        //
        iterate<struct wbuf, wbufwriteleafentry>(wb);
    }
}

// If we have fixed-length keys, we prepare the dmt and mempool.
// The mempool is prepared by removing any fragmented space and ordering leafentries in the same order as their keys.
void bn_data::prepare_to_serialize(void) {
    if (m_buffer.value_length_is_fixed()) {
        m_buffer.prepare_for_serialize();
        dmt_compress_kvspace(0, nullptr, true);  // Gets it ready for easy serialization.
    }
}

void bn_data::serialize_header(struct wbuf *wb) const {
    bool fixed = m_buffer.value_length_is_fixed();

    //key_data_size
    wbuf_nocrc_uint(wb, m_disksize_of_keys);
    //val_data_size
    wbuf_nocrc_uint(wb, toku_mempool_get_used_size(&m_buffer_mempool));
    //fixed_klpair_length
    wbuf_nocrc_uint(wb, m_buffer.get_fixed_length());
    // all_keys_same_length
    wbuf_nocrc_uint8_t(wb, fixed);
    // keys_vals_separate
    wbuf_nocrc_uint8_t(wb, fixed);
}

void bn_data::serialize_rest(struct wbuf *wb) const {
    //Write keys
    invariant(m_buffer.value_length_is_fixed()); //Assumes prepare_to_serialize was called
    m_buffer.serialize_values(m_disksize_of_keys, wb);

    //Write leafentries
    //Just ran dmt_compress_kvspace so there is no fragmentation and also leafentries are in sorted order.
    paranoid_invariant(toku_mempool_get_frag_size(&m_buffer_mempool) == 0);
    uint32_t val_data_size = toku_mempool_get_used_size(&m_buffer_mempool);
    wbuf_nocrc_literal_bytes(wb, toku_mempool_get_base(&m_buffer_mempool), val_data_size);
}

// Deserialize from rbuf
void bn_data::deserialize_from_rbuf(uint32_t num_entries, struct rbuf *rb, uint32_t data_size, uint32_t version) {
    uint32_t key_data_size = data_size;  // overallocate if < version 26 (best guess that is guaranteed not too small)
    uint32_t val_data_size = data_size;  // overallocate if < version 26 (best guess that is guaranteed not too small)

    bool all_keys_same_length = false;
    bool keys_vals_separate = false;
    uint32_t fixed_klpair_length = 0;

    // In version 25 and older there is no header.  Skip reading header for old version.
    if (version >= FT_LAYOUT_VERSION_26) {
        uint32_t ndone_before = rb->ndone;
        key_data_size = rbuf_int(rb);
        val_data_size = rbuf_int(rb);
        fixed_klpair_length = rbuf_int(rb);  // 0 if !all_keys_same_length
        all_keys_same_length = rbuf_char(rb);
        keys_vals_separate = rbuf_char(rb);
        invariant(all_keys_same_length == keys_vals_separate);  // Until we support otherwise
        uint32_t header_size = rb->ndone - ndone_before;
        data_size -= header_size;
        invariant(header_size == HEADER_LENGTH);
        if (keys_vals_separate) {
            invariant(fixed_klpair_length >= sizeof(klpair_struct) || num_entries == 0);
            initialize_from_separate_keys_and_vals(num_entries, rb, data_size, version,
                                                   key_data_size, val_data_size, all_keys_same_length,
                                                   fixed_klpair_length);
            return;
        }
    }
    // Version >= 26 and version 25 deserialization are now identical except that <= 25 might allocate too much memory.
    bytevec bytes;
    rbuf_literal_bytes(rb, &bytes, data_size);
    const unsigned char *CAST_FROM_VOIDP(buf, bytes);
    if (data_size == 0) {
        invariant_zero(num_entries);
    }
    init_zero();
    klpair_dmt_t::builder dmt_builder;
    dmt_builder.create(num_entries, key_data_size);

    // TODO(leif): clean this up (#149)
    unsigned char *newmem = nullptr;
    // add 25% extra wiggle room
    uint32_t allocated_bytes_vals = val_data_size + (val_data_size / 4);
    CAST_FROM_VOIDP(newmem, toku_xmalloc(allocated_bytes_vals));
    const unsigned char* curr_src_pos = buf;
    unsigned char* curr_dest_pos = newmem;
    for (uint32_t i = 0; i < num_entries; i++) {
        uint8_t curr_type = curr_src_pos[0];
        curr_src_pos++;
        // first thing we do is lay out the key,
        // to do so, we must extract it from the leafentry
        // and write it in
        uint32_t keylen = 0;
        const void* keyp = nullptr;
        keylen = *(uint32_t *)curr_src_pos;
        curr_src_pos += sizeof(uint32_t);
        uint32_t clean_vallen = 0;
        uint32_t num_cxrs = 0;
        uint8_t num_pxrs = 0;
        if (curr_type == LE_CLEAN) {
            clean_vallen = toku_dtoh32(*(uint32_t *)curr_src_pos);
            curr_src_pos += sizeof(clean_vallen); // val_len
            keyp = curr_src_pos;
            curr_src_pos += keylen;
        }
        else {
            paranoid_invariant(curr_type == LE_MVCC);
            num_cxrs = toku_htod32(*(uint32_t *)curr_src_pos);
            curr_src_pos += sizeof(uint32_t); // num_cxrs
            num_pxrs = curr_src_pos[0];
            curr_src_pos += sizeof(uint8_t); //num_pxrs
            keyp = curr_src_pos;
            curr_src_pos += keylen;
        }
        uint32_t le_offset = curr_dest_pos - newmem;
        dmt_builder.append(klpair_dmtwriter(keylen, le_offset, keyp));
        add_key(keylen);

        // now curr_dest_pos is pointing to where the leafentry should be packed
        curr_dest_pos[0] = curr_type;
        curr_dest_pos++;
        if (curr_type == LE_CLEAN) {
             *(uint32_t *)curr_dest_pos = toku_htod32(clean_vallen);
             curr_dest_pos += sizeof(clean_vallen);
             memcpy(curr_dest_pos, curr_src_pos, clean_vallen); // copy the val
             curr_dest_pos += clean_vallen;
             curr_src_pos += clean_vallen;
        }
        else {
            // pack num_cxrs and num_pxrs
            *(uint32_t *)curr_dest_pos = toku_htod32(num_cxrs);
            curr_dest_pos += sizeof(num_cxrs);
            *(uint8_t *)curr_dest_pos = num_pxrs;
            curr_dest_pos += sizeof(num_pxrs);
            // now we need to pack the rest of the data
            uint32_t num_rest_bytes = leafentry_rest_memsize(num_pxrs, num_cxrs, const_cast<uint8_t*>(curr_src_pos));
            memcpy(curr_dest_pos, curr_src_pos, num_rest_bytes);
            curr_dest_pos += num_rest_bytes;
            curr_src_pos += num_rest_bytes;
        }
    }
    dmt_builder.build(&this->m_buffer);
    toku_note_deserialized_basement_node(m_buffer.value_length_is_fixed());

    uint32_t num_bytes_read = (uint32_t)(curr_src_pos - buf);
    invariant(num_bytes_read == data_size);

    uint32_t num_bytes_written = curr_dest_pos - newmem + m_disksize_of_keys;
    invariant(num_bytes_written == data_size);
    toku_mempool_init(&m_buffer_mempool, newmem, (size_t)(curr_dest_pos - newmem), allocated_bytes_vals);

    invariant(get_disk_size() == data_size);
    // Versions older than 26 might have allocated too much memory.  Try to shrink the mempool now that we
    // know how much memory we need.
    if (version < FT_LAYOUT_VERSION_26) {
        // Unnecessary after version 26
        // Reallocate smaller mempool to save memory
        invariant_zero(toku_mempool_get_frag_size(&m_buffer_mempool));
        toku_mempool_realloc_larger(&m_buffer_mempool, toku_mempool_get_used_size(&m_buffer_mempool));
    }
}

uint64_t bn_data::get_memory_size() {
    uint64_t retval = 0;
    //TODO: Maybe ask for memory_size instead of mempool_footprint (either this todo or the next)
    // include fragmentation overhead but do not include space in the
    // mempool that has not yet been allocated for leaf entries
    size_t poolsize = toku_mempool_footprint(&m_buffer_mempool);
    retval += poolsize;
    // This one includes not-yet-allocated for nodes (just like old constant-key omt)
    //TODO: Maybe ask for mempool_footprint instead of memory_size.
    retval += m_buffer.memory_size();
    invariant(retval >= get_disk_size());
    return retval;
}

void bn_data::delete_leafentry (
    uint32_t idx,
    uint32_t keylen,
    uint32_t old_le_size
    )
{
    remove_key(keylen);
    m_buffer.delete_at(idx);
    toku_mempool_mfree(&m_buffer_mempool, nullptr, old_le_size);
}

/* mempool support */

struct dmt_compressor_state {
    struct mempool *new_kvspace;
    class bn_data *bd;
};

static int move_it (const uint32_t, klpair_struct *klpair, const uint32_t idx UU(), struct dmt_compressor_state * const oc) {
    LEAFENTRY old_le = oc->bd->get_le_from_klpair(klpair);
    uint32_t size = leafentry_memsize(old_le);
    void* newdata = toku_mempool_malloc(oc->new_kvspace, size, 1);
    paranoid_invariant_notnull(newdata); // we do this on a fresh mempool, so nothing bad should happen
    memcpy(newdata, old_le, size);
    klpair->le_offset = toku_mempool_get_offset_from_pointer_and_base(oc->new_kvspace, newdata);
    return 0;
}

// Compress things, and grow or shrink the mempool if needed.
// May (always if force_compress) have a side effect of putting contents of mempool in sorted order.
void bn_data::dmt_compress_kvspace(size_t added_size, void **maybe_free, bool force_compress) {
    uint32_t total_size_needed = toku_mempool_get_used_size(&m_buffer_mempool) + added_size;

    // If there is no fragmentation, e.g. in serial inserts, we can just increase the size
    // of the mempool and move things over with a cheap memcpy. If force_compress is true, 
    // the caller needs the side effect that all contents are put in sorted order.
    bool do_compress = toku_mempool_get_frag_size(&m_buffer_mempool) > 0 || force_compress;

    void *old_mempool_base = toku_mempool_get_base(&m_buffer_mempool);
    struct mempool new_kvspace;
    if (do_compress) {
        size_t requested_size = force_compress ? total_size_needed : ((total_size_needed * 3) / 2);
        toku_mempool_construct(&new_kvspace, requested_size);
        struct dmt_compressor_state oc = { &new_kvspace, this };
        m_buffer.iterate_ptr< decltype(oc), move_it >(&oc);
    } else {
        toku_mempool_construct(&new_kvspace, total_size_needed);
        size_t old_offset_limit = toku_mempool_get_offset_limit(&m_buffer_mempool);
        void *new_mempool_base = toku_mempool_malloc(&new_kvspace, old_offset_limit, 1);
        memcpy(new_mempool_base, old_mempool_base, old_offset_limit);
    }

    if (maybe_free) {
        *maybe_free = old_mempool_base;
    } else {
        toku_free(old_mempool_base);
    }
    m_buffer_mempool = new_kvspace;
}

// Effect: Allocate a new object of size SIZE in MP.  If MP runs out of space, allocate new a new mempool space, and copy all the items
//  from the OMT (which items refer to items in the old mempool) into the new mempool.
//  If MAYBE_FREE is nullptr then free the old mempool's space.
//  Otherwise, store the old mempool's space in maybe_free.
LEAFENTRY bn_data::mempool_malloc_and_update_dmt(size_t size, void **maybe_free) {
    void *v = toku_mempool_malloc(&m_buffer_mempool, size, 1);
    if (v == nullptr) {
        dmt_compress_kvspace(size, maybe_free, false);
        v = toku_mempool_malloc(&m_buffer_mempool, size, 1);
        paranoid_invariant_notnull(v);
    }
    return (LEAFENTRY)v;
}

void bn_data::get_space_for_overwrite(
    uint32_t idx,
    const void* keyp UU(),
    uint32_t keylen UU(),
    uint32_t old_le_size,
    uint32_t new_size,
    LEAFENTRY* new_le_space,
    void **const maybe_free
    )
{
    *maybe_free = nullptr;
    LEAFENTRY new_le = mempool_malloc_and_update_dmt(new_size, maybe_free);
    toku_mempool_mfree(&m_buffer_mempool, nullptr, old_le_size);
    klpair_struct* klp = nullptr;
    uint32_t klpair_len;
    int r = m_buffer.fetch(idx, &klpair_len, &klp);
    invariant_zero(r);
    paranoid_invariant(klp!=nullptr);
    // Key never changes.
    paranoid_invariant(keylen_from_klpair_len(klpair_len) == keylen);

    size_t new_le_offset = toku_mempool_get_offset_from_pointer_and_base(&this->m_buffer_mempool, new_le);
    paranoid_invariant(new_le_offset <= UINT32_MAX - new_size);  // Not using > 4GB
    klp->le_offset = new_le_offset;

    paranoid_invariant(new_le == get_le_from_klpair(klp));
    *new_le_space = new_le;
}

void bn_data::get_space_for_insert(
    uint32_t idx,
    const void* keyp,
    uint32_t keylen,
    size_t size,
    LEAFENTRY* new_le_space,
    void **const maybe_free
    )
{
    add_key(keylen);

    *maybe_free = nullptr;
    LEAFENTRY new_le = mempool_malloc_and_update_dmt(size, maybe_free);
    size_t new_le_offset = toku_mempool_get_offset_from_pointer_and_base(&this->m_buffer_mempool, new_le);

    klpair_dmtwriter kl(keylen, new_le_offset, keyp);
    m_buffer.insert_at(kl, idx);

    *new_le_space = new_le;
}

class split_klpairs_extra {
    bn_data *const m_left_bn;
    bn_data *const m_right_bn;
    klpair_dmt_t::builder *const m_left_builder;
    klpair_dmt_t::builder *const m_right_builder;
    struct mempool *const m_left_dest_mp;
    uint32_t m_split_at;

    struct mempool *left_dest_mp(void) const { return m_left_dest_mp; }
    struct mempool *right_dest_mp(void) const { return &m_right_bn->m_buffer_mempool; }

    void copy_klpair(const uint32_t klpair_len, const klpair_struct &klpair,
                     klpair_dmt_t::builder *const builder,
                     struct mempool *const dest_mp,
                     bn_data *const bn) {
        LEAFENTRY old_le = m_left_bn->get_le_from_klpair(&klpair);
        size_t le_size = leafentry_memsize(old_le);

        void *new_le = toku_mempool_malloc(dest_mp, le_size, 1);
        paranoid_invariant_notnull(new_le);
        memcpy(new_le, old_le, le_size);
        size_t le_offset = toku_mempool_get_offset_from_pointer_and_base(dest_mp, new_le);
        size_t keylen = keylen_from_klpair_len(klpair_len);
        builder->append(klpair_dmtwriter(keylen, le_offset, klpair.key));

        bn->add_key(keylen);
    }

    int move_leafentry(const uint32_t klpair_len, const klpair_struct &klpair, const uint32_t idx) {
        m_left_bn->remove_key(keylen_from_klpair_len(klpair_len));

        if (idx < m_split_at) {
            copy_klpair(klpair_len, klpair, m_left_builder, left_dest_mp(), m_left_bn);
        } else {
            copy_klpair(klpair_len, klpair, m_right_builder, right_dest_mp(), m_right_bn);
        }
        return 0;
    }

  public:
    split_klpairs_extra(bn_data *const left_bn, bn_data *const right_bn,
                        klpair_dmt_t::builder *const left_builder,
                        klpair_dmt_t::builder *const right_builder,
                        struct mempool *const left_new_mp,
                        uint32_t split_at)
        : m_left_bn(left_bn),
          m_right_bn(right_bn),
          m_left_builder(left_builder),
          m_right_builder(right_builder),
          m_left_dest_mp(left_new_mp),
          m_split_at(split_at) {}
    static int cb(const uint32_t klpair_len, const klpair_struct &klpair, const uint32_t idx, split_klpairs_extra *const thisp) {
        return thisp->move_leafentry(klpair_len, klpair, idx);
    }
};

void bn_data::split_klpairs(
     bn_data* right_bd,
     uint32_t split_at //lower bound inclusive for right_bd
     )
{
    // We use move_leafentries_to during a split, and the split algorithm should never call this
    // if it's splitting on a boundary, so there must be some leafentries in the range to move.
    paranoid_invariant(split_at < num_klpairs());

    right_bd->init_zero();

    size_t mpsize = toku_mempool_get_used_size(&m_buffer_mempool);   // overkill, but safe

    struct mempool new_left_mp;
    toku_mempool_construct(&new_left_mp, mpsize);

    struct mempool *right_mp = &right_bd->m_buffer_mempool;
    toku_mempool_construct(right_mp, mpsize);

    klpair_dmt_t::builder left_dmt_builder;
    left_dmt_builder.create(split_at, m_disksize_of_keys);  // overkill, but safe (builder will realloc at the end)

    klpair_dmt_t::builder right_dmt_builder;
    right_dmt_builder.create(num_klpairs() - split_at, m_disksize_of_keys);  // overkill, but safe (builder will realloc at the end)

    split_klpairs_extra extra(this, right_bd, &left_dmt_builder, &right_dmt_builder, &new_left_mp, split_at);

    int r = m_buffer.iterate<split_klpairs_extra, split_klpairs_extra::cb>(&extra);
    invariant_zero(r);

    m_buffer.destroy();
    toku_mempool_destroy(&m_buffer_mempool);

    m_buffer_mempool = new_left_mp;

    left_dmt_builder.build(&m_buffer);
    right_dmt_builder.build(&right_bd->m_buffer);

    // Potentially shrink memory pool for destination.
    // We overallocated ("overkill") above
    struct mempool *const left_mp = &m_buffer_mempool;
    paranoid_invariant_zero(toku_mempool_get_frag_size(left_mp));
    toku_mempool_realloc_larger(left_mp, toku_mempool_get_used_size(left_mp));
    paranoid_invariant_zero(toku_mempool_get_frag_size(right_mp));
    toku_mempool_realloc_larger(right_mp, toku_mempool_get_used_size(right_mp));
}

uint64_t bn_data::get_disk_size() {
    return m_disksize_of_keys +
           toku_mempool_get_used_size(&m_buffer_mempool);
}

struct verify_le_in_mempool_state {
    size_t offset_limit;
    class bn_data *bd;
};

static int verify_le_in_mempool (const uint32_t, klpair_struct *klpair, const uint32_t idx UU(), struct verify_le_in_mempool_state * const state) {
    invariant(klpair->le_offset < state->offset_limit);

    LEAFENTRY le = state->bd->get_le_from_klpair(klpair);
    uint32_t size = leafentry_memsize(le);

    size_t end_offset = klpair->le_offset+size;

    invariant(end_offset <= state->offset_limit);
    return 0;
}

//This is a debug-only (paranoid) verification.
//Verifies the dmt is valid, and all leafentries are entirely in the mempool's memory.
void bn_data::verify_mempool(void) {
    //Verify the dmt itself <- paranoid and slow
    m_buffer.verify();

    verify_le_in_mempool_state state = { .offset_limit = toku_mempool_get_offset_limit(&m_buffer_mempool), .bd = this };
    //Verify every leafentry pointed to by the keys in the dmt are fully inside the mempool
    m_buffer.iterate_ptr< decltype(state), verify_le_in_mempool >(&state);
}

uint32_t bn_data::num_klpairs(void) const {
    return m_buffer.size();
}

void bn_data::destroy(void) {
    // The buffer may have been freed already, in some cases.
    m_buffer.destroy();
    toku_mempool_destroy(&m_buffer_mempool);
    m_disksize_of_keys = 0;
}

void bn_data::set_contents_as_clone_of_sorted_array(
    uint32_t num_les,
    const void** old_key_ptrs,
    uint32_t* old_keylens,
    LEAFENTRY* old_les,
    size_t *le_sizes,
    size_t total_key_size,
    size_t total_le_size
    )
{
    //Enforce "just created" invariant.
    paranoid_invariant_zero(m_disksize_of_keys);
    paranoid_invariant_zero(num_klpairs());
    paranoid_invariant_null(toku_mempool_get_base(&m_buffer_mempool));
    paranoid_invariant_zero(toku_mempool_get_size(&m_buffer_mempool));

    toku_mempool_construct(&m_buffer_mempool, total_le_size);
    m_buffer.destroy();
    m_disksize_of_keys = 0;

    klpair_dmt_t::builder dmt_builder;
    dmt_builder.create(num_les, total_key_size);

    for (uint32_t idx = 0; idx < num_les; idx++) {
        void* new_le = toku_mempool_malloc(&m_buffer_mempool, le_sizes[idx], 1);
        paranoid_invariant_notnull(new_le);
        memcpy(new_le, old_les[idx], le_sizes[idx]);
        size_t le_offset = toku_mempool_get_offset_from_pointer_and_base(&m_buffer_mempool, new_le);
        dmt_builder.append(klpair_dmtwriter(old_keylens[idx], le_offset, old_key_ptrs[idx]));
        add_key(old_keylens[idx]);
    }
    dmt_builder.build(&this->m_buffer);
}

LEAFENTRY bn_data::get_le_from_klpair(const klpair_struct *klpair) const {
    void * ptr = toku_mempool_get_pointer_from_base_and_offset(&this->m_buffer_mempool, klpair->le_offset);
    LEAFENTRY CAST_FROM_VOIDP(le, ptr);
    return le;
}


// get info about a single leafentry by index
int bn_data::fetch_le(uint32_t idx, LEAFENTRY *le) {
    klpair_struct* klpair = nullptr;
    int r = m_buffer.fetch(idx, nullptr, &klpair);
    if (r == 0) {
        *le = get_le_from_klpair(klpair);
    }
    return r;
}

int bn_data::fetch_klpair(uint32_t idx, LEAFENTRY *le, uint32_t *len, void** key) {
    klpair_struct* klpair = nullptr;
    uint32_t klpair_len;
    int r = m_buffer.fetch(idx, &klpair_len, &klpair);
    if (r == 0) {
        *len = keylen_from_klpair_len(klpair_len);
        *key = klpair->key;
        *le = get_le_from_klpair(klpair);
    }
    return r;
}

int bn_data::fetch_klpair_disksize(uint32_t idx, size_t *size) {
    klpair_struct* klpair = nullptr;
    uint32_t klpair_len;
    int r = m_buffer.fetch(idx, &klpair_len, &klpair);
    if (r == 0) {
        *size = klpair_disksize(klpair_len, klpair);
    }
    return r;
}

int bn_data::fetch_key_and_len(uint32_t idx, uint32_t *len, void** key) {
    klpair_struct* klpair = nullptr;
    uint32_t klpair_len;
    int r = m_buffer.fetch(idx, &klpair_len, &klpair);
    if (r == 0) {
        *len = keylen_from_klpair_len(klpair_len);
        *key = klpair->key;
    }
    return r;
}

void bn_data::clone(bn_data* orig_bn_data) {
    toku_mempool_clone(&orig_bn_data->m_buffer_mempool, &m_buffer_mempool);
    m_buffer.clone(orig_bn_data->m_buffer);
    this->m_disksize_of_keys = orig_bn_data->m_disksize_of_keys;
}

