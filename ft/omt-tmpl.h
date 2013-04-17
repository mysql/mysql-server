/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#if !defined(OMT_TMPL_H)
#define OMT_TMPL_H

#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <toku_assert.h>
#include <memory.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <db.h>

#if defined(__ICL) || defined(__ICC) || defined(__clang__)
# define static_assert(foo, bar) // nothing
#else
# include <type_traits>
#endif

namespace toku {

/**
 * Order Maintenance Tree (OMT)
 *
 * Maintains a collection of totally ordered values, where each value has an integer weight.
 * The OMT is a mutable datatype.
 *
 * The Abstraction:
 *
 * An OMT is a vector of values, $V$, where $|V|$ is the length of the vector.
 * The vector is numbered from $0$ to $|V|-1$.
 * Each value has a weight.  The weight of the $i$th element is denoted $w(V_i)$.
 *
 * We can create a new OMT, which is the empty vector.
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
 * We can also split an OMT into two OMTs, splitting the weight of the values evenly.
 * Find a value $j$ such that the values to the left of $j$ have about the same total weight as the values to the right of $j$.
 * The resulting two OMTs contain the values to the left of $j$ and the values to the right of $j$ respectively.
 * All of the values from the original OMT go into one of the new OMTs.
 * If the weights of the values don't split exactly evenly, then the implementation has the freedom to choose whether
 *  the new left OMT or the new right OMT is larger.
 *
 * Performance:
 *  Insertion and deletion should run with $O(\log |V|)$ time and $O(\log |V|)$ calls to the Heaviside function.
 *  The memory required is O(|V|).
 */
template<typename omtdata_t,
         typename omtdataout_t=omtdata_t>
struct omt {
    /**
     * Effect: Create an empty OMT.
     * Performance: constant time.
     */
    void create(void)
    {
        this->create_internal(2);
    }

    /**
     * 
     */
    void create_no_array(void)
    {
        this->create_internal_no_array(0);
    }

    /**
     * Effect: Create a OMT containing values.  The number of values is in numvalues.
     *  Stores the new OMT in *omtp.
     * Requires: this has not been created yet
     * Requires: values != NULL
     * Requires: values is sorted
     * Performance:  time=O(numvalues)
     * Rationale:    Normally to insert N values takes O(N lg N) amortized time.
     *               If the N values are known in advance, are sorted, and
     *               the structure is empty, we can batch insert them much faster.
     */
    __attribute__((nonnull))
    void create_from_sorted_array(const omtdata_t *const values, const uint32_t numvalues)
    {
        this->create_internal(numvalues);
        memcpy(this->d.a.values, values, numvalues * (sizeof values[0]));
        this->d.a.num_values = numvalues;
    }

    /**
     * Effect: Create an OMT containing values.  The number of values is in numvalues.
     *         On success the OMT takes ownership of *values array, and sets values=NULL.
     * Requires: this has not been created yet
     * Requires: values != NULL
     * Requires: *values is sorted
     * Requires: *values was allocated with toku_malloc
     * Requires: Capacity of the *values array is <= new_capacity
     * Requires: On success, *values may not be accessed again by the caller.
     * Performance:  time=O(1)
     * Rational:     create_from_sorted_array takes O(numvalues) time.
     *               By taking ownership of the array, we save a malloc and memcpy,
     *               and possibly a free (if the caller is done with the array).
     */
    __attribute__((nonnull))
    void create_steal_sorted_array(omtdata_t **const values, const uint32_t numvalues, const uint32_t new_capacity)
    {
        invariant_notnull(values);
        this->create_internal_no_array(new_capacity);
        this->d.a.num_values = numvalues;
        this->d.a.values = *values;
        *values = nullptr;
    }

    /**
     * Effect: Create a new OMT, storing it in *newomt.
     *  The values to the right of index (starting at index) are moved to *newomt.
     * Requires: newomt != NULL
     * Returns
     *    0             success,
     *    EINVAL        if index > toku_omt_size(omt)
     * On nonzero return, omt and *newomt are unmodified.
     * Performance: time=O(n)
     * Rationale:  We don't need a split-evenly operation.  We need to split items so that their total sizes
     *  are even, and other similar splitting criteria.  It's easy to split evenly by calling size(), and dividing by two.
     */
    __attribute__((nonnull))
    int split_at(omt *const newomt, const uint32_t idx) {
        invariant_notnull(newomt);
        if (idx > this->size()) { return EINVAL; }
        this->convert_to_array();
        const uint32_t newsize = this->size() - idx;
        newomt->create_from_sorted_array(&this->d.a.values[this->d.a.start_idx + idx], newsize);
        this->d.a.num_values = idx;
        this->maybe_resize_array();
        return 0;
    }

    /**
     * Effect: Appends leftomt and rightomt to produce a new omt.
     *  Creates this as the new omt.
     *  leftomt and rightomt are destroyed.
     * Performance: time=O(n) is acceptable, but one can imagine implementations that are O(\log n) worst-case.
     */
    __attribute__((nonnull))
    void merge(omt *const leftomt, omt *const rightomt) {
        invariant_notnull(leftomt);
        invariant_notnull(rightomt);
        const uint32_t leftsize = leftomt->size();
        const uint32_t rightsize = rightomt->size();
        const uint32_t newsize = leftsize + rightsize;

        if (leftomt->is_array) {
            if (leftomt->capacity - (leftomt->d.a.start_idx + leftomt->d.a.num_values) >= rightsize) {
                this->create_steal_sorted_array(leftomt->d.a.values, leftomt->d.a.num_values, leftomt->capacity);
                this->d.a.start_idx = leftomt->d.a.start_idx;
            } else {
                this->create_internal(newsize);
                memcpy(&this->d.a.values[0],
                       &leftomt->d.a.values[leftomt->d.a.start_idx],
                       leftomt->d.a.num_values * (sizeof this->d.a.values[0]));
            }
        } else {
            this->create_internal(newsize);
            leftomt->fill_array_with_subtree_values(&this->d.a.values[0], leftomt->d.t.root);
        }
        leftomt->destroy();
        this->d.a.num_values = leftsize;

        if (rightomt->is_array) {
            memcpy(&this->d.a.values[this->d.a.start_idx + this->d.a.num_values],
                   &rightomt->d.a.values[rightomt->d.a.start_idx],
                   rightomt->d.a.num_values * (sizeof this->d.a.values[0]));
        } else {
            rightomt->fill_array_with_subtree_values(&this->d.a.values[this->d.a.start_idx + this->d.a.num_values],
                                                     rightomt->d.t.root);
        }
        rightomt->destroy();
        this->d.a.num_values += rightsize;
        invariant(this->size() == newsize);
    }

    /**
     * Effect: Creates a copy of an omt.
     *  Creates this as the clone.
     *  Each element is copied directly.  If they are pointers, the underlying data is not duplicated.
     * Performance: O(n) or the running time of fill_array_with_subtree_values()
     */
    void clone(const omt &src)
    {
        this->create_internal(src.size());
        if (src.is_array) {
            memcpy(&this->d.a.values[0], &src.d.a.values[src.d.a.start_idx], src.d.a.num_values * (sizeof this->d.a.values[0]));
        } else {
            src.fill_array_with_subtree_values(&this->d.a.values[0], src.d.t.root);
        }
        this->d.a.num_values = src.size();
    }

    /**
     * Effect: Creates a copy of an omt.
     *  Creates this as the clone.
     *  Each element is assumed to be a pointer, and the underlying data is duplicated for the clone using toku_malloc.
     * Performance: the running time of iterate()
     */
    void deep_clone(const omt &src)
    {
        this->create_internal(src.size());
        int r = src.iterate<omt, deep_clone_iter>(this);
        lazy_assert_zero(r);
        this->d.a.num_values = src.size();
    }

    /**
     * Effect: Set the tree to be empty.
     *  Note: Will not reallocate or resize any memory.
     * Performance: time=O(1)
     */
    void clear(void)
    {
        if (this->is_array) {
            this->d.a.start_idx = 0;
            this->d.a.num_values = 0;
        } else {
            this->d.t.root = NODE_NULL;
            this->d.t.free_idx = 0;
        }
    }

    /**
     * Effect:  Destroy an OMT, freeing all its memory.
     *   If the values being stored are pointers, their underlying data is not freed.  See free_items()
     *   Those values may be freed before or after calling toku_omt_destroy.
     * Rationale: Returns no values since free() cannot fail.
     * Rationale: Does not free the underlying pointers to reduce complexity.
     * Performance:  time=O(1)
     * 
     */
    void destroy(void)
    {
        this->clear();
        this->capacity = 0;
        if (this->is_array) {
            if (this->d.a.values != nullptr) {
                toku_free(this->d.a.values);
            }
            this->d.a.values = nullptr;
        } else {
            if (this->d.t.nodes != nullptr) {
                toku_free(this->d.t.nodes);
            }
            this->d.t.nodes = nullptr;
        }
    }

    /**
     * Effect: return |this|.
     * Performance:  time=O(1)
     */
    inline uint32_t size(void) const
    {
        if (this->is_array) {
            return this->d.a.num_values;
        } else {
            return this->nweight(this->d.t.root);
        }
    }

    /**
     * Effect:  Insert value into the OMT.
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
     * On nonzero return, omt is unchanged.
     * Performance: time=O(\log N) amortized.
     * Rationale: Some future implementation may be O(\log N) worst-case time, but O(\log N) amortized is good enough for now.
     */
    template<typename omtcmp_t,
             int (*h)(const omtdata_t &, const omtcmp_t &)>
    int insert(const omtdata_t &value, const omtcmp_t &v, uint32_t *const idx)
    {
        int r;
        uint32_t insert_idx;

        r = this->find_zero<omtcmp_t, h>(v, nullptr, &insert_idx);
        if (r==0) {
            if (idx) *idx = insert_idx;
            return DB_KEYEXIST;
        }
        if (r != DB_NOTFOUND) return r;

        if ((r = this->insert_at(value, insert_idx))) return r;
        if (idx) *idx = insert_idx;

        return 0;
    }

    /**
     * Effect: Increases indexes of all items at slot >= idx by 1.
     *         Insert value into the position at idx.
     * Returns:
     *   0         success
     *   EINVAL    if idx > this->size()
     * On error, omt is unchanged.
     * Performance: time=O(\log N) amortized time.
     * Rationale: Some future implementation may be O(\log N) worst-case time, but O(\log N) amortized is good enough for now.
     */
    int insert_at(const omtdata_t &value, const uint32_t idx)
    {
        if (idx > this->size()) { return EINVAL; }
        this->maybe_resize_or_convert(this->size() + 1);
        if (this->is_array && idx != this->d.a.num_values &&
            (idx != 0 || this->d.a.start_idx == 0)) {
            this->convert_to_tree();
        }
        if (this->is_array) {
            if (idx == this->d.a.num_values) {
                this->d.a.values[this->d.a.start_idx + this->d.a.num_values] = value;
            }
            else {
                this->d.a.values[--this->d.a.start_idx] = value;
            }
            this->d.a.num_values++;
        }
        else {
            node_idx *rebalance_idx = nullptr;
            this->insert_internal(&this->d.t.root, value, idx, &rebalance_idx);
            if (rebalance_idx != nullptr) {
                this->rebalance(rebalance_idx);
            }
        }
        return 0;
    }

    /**
     * Effect:  Replaces the item at idx with value.
     * Returns:
     *   0       success
     *   EINVAL  if idx>=this->size()
     * On error, omt is unchanged.
     * Performance: time=O(\log N)
     * Rationale: The FT needs to be able to replace a value with another copy of the same value (allocated in a different location)
     * 
     */
    int set_at(const omtdata_t &value, const uint32_t idx)
    {
        if (idx >= this->size()) { return EINVAL; }
        if (this->is_array) {
            this->set_at_internal_array(value, idx);
        } else {
            this->set_at_internal(this->d.t.root, value, idx);
        }
        return 0;
    }

    /**
     * Effect: Delete the item in slot idx.
     *         Decreases indexes of all items at slot > idx by 1.
     * Returns
     *     0            success
     *     EINVAL       if idx>=this->size()
     * On error, omt is unchanged.
     * Rationale: To delete an item, first find its index using find or find_zero, then delete it.
     * Performance: time=O(\log N) amortized.
     */
    int delete_at(const uint32_t idx)
    {
        if (idx >= this->size()) { return EINVAL; }
        this->maybe_resize_or_convert(this->size() - 1);
        if (this->is_array && idx != 0 && idx != this->d.a.num_values - 1) {
            this->convert_to_tree();
        }
        if (this->is_array) {
            //Testing for 0 does not rule out it being the last entry.
            //Test explicitly for num_values-1
            if (idx != this->d.a.num_values - 1) {
                this->d.a.start_idx++;
            }
            this->d.a.num_values--;
        } else {
            node_idx *rebalance_idx = nullptr;
            this->delete_internal(&this->d.t.root, idx, nullptr, &rebalance_idx);
            if (rebalance_idx != nullptr) {
                this->rebalance(rebalance_idx);
            }
        }
        return 0;
    }

    /**
     * Effect:  Iterate over the values of the omt, from left to right, calling f on each value.
     *  The first argument passed to f is a ref-to-const of the value stored in the omt.
     *  The second argument passed to f is the index of the value.
     *  The third argument passed to f is iterate_extra.
     *  The indices run from 0 (inclusive) to this->size() (exclusive).
     * Requires: f != NULL
     * Returns:
     *  If f ever returns nonzero, then the iteration stops, and the value returned by f is returned by iterate.
     *  If f always returns zero, then iterate returns 0.
     * Requires:  Don't modify the omt while running.  (E.g., f may not insert or delete values from the omt.)
     * Performance: time=O(i+\log N) where i is the number of times f is called, and N is the number of elements in the omt.
     * Rationale: Although the functional iterator requires defining another function (as opposed to C++ style iterator), it is much easier to read.
     * Rationale: We may at some point use functors, but for now this is a smaller change from the old OMT.
     */
    template<typename iterate_extra_t,
             int (*f)(const omtdata_t &, const uint32_t, iterate_extra_t *const)>
    int iterate(iterate_extra_t *const iterate_extra) const {
        return this->iterate_on_range<iterate_extra_t, f>(0, this->size(), iterate_extra);
    }

    /**
     * Effect:  Iterate over the values of the omt, from left to right, calling f on each value.
     *  The first argument passed to f is a ref-to-const of the value stored in the omt.
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
     * Requires:  Don't modify the omt while running.  (E.g., f may not insert or delete values from the omt.)
     * Performance: time=O(i+\log N) where i is the number of times f is called, and N is the number of elements in the omt.
     * Rational: Although the functional iterator requires defining another function (as opposed to C++ style iterator), it is much easier to read.
     */
    template<typename iterate_extra_t,
             int (*f)(const omtdata_t &, const uint32_t, iterate_extra_t *const)>
    int iterate_on_range(const uint32_t left, const uint32_t right, iterate_extra_t *const iterate_extra) const {
        if (right > this->size()) { return EINVAL; }
        if (this->is_array) {
            return this->iterate_internal_array<iterate_extra_t, f>(left, right, iterate_extra);
        }
        return this->iterate_internal<iterate_extra_t, f>(left, right, this->d.t.root, 0, iterate_extra);
    }

    /**
     * Effect:  Iterate over the values of the omt, from left to right, calling f on each value.
     *  The first argument passed to f is a pointer to the value stored in the omt.
     *  The second argument passed to f is the index of the value.
     *  The third argument passed to f is iterate_extra.
     *  The indices run from 0 (inclusive) to this->size() (exclusive).
     * Requires: same as for iterate()
     * Returns: same as for iterate()
     * Performance: same as for iterate()
     * Rationale: In general, most iterators should use iterate() since they should not modify the data stored in the omt.  This function is for iterators which need to modify values (for example, free_items).
     * Rationale: We assume if you are transforming the data in place, you want to do it to everything at once, so there is not yet an iterate_on_range_ptr (but there could be).
     */
    template<typename iterate_extra_t,
             int (*f)(omtdata_t *, const uint32_t, iterate_extra_t *const)>
    void iterate_ptr(iterate_extra_t *const iterate_extra) {
        if (this->is_array) {
            this->iterate_ptr_internal_array<iterate_extra_t, f>(0, this->size(), iterate_extra);
        } else {
            this->iterate_ptr_internal<iterate_extra_t, f>(0, this->size(), this->d.t.root, 0, iterate_extra);
        }
    }

    /**
     * Effect: Iterate over the values of the omt, from left to right, freeing each value with toku_free
     * Requires: all items in OMT to have been malloced with toku_malloc
     * Rational: This function was added due to a problem encountered in ft-ops.c. We needed to free the elements and then
     *   destroy the OMT. However, destroying the OMT requires invalidating cursors. This cannot be done if the values of the OMT
     *   have been already freed. So, this function is written to invalidate cursors and free items.
     */
    void free_items(void) {
        this->iterate_ptr<void, free_items_iter>(nullptr);
    }

    /**
     * Effect: Set *value=V_idx
     * Returns
     *    0             success
     *    EINVAL        if index>=toku_omt_size(omt)
     * On nonzero return, *value is unchanged
     * Performance: time=O(\log N)
     */
    int fetch(const uint32_t idx, omtdataout_t *const value) const
    {
        if (idx >= this->size()) { return EINVAL; }
        if (this->is_array) {
            this->fetch_internal_array(idx, value);
        } else {
            this->fetch_internal(this->d.t.root, idx, value);
        }
        return 0;
    }

    /**
     * Effect:  Find the smallest i such that h(V_i, extra)>=0
     *  If there is such an i and h(V_i,extra)==0 then set *idxp=i, set *value = V_i, and return 0.
     *  If there is such an i and h(V_i,extra)>0  then set *idxp=i and return DB_NOTFOUND.
     *  If there is no such i then set *idx=this->size() and return DB_NOTFOUND.
     * Note: value is of type omtdataout_t, which may be of type (omtdata_t) or (omtdata_t *) but is fixed by the instantiation.
     *  If it is the value type, then the value is copied out (even if the value type is a pointer to something else)
     *  If it is the pointer type, then *value is set to a pointer to the data within the omt.
     *  This is determined by the type of the omt as initially declared.
     *   If the omt is declared as omt<foo_t>, then foo_t's will be stored and foo_t's will be returned by find and related functions.
     *   If the omt is declared as omt<foo_t, foo_t *>, then foo_t's will be stored, and pointers to the stored items will be returned by find and related functions.
     * Rationale:
     *  Structs too small for malloc should be stored directly in the omt.
     *  These structs may need to be edited as they exist inside the omt, so we need a way to get a pointer within the omt.
     *  Using separate functions for returning pointers and values increases code duplication and reduces type-checking.
     *  That also reduces the ability of the creator of a data structure to give advice to its future users.
     *  Slight overloading in this case seemed to provide a better API and better type checking.
     */
    template<typename omtcmp_t,
             int (*h)(const omtdata_t &, const omtcmp_t &)>
    int find_zero(const omtcmp_t &extra, omtdataout_t *const value, uint32_t *const idxp) const
    {
        uint32_t tmp_index;
        uint32_t *const child_idxp = (idxp != nullptr) ? idxp : &tmp_index;
        int r;
        if (this->is_array) {
            r = this->find_internal_zero_array<omtcmp_t, h>(extra, value, child_idxp);
        }
        else {
            r = this->find_internal_zero<omtcmp_t, h>(this->d.t.root, extra, value, child_idxp);
        }
        return r;
    }

    template<typename omtcmp_t,
             int (*h)(const omtdata_t &, const omtcmp_t &)>
    int find(const omtcmp_t &extra, int direction, omtdataout_t *const value, uint32_t *const idxp) const
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
    {
        uint32_t tmp_index;
        uint32_t *const child_idxp = (idxp != nullptr) ? idxp : &tmp_index;
        invariant(direction != 0);
        if (direction < 0) {
            if (this->is_array) {
                return this->find_internal_minus_array<omtcmp_t, h>(extra, value, child_idxp);
            } else {
                return this->find_internal_minus<omtcmp_t, h>(this->d.t.root, extra, value, child_idxp);
            }
        } else {
            if (this->is_array) {
                return this->find_internal_plus_array<omtcmp_t, h>(extra, value, child_idxp);
            } else {
                return this->find_internal_plus<omtcmp_t, h>(this->d.t.root, extra, value, child_idxp);
            }
        }
    }

    /**
     * Effect: Return the size (in bytes) of the omt, as it resides in main memory.  If the data stored are pointers, don't include the size of what they all point to.
     */
    size_t memory_size(void) {
        if (this->is_array) {
            return (sizeof *this) + this->capacity * (sizeof this->d.a.values[0]);
        }
        return (sizeof *this) + this->capacity * (sizeof this->d.t.nodes[0]);
    }

private:
    typedef uint32_t node_idx;
    enum {
        NODE_NULL = UINT32_MAX
    };

    struct omt_node {
        uint32_t weight;
        node_idx left;
        node_idx right;
        omtdata_t value;
    } __attribute__((__packed__));

    struct omt_array {
        uint32_t start_idx;
        uint32_t num_values;
        omtdata_t *values;
    };

    struct omt_tree {
        node_idx root;
        node_idx free_idx;
        struct omt_node *nodes;
    };

    bool is_array;
    uint32_t capacity;
    union {
        struct omt_array a;
        struct omt_tree t;
    } d;


    void create_internal_no_array(const uint32_t new_capacity) {
        this->is_array = true;
        this->capacity = new_capacity;
        this->d.a.start_idx = 0;
        this->d.a.num_values = 0;
        this->d.a.values = nullptr;
    }

    void create_internal(const uint32_t new_capacity) {
        this->create_internal_no_array(new_capacity);
        XMALLOC_N(this->capacity, this->d.a.values);
    }

    inline uint32_t nweight(const node_idx idx) const {
        if (idx == NODE_NULL) {
            return 0;
        } else {
            return this->d.t.nodes[idx].weight;
        }
    }

    inline node_idx node_malloc(void) {
        invariant(this->d.t.free_idx < this->capacity);
        return this->d.t.free_idx++;
    }

    inline void node_free(const node_idx idx) {
        invariant(idx < this->capacity);
    }

    inline void maybe_resize_array(const uint32_t n) {
        const uint32_t new_size = n<=2 ? 4 : 2*n;
        const uint32_t room = this->capacity - this->d.a.start_idx;

        if (room < n || this->capacity / 2 >= new_size) {
            omtdata_t *XMALLOC_N(new_size, tmp_values);
            memcpy(tmp_values, &this->d.a.values[this->d.a.start_idx],
                   this->d.a.num_values * (sizeof tmp_values[0]));
            this->d.a.start_idx = 0;
            this->capacity = new_size;
            toku_free(this->d.a.values);
            this->d.a.values = tmp_values;
        }
    }

    __attribute__((nonnull))
    inline void fill_array_with_subtree_values(omtdata_t *const array, const node_idx tree_idx) const {
        if (tree_idx==NODE_NULL) return;
        const omt_node &tree = this->d.t.nodes[tree_idx];
        this->fill_array_with_subtree_values(&array[0], tree.left);
        array[this->nweight(tree.left)] = tree.value;
        this->fill_array_with_subtree_values(&array[this->nweight(tree.left) + 1], tree.right);
    }

    inline void convert_to_array(void) {
        if (!this->is_array) {
            const uint32_t num_values = this->size();
            uint32_t new_size = 2*num_values;
            new_size = new_size < 4 ? 4 : new_size;

            omtdata_t *XMALLOC_N(new_size, tmp_values);
            this->fill_array_with_subtree_values(tmp_values, this->d.t.root);
            toku_free(this->d.t.nodes);
            this->is_array       = true;
            this->capacity       = new_size;
            this->d.a.num_values = num_values;
            this->d.a.values     = tmp_values;
            this->d.a.start_idx  = 0;
        }
    }

    __attribute__((nonnull))
    inline void rebuild_from_sorted_array(node_idx *const n_idxp, const omtdata_t *const values, const uint32_t numvalues) {
        if (numvalues==0) {
            *n_idxp = NODE_NULL;
        } else {
            const uint32_t halfway = numvalues/2;
            const node_idx newidx = this->node_malloc();
            omt_node *const newnode = &this->d.t.nodes[newidx];
            newnode->weight = numvalues;
            newnode->value = values[halfway];
            *n_idxp = newidx; // update everything before the recursive calls so the second call can be a tail call.
            this->rebuild_from_sorted_array(&newnode->left,  &values[0], halfway);
            this->rebuild_from_sorted_array(&newnode->right, &values[halfway+1], numvalues - (halfway+1));
        }
    }

    inline void convert_to_tree(void) {
        if (this->is_array) {
            const uint32_t num_nodes = this->size();
            uint32_t new_size  = num_nodes*2;
            new_size = new_size < 4 ? 4 : new_size;

            omt_node *XMALLOC_N(new_size, new_nodes);
            omtdata_t *const values = this->d.a.values;
            omtdata_t *const tmp_values = &values[this->d.a.start_idx];
            this->is_array = false;
            this->d.t.nodes = new_nodes;
            this->capacity = new_size;
            this->d.t.free_idx = 0;
            this->d.t.root = NODE_NULL;
            this->rebuild_from_sorted_array(&this->d.t.root, tmp_values, num_nodes);
            toku_free(values);
        }
    }

    inline void maybe_resize_or_convert(const uint32_t n) {
        if (this->is_array) {
            this->maybe_resize_array(n);
        } else {
            const uint32_t new_size = n<=2 ? 4 : 2*n;
            const uint32_t num_nodes = this->nweight(this->d.t.root);
            if ((this->capacity/2 >= new_size) ||
                (this->d.t.free_idx >= this->capacity && num_nodes < n) ||
                (this->capacity<n)) {
                this->convert_to_array();
            }
        }
    }

    inline bool will_need_rebalance(const node_idx n_idx, const int leftmod, const int rightmod) const {
        if (n_idx==NODE_NULL) { return false; }
        const omt_node &n = this->d.t.nodes[n_idx];
        // one of the 1's is for the root.
        // the other is to take ceil(n/2)
        const uint32_t weight_left  = this->nweight(n.left)  + leftmod;
        const uint32_t weight_right = this->nweight(n.right) + rightmod;
        return ((1+weight_left < (1+1+weight_right)/2)
                ||
                (1+weight_right < (1+1+weight_left)/2));
    }

    __attribute__((nonnull))
    inline void insert_internal(node_idx *const n_idxp, const omtdata_t &value, const uint32_t idx, node_idx **const rebalance_idx) {
        if (*n_idxp == NODE_NULL) {
            invariant_zero(idx);
            const node_idx newidx = this->node_malloc();
            omt_node *const newnode = &this->d.t.nodes[newidx];
            newnode->weight = 1;
            newnode->left = NODE_NULL;
            newnode->right = NODE_NULL;
            newnode->value = value;
            *n_idxp = newidx;
        } else {
            const node_idx thisidx = *n_idxp;
            omt_node *const n = &this->d.t.nodes[thisidx];
            n->weight++;
            if (idx <= this->nweight(n->left)) {
                if (*rebalance_idx == nullptr && this->will_need_rebalance(thisidx, 1, 0)) {
                    *rebalance_idx = n_idxp;
                }
                this->insert_internal(&n->left, value, idx, rebalance_idx);
            } else {
                if (*rebalance_idx == nullptr && this->will_need_rebalance(thisidx, 0, 1)) {
                    *rebalance_idx = n_idxp;
                }
                const uint32_t sub_index = idx - this->nweight(n->left) - 1;
                this->insert_internal(&n->right, value, sub_index, rebalance_idx);
            }
        }
    }

    inline void set_at_internal_array(const omtdata_t &value, const uint32_t idx) {
        this->d.a.values[this->d.a.start_idx + idx] = value;
    }

    inline void set_at_internal(const node_idx n_idx, const omtdata_t &value, const uint32_t idx) {
        invariant(n_idx != NODE_NULL);
        omt_node *const n = &this->d.t.nodes[n_idx];
        const uint32_t leftweight = this->nweight(n->left);
        if (idx < leftweight) {
            this->set_at_internal(n->left, value, idx);
        } else if (idx == leftweight) {
            n->value = value;
        } else {
            this->set_at_internal(n->right, value, idx - leftweight - 1);
        }
    }

    inline void delete_internal(node_idx *const n_idxp, const uint32_t idx, omt_node *const copyn, node_idx **const rebalance_idx) {
        invariant_notnull(n_idxp);
        invariant_notnull(rebalance_idx);
        invariant(*n_idxp != NODE_NULL);
        omt_node *const n = &this->d.t.nodes[*n_idxp];
        const uint32_t leftweight = this->nweight(n->left);
        if (idx < leftweight) {
            n->weight--;
            if (*rebalance_idx == nullptr && this->will_need_rebalance(*n_idxp, -1, 0)) {
                *rebalance_idx = n_idxp;
            }
            this->delete_internal(&n->left, idx, copyn, rebalance_idx);
        } else if (idx == leftweight) {
            if (n->left == NODE_NULL) {
                const uint32_t oldidx = *n_idxp;
                *n_idxp = n->right;
                if (copyn != nullptr) {
                    copyn->value = n->value;
                }
                this->node_free(oldidx);
            } else if (n->right == NODE_NULL) {
                const uint32_t oldidx = *n_idxp;
                *n_idxp = n->left;
                if (copyn != nullptr) {
                    copyn->value = n->value;
                }
                this->node_free(oldidx);
            } else {
                if (*rebalance_idx == nullptr && this->will_need_rebalance(*n_idxp, 0, -1)) {
                    *rebalance_idx = n_idxp;
                }
                // don't need to copy up value, it's only used by this
                // next call, and when that gets to the bottom there
                // won't be any more recursion
                n->weight--;
                this->delete_internal(&n->right, 0, n, rebalance_idx);
            }
        } else {
            n->weight--;
            if (*rebalance_idx == nullptr && this->will_need_rebalance(*n_idxp, 0, -1)) {
                *rebalance_idx = n_idxp;
            }
            this->delete_internal(&n->right, idx - leftweight - 1, copyn, rebalance_idx);
        }
    }

    template<typename iterate_extra_t,
             int (*f)(const omtdata_t &, const uint32_t, iterate_extra_t *const)>
    inline int iterate_internal_array(const uint32_t left, const uint32_t right,
                                      iterate_extra_t *const iterate_extra) const {
        int r;
        for (uint32_t i = left; i < right; ++i) {
            r = f(this->d.a.values[this->d.a.start_idx + i], i, iterate_extra);
            if (r != 0) {
                return r;
            }
        }
        return 0;
    }

    template<typename iterate_extra_t,
             int (*f)(omtdata_t *, const uint32_t, iterate_extra_t *const)>
    inline void iterate_ptr_internal(const uint32_t left, const uint32_t right,
                                     const node_idx n_idx, const uint32_t idx,
                                     iterate_extra_t *const iterate_extra) {
        if (n_idx != NODE_NULL) { 
            omt_node *const n = this->d.t.nodes[n_idx];
            const uint32_t idx_root = idx + this->nweight(n->left);
            if (left < idx_root) {
                this->iterate_ptr_internal<iterate_extra_t, f>(left, right, n->left, idx, iterate_extra);
            }
            if (left <= idx_root && idx_root < right) {
                int r = f(&n->value, idx_root, iterate_extra);
                lazy_assert_zero(r);
            }
            if (idx_root + 1 < right) {
                this->iterate_ptr_internal<iterate_extra_t, f>(left, right, n->right, idx_root + 1, iterate_extra);
            }
        }
    }

    template<typename iterate_extra_t,
             int (*f)(omtdata_t *, const uint32_t, iterate_extra_t *const)>
    inline void iterate_ptr_internal_array(const uint32_t left, const uint32_t right,
                                           iterate_extra_t *const iterate_extra) {
        for (uint32_t i = left; i < right; ++i) {
            int r = f(&this->d.a.values[this->d.a.start_idx + i], i, iterate_extra);
            lazy_assert_zero(r);
        }
    }

    template<typename iterate_extra_t,
             int (*f)(const omtdata_t &, const uint32_t, iterate_extra_t *const)>
    inline int iterate_internal(const uint32_t left, const uint32_t right,
                                const node_idx n_idx, const uint32_t idx,
                                iterate_extra_t *const iterate_extra) const {
        if (n_idx == NODE_NULL) { return 0; }
        int r;
        const omt_node &n = this->d.t.nodes[n_idx];
        const uint32_t idx_root = idx + this->nweight(n.left);
        if (left < idx_root) {
            r = this->iterate_internal<iterate_extra_t, f>(left, right, n.left, idx, iterate_extra);
            if (r != 0) { return r; }
        }
        if (left <= idx_root && idx_root < right) {
            r = f(n.value, idx_root, iterate_extra);
            if (r != 0) { return r; }
        }
        if (idx_root + 1 < right) {
            return this->iterate_internal<iterate_extra_t, f>(left, right, n.right, idx_root + 1, iterate_extra);
        }
        return 0;
    }

    inline void fetch_internal_array(const uint32_t i, omtdataout_t *value) const {
        if (value != nullptr) {
            copyout(value, &this->d.a.values[this->d.a.start_idx + i]);
        }
    }

    inline void fetch_internal(const node_idx idx, const uint32_t i, omtdataout_t *value) const {
        omt_node *const n = &this->d.t.nodes[idx];
        const uint32_t leftweight = this->nweight(n->left);
        if (i < leftweight) {
            this->fetch_internal(n->left, i, value);
        } else if (i == leftweight) {
            if (value != nullptr) {
                copyout(value, n);
            }
        } else {
            this->fetch_internal(n->right, i - leftweight - 1, value);
        }
    }

    __attribute__((nonnull))
    inline void fill_array_with_subtree_idxs(node_idx *const array, const node_idx tree_idx) const {
        if (tree_idx != NODE_NULL) {
            const omt_node &tree = this->d.t.nodes[tree_idx];
            this->fill_array_with_subtree_idxs(&array[0], tree.left);
            array[this->nweight(tree.left)] = tree_idx;
            this->fill_array_with_subtree_idxs(&array[this->nweight(tree.left) + 1], tree.right);
        }
    }

    __attribute__((nonnull))
    inline void rebuild_subtree_from_idxs(node_idx *const n_idxp, const node_idx *const idxs, const uint32_t numvalues) {
        if (numvalues==0) {
            *n_idxp = NODE_NULL;
        } else {
            uint32_t halfway = numvalues/2;
            *n_idxp = idxs[halfway];
            //node_idx newidx = idxs[halfway];
            omt_node *const newnode = &this->d.t.nodes[*n_idxp];
            newnode->weight = numvalues;
            // value is already in there.
            this->rebuild_subtree_from_idxs(&newnode->left,  &idxs[0], halfway);
            this->rebuild_subtree_from_idxs(&newnode->right, &idxs[halfway+1], numvalues-(halfway+1));
            //n_idx = newidx;
        }
    }

    __attribute__((nonnull))
    inline void rebalance(node_idx *const n_idxp) {
        node_idx idx = *n_idxp;
        if (idx==this->d.t.root) {
            //Try to convert to an array.
            //If this fails, (malloc) nothing will have changed.
            //In the failure case we continue on to the standard rebalance
            //algorithm.
            this->convert_to_array();
        } else {
            const omt_node &n = this->d.t.nodes[idx];
            node_idx *tmp_array;
            size_t mem_needed = n.weight * (sizeof tmp_array[0]);
            size_t mem_free = (this->capacity - this->d.t.free_idx) * (sizeof this->d.t.nodes[0]);
            bool malloced;
            if (mem_needed<=mem_free) {
                //There is sufficient free space at the end of the nodes array
                //to hold enough node indexes to rebalance.
                malloced = false;
                tmp_array = reinterpret_cast<node_idx *>(&this->d.t.nodes[this->d.t.free_idx]);
            }
            else {
                malloced = true;
                XMALLOC_N(n.weight, tmp_array);
            }
            this->fill_array_with_subtree_idxs(tmp_array, idx);
            this->rebuild_subtree_from_idxs(n_idxp, tmp_array, n.weight);
            if (malloced) toku_free(tmp_array);
        }
    }

    __attribute__((nonnull))
    static inline void copyout(omtdata_t *const out, const omt_node *const n) {
        *out = n->value;
    }

    __attribute__((nonnull))
    static inline void copyout(omtdata_t **const out, omt_node *const n) {
        *out = &n->value;
    }

    __attribute__((nonnull))
    static inline void copyout(omtdata_t *const out, const omtdata_t *const stored_value_ptr) {
        *out = *stored_value_ptr;
    }

    __attribute__((nonnull))
    static inline void copyout(omtdata_t **const out, omtdata_t *const stored_value_ptr) {
        *out = stored_value_ptr;
    }

    template<typename omtcmp_t,
             int (*h)(const omtdata_t &, const omtcmp_t &)>
    inline int find_internal_zero_array(const omtcmp_t &extra, omtdataout_t *value, uint32_t *const idxp) const {
        invariant_notnull(idxp);
        uint32_t min = this->d.a.start_idx;
        uint32_t limit = this->d.a.start_idx + this->d.a.num_values;
        uint32_t best_pos = NODE_NULL;
        uint32_t best_zero = NODE_NULL;

        while (min!=limit) {
            uint32_t mid = (min + limit) / 2;
            int hv = h(this->d.a.values[mid], extra);
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
        if (best_zero!=NODE_NULL) {
            //Found a zero
            if (value != nullptr) {
                copyout(value, &this->d.a.values[best_zero]);
            }
            *idxp = best_zero - this->d.a.start_idx;
            return 0;
        }
        if (best_pos!=NODE_NULL) *idxp = best_pos - this->d.a.start_idx;
        else                     *idxp = this->d.a.num_values;
        return DB_NOTFOUND;
    }

    template<typename omtcmp_t,
             int (*h)(const omtdata_t &, const omtcmp_t &)>
    inline int find_internal_zero(const node_idx n_idx, const omtcmp_t &extra, omtdataout_t *value, uint32_t *const idxp) const {
        invariant_notnull(idxp);
        if (n_idx==NODE_NULL) {
            *idxp = 0;
            return DB_NOTFOUND;
        }
        omt_node *const n = &this->d.t.nodes[n_idx];
        int hv = h(n->value, extra);
        if (hv<0) {
            int r = this->find_internal_zero<omtcmp_t, h>(n->right, extra, value, idxp);
            *idxp += this->nweight(n->left)+1;
            return r;
        } else if (hv>0) {
            return this->find_internal_zero<omtcmp_t, h>(n->left, extra, value, idxp);
        } else {
            int r = this->find_internal_zero<omtcmp_t, h>(n->left, extra, value, idxp);
            if (r==DB_NOTFOUND) {
                *idxp = this->nweight(n->left);
                if (value != nullptr) {
                    copyout(value, n);
                }
                r = 0;
            }
            return r;
        }
    }

    template<typename omtcmp_t,
             int (*h)(const omtdata_t &, const omtcmp_t &)>
    inline int find_internal_plus_array(const omtcmp_t &extra, omtdataout_t *value, uint32_t *const idxp) const {
        invariant_notnull(idxp);
        uint32_t min = this->d.a.start_idx;
        uint32_t limit = this->d.a.start_idx + this->d.a.num_values;
        uint32_t best = NODE_NULL;

        while (min != limit) {
            const uint32_t mid = (min + limit) / 2;
            const int hv = h(this->d.a.values[mid], extra);
            if (hv > 0) {
                best = mid;
                limit = mid;
            } else {
                min = mid + 1;
            }
        }
        if (best == NODE_NULL) { return DB_NOTFOUND; }
        if (value != nullptr) {
            copyout(value, &this->d.a.values[best]);
        }
        *idxp = best - this->d.a.start_idx;
        return 0;
    }

    template<typename omtcmp_t,
             int (*h)(const omtdata_t &, const omtcmp_t &)>
    inline int find_internal_plus(const node_idx n_idx, const omtcmp_t &extra, omtdataout_t *value, uint32_t *const idxp) const {
        invariant_notnull(idxp);
        if (n_idx==NODE_NULL) {
            return DB_NOTFOUND;
        }
        omt_node *const n = &this->d.t.nodes[n_idx];
        int hv = h(n->value, extra);
        int r;
        if (hv > 0) {
            r = this->find_internal_plus<omtcmp_t, h>(n->left, extra, value, idxp);
            if (r == DB_NOTFOUND) {
                *idxp = this->nweight(n->left);
                if (value != nullptr) {
                    copyout(value, n);
                }
                r = 0;
            }
        } else {
            r = this->find_internal_plus<omtcmp_t, h>(n->right, extra, value, idxp);
            if (r == 0) {
                *idxp += this->nweight(n->left) + 1;
            }
        }
        return r;
    }

    template<typename omtcmp_t,
             int (*h)(const omtdata_t &, const omtcmp_t &)>
    inline int find_internal_minus_array(const omtcmp_t &extra, omtdataout_t *value, uint32_t *const idxp) const {
        invariant_notnull(idxp);
        uint32_t min = this->d.a.start_idx;
        uint32_t limit = this->d.a.start_idx + this->d.a.num_values;
        uint32_t best = NODE_NULL;

        while (min != limit) {
            const uint32_t mid = (min + limit) / 2;
            const int hv = h(this->d.a.values[mid], extra);
            if (hv < 0) {
                best = mid;
                min = mid + 1;
            } else {
                limit = mid;
            }
        }
        if (best == NODE_NULL) { return DB_NOTFOUND; }
        if (value != nullptr) {
            copyout(value, &this->d.a.values[best]);
        }
        *idxp = best - this->d.a.start_idx;
        return 0;
    }

    template<typename omtcmp_t,
             int (*h)(const omtdata_t &, const omtcmp_t &)>
    inline int find_internal_minus(const node_idx n_idx, const omtcmp_t &extra, omtdataout_t *value, uint32_t *const idxp) const {
        invariant_notnull(idxp);
        if (n_idx==NODE_NULL) {
            return DB_NOTFOUND;
        }
        omt_node *const n = &this->d.t.nodes[n_idx];
        int hv = h(n->value, extra);
        if (hv < 0) {
            int r = this->find_internal_minus<omtcmp_t, h>(n->right, extra, value, idxp);
            if (r == 0) {
                *idxp += this->nweight(n->left) + 1;
            } else if (r == DB_NOTFOUND) {
                *idxp = this->nweight(n->left);
                if (value != nullptr) {
                    copyout(value, n);
                }
                r = 0;
            }
            return r;
        } else {
            return this->find_internal_minus<omtcmp_t, h>(n->left, extra, value, idxp);
        }
    }

    __attribute__((nonnull))
    static inline int deep_clone_iter(const omtdata_t &value, const uint32_t idx, omt *const dest) {
        static_assert(std::is_pointer<omtdata_t>::value, "omtdata_t isn't a pointer, can't do deep clone");
        invariant_notnull(dest);
        invariant(idx == dest->d.a.num_values);
        invariant(idx < dest->capacity);
        XMEMDUP(dest->d.a.values[dest->d.a.num_values++], value);
        return 0;
    }

    static inline int free_items_iter(omtdata_t *value, const uint32_t UU(idx), void *const UU(unused)) {
        static_assert(std::is_pointer<omtdata_t>::value, "omtdata_t isn't a pointer, can't do free items");
        invariant_notnull(*value);
        toku_free(*value);
        return 0;
    }
};

};

#endif  /* #ifndef OMT_TMPL_H */
