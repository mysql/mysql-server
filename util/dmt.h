/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#pragma once

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

#include <stdint.h>
#include <memory.h>
#include <toku_portability.h>
#include <toku_race_tools.h>
#include "growable_array.h"
#include "../ft/wbuf.h"
#include <vector>

namespace toku {
typedef uint32_t node_offset;


/**
 * Order Maintenance Tree (DMT)
 *
 * Maintains a collection of totally ordered values, where each value has an integer weight.
 * The DMT is a mutable datatype.
 *
 * The Abstraction:
 *
 * An DMT is a vector of values, $V$, where $|V|$ is the length of the vector.
 * The vector is numbered from $0$ to $|V|-1$.
 * Each value has a weight.  The weight of the $i$th element is denoted $w(V_i)$.
 *
 * We can create a new DMT, which is the empty vector.
 *
 * We can insert a new element $x$ into slot $i$, changing $V$ into $V'$ where
 *  $|V'|=1+|V|$       and
 *
 *   V'_j = V_j       if $j<i$
 *          x         if $j=i$
 *          V_{j-1}   if $j>i$.
 *
 * We can specify $i$ using a kind of function instead of as an integer.
 * Let $b$ be a function mapping from values to nonzero integers, such that
 * the signum of $b$ is monotically increasing.
 * We can specify $i$ as the minimum integer such that $b(V_i)>0$.
 *
 * We look up a value using its index, or using a Heaviside function.
 * For lookups, we allow $b$ to be zero for some values, and again the signum of $b$ must be monotonically increasing.
 * When lookup up values, we can look up
 *  $V_i$ where $i$ is the minimum integer such that $b(V_i)=0$.   (With a special return code if no such value exists.)
 *      (Rationale:  Ordinarily we want $i$ to be unique.  But for various reasons we want to allow multiple zeros, and we want the smallest $i$ in that case.)
 *  $V_i$ where $i$ is the minimum integer such that $b(V_i)>0$.   (Or an indication that no such value exists.)
 *  $V_i$ where $i$ is the maximum integer such that $b(V_i)<0$.   (Or an indication that no such value exists.)
 *
 * When looking up a value using a Heaviside function, we get the value and its index.
 *
 * We can also split an DMT into two DMTs, splitting the weight of the values evenly.
 * Find a value $j$ such that the values to the left of $j$ have about the same total weight as the values to the right of $j$.
 * The resulting two DMTs contain the values to the left of $j$ and the values to the right of $j$ respectively.
 * All of the values from the original DMT go into one of the new DMTs.
 * If the weights of the values don't split exactly evenly, then the implementation has the freedom to choose whether
 *  the new left DMT or the new right DMT is larger.
 *
 * Performance:
 *  Insertion and deletion should run with $O(\log |V|)$ time and $O(\log |V|)$ calls to the Heaviside function.
 *  The memory required is O(|V|).
 *
 * Usage:
 *  The dmt is templated by two parameters:
 *   - dmtdata_t is what will be stored within the dmt.  These could be pointers or real data types (ints, structs).
 *   - dmtdataout_t is what will be returned by find and related functions.  By default, it is the same as dmtdata_t, but you can set it to (dmtdata_t *).
 *  To create an dmt which will store "TXNID"s, for example, it is a good idea to typedef the template:
 *   typedef dmt<TXNID> txnid_dmt_t;
 *  If you are storing structs, you may want to be able to get a pointer to the data actually stored in the dmt (see find_zero).  To do this, use the second template parameter:
 *   typedef dmt<struct foo, struct foo *> foo_dmt_t;
 */

namespace dmt_internal {

class subtree {
private:
    uint32_t m_index;
public:
    static const uint32_t NODE_NULL = UINT32_MAX;
    inline void set_to_null(void) {
        m_index = NODE_NULL;
    }

    inline bool is_null(void) const {
        return NODE_NULL == this->get_offset();
    }

    inline node_offset get_offset(void) const {
        return m_index;
    }

    inline void set_offset(node_offset index) {
        paranoid_invariant(index != NODE_NULL);
        m_index = index;
    }
} __attribute__((__packed__,aligned(4)));

template<typename dmtdata_t>
class dmt_node_templated {
public:
    uint32_t weight;
    subtree left;
    subtree right;
    uint32_t value_length;
    dmtdata_t value;
} __attribute__((__packed__,aligned(4)));

}

using namespace toku::dmt_internal;

// Each data type used in a dmt requires a dmt_functor (allows you to insert/etc with dynamic sized types).
// There is no default implementation.
template<typename dmtdata_t>
class dmt_functor {
    // Ensures that if you forget to use partial specialization this compile error will remind you to use it.
    // We would use static_assert(false, ...) here except that it would cause a compile error even if dmt_functor<>
    // We instead use an expression that evaluates to false that the compiler won't evaluate unless dmt_functor<> is used.
    static_assert(!std::is_same<dmtdata_t, dmtdata_t>::value, "Cannot use default dmt_functor<>. Use partial specialization.");
    // Defines the interface:
    static size_t get_dmtdata_t_size(const dmtdata_t &) { return 0; }
    size_t get_dmtdatain_t_size(void) { return 0; }
    void write_dmtdata_t_to(dmtdata_t *const dest) {}
};

template<typename dmtdata_t,
         typename dmtdataout_t=dmtdata_t
        >
class dmt {
private:
    typedef dmt_node_templated<dmtdata_t> dmt_node;
    typedef dmt_functor<dmtdata_t> dmtdatain_t;

public:
    static const uint8_t ALIGNMENT = 4;

    class builder {
    public:
        void append(const dmtdatain_t &value);
        void create(uint32_t n_values, uint32_t n_value_bytes);
        bool value_length_is_fixed(void);
        void build(dmt<dmtdata_t, dmtdataout_t> *dest);
    private:
        uint32_t max_values;
        uint32_t max_value_bytes;
        node_offset *sorted_node_offsets;
        bool temp_valid;
        dmt<dmtdata_t, dmtdataout_t> temp;
    };

    /**
     * Effect: Create an empty DMT.
     * Performance: constant time.
     */
    void create(void);

    /**
     * Effect: Create a DMT containing values.  The number of values is in numvalues.
     * Requires: this has not been created yet
     * Rationale:    Normally to insert N values takes O(N lg N) amortized time.
     *               If the N values are known in advance, are sorted, and
     *               the structure is empty, we can batch insert them much faster.
     */
    __attribute__((nonnull))
    void create_from_sorted_memory_of_fixed_size_elements(
            const void *mem,
            const uint32_t numvalues,
            const uint32_t mem_length,
            const uint32_t fixed_value_length);

    /**
     * Effect: Creates a copy of an dmt.
     *  Creates this as the clone.
     *  Each element is copied directly.  If they are pointers, the underlying data is not duplicated.
     * Performance: O(n) or the running time of fill_array_with_subtree_values()
     */
    void clone(const dmt &src);

    /**
     * Effect: Set the tree to be empty.
     *  Note: Will not reallocate or resize any memory.
     * Performance: time=O(1)
     */
    void clear(void);

    /**
     * Effect:  Destroy an DMT, freeing all its memory.
     *   If the values being stored are pointers, their underlying data is not freed.  See free_items()
     *   Those values may be freed before or after calling toku_dmt_destroy.
     * Rationale: Returns no values since free() cannot fail.
     * Rationale: Does not free the underlying pointers to reduce complexity.
     * Performance:  time=O(1)
     */
    void destroy(void);

    /**
     * Effect: return |this|.
     * Performance:  time=O(1)
     */
    uint32_t size(void) const;

    void serialize_values(uint32_t expected_unpadded_memory, struct wbuf *wb) const;

    /**
     * Effect:  Insert value into the DMT.
     *   If there is some i such that $h(V_i, v)=0$ then returns DB_KEYEXIST.
     *   Otherwise, let i be the minimum value such that $h(V_i, v)>0$.
     *      If no such i exists, then let i be |V|
     *   Then this has the same effect as
     *    insert_at(tree, value, i);
     *   If idx!=NULL then i is stored in *idx
     * Requires:  The signum of h must be monotonically increasing.
     * Returns:
     *    0            success
     *    DB_KEYEXIST  the key is present (h was equal to zero for some value)
     * On nonzero return, dmt is unchanged.
     * Performance: time=O(\log N) amortized.
     * Rationale: Some future implementation may be O(\log N) worst-case time, but O(\log N) amortized is good enough for now.
     */
    template<typename dmtcmp_t, int (*h)(const uint32_t size, const dmtdata_t &, const dmtcmp_t &)>
    int insert(const dmtdatain_t &value, const dmtcmp_t &v, uint32_t *const idx);

    /**
     * Effect: Increases indexes of all items at slot >= idx by 1.
     *         Insert value into the position at idx.
     * Returns:
     *   0         success
     *   EINVAL    if idx > this->size()
     * On error, dmt is unchanged.
     * Performance: time=O(\log N) amortized time.
     * Rationale: Some future implementation may be O(\log N) worst-case time, but O(\log N) amortized is good enough for now.
     */
    int insert_at(const dmtdatain_t &value, const uint32_t idx);

    /**
     * Effect: Delete the item in slot idx.
     *         Decreases indexes of all items at slot > idx by 1.
     * Returns
     *     0            success
     *     EINVAL       if idx>=this->size()
     * On error, dmt is unchanged.
     * Rationale: To delete an item, first find its index using find or find_zero, then delete it.
     * Performance: time=O(\log N) amortized.
     */
    int delete_at(const uint32_t idx);

    /**
     * Effect:  Iterate over the values of the dmt, from left to right, calling f on each value.
     *  The first argument passed to f is a ref-to-const of the value stored in the dmt.
     *  The second argument passed to f is the index of the value.
     *  The third argument passed to f is iterate_extra.
     *  The indices run from 0 (inclusive) to this->size() (exclusive).
     * Requires: f != NULL
     * Returns:
     *  If f ever returns nonzero, then the iteration stops, and the value returned by f is returned by iterate.
     *  If f always returns zero, then iterate returns 0.
     * Requires:  Don't modify the dmt while running.  (E.g., f may not insert or delete values from the dmt.)
     * Performance: time=O(i+\log N) where i is the number of times f is called, and N is the number of elements in the dmt.
     * Rationale: Although the functional iterator requires defining another function (as opposed to C++ style iterator), it is much easier to read.
     * Rationale: We may at some point use functors, but for now this is a smaller change from the old DMT.
     */
    template<typename iterate_extra_t,
             int (*f)(const uint32_t, const dmtdata_t &, const uint32_t, iterate_extra_t *const)>
    int iterate(iterate_extra_t *const iterate_extra) const;

    /**
     * Effect:  Iterate over the values of the dmt, from left to right, calling f on each value.
     *  The first argument passed to f is a ref-to-const of the value stored in the dmt.
     *  The second argument passed to f is the index of the value.
     *  The third argument passed to f is iterate_extra.
     *  The indices run from 0 (inclusive) to this->size() (exclusive).
     *  We will iterate only over [left,right)
     *
     * Requires: left <= right
     * Requires: f != NULL
     * Returns:
     *  EINVAL  if right > this->size()
     *  If f ever returns nonzero, then the iteration stops, and the value returned by f is returned by iterate_on_range.
     *  If f always returns zero, then iterate_on_range returns 0.
     * Requires:  Don't modify the dmt while running.  (E.g., f may not insert or delete values from the dmt.)
     * Performance: time=O(i+\log N) where i is the number of times f is called, and N is the number of elements in the dmt.
     * Rational: Although the functional iterator requires defining another function (as opposed to C++ style iterator), it is much easier to read.
     */
    template<typename iterate_extra_t,
             int (*f)(const uint32_t, const dmtdata_t &, const uint32_t, iterate_extra_t *const)>
    int iterate_on_range(const uint32_t left, const uint32_t right, iterate_extra_t *const iterate_extra) const;

    // Attempt to verify this dmt is well formed.  (Crashes/asserts/aborts if not well formed)
    void verify(void) const;

    /**
     * Effect:  Iterate over the values of the dmt, from left to right, calling f on each value.
     *  The first argument passed to f is a pointer to the value stored in the dmt.
     *  The second argument passed to f is the index of the value.
     *  The third argument passed to f is iterate_extra.
     *  The indices run from 0 (inclusive) to this->size() (exclusive).
     * Requires: same as for iterate()
     * Returns: same as for iterate()
     * Performance: same as for iterate()
     * Rationale: In general, most iterators should use iterate() since they should not modify the data stored in the dmt.  This function is for iterators which need to modify values (for example, free_items).
     * Rationale: We assume if you are transforming the data in place, you want to do it to everything at once, so there is not yet an iterate_on_range_ptr (but there could be).
     */
    template<typename iterate_extra_t,
             int (*f)(const uint32_t, dmtdata_t *, const uint32_t, iterate_extra_t *const)>
    void iterate_ptr(iterate_extra_t *const iterate_extra);

    /**
     * Effect: Set *value=V_idx
     * Returns
     *    0             success
     *    EINVAL        if index>=toku_dmt_size(dmt)
     * On nonzero return, *value is unchanged
     * Performance: time=O(\log N)
     */
    int fetch(const uint32_t idx, uint32_t *const value_size, dmtdataout_t *const value) const;

    /**
     * Effect:  Find the smallest i such that h(V_i, extra)>=0
     *  If there is such an i and h(V_i,extra)==0 then set *idxp=i, set *value = V_i, and return 0.
     *  If there is such an i and h(V_i,extra)>0  then set *idxp=i and return DB_NOTFOUND.
     *  If there is no such i then set *idx=this->size() and return DB_NOTFOUND.
     * Note: value is of type dmtdataout_t, which may be of type (dmtdata_t) or (dmtdata_t *) but is fixed by the instantiation.
     *  If it is the value type, then the value is copied out (even if the value type is a pointer to something else)
     *  If it is the pointer type, then *value is set to a pointer to the data within the dmt.
     *  This is determined by the type of the dmt as initially declared.
     *   If the dmt is declared as dmt<foo_t>, then foo_t's will be stored and foo_t's will be returned by find and related functions.
     *   If the dmt is declared as dmt<foo_t, foo_t *>, then foo_t's will be stored, and pointers to the stored items will be returned by find and related functions.
     * Rationale:
     *  Structs too small for malloc should be stored directly in the dmt.
     *  These structs may need to be edited as they exist inside the dmt, so we need a way to get a pointer within the dmt.
     *  Using separate functions for returning pointers and values increases code duplication and reduces type-checking.
     *  That also reduces the ability of the creator of a data structure to give advice to its future users.
     *  Slight overloading in this case seemed to provide a better API and better type checking.
     */
    template<typename dmtcmp_t,
             int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
    int find_zero(const dmtcmp_t &extra, uint32_t *const value_size, dmtdataout_t *const value, uint32_t *const idxp) const;

    /**
     *   Effect:
     *    If direction >0 then find the smallest i such that h(V_i,extra)>0.
     *    If direction <0 then find the largest  i such that h(V_i,extra)<0.
     *    (Direction may not be equal to zero.)
     *    If value!=NULL then store V_i in *value
     *    If idxp!=NULL then store i in *idxp.
     *   Requires: The signum of h is monotically increasing.
     *   Returns
     *      0             success
     *      DB_NOTFOUND   no such value is found.
     *   On nonzero return, *value and *idxp are unchanged
     *   Performance: time=O(\log N)
     *   Rationale:
     *     Here's how to use the find function to find various things
     *       Cases for find:
     *        find first value:         ( h(v)=+1, direction=+1 )
     *        find last value           ( h(v)=-1, direction=-1 )
     *        find first X              ( h(v)=(v< x) ? -1 : 1    direction=+1 )
     *        find last X               ( h(v)=(v<=x) ? -1 : 1    direction=-1 )
     *        find X or successor to X  ( same as find first X. )
     *
     *   Rationale: To help understand heaviside functions and behavor of find:
     *    There are 7 kinds of heaviside functions.
     *    The signus of the h must be monotonically increasing.
     *    Given a function of the following form, A is the element
     *    returned for direction>0, B is the element returned
     *    for direction<0, C is the element returned for
     *    direction==0 (see find_zero) (with a return of 0), and D is the element
     *    returned for direction==0 (see find_zero) with a return of DB_NOTFOUND.
     *    If any of A, B, or C are not found, then asking for the
     *    associated direction will return DB_NOTFOUND.
     *    See find_zero for more information.
     *
     *    Let the following represent the signus of the heaviside function.
     *
     *    -...-
     *        A
     *         D
     *
     *    +...+
     *    B
     *    D
     *
     *    0...0
     *    C
     *
     *    -...-0...0
     *        AC
     *
     *    0...0+...+
     *    C    B
     *
     *    -...-+...+
     *        AB
     *         D
     *
     *    -...-0...0+...+
     *        AC    B
     */
    template<typename dmtcmp_t,
             int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
    int find(const dmtcmp_t &extra, int direction, uint32_t *const value_size, dmtdataout_t *const value, uint32_t *const idxp) const;

    /**
     * Effect: Return the size (in bytes) of the dmt, as it resides in main memory.  If the data stored are pointers, don't include the size of what they all point to.
     */
    size_t memory_size(void);

    bool value_length_is_fixed(void) const;

    uint32_t get_fixed_length(void) const;

    void prepare_for_serialize(void);

private:
    // Do a bit of verification that subtree and nodes act like packed c structs and do not introduce unnecessary padding for alignment.
    ENSURE_POD(subtree);
    static_assert(ALIGNMENT > 0, "ALIGNMENT <= 0");
    static_assert((ALIGNMENT & (ALIGNMENT - 1)) == 0, "ALIGNMENT not a power of 2");
    static_assert(sizeof(dmt_node) - sizeof(dmtdata_t) == __builtin_offsetof(dmt_node, value), "value is not last field in node");
    static_assert(4 * sizeof(uint32_t) == __builtin_offsetof(dmt_node, value), "dmt_node is padded");
    static_assert(__builtin_offsetof(dmt_node, value) % ALIGNMENT == 0, "dmt_node requires padding for alignment");
    ENSURE_POD(dmt_node);

    struct dmt_array {
        uint32_t num_values;
    };

    struct dmt_tree {
        subtree root;
    };

    // TODO(yoni): comment relationship between values_same_size, num values, value_length, note about tree still keeping track (until lost once)
    bool values_same_size;
    uint32_t value_length;
    struct mempool mp;
    bool is_array;
    union {
        struct dmt_array a;
        struct dmt_tree t;
    } d;

    uint32_t get_fixed_length_alignment_overhead(void) const;

    void verify_internal(const subtree &subtree, std::vector<bool> *touched) const;

    dmt_node & get_node(const subtree &subtree) const;

    dmt_node & get_node(const node_offset offset) const;

    uint32_t nweight(const subtree &subtree) const;

    node_offset node_malloc_and_set_value(const dmtdatain_t &value);

    void node_set_value(dmt_node *n, const dmtdatain_t &value);

    void node_free(const subtree &st);

    void maybe_resize_array_for_insert(void);

    void convert_to_tree(void);

    void maybe_resize_tree(const dmtdatain_t * value);

    bool will_need_rebalance(const subtree &subtree, const int leftmod, const int rightmod) const;

    __attribute__((nonnull))
    void insert_internal(subtree *const subtreep, const dmtdatain_t &value, const uint32_t idx, subtree **const rebalance_subtree);

    template<bool with_resize>
    int insert_at_array_end(const dmtdatain_t& value_in);

    dmtdata_t * alloc_array_value_end(void);

    dmtdata_t * get_array_value(const uint32_t idx) const;

    dmtdata_t * get_array_value_internal(const struct mempool *mempool, const uint32_t idx) const;

    void convert_from_array_to_tree(void);

    void convert_from_tree_to_array(void);

    __attribute__((nonnull(2,5)))
    void delete_internal(subtree *const subtreep, const uint32_t idx, subtree *const subtree_replace, subtree **const rebalance_subtree);

    template<typename iterate_extra_t,
             int (*f)(const uint32_t, const dmtdata_t &, const uint32_t, iterate_extra_t *const)>
    int iterate_internal_array(const uint32_t left, const uint32_t right,
                                      iterate_extra_t *const iterate_extra) const;

    template<typename iterate_extra_t,
             int (*f)(const uint32_t, dmtdata_t *, const uint32_t, iterate_extra_t *const)>
    void iterate_ptr_internal(const uint32_t left, const uint32_t right,
                                     const subtree &subtree, const uint32_t idx,
                                     iterate_extra_t *const iterate_extra);

    template<typename iterate_extra_t,
             int (*f)(const uint32_t, dmtdata_t *, const uint32_t, iterate_extra_t *const)>
    void iterate_ptr_internal_array(const uint32_t left, const uint32_t right,
                                           iterate_extra_t *const iterate_extra);

    template<typename iterate_extra_t,
             int (*f)(const uint32_t, const dmtdata_t &, const uint32_t, iterate_extra_t *const)>
    int iterate_internal(const uint32_t left, const uint32_t right,
                                const subtree &subtree, const uint32_t idx,
                                iterate_extra_t *const iterate_extra) const;

    void fetch_internal_array(const uint32_t i, uint32_t *const value_len, dmtdataout_t *const value) const;

    void fetch_internal(const subtree &subtree, const uint32_t i, uint32_t *const value_len, dmtdataout_t *const value) const;

    __attribute__((nonnull))
    void fill_array_with_subtree_offsets(node_offset *const array, const subtree &subtree) const;

    __attribute__((nonnull))
    void rebuild_subtree_from_offsets(subtree *const subtree, const node_offset *const offsets, const uint32_t numvalues);

    __attribute__((nonnull))
    void rebalance(subtree *const subtree);

    __attribute__((nonnull))
    static void copyout(uint32_t *const outlen, dmtdata_t *const out, const dmt_node *const n);

    __attribute__((nonnull))
    static void copyout(uint32_t *const outlen, dmtdata_t **const out, dmt_node *const n);

    __attribute__((nonnull))
    static void copyout(uint32_t *const outlen, dmtdata_t *const out, const uint32_t len, const dmtdata_t *const stored_value_ptr);

    __attribute__((nonnull))
    static void copyout(uint32_t *const outlen, dmtdata_t **const out, const uint32_t len, dmtdata_t *const stored_value_ptr);

    template<typename dmtcmp_t,
             int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
    int find_internal_zero_array(const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const;

    template<typename dmtcmp_t,
             int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
    int find_internal_zero(const subtree &subtree, const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const;

    template<typename dmtcmp_t,
             int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
    int find_internal_plus_array(const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const;

    template<typename dmtcmp_t,
             int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
    int find_internal_plus(const subtree &subtree, const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const;

    template<typename dmtcmp_t,
             int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
    int find_internal_minus_array(const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const;

    template<typename dmtcmp_t,
             int (*h)(const uint32_t, const dmtdata_t &, const dmtcmp_t &)>
    int find_internal_minus(const subtree &subtree, const dmtcmp_t &extra, uint32_t *const value_len, dmtdataout_t *const value, uint32_t *const idxp) const;

    node_offset* alloc_temp_node_offsets(uint32_t num_idxs);

    uint32_t align(const uint32_t x) const;
};

} // namespace toku

// include the implementation here
#include "dmt.cc"

