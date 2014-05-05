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

#include <string.h>
#include <db.h>

#include <portability/memory.h>
#include <limits.h>

namespace toku {

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::create(void) {
    toku_mempool_zero(&this->mp);
    this->values_same_size = true;
    this->value_length = 0;
    this->is_array = true;
    this->d.a.num_values = 0;
    //TODO: maybe allocate enough space for something by default?
    //      We may be relying on not needing to allocate space the first time (due to limited time spent while a lock is held)
}

/**
 * Note: create_from_sorted_memory_of_fixed_size_elements does not take ownership of 'mem'.
 * Owner is still responsible for freeing it.
 * While in the OMT a similar function would steal ownership, this doesn't make sense for the DMT because
 * we (usually) have to add padding for alignment (mem has all of the elements PACKED).
 * Also all current uses (as of Jan 12, 2014) of this function would require mallocing a new array
 * in order to allow stealing.
 */
template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::create_from_sorted_memory_of_fixed_size_elements(
        const void *mem,
        const uint32_t numvalues,
        const uint32_t mem_length,
        const uint32_t fixed_value_length) {
    this->values_same_size = true;
    this->value_length = fixed_value_length;
    this->is_array = true;
    this->d.a.num_values = numvalues;
    const uint8_t pad_bytes = get_fixed_length_alignment_overhead();
    uint32_t aligned_memsize = mem_length + numvalues * pad_bytes;
    toku_mempool_construct(&this->mp, aligned_memsize);
    if (aligned_memsize > 0) {
        paranoid_invariant(numvalues > 0);
        void *ptr = toku_mempool_malloc(&this->mp, aligned_memsize, 1);
        paranoid_invariant_notnull(ptr);
        uint8_t * const CAST_FROM_VOIDP(dest, ptr);
        const uint8_t * const CAST_FROM_VOIDP(src, mem);
        if (pad_bytes == 0) {
            paranoid_invariant(aligned_memsize == mem_length);
            memcpy(dest, src, aligned_memsize);
        } else {
            // TODO(leif): check what vectorizes best: multiplying like this or adding to offsets
            const uint32_t fixed_len = this->value_length;
            const uint32_t fixed_aligned_len = align(this->value_length);
            paranoid_invariant(this->d.a.num_values*fixed_len == mem_length);
            for (uint32_t i = 0; i < this->d.a.num_values; i++) {
                memcpy(&dest[i*fixed_aligned_len], &src[i*fixed_len], fixed_len);
            }
        }
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::clone(const dmt &src) {
    *this = src;
    toku_mempool_clone(&src.mp, &this->mp);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::clear(void) {
    this->is_array = true;
    this->d.a.num_values = 0;
    this->values_same_size = true;  // Reset state
    this->value_length = 0;
    //TODO(leif): Note that this can mess with our memory_footprint calculation (we may touch past what is marked as 'used' in the mempool)
    //            One 'fix' is for mempool to also track what was touched, and reset() shouldn't reset that, though realloc() might.
    toku_mempool_reset(&this->mp);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::destroy(void) {
    this->clear();
    toku_mempool_destroy(&this->mp);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
uint32_t dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::size(void) const {
    if (this->is_array) {
        return this->d.a.num_values;
    } else {
        return this->nweight(this->d.t.root);
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
uint32_t dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::nweight(const subtree &subtree) const {
    if (subtree.is_null()) {
        return 0;
    } else {
        const dmt_node & node = get_node(subtree);
        return node.weight;
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename dmtcmp_t, int (*h)(const uint32_t size, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::insert(const dmtwriter_t &value, const dmtcmp_t &v, uint32_t *const idx) {
    int r;
    uint32_t insert_idx;

    r = this->find_zero<dmtcmp_t, h>(v, nullptr, nullptr, &insert_idx);
    if (r==0) {
        if (idx) *idx = insert_idx;
        return DB_KEYEXIST;
    }
    if (r != DB_NOTFOUND) return r;

    if ((r = this->insert_at(value, insert_idx))) return r;
    if (idx) *idx = insert_idx;

    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::insert_at(const dmtwriter_t &value, const uint32_t idx) {
    if (idx > this->size()) { return EINVAL; }

    bool same_size = this->values_same_size && (this->size() == 0 || value.get_size() == this->value_length);
    if (this->is_array) {
        if (same_size && idx == this->d.a.num_values) {
            return this->insert_at_array_end<true>(value);
        }
        this->convert_from_array_to_tree();
    }
    // Is a tree.
    paranoid_invariant(!is_array);
    if (!same_size) {
        this->values_same_size = false;
        this->value_length = 0;
    }

    this->maybe_resize_tree(&value);
    subtree *rebalance_subtree = nullptr;
    this->insert_internal(&this->d.t.root, value, idx, &rebalance_subtree);
    if (rebalance_subtree != nullptr) {
        this->rebalance(rebalance_subtree);
    }
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<bool with_resize>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::insert_at_array_end(const dmtwriter_t& value_in) {
    paranoid_invariant(this->is_array);
    paranoid_invariant(this->values_same_size);
    if (this->d.a.num_values == 0) {
        this->value_length = value_in.get_size();
    }
    paranoid_invariant(this->value_length == value_in.get_size());

    if (with_resize) {
        this->maybe_resize_array_for_insert();
    }
    dmtdata_t *dest = this->alloc_array_value_end();
    value_in.write_to(dest);
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
dmtdata_t * dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::alloc_array_value_end(void) {
    paranoid_invariant(this->is_array);
    paranoid_invariant(this->values_same_size);
    this->d.a.num_values++;

    void *ptr = toku_mempool_malloc(&this->mp, align(this->value_length), 1);
    paranoid_invariant_notnull(ptr);
    paranoid_invariant(reinterpret_cast<size_t>(ptr) % ALIGNMENT == 0);
    dmtdata_t *CAST_FROM_VOIDP(n, ptr);
    paranoid_invariant(n == get_array_value(this->d.a.num_values - 1));
    return n;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
dmtdata_t * dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::get_array_value(const uint32_t idx) const {
    paranoid_invariant(this->is_array);
    paranoid_invariant(this->values_same_size);

    paranoid_invariant(idx < this->d.a.num_values);
    return get_array_value_internal(&this->mp, idx);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
dmtdata_t * dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::get_array_value_internal(const struct mempool *mempool, const uint32_t idx) const {
    void* ptr = toku_mempool_get_pointer_from_base_and_offset(mempool, idx * align(this->value_length));
    dmtdata_t *CAST_FROM_VOIDP(value, ptr);
    return value;
}

//TODO(leif) write microbenchmarks to compare growth factor.  Note:  growth factor here is actually 2.5 because of mempool_construct
template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::maybe_resize_array_for_insert(void) {
    bool space_available = toku_mempool_get_free_size(&this->mp) >= align(this->value_length);

    if (!space_available) {
        const uint32_t n = this->d.a.num_values + 1;
        const uint32_t new_n = n <=2 ? 4 : 2*n;
        const uint32_t new_space = align(this->value_length) * new_n;

        struct mempool new_kvspace;
        toku_mempool_construct(&new_kvspace, new_space);
        size_t copy_bytes = this->d.a.num_values * align(this->value_length);
        invariant(copy_bytes + align(this->value_length) <= new_space);
        paranoid_invariant(copy_bytes <= toku_mempool_get_used_size(&this->mp));
        // Copy over to new mempool
        if (this->d.a.num_values > 0) {
            void* dest = toku_mempool_malloc(&new_kvspace, copy_bytes, 1);
            invariant(dest!=nullptr);
            memcpy(dest, get_array_value(0), copy_bytes);
        }
        toku_mempool_destroy(&this->mp);
        this->mp = new_kvspace;
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
uint32_t dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::align(const uint32_t x) const {
    return roundup_to_multiple(ALIGNMENT, x);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::prepare_for_serialize(void) {
    if (!this->is_array) {
        this->convert_from_tree_to_array();
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::convert_from_tree_to_array(void) {
    paranoid_invariant(!this->is_array);
    paranoid_invariant(this->values_same_size);
    
    const uint32_t num_values = this->size();

    node_offset *tmp_array;
    bool malloced = false;
    tmp_array = alloc_temp_node_offsets(num_values);
    if (!tmp_array) {
        malloced = true;
        XMALLOC_N(num_values, tmp_array);
    }
    this->fill_array_with_subtree_offsets(tmp_array, this->d.t.root);

    struct mempool new_mp;
    const uint32_t fixed_len = this->value_length;
    const uint32_t fixed_aligned_len = align(this->value_length);
    size_t mem_needed = num_values * fixed_aligned_len;
    toku_mempool_construct(&new_mp, mem_needed);
    uint8_t* CAST_FROM_VOIDP(dest, toku_mempool_malloc(&new_mp, mem_needed, 1));
    paranoid_invariant_notnull(dest);
    for (uint32_t i = 0; i < num_values; i++) {
        const dmt_node &n = get_node(tmp_array[i]);
        memcpy(&dest[i*fixed_aligned_len], &n.value, fixed_len);
    }
    toku_mempool_destroy(&this->mp);
    this->mp = new_mp;
    this->is_array = true;
    this->d.a.num_values = num_values;

    if (malloced) toku_free(tmp_array);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::convert_from_array_to_tree(void) {
    paranoid_invariant(this->is_array);
    paranoid_invariant(this->values_same_size);
    
    //save array-format information to locals
    const uint32_t num_values = this->d.a.num_values;

    node_offset *tmp_array;
    bool malloced = false;
    tmp_array = alloc_temp_node_offsets(num_values);
    if (!tmp_array) {
        malloced = true;
        XMALLOC_N(num_values, tmp_array);
    }

    struct mempool old_mp = this->mp;
    size_t mem_needed = num_values * align(this->value_length + __builtin_offsetof(dmt_node, value));
    toku_mempool_construct(&this->mp, mem_needed);

    for (uint32_t i = 0; i < num_values; i++) {
        dmtwriter_t writer(this->value_length, get_array_value_internal(&old_mp, i));
        tmp_array[i] = node_malloc_and_set_value(writer);
    }
    this->is_array = false;
    this->rebuild_subtree_from_offsets(&this->d.t.root, tmp_array, num_values);

    if (malloced) toku_free(tmp_array);
    toku_mempool_destroy(&old_mp);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::delete_at(const uint32_t idx) {
    uint32_t n = this->size();
    if (idx >= n) { return EINVAL; }

    if (n == 1) {
        this->clear();  //Emptying out the entire dmt.
        return 0;
    }
    if (this->is_array) {
        this->convert_from_array_to_tree();
    }
    paranoid_invariant(!is_array);

    subtree *rebalance_subtree = nullptr;
    this->delete_internal(&this->d.t.root, idx, nullptr, &rebalance_subtree);
    if (rebalance_subtree != nullptr) {
        this->rebalance(rebalance_subtree);
    }
    this->maybe_resize_tree(nullptr);
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, const dmtdata_t &, const uint32_t, iterate_extra_t *const)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::iterate(iterate_extra_t *const iterate_extra) const {
    return this->iterate_on_range<iterate_extra_t, f>(0, this->size(), iterate_extra);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, const dmtdata_t &, const uint32_t, iterate_extra_t *const)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::iterate_on_range(const uint32_t left, const uint32_t right, iterate_extra_t *const iterate_extra) const {
    if (right > this->size()) { return EINVAL; }
    if (left == right) { return 0; }
    if (this->is_array) {
        return this->iterate_internal_array<iterate_extra_t, f>(left, right, iterate_extra);
    }
    return this->iterate_internal<iterate_extra_t, f>(left, right, this->d.t.root, 0, iterate_extra);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::verify(void) const {
    uint32_t num_values = this->size();
    invariant(num_values < UINT32_MAX);
    size_t pool_used = toku_mempool_get_used_size(&this->mp);
    size_t pool_size = toku_mempool_get_size(&this->mp);
    size_t pool_frag = toku_mempool_get_frag_size(&this->mp);
    invariant(pool_used <= pool_size);
    if (this->is_array) {
        invariant(this->values_same_size);
        invariant(num_values == this->d.a.num_values);

        // We know exactly how much memory should be used.
        invariant(pool_used == num_values * align(this->value_length));

        // Array form must have 0 fragmentation in mempool.
        invariant(pool_frag == 0);
    } else {
        if (this->values_same_size) {
            // We know exactly how much memory should be used.
            invariant(pool_used == num_values * align(this->value_length + __builtin_offsetof(dmt_node, value)));
        } else {
            // We can only do a lower bound on memory usage.
            invariant(pool_used >= num_values * __builtin_offsetof(dmt_node, value));
        }
        std::vector<bool> touched(pool_size, false);
        verify_internal(this->d.t.root, &touched);
        size_t bytes_used = 0;
        for (size_t i = 0; i < pool_size; i++) {
            if (touched.at(i)) {
                ++bytes_used;
            }
        }
        invariant(bytes_used == pool_used);
    }
}

// Verifies all weights are internally consistent.
template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::verify_internal(const subtree &subtree, std::vector<bool> *touched) const {
    if (subtree.is_null()) {
        return;
    }
    const dmt_node &node = get_node(subtree);

    if (this->values_same_size) {
        invariant(node.value_length == this->value_length);
    }

    size_t offset = toku_mempool_get_offset_from_pointer_and_base(&this->mp, &node);
    size_t node_size = align(__builtin_offsetof(dmt_node, value) + node.value_length);
    invariant(offset <= touched->size());
    invariant(offset+node_size <= touched->size());
    invariant(offset % ALIGNMENT == 0);
    // Mark memory as touched and never allocated to multiple nodes.
    for (size_t i = offset; i < offset+node_size; ++i) {
        invariant(!touched->at(i));
        touched->at(i) = true;
    }

    const uint32_t leftweight = this->nweight(node.left);
    const uint32_t rightweight = this->nweight(node.right);

    invariant(leftweight + rightweight + 1 == this->nweight(subtree));
    verify_internal(node.left, touched);
    verify_internal(node.right, touched);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, dmtdata_t *, const uint32_t, iterate_extra_t *const)>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::iterate_ptr(iterate_extra_t *const iterate_extra) {
    if (this->is_array) {
        this->iterate_ptr_internal_array<iterate_extra_t, f>(0, this->size(), iterate_extra);
    } else {
        this->iterate_ptr_internal<iterate_extra_t, f>(0, this->size(), this->d.t.root, 0, iterate_extra);
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::fetch(const uint32_t idx, uint32_t *const value_len, dmtdataout_t *const value) const {
    if (idx >= this->size()) { return EINVAL; }
    if (this->is_array) {
        this->fetch_internal_array(idx, value_len, value);
    } else {
        this->fetch_internal(this->d.t.root, idx, value_len, value);
    }
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::find_zero(const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    uint32_t tmp_index;
    uint32_t *const child_idxp = (idxp != nullptr) ? idxp : &tmp_index;
    int r;
    if (this->is_array) {
        r = this->find_internal_zero_array<dmtcmp_t, h>(extra, value_len, value, child_idxp);
    }
    else {
        r = this->find_internal_zero<dmtcmp_t, h>(this->d.t.root, extra, value_len, value, child_idxp);
    }
    return r;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::find(const dmtcmp_t &extra, int direction, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    uint32_t tmp_index;
    uint32_t *const child_idxp = (idxp != nullptr) ? idxp : &tmp_index;
    paranoid_invariant(direction != 0);
    if (direction < 0) {
        if (this->is_array) {
            return this->find_internal_minus_array<dmtcmp_t, h>(extra, value_len,  value, child_idxp);
        } else {
            return this->find_internal_minus<dmtcmp_t, h>(this->d.t.root, extra, value_len,  value, child_idxp);
        }
    } else {
        if (this->is_array) {
            return this->find_internal_plus_array<dmtcmp_t, h>(extra, value_len,  value, child_idxp);
        } else {
            return this->find_internal_plus<dmtcmp_t, h>(this->d.t.root, extra, value_len, value, child_idxp);
        }
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
size_t dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::memory_size(void) {
    return (sizeof *this) + toku_mempool_get_size(&this->mp);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
dmt_node_templated<dmtdata_t> & dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::get_node(const subtree &subtree) const {
    paranoid_invariant(!subtree.is_null());
    return get_node(subtree.get_offset());
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
dmt_node_templated<dmtdata_t> & dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::get_node(const node_offset offset) const {
    void* ptr = toku_mempool_get_pointer_from_base_and_offset(&this->mp, offset);
    dmt_node *CAST_FROM_VOIDP(node, ptr);
    return *node;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::node_set_value(dmt_node * n, const dmtwriter_t &value) {
    n->value_length = value.get_size();
    value.write_to(&n->value);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
node_offset dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::node_malloc_and_set_value(const dmtwriter_t &value) {
    size_t val_size = value.get_size();
    size_t size_to_alloc = __builtin_offsetof(dmt_node, value) + val_size;
    size_to_alloc = align(size_to_alloc);
    void* np = toku_mempool_malloc(&this->mp, size_to_alloc, 1);
    paranoid_invariant_notnull(np);
    dmt_node *CAST_FROM_VOIDP(n, np);
    node_set_value(n, value);

    return toku_mempool_get_offset_from_pointer_and_base(&this->mp, np);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::node_free(const subtree &st) {
    dmt_node &n = get_node(st);
    size_t size_to_free = __builtin_offsetof(dmt_node, value) + n.value_length;
    size_to_free = align(size_to_free);
    toku_mempool_mfree(&this->mp, &n, size_to_free);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::maybe_resize_tree(const dmtwriter_t * value) {
    const ssize_t curr_capacity = toku_mempool_get_size(&this->mp);
    const ssize_t curr_free = toku_mempool_get_free_size(&this->mp);
    const ssize_t curr_used = toku_mempool_get_used_size(&this->mp);
    ssize_t add_size = 0;
    if (value) {
        add_size = __builtin_offsetof(dmt_node, value) + value->get_size();
        add_size = align(add_size);
    }

    const ssize_t need_size = curr_used + add_size;
    paranoid_invariant(need_size <= UINT32_MAX);
    //TODO(leif) consider different growth rates
    const ssize_t new_size = 2*need_size;
    paranoid_invariant(new_size <= UINT32_MAX);

    if ((curr_capacity / 2 >= new_size) || // Way too much allocated
        (curr_free < add_size)) {  // No room in mempool
        // Copy all memory and reconstruct dmt in new mempool.
        if (curr_free < add_size && toku_mempool_get_frag_size(&this->mp) == 0) {
            // TODO(yoni) or TODO(leif) consider doing this not just when frag size is zero, but also when it is a small percentage of the total mempool size
            // Offsets remain the same in the new mempool so we can just realloc.
            toku_mempool_realloc_larger(&this->mp, new_size);
        } else if (!this->d.t.root.is_null()) {
            struct mempool new_kvspace;
            toku_mempool_construct(&new_kvspace, new_size);

            const dmt_node &n = get_node(this->d.t.root);
            node_offset *tmp_array;
            bool malloced = false;
            tmp_array = alloc_temp_node_offsets(n.weight);
            if (!tmp_array) {
                malloced = true;
                XMALLOC_N(n.weight, tmp_array);
            }
            this->fill_array_with_subtree_offsets(tmp_array, this->d.t.root);
            for (node_offset i = 0; i < n.weight; i++) {
                dmt_node &node = get_node(tmp_array[i]);
                const size_t bytes_to_copy = __builtin_offsetof(dmt_node, value) + node.value_length;
                const size_t bytes_to_alloc = align(bytes_to_copy);
                void* newdata = toku_mempool_malloc(&new_kvspace, bytes_to_alloc, 1);
                memcpy(newdata, &node, bytes_to_copy);
                tmp_array[i] = toku_mempool_get_offset_from_pointer_and_base(&new_kvspace, newdata);
            }

            struct mempool old_kvspace = this->mp;
            this->mp = new_kvspace;
            this->rebuild_subtree_from_offsets(&this->d.t.root, tmp_array, n.weight);
            if (malloced) toku_free(tmp_array);
            toku_mempool_destroy(&old_kvspace);
        } else {
            toku_mempool_destroy(&this->mp);
            toku_mempool_construct(&this->mp, new_size);
        }
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
bool dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::will_need_rebalance(const subtree &subtree, const int leftmod, const int rightmod) const {
    if (subtree.is_null()) { return false; }
    const dmt_node &n = get_node(subtree);
    // one of the 1's is for the root.
    // the other is to take ceil(n/2)
    const uint32_t weight_left  = this->nweight(n.left)  + leftmod;
    const uint32_t weight_right = this->nweight(n.right) + rightmod;
    return ((1+weight_left < (1+1+weight_right)/2)
            ||
            (1+weight_right < (1+1+weight_left)/2));
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::insert_internal(subtree *const subtreep, const dmtwriter_t &value, const uint32_t idx, subtree **const rebalance_subtree) {
    if (subtreep->is_null()) {
        paranoid_invariant_zero(idx);
        const node_offset newoffset = this->node_malloc_and_set_value(value);
        dmt_node &newnode = get_node(newoffset);
        newnode.weight = 1;
        newnode.left.set_to_null();
        newnode.right.set_to_null();
        subtreep->set_offset(newoffset);
    } else {
        dmt_node &n = get_node(*subtreep);
        n.weight++;
        if (idx <= this->nweight(n.left)) {
            if (*rebalance_subtree == nullptr && this->will_need_rebalance(*subtreep, 1, 0)) {
                *rebalance_subtree = subtreep;
            }
            this->insert_internal(&n.left, value, idx, rebalance_subtree);
        } else {
            if (*rebalance_subtree == nullptr && this->will_need_rebalance(*subtreep, 0, 1)) {
                *rebalance_subtree = subtreep;
            }
            const uint32_t sub_index = idx - this->nweight(n.left) - 1;
            this->insert_internal(&n.right, value, sub_index, rebalance_subtree);
        }
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::delete_internal(subtree *const subtreep, const uint32_t idx, subtree *const subtree_replace, subtree **const rebalance_subtree) {
    paranoid_invariant_notnull(subtreep);
    paranoid_invariant_notnull(rebalance_subtree);
    paranoid_invariant(!subtreep->is_null());
    dmt_node &n = get_node(*subtreep);
    const uint32_t leftweight = this->nweight(n.left);
    if (idx < leftweight) {
        n.weight--;
        if (*rebalance_subtree == nullptr && this->will_need_rebalance(*subtreep, -1, 0)) {
            *rebalance_subtree = subtreep;
        }
        this->delete_internal(&n.left, idx, subtree_replace, rebalance_subtree);
    } else if (idx == leftweight) {
        // Found the correct index.
        if (n.left.is_null()) {
            paranoid_invariant_zero(idx);
            // Delete n and let parent point to n.right
            subtree ptr_this = *subtreep;
            *subtreep = n.right;
            subtree to_free;
            if (subtree_replace != nullptr) {
                // Swap self with the other node.  Taking over all responsibility.
                to_free = *subtree_replace;
                dmt_node &ancestor = get_node(*subtree_replace);
                if (*rebalance_subtree == &ancestor.right) {
                    // Take over rebalance responsibility.
                    *rebalance_subtree = &n.right;
                }
                n.weight = ancestor.weight;
                n.left = ancestor.left;
                n.right = ancestor.right;
                *subtree_replace = ptr_this;
            } else {
                to_free = ptr_this;
            }
            this->node_free(to_free);
        } else if (n.right.is_null()) {
            // Delete n and let parent point to n.left
            subtree to_free = *subtreep;
            *subtreep = n.left;
            paranoid_invariant(idx>0);
            paranoid_invariant_null(subtree_replace);  // To be recursive, we're looking for index 0.  n is index > 0 here.
            this->node_free(to_free);
        } else {
            if (*rebalance_subtree == nullptr && this->will_need_rebalance(*subtreep, 0, -1)) {
                *rebalance_subtree = subtreep;
            }
            // don't need to copy up value, it's only used by this
            // next call, and when that gets to the bottom there
            // won't be any more recursion
            n.weight--;
            this->delete_internal(&n.right, 0, subtreep, rebalance_subtree);
        }
    } else {
        n.weight--;
        if (*rebalance_subtree == nullptr && this->will_need_rebalance(*subtreep, 0, -1)) {
            *rebalance_subtree = subtreep;
        }
        this->delete_internal(&n.right, idx - leftweight - 1, subtree_replace, rebalance_subtree);
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, const dmtdata_t &, const uint32_t, iterate_extra_t *const)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::iterate_internal_array(const uint32_t left, const uint32_t right,
                                                         iterate_extra_t *const iterate_extra) const {
    int r;
    for (uint32_t i = left; i < right; ++i) {
        r = f(this->value_length, *get_array_value(i), i, iterate_extra);
        if (r != 0) {
            return r;
        }
    }
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, dmtdata_t *, const uint32_t, iterate_extra_t *const)>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::iterate_ptr_internal(const uint32_t left, const uint32_t right,
                                                        const subtree &subtree, const uint32_t idx,
                                                        iterate_extra_t *const iterate_extra) {
    if (!subtree.is_null()) { 
        dmt_node &n = get_node(subtree);
        const uint32_t idx_root = idx + this->nweight(n.left);
        if (left < idx_root) {
            this->iterate_ptr_internal<iterate_extra_t, f>(left, right, n.left, idx, iterate_extra);
        }
        if (left <= idx_root && idx_root < right) {
            int r = f(n.value_length, &n.value, idx_root, iterate_extra);
            lazy_assert_zero(r);
        }
        if (idx_root + 1 < right) {
            this->iterate_ptr_internal<iterate_extra_t, f>(left, right, n.right, idx_root + 1, iterate_extra);
        }
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, dmtdata_t *, const uint32_t, iterate_extra_t *const)>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::iterate_ptr_internal_array(const uint32_t left, const uint32_t right,
                                                              iterate_extra_t *const iterate_extra) {
    for (uint32_t i = left; i < right; ++i) {
        int r = f(this->value_length, get_array_value(i), i, iterate_extra);
        lazy_assert_zero(r);
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename iterate_extra_t,
         int (*f)(const uint32_t, const dmtdata_t &, const uint32_t, iterate_extra_t *const)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::iterate_internal(const uint32_t left, const uint32_t right,
                                                   const subtree &subtree, const uint32_t idx,
                                                   iterate_extra_t *const iterate_extra) const {
    if (subtree.is_null()) { return 0; }
    int r;
    const dmt_node &n = get_node(subtree);
    const uint32_t idx_root = idx + this->nweight(n.left);
    if (left < idx_root) {
        r = this->iterate_internal<iterate_extra_t, f>(left, right, n.left, idx, iterate_extra);
        if (r != 0) { return r; }
    }
    if (left <= idx_root && idx_root < right) {
        r = f(n.value_length, n.value, idx_root, iterate_extra);
        if (r != 0) { return r; }
    }
    if (idx_root + 1 < right) {
        return this->iterate_internal<iterate_extra_t, f>(left, right, n.right, idx_root + 1, iterate_extra);
    }
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::fetch_internal_array(const uint32_t i, uint32_t *const value_len, dmtdataout_t *const value) const {
    copyout(value_len, value, this->value_length, get_array_value(i));
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::fetch_internal(const subtree &subtree, const uint32_t i, uint32_t *const value_len, dmtdataout_t *const value) const {
    dmt_node &n = get_node(subtree);
    const uint32_t leftweight = this->nweight(n.left);
    if (i < leftweight) {
        this->fetch_internal(n.left, i, value_len, value);
    } else if (i == leftweight) {
        copyout(value_len, value, &n);
    } else {
        this->fetch_internal(n.right, i - leftweight - 1, value_len, value);
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::fill_array_with_subtree_offsets(node_offset *const array, const subtree &subtree) const {
    if (!subtree.is_null()) {
        const dmt_node &tree = get_node(subtree);
        this->fill_array_with_subtree_offsets(&array[0], tree.left);
        array[this->nweight(tree.left)] = subtree.get_offset();
        this->fill_array_with_subtree_offsets(&array[this->nweight(tree.left) + 1], tree.right);
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::rebuild_subtree_from_offsets(subtree *const subtree, const node_offset *const offsets, const uint32_t numvalues) {
    if (numvalues==0) {
        subtree->set_to_null();
    } else {
        uint32_t halfway = numvalues/2;
        subtree->set_offset(offsets[halfway]);
        dmt_node &newnode = get_node(offsets[halfway]);
        newnode.weight = numvalues;
        // value is already in there.
        this->rebuild_subtree_from_offsets(&newnode.left,  &offsets[0], halfway);
        this->rebuild_subtree_from_offsets(&newnode.right, &offsets[halfway+1], numvalues-(halfway+1));
    }
}

//TODO(leif): Note that this can mess with our memory_footprint calculation (we may touch past what is marked as 'used' in the mempool)
template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
node_offset* dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::alloc_temp_node_offsets(uint32_t num_offsets) {
    size_t mem_needed = num_offsets * sizeof(node_offset);
    size_t mem_free;
    mem_free = toku_mempool_get_free_size(&this->mp);
    node_offset* CAST_FROM_VOIDP(tmp, toku_mempool_get_next_free_ptr(&this->mp));
    if (mem_free >= mem_needed) {
        return tmp;
    }
    return nullptr;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::rebalance(subtree *const subtree) {
    paranoid_invariant(!subtree->is_null());

    // There is a possible "optimization" here:
    //   if (this->values_same_size && subtree == &this->d.t.root) {
    //       this->convert_from_tree_to_array();
    //       return;
    //   }
    // but we don't want to do it because it involves actually copying values around
    // as opposed to stopping in the middle of rebalancing (like in the OMT)

    node_offset offset = subtree->get_offset();
    const dmt_node &n = get_node(offset);
    node_offset *tmp_array;
    bool malloced = false;
    tmp_array = alloc_temp_node_offsets(n.weight);
    if (!tmp_array) {
        malloced = true;
        XMALLOC_N(n.weight, tmp_array);
    }
    this->fill_array_with_subtree_offsets(tmp_array, *subtree);
    this->rebuild_subtree_from_offsets(subtree, tmp_array, n.weight);
    if (malloced) toku_free(tmp_array);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::copyout(uint32_t *const outlen, dmtdata_t *const out, const dmt_node *const n) {
    if (outlen) {
        *outlen = n->value_length;
    }
    if (out) {
        *out = n->value;
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::copyout(uint32_t *const outlen, dmtdata_t **const out, dmt_node *const n) {
    if (outlen) {
        *outlen = n->value_length;
    }
    if (out) {
        *out = &n->value;
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::copyout(uint32_t *const outlen, dmtdata_t *const out, const uint32_t len, const dmtdata_t *const stored_value_ptr) {
    if (outlen) {
        *outlen = len;
    }
    if (out) {
        *out = *stored_value_ptr;
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::copyout(uint32_t *const outlen, dmtdata_t **const out, const uint32_t len, dmtdata_t *const stored_value_ptr) {
    if (outlen) {
        *outlen = len;
    }
    if (out) {
        *out = stored_value_ptr;
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::find_internal_zero_array(const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    paranoid_invariant_notnull(idxp);
    uint32_t min = 0;
    uint32_t limit = this->d.a.num_values;
    uint32_t best_pos = subtree::NODE_NULL;
    uint32_t best_zero = subtree::NODE_NULL;

    while (min!=limit) {
        uint32_t mid = (min + limit) / 2;
        int hv = h(this->value_length, *get_array_value(mid), extra);
        if (hv<0) {
            min = mid+1;
        }
        else if (hv>0) {
            best_pos  = mid;
            limit     = mid;
        }
        else {
            best_zero = mid;
            limit     = mid;
        }
    }
    if (best_zero!=subtree::NODE_NULL) {
        //Found a zero
        copyout(value_len, value, this->value_length, get_array_value(best_zero));
        *idxp = best_zero;
        return 0;
    }
    if (best_pos!=subtree::NODE_NULL) *idxp = best_pos;
    else                     *idxp = this->d.a.num_values;
    return DB_NOTFOUND;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::find_internal_zero(const subtree &subtree, const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    paranoid_invariant_notnull(idxp);
    if (subtree.is_null()) {
        *idxp = 0;
        return DB_NOTFOUND;
    }
    dmt_node &n = get_node(subtree);
    int hv = h(n.value_length, n.value, extra);
    if (hv<0) {
        int r = this->find_internal_zero<dmtcmp_t, h>(n.right, extra, value_len, value, idxp);
        *idxp += this->nweight(n.left)+1;
        return r;
    } else if (hv>0) {
        return this->find_internal_zero<dmtcmp_t, h>(n.left, extra, value_len, value, idxp);
    } else {
        int r = this->find_internal_zero<dmtcmp_t, h>(n.left, extra, value_len, value, idxp);
        if (r==DB_NOTFOUND) {
            *idxp = this->nweight(n.left);
            copyout(value_len, value, &n);
            r = 0;
        }
        return r;
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::find_internal_plus_array(const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    paranoid_invariant_notnull(idxp);
    uint32_t min = 0;
    uint32_t limit = this->d.a.num_values;
    uint32_t best = subtree::NODE_NULL;

    while (min != limit) {
        const uint32_t mid = (min + limit) / 2;
        const int hv = h(this->value_length, *get_array_value(mid), extra);
        if (hv > 0) {
            best = mid;
            limit = mid;
        } else {
            min = mid + 1;
        }
    }
    if (best == subtree::NODE_NULL) { return DB_NOTFOUND; }
    copyout(value_len, value, this->value_length, get_array_value(best));
    *idxp = best;
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::find_internal_plus(const subtree &subtree, const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    paranoid_invariant_notnull(idxp);
    if (subtree.is_null()) {
        return DB_NOTFOUND;
    }
    dmt_node & n = get_node(subtree);
    int hv = h(n.value_length, n.value, extra);
    int r;
    if (hv > 0) {
        r = this->find_internal_plus<dmtcmp_t, h>(n.left, extra, value_len, value, idxp);
        if (r == DB_NOTFOUND) {
            *idxp = this->nweight(n.left);
            copyout(value_len, value, &n);
            r = 0;
        }
    } else {
        r = this->find_internal_plus<dmtcmp_t, h>(n.right, extra, value_len, value, idxp);
        if (r == 0) {
            *idxp += this->nweight(n.left) + 1;
        }
    }
    return r;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::find_internal_minus_array(const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    paranoid_invariant_notnull(idxp);
    uint32_t min = 0;
    uint32_t limit = this->d.a.num_values;
    uint32_t best = subtree::NODE_NULL;

    while (min != limit) {
        const uint32_t mid = (min + limit) / 2;
        const int hv = h(this->value_length, *get_array_value(mid), extra);
        if (hv < 0) {
            best = mid;
            min = mid + 1;
        } else {
            limit = mid;
        }
    }
    if (best == subtree::NODE_NULL) { return DB_NOTFOUND; }
    copyout(value_len, value, this->value_length, get_array_value(best));
    *idxp = best;
    return 0;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
template<typename dmtcmp_t,
         int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
int dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::find_internal_minus(const subtree &subtree, const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const {
    paranoid_invariant_notnull(idxp);
    if (subtree.is_null()) {
        return DB_NOTFOUND;
    }
    dmt_node & n = get_node(subtree);
    int hv = h(n.value_length, n.value, extra);
    if (hv < 0) {
        int r = this->find_internal_minus<dmtcmp_t, h>(n.right, extra, value_len, value, idxp);
        if (r == 0) {
            *idxp += this->nweight(n.left) + 1;
        } else if (r == DB_NOTFOUND) {
            *idxp = this->nweight(n.left);
            copyout(value_len, value, &n);
            r = 0;
        }
        return r;
    } else {
        return this->find_internal_minus<dmtcmp_t, h>(n.left, extra, value_len, value, idxp);
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
uint32_t dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::get_fixed_length(void) const {
    return this->values_same_size ? this->value_length : 0;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
uint32_t dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::get_fixed_length_alignment_overhead(void) const {
    return this->values_same_size ? align(this->value_length) - this->value_length : 0;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
bool dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::value_length_is_fixed(void) const {
    return this->values_same_size;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::serialize_values(uint32_t expected_unpadded_memory, struct wbuf *wb) const {
    invariant(this->is_array);
    invariant(this->values_same_size);
    const uint8_t pad_bytes = get_fixed_length_alignment_overhead();
    const uint32_t fixed_len = this->value_length;
    const uint32_t fixed_aligned_len = align(this->value_length);
    paranoid_invariant(expected_unpadded_memory == this->d.a.num_values * this->value_length);
    paranoid_invariant(toku_mempool_get_used_size(&this->mp) >=
                       expected_unpadded_memory + pad_bytes * this->d.a.num_values);
    if (this->d.a.num_values == 0) {
        // Nothing to serialize
    } else if (pad_bytes == 0) {
        // Basically a memcpy
        wbuf_nocrc_literal_bytes(wb, get_array_value(0), expected_unpadded_memory);
    } else {
        uint8_t* const dest = wbuf_nocrc_reserve_literal_bytes(wb, expected_unpadded_memory);
        const uint8_t* const src = reinterpret_cast<uint8_t*>(get_array_value(0));
        //TODO(leif) maybe look at vectorization here
        for (uint32_t i = 0; i < this->d.a.num_values; i++) {
            memcpy(&dest[i*fixed_len], &src[i*fixed_aligned_len], fixed_len);
        }
    }
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::builder::create(uint32_t _max_values, uint32_t _max_value_bytes) {
    this->max_values = _max_values;
    this->max_value_bytes = _max_value_bytes;
    this->temp.create();
    paranoid_invariant_null(toku_mempool_get_base(&this->temp.mp));
    this->temp_valid = true;
    this->sorted_node_offsets = nullptr;
    // Include enough space for alignment padding
    size_t initial_space = (ALIGNMENT - 1) * _max_values + _max_value_bytes;

    toku_mempool_construct(&this->temp.mp, initial_space);  // Adds 25%
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::builder::append(const dmtwriter_t &value) {
    paranoid_invariant(this->temp_valid);
    //NOTE: Always use d.a.num_values for size because we have not yet created root.
    if (this->temp.values_same_size && (this->temp.d.a.num_values == 0 || value.get_size() == this->temp.value_length)) {
        temp.insert_at_array_end<false>(value);
        return;
    }
    if (this->temp.is_array) {
        // Convert to tree format (without weights and linkage)
        XMALLOC_N(this->max_values, this->sorted_node_offsets);

        // Include enough space for alignment padding
        size_t mem_needed = (ALIGNMENT - 1 + __builtin_offsetof(dmt_node, value)) * max_values + max_value_bytes;
        struct mempool old_mp = this->temp.mp;

        const uint32_t num_values = this->temp.d.a.num_values;
        toku_mempool_construct(&this->temp.mp, mem_needed);

        // Copy over and get node_offsets
        for (uint32_t i = 0; i < num_values; i++) {
            dmtwriter_t writer(this->temp.value_length, this->temp.get_array_value_internal(&old_mp, i));
            this->sorted_node_offsets[i] = this->temp.node_malloc_and_set_value(writer);
        }
        this->temp.is_array = false;
        this->temp.values_same_size = false;
        this->temp.value_length = 0;
        toku_mempool_destroy(&old_mp);
    }
    paranoid_invariant(!this->temp.is_array);
    this->sorted_node_offsets[this->temp.d.a.num_values++] = this->temp.node_malloc_and_set_value(value);
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
bool dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::builder::value_length_is_fixed(void) {
    paranoid_invariant(this->temp_valid);
    return this->temp.values_same_size;
}

template<typename dmtdata_t, typename dmtdataout_t, typename dmtwriter_t>
void dmt<dmtdata_t, dmtdataout_t, dmtwriter_t>::builder::build(dmt<dmtdata_t, dmtdataout_t, dmtwriter_t> *dest) {
    invariant(this->temp_valid);
    //NOTE: Always use d.a.num_values for size because we have not yet created root.
    invariant(this->temp.d.a.num_values <= this->max_values);
    // Memory invariant is taken care of incrementally (during append())

    if (!this->temp.is_array) {
        invariant_notnull(this->sorted_node_offsets);
        this->temp.rebuild_subtree_from_offsets(&this->temp.d.t.root, this->sorted_node_offsets, this->temp.d.a.num_values);
        toku_free(this->sorted_node_offsets);
        this->sorted_node_offsets = nullptr;
    }
    paranoid_invariant_null(this->sorted_node_offsets);

    const size_t used = toku_mempool_get_used_size(&this->temp.mp);
    const size_t allocated = toku_mempool_get_size(&this->temp.mp);
    // We want to use no more than (about) the actual used space + 25% overhead for mempool growth.
    // When we know the elements are fixed-length, we use the better dmt constructor.
    // In practice, as of Jan 2014, we use the builder in two cases:
    //  - When we know the elements are not fixed-length.
    //  - During upgrade of a pre version 26 basement node.
    // During upgrade, we will probably wildly overallocate because we don't account for the values that aren't stored in the dmt, so here we want to shrink the mempool.
    // When we know the elements are not fixed-length, we still know how much memory they occupy in total, modulo alignment, so we want to allow for mempool overhead and worst-case alignment overhead, and not shrink the mempool.
    const size_t max_allowed = used + (ALIGNMENT-1) * this->temp.size();
    const size_t max_allowed_with_mempool_overhead = max_allowed + max_allowed / 4;
    //TODO(leif): get footprint calculation correct (under jemalloc) and add some form of footprint constraint
    if (allocated > max_allowed_with_mempool_overhead) {
        // Reallocate smaller mempool to save memory
        invariant_zero(toku_mempool_get_frag_size(&this->temp.mp));
        struct mempool new_mp;
        toku_mempool_construct(&new_mp, used);
        void * newbase = toku_mempool_malloc(&new_mp, used, 1);
        invariant_notnull(newbase);
        memcpy(newbase, toku_mempool_get_base(&this->temp.mp), used);
        toku_mempool_destroy(&this->temp.mp);
        this->temp.mp = new_mp;
    }

    *dest = this->temp;
    this->temp_valid = false;

}
} // namespace toku
