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

void bn_data::add_keys(uint32_t n_keys, uint32_t combined_keylen) {
    invariant(n_keys * sizeof(uint32_t) <= combined_keylen);
    m_disksize_of_keys += combined_keylen;
}

void bn_data::remove_key(uint32_t keylen) {
    m_disksize_of_keys -= sizeof(keylen) + keylen;
}

void bn_data::initialize_from_separate_keys_and_vals(uint32_t num_entries, struct rbuf *rb, uint32_t data_size, uint32_t version UU(),
                                                     uint32_t key_data_size, uint32_t val_data_size, bool all_keys_same_length,
                                                     uint32_t fixed_key_length) {
    paranoid_invariant(version >= FT_LAYOUT_VERSION_25);  // Support was added @25
    uint32_t ndone_before = rb->ndone;
    init_zero();
    invariant(all_keys_same_length);  // Until otherwise supported.
    bytevec keys_src;
    rbuf_literal_bytes(rb, &keys_src, key_data_size);
    //Generate dmt
    this->m_buffer.create_from_sorted_memory_of_fixed_size_elements(
            keys_src, num_entries, key_data_size, fixed_key_length);
    toku_mempool_construct(&this->m_buffer_mempool, val_data_size);

    bytevec vals_src;
    rbuf_literal_bytes(rb, &vals_src, val_data_size);

    if (num_entries > 0) {
        void *vals_dest = toku_mempool_malloc(&this->m_buffer_mempool, val_data_size, 1);
        paranoid_invariant_notnull(vals_dest);
        memcpy(vals_dest, vals_src, val_data_size);
    }

    add_keys(num_entries, num_entries * fixed_key_length);

    toku_note_deserialized_basement_node(all_keys_same_length);

    invariant(rb->ndone - ndone_before == data_size);
}
// static inline void rbuf_literal_bytes (struct rbuf *r, bytevec *bytes, unsigned int n_bytes) {

void bn_data::prepare_to_serialize(void) {
    if (m_buffer.is_value_length_fixed()) {
        m_buffer.prepare_for_serialize();
        omt_compress_kvspace(0, nullptr, true);  // Gets it ready for easy serialization.
    }
}

void bn_data::serialize_header(struct wbuf *wb) const {
    bool fixed = m_buffer.is_value_length_fixed();

    //key_data_size
    wbuf_nocrc_uint(wb, m_disksize_of_keys);
    //val_data_size
    wbuf_nocrc_uint(wb, toku_mempool_get_used_space(&m_buffer_mempool));
    //fixed_key_length
    wbuf_nocrc_uint(wb, m_buffer.get_fixed_length());
    // all_keys_same_length
    wbuf_nocrc_uint8_t(wb, fixed);
    // keys_vals_separate
    wbuf_nocrc_uint8_t(wb, fixed);
}

void bn_data::serialize_rest(struct wbuf *wb) const {
    //Write keys
    invariant(m_buffer.is_value_length_fixed()); //Assumes prepare_to_serialize was called
    m_buffer.serialize_values(m_disksize_of_keys, wb);

    //Write leafentries
    paranoid_invariant(toku_mempool_get_frag_size(&m_buffer_mempool) == 0); //Just ran omt_compress_kvspace
    uint32_t val_data_size = toku_mempool_get_used_space(&m_buffer_mempool);
    wbuf_nocrc_literal_bytes(wb, toku_mempool_get_base(&m_buffer_mempool), val_data_size);
}

bool bn_data::need_to_serialize_each_leafentry_with_key(void) const {
    return !m_buffer.is_value_length_fixed();
}

void bn_data::initialize_from_data(uint32_t num_entries, struct rbuf *rb, uint32_t data_size, uint32_t version) {
    uint32_t key_data_size = data_size;  // overallocate if < version 25
    uint32_t val_data_size = data_size;  // overallocate if < version 25

    bool all_keys_same_length = false;
    bool keys_vals_separate = false;
    uint32_t fixed_key_length = 0;

    if (version >= FT_LAYOUT_VERSION_25) {
        uint32_t ndone_before = rb->ndone;
        key_data_size = rbuf_int(rb);
        val_data_size = rbuf_int(rb);
        fixed_key_length = rbuf_int(rb);  // 0 if !all_keys_same_length
        all_keys_same_length = rbuf_char(rb);
        keys_vals_separate = rbuf_char(rb);
        invariant(all_keys_same_length == keys_vals_separate);  // Until we support this
        uint32_t header_size = rb->ndone - ndone_before;
        data_size -= header_size;
        invariant(header_size == HEADER_LENGTH);
        if (keys_vals_separate) {
            initialize_from_separate_keys_and_vals(num_entries, rb, data_size, version,
                                                   key_data_size, val_data_size, all_keys_same_length,
                                                   fixed_key_length);
            return;
        }
    }
    bytevec bytes;
    rbuf_literal_bytes(rb, &bytes, data_size);
    const unsigned char *CAST_FROM_VOIDP(buf, bytes);
    if (data_size == 0) {
        invariant_zero(num_entries);
    }
    init_zero();
    klpair_dmt_t::builder dmt_builder;
    dmt_builder.create(num_entries, key_data_size);

    unsigned char *newmem = NULL;
    // add same wiggle room that toku_mempool_construct would, 25% extra
    uint32_t allocated_bytes_vals = val_data_size + val_data_size/4;
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
        const void* keyp = NULL;
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
        dmt_builder.insert_sorted(toku::dmt_functor<klpair_struct>(keylen, le_offset, keyp));
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
    dmt_builder.build_and_destroy(&this->m_buffer);
    toku_note_deserialized_basement_node(m_buffer.is_value_length_fixed());

#if TOKU_DEBUG_PARANOID
    uint32_t num_bytes_read = (uint32_t)(curr_src_pos - buf);
    paranoid_invariant( num_bytes_read == data_size);

    uint32_t num_bytes_written = curr_dest_pos - newmem + m_disksize_of_keys;
    paranoid_invariant( num_bytes_written == data_size);
#endif
    toku_mempool_init(&m_buffer_mempool, newmem, (size_t)(curr_dest_pos - newmem), allocated_bytes_vals);

    paranoid_invariant(get_disk_size() == data_size);
    if (version < FT_LAYOUT_VERSION_25) {
        //Maybe shrink mempool.  Unnecessary after version 25
        size_t used = toku_mempool_get_used_space(&m_buffer_mempool);
        size_t max_allowed = used + used / 4;
        size_t allocated = toku_mempool_get_size(&m_buffer_mempool);
        size_t footprint = toku_mempool_footprint(&m_buffer_mempool);
        if (allocated > max_allowed && footprint > max_allowed) {
            // Reallocate smaller mempool to save memory
            invariant_zero(toku_mempool_get_frag_size(&m_buffer_mempool));
            struct mempool new_mp;
            toku_mempool_construct(&new_mp, used);
            void * newbase = toku_mempool_malloc(&new_mp, used, 1);
            invariant_notnull(newbase);
            memcpy(newbase, toku_mempool_get_base(&m_buffer_mempool), used);
            toku_mempool_destroy(&m_buffer_mempool);
            m_buffer_mempool = new_mp;
        }
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
    toku_mempool_mfree(&m_buffer_mempool, nullptr, old_le_size); // Must pass nullptr, since le is no good any more.
}

/* mempool support */

struct omt_compressor_state {
    struct mempool *new_kvspace;
    class bn_data *bd;
};

static int move_it (const uint32_t, klpair_struct *klpair, const uint32_t idx UU(), struct omt_compressor_state * const oc) {
    LEAFENTRY old_le = oc->bd->get_le_from_klpair(klpair);
    uint32_t size = leafentry_memsize(old_le);
    void* newdata = toku_mempool_malloc(oc->new_kvspace, size, 1);
    paranoid_invariant_notnull(newdata); // we do this on a fresh mempool, so nothing bad should happen
    memcpy(newdata, old_le, size);
    klpair->le_offset = toku_mempool_get_offset_from_pointer_and_base(oc->new_kvspace, newdata);
    return 0;
}

// Compress things, and grow the mempool if needed.
void bn_data::omt_compress_kvspace(size_t added_size, void **maybe_free, bool force_compress) {
    uint32_t total_size_needed = toku_mempool_get_used_space(&m_buffer_mempool) + added_size;
    // set the new mempool size to be twice of the space we actually need.
    // On top of the 25% that is padded within toku_mempool_construct (which we
    // should consider getting rid of), that should be good enough.
    if (!force_compress && toku_mempool_get_frag_size(&m_buffer_mempool) == 0) {
        // Skip iterate, just realloc.
        toku_mempool_realloc_larger(&m_buffer_mempool, 2*total_size_needed);
        if (maybe_free) {
            *maybe_free = nullptr;
        }
        return;
    }
    struct mempool new_kvspace;
    toku_mempool_construct(&new_kvspace, 2*total_size_needed);
    struct omt_compressor_state oc = { &new_kvspace, this};
    m_buffer.iterate_ptr< decltype(oc), move_it >(&oc);

    if (maybe_free) {
        *maybe_free = m_buffer_mempool.base;
    } else {
        toku_free(m_buffer_mempool.base);
    }
    m_buffer_mempool = new_kvspace;
}

// Effect: Allocate a new object of size SIZE in MP.  If MP runs out of space, allocate new a new mempool space, and copy all the items
//  from the OMT (which items refer to items in the old mempool) into the new mempool.
//  If MAYBE_FREE is NULL then free the old mempool's space.
//  Otherwise, store the old mempool's space in maybe_free.
LEAFENTRY bn_data::mempool_malloc_and_update_omt(size_t size, void **maybe_free) {
    void *v = toku_mempool_malloc(&m_buffer_mempool, size, 1);
    if (v == NULL) {
        omt_compress_kvspace(size, maybe_free, false);
        v = toku_mempool_malloc(&m_buffer_mempool, size, 1);
        paranoid_invariant_notnull(v);
    }
    return (LEAFENTRY)v;
}

//TODO: probably not free the "maybe_free" right away?
void bn_data::get_space_for_overwrite(
    uint32_t idx,
    const void* keyp UU(),
    uint32_t keylen UU(),
    uint32_t old_le_size,
    uint32_t new_size,
    LEAFENTRY* new_le_space
    )
{
    void* maybe_free = nullptr;
    LEAFENTRY new_le = mempool_malloc_and_update_omt(
        new_size,
        &maybe_free
        );
    toku_mempool_mfree(&m_buffer_mempool, nullptr, old_le_size);  // Must pass nullptr, since le is no good any more.
    KLPAIR klp = nullptr;
    uint32_t klpair_len;  //TODO: maybe delete klpair_len
    int r = m_buffer.fetch(idx, &klpair_len, &klp);
    invariant_zero(r);
    paranoid_invariant(klp!=nullptr);
    // Key never changes.
    paranoid_invariant(keylen_from_klpair_len(klpair_len) == keylen);
    paranoid_invariant(!memcmp(klp->key_le, keyp, keylen));  // TODO: can keyp be pointing to the old space?  If so this could fail

    size_t new_le_offset = toku_mempool_get_offset_from_pointer_and_base(&this->m_buffer_mempool, new_le);
    paranoid_invariant(new_le_offset <= UINT32_MAX - new_size);  // Not using > 4GB
    klp->le_offset = new_le_offset;

    paranoid_invariant(new_le == get_le_from_klpair(klp));
    *new_le_space = new_le;
    // free at end, so that the keyp and keylen
    // passed in is still valid
    if (maybe_free) {
        toku_free(maybe_free);
    }
}

//TODO: probably not free the "maybe_free" right away?
void bn_data::get_space_for_insert(
    uint32_t idx,
    const void* keyp,
    uint32_t keylen,
    size_t size,
    LEAFENTRY* new_le_space
    )
{
    add_key(keylen);

    void* maybe_free = nullptr;
    LEAFENTRY new_le = mempool_malloc_and_update_omt(
        size,
        &maybe_free
        );
    size_t new_le_offset = toku_mempool_get_offset_from_pointer_and_base(&this->m_buffer_mempool, new_le);

    toku::dmt_functor<klpair_struct> kl(keylen, new_le_offset, keyp);
    m_buffer.insert_at(kl, idx);

    *new_le_space = new_le;
    // free at end, so that the keyp and keylen
    // passed in is still valid (you never know if
    // it was part of the old mempool, this is just
    // safer).
    if (maybe_free) {
        toku_free(maybe_free);
    }
}

void bn_data::move_leafentries_to(
     BN_DATA dest_bd,
     uint32_t lbi, //lower bound inclusive
     uint32_t ube //upper bound exclusive
     )
//Effect: move leafentries in the range [lbi, ube) from this to src_omt to newly created dest_omt
{
    //TODO: improve speed: maybe use dmt_builder for one or both, or implement some version of optimized split_at?
    paranoid_invariant(lbi < ube);
    paranoid_invariant(ube <= omt_size());

    dest_bd->initialize_empty();

    size_t mpsize = toku_mempool_get_used_space(&m_buffer_mempool);   // overkill, but safe
    struct mempool *dest_mp = &dest_bd->m_buffer_mempool;
    struct mempool *src_mp  = &m_buffer_mempool;
    toku_mempool_construct(dest_mp, mpsize);

    for (uint32_t i = lbi; i < ube; i++) {
        KLPAIR curr_kl = nullptr;
        uint32_t curr_kl_len;
        int r = m_buffer.fetch(i, &curr_kl_len, &curr_kl);
        invariant_zero(r);

        LEAFENTRY old_le = get_le_from_klpair(curr_kl);
        size_t le_size = leafentry_memsize(old_le);
        void* new_le = toku_mempool_malloc(dest_mp, le_size, 1);
        memcpy(new_le, old_le, le_size);
        size_t le_offset = toku_mempool_get_offset_from_pointer_and_base(dest_mp, new_le);
        dest_bd->m_buffer.insert_at(dmt_functor<klpair_struct>(keylen_from_klpair_len(curr_kl_len), le_offset, curr_kl->key_le), i-lbi);

        this->remove_key(keylen_from_klpair_len(curr_kl_len));
        dest_bd->add_key(keylen_from_klpair_len(curr_kl_len));

        toku_mempool_mfree(src_mp, old_le, le_size);
    }

    // now remove the elements from src_omt
    for (uint32_t i=ube-1; i >= lbi; i--) {
        m_buffer.delete_at(i);
    }
}

uint64_t bn_data::get_disk_size() {
    return m_disksize_of_keys +
           toku_mempool_get_used_space(&m_buffer_mempool);
}

void bn_data::verify_mempool(void) {
    // TODO: implement something
    // TODO: check 7.0 code and see if there was anything there?
}

uint32_t bn_data::omt_size(void) const {
    return m_buffer.size();
}

void bn_data::destroy(void) {
    // The buffer may have been freed already, in some cases.
    m_buffer.destroy();
    toku_mempool_destroy(&m_buffer_mempool);
    m_disksize_of_keys = 0;
}

//TODO: Splitting key/val requires changing this
void bn_data::replace_contents_with_clone_of_sorted_array(
    uint32_t num_les,
    const void** old_key_ptrs,
    uint32_t* old_keylens,
    LEAFENTRY* old_les, 
    size_t *le_sizes, 
    size_t total_key_size,
    size_t total_le_size
    ) 
{
    toku_mempool_construct(&m_buffer_mempool, total_le_size);
    m_buffer.destroy();
    m_disksize_of_keys = 0;

    klpair_dmt_t::builder dmt_builder;
    dmt_builder.create(num_les, total_key_size);

    //TODO: speed this up with some form of mass create dmt
    for (uint32_t idx = 0; idx < num_les; idx++) {
        void* new_le = toku_mempool_malloc(&m_buffer_mempool, le_sizes[idx], 1);
        memcpy(new_le, old_les[idx], le_sizes[idx]);
        size_t le_offset = toku_mempool_get_offset_from_pointer_and_base(&m_buffer_mempool, new_le);
        dmt_builder.insert_sorted(dmt_functor<klpair_struct>(old_keylens[idx], le_offset, old_key_ptrs[idx]));
        add_key(old_keylens[idx]);
    }
    dmt_builder.build_and_destroy(&this->m_buffer);
}

LEAFENTRY bn_data::get_le_from_klpair(const klpair_struct *klpair) const {
    void * ptr = toku_mempool_get_pointer_from_base_and_offset(&this->m_buffer_mempool, klpair->le_offset);
    LEAFENTRY CAST_FROM_VOIDP(le, ptr);
    return le;
}


// get info about a single leafentry by index
int bn_data::fetch_le(uint32_t idx, LEAFENTRY *le) {
    KLPAIR klpair = NULL;
    int r = m_buffer.fetch(idx, nullptr, &klpair);
    if (r == 0) {
        *le = get_le_from_klpair(klpair);
    }
    return r;
}

int bn_data::fetch_klpair(uint32_t idx, LEAFENTRY *le, uint32_t *len, void** key) {
    KLPAIR klpair = NULL;
    uint32_t klpair_len;
    int r = m_buffer.fetch(idx, &klpair_len, &klpair);
    if (r == 0) {
        *len = keylen_from_klpair_len(klpair_len);
        *key = klpair->key_le;
        *le = get_le_from_klpair(klpair);
    }
    return r;
}

int bn_data::fetch_klpair_disksize(uint32_t idx, size_t *size) {
    KLPAIR klpair = NULL;
    uint32_t klpair_len;
    int r = m_buffer.fetch(idx, &klpair_len, &klpair);
    if (r == 0) {
        *size = klpair_disksize(klpair_len, klpair);
    }
    return r;
}

int bn_data::fetch_le_key_and_len(uint32_t idx, uint32_t *len, void** key) {
    KLPAIR klpair = NULL;
    uint32_t klpair_len;
    int r = m_buffer.fetch(idx, &klpair_len, &klpair);
    if (r == 0) {
        *len = keylen_from_klpair_len(klpair_len);
        *key = klpair->key_le;
    }
    return r;
}

void bn_data::clone(bn_data* orig_bn_data) {
    toku_mempool_clone(&orig_bn_data->m_buffer_mempool, &m_buffer_mempool);
    m_buffer.clone(orig_bn_data->m_buffer);
    this->m_disksize_of_keys = orig_bn_data->m_disksize_of_keys;
}

