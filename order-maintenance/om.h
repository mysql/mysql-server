#if !defined(OMT_H)
#define OMT_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

// Order Maintenance Array (OMT)
//
// Maintains a collection of totally ordered values, where each value has an integer weight.
// The OMT is a mutable datatype.
//
// The Abstraction:
//
// An OMT is a vector of values, $V$, where $|V|$ is the length of the vector.
// The vector is numbered from $0$ to $|V|-1$.
// Each value has a weight.  The weight of the $i$th element is denoted $w(V_i)$.
//
// We can create a new OMT, which is the empty vector.
//
// We can insert a new element $x$ into slot $i$, changing $V$ into $V'$ where
//  $|V'|=1+|V|$       and
//
//   V'_j = V_j       if $j<i$
//          x         if $j=i$
//          V_{j-1}   if $j>i$.
//
// We can specify $i$ using a kind of function instead of as an integer.
// Let $b$ be a function mapping from values to nonzero integers, such that
// the signum of $b$ is monotically increasing.
// We can specify $i$ as the minimum integer such that $b(V_i)>0$.
//
// We look up a value using its index, or using a Heaviside function.
// For lookups, we allow $b$ to be zero for some values, and again the signum of $b$ must be monotonically increasing.
// When lookup up values, we can look up
//  $V_i$ where $i$ is the minimum integer such that $b(V_i)=0$.   (With a special return code if no such value exists.)
//      (Rationale:  Ordinarily we want $i$ to be unique.  But for various reasons we want to allow multiple zeros, and we want the smallest $i$ in that case.)
//  $V_i$ where $i$ is the minimum integer such that $b(V_i)>0$.   (Or an indication that no such value exists.)
//  $V_i$ where $i$ is the maximum integer such that $b(V_i)<0$.   (Or an indication that no such value exists.)
//
// When looking up a value using a Heaviside function, we get the value and its index.
//
// We can also split an OMT into two OMTs, splitting the weight of the values evenly.
// Find a value $j$ such that the values to the left of $j$ have about the same total weight as the values to the right of $j$.
// The resulting two OMTs contain the values to the left of $j$ and the values to the right of $j$ respectively.
// All of the values from the original OMT go into one of the new OMTs.
// If the weights of the values don't split exactly evenly, then the implementation has the freedom to choose whether
//  the new left OMT or the new right OMT is larger.
//
// Performance:
//  Insertion and deletion should run with $O(\log |V|)$ time and $O(\log |V|)$ calls to the Heaviside function.
//  The memory required is O(|V|).
//
// The programming API:

typedef struct value *OMTVALUE; // A slight improvement over using void*.
typedef struct oma *OMT;

int toku_oma_create (OMT *omap);
// Effect: Create an empty OMT.  Stores it in *omap.
// Requires: omap != NULL
// Returns:
//   0        success
//   ENOMEM   out of memory (and doesn't modify *omap)
// Performance: constant time.

int toku_oma_create_from_sorted_array(OMT* omap, OMTVALUE *values, u_int32_t numvalues);
// Effect: Create a OMT containing values.  The number of values is in numvalues.
//  Stores the new OMT in *omap.
// Requires: omap != NULL
// Requires: values != NULL
// Returns:
//   0        success
//   ENOMEM   out of memory (and doesn't modify *omap)
// Performance:  time=O(numvalues)
// Rational:     Normally to insert N values takes O(N lg N) amortized time.
//               If the N values are known in advance, are sorted, and
//               the structure is empty, we can batch insert them much faster.
// Hack:         Can be temporarily implemented in O(numvalues * lg numvalues)
//               by wrapping toku_oma_create and repeated toku_oma_insert_at
//               until we have time to implement properly.

void toku_oma_destroy(OMT *omap);
// Effect:  Destroy an OMT, freeing all its memory.
//   Does not free the OMTVALUEs stored in the OMT.
//   Those values may be freed before or after calling toku_oma_destroy.
//   Also sets *omap=NULL.
// Requires: omap != NULL
// Requires: *omap != NULL
// Rationale:  The usage is to do something like
//   toku_oma_destroy(&s->oma);
// and now s->oma will have a NULL pointer instead of a dangling freed pointer.
// Rationale: Returns no values since free() cannot fail.
// Rationale: Does not free the OMTVALUEs to reduce complexity.
// Performance:  time=O(toku_oma_size(*omap))

u_int32_t toku_oma_size(OMT V);
// Effect: return |V|.
// Requires: V != NULL
// Performance:  time=O(1)

int toku_oma_iterate(OMT oma, int (*f)(OMTVALUE, u_int32_t, void*), void*v);
// Effect:  Iterate over the values of the oma, from left to right, calling f on each value.
//  The second argument passed to f is the index of the value.
//  The third argument passed to f is v.
//  The indices run from 0 (inclusive) to toku_oma_size(oma) (exclusive).
// Requires: oma != NULL
// Requires: f != NULL
// Returns:
//  If f ever returns nonzero, then the iteration stops, and the value returned by f is returned by toku_oma_iterate.
//  If f always returns zero, then toku_oma_iterate returns 0.
// Requires:  Don't modify oma while running.  (E.g., f may not insert or delete values form oma.)
// Performance: time=O(i+\log N) where i is the number of times f is called, and N is the number of elements in oma.
// Rational: Although the functional iterator requires defining another function (as opposed to C++ style iterator), it is much easier to read.

int toku_oma_insert_at(OMT oma, OMTVALUE value, u_int32_t index);
// Effect: Increases indexes of all items at slot >= index by 1.
//         Insert value into the position at index.
// Requires: oma != NULL
// Requires: value != NULL
//
// Returns:
//   0         success
//   ERANGE    if index>toku_oma_size(oma)
//   ENOMEM
// On error, oma is unchanged.
// Performance: time=O(\log N) amortized time.
// Rationale: Some future implementation may be O(\log N) worst-case time, but O(\log N) amortized is good enough for now.

int toku_oma_insert(OMT oma, OMTVALUE value, int(*h)(OMTVALUE, void*v), void *v, u_int32_t* index);
// Effect:  Insert value into the OMT.
//   If there is some i such that $h(V_i, v)=0$ then returns DB_KEYEXIST.
//   Otherwise, let i be the minimum value such that $h(V_i, v)>0$.
//      If no such i exists, then let i be |V|
//   Then this has the same effect as
//    oma_insert_at(tree, value, i);
//   i is stored in *index
// Requires: oma != NULL
// Requires: value != NULL
// Requires: index != NULL
// Requires:  The signum of h must be monotonically increasing.
// Returns:
//    0            success
//    DB_KEYEXIST  the key is present (h was equal to zero for some value)
//    ENOMEM      
// On nonzero return, oma is unchanged.
// On nonzero non-DB_KEYEXIST return, *index is unchanged.
// Performance: time=O(\log N) amortized.
// Rationale: Some future implementation may be O(\log N) worst-case time, but O(\log N) amortized is good enough for now.

int toku_oma_delete_at(OMT oma, u_int32_t index);
// Effect: Delete the item in slot index.
//         Decreases indexes of all items at slot >= index by 1.
// Requires: oma != NULL
// Returns
//     0            success
//     ERANGE       if index>=toku_oma_size(oma)
// On error, oma is unchanged.
// Rationale: To delete an item, first find its index using toku_oma_find, then delete it.
// Performance: time=O(\log N) amortized.


int toku_oma_find_index (OMT V, u_int32_t i, VALUE *v);
// Effect: Set *v=V_i
// Requires: oma != NULL
// Requires: v   != NULL
// Returns
//    0             success
//    ERANGE        if i out of range
// On nonzero return, *v is unchanged.
// Performance: time=O(\log N)

int toku_oma_find(OMT V, int (*h)(VALUE, void*extra), void*extra, int direction, VALUE *value, u_int32_t *index);
// Effect:
//  If direction==0 then find the smallest i such that h(V_i,extra)==0. 
//  If direction >0 then find the smallest i such that h(V_i,extra)>0.
//  If direction <0 then find the largest  i such that h(V_i,extra)<0.
//  store V_i in *value
//  store i in *index
// Requires: V != NULL
// Requires: h != NULL
// Requires: value != NULL
// Requires: index != NULL
// Returns
//    0             success
//    DB_NOTFOUND   no such value is found.
// On nonzero return, *value and *index are unchanged.
// Performance: time=O(\log N)

int toku_oma_split_at(OMT oma, OMT *newoma, u_int32_t index);
// Effect: Create a new OMT, storing it in *newoma.
//  The values to the right of index (starting at index) are moved to *newoma.
// Requires: oma != NULL
// Requires: newoma != NULL
// Returns
//    0             success,
//    ERANGE        if index >= toku_oma_size(oma)
//    ENOMEM
// On nonzero return, oma and *newoma are unmodified.
// Performance: time=O(n)
// Rationale:  We don't need a split-evenly operation.  We need to split items so that their total sizes
//  are even, and other similar splitting criteria.  It's easy to split evenly by calling toku_oma_size(), and dividing by two.
 
int toku_oma_merge(OMT leftoma, OMT rightoma, OMT *newoma);
// Effect: Appends leftoma and rightoma to produce a new oma.
//  Sets *newoma to the new oma.
//  leftoma and rightoma are left unchanged.
// Requires: leftoma != NULL
// Requires: rightoma != NULL
// Requires: newoma != NULL
// Returns 0 on success
//   ENOMEM on out of memory.
// On error, nothing is modified.
// Performance: time=O(n) is acceptable, but one can imagine implementations that are O(\log n) worst-case.

#endif  /* #ifndef OMT_H */
