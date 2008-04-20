#if !defined(OM_H)
#define OM_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

// Order Maintenance Array (OMA)
//
// Maintains a collection of totally ordered values, where each value has an integer weight.
// The OMA is a mutable datatype.
//
// The Abstraction:
//
// An OMA is a vector of values, $V$, where $|V|$ is the length of the vector.
// The vector is numbered from $0$ to $|V|-1$.
// Each value has a weight.  The weight of the $i$th element is denoted $w(V_i)$.
//
// We can create a new OMA, which is the empty vector.
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
// We can also split an OMA into two OMAs, splitting the weight of the values evenly.
// Find a value $j$ such that the values to the left of $j$ have about the same total weight as the values to the right of $j$.
// The resulting two OMAs contain the values to the left of $j$ and the values to the right of $j$ respectively.
// All of the values from the original OMA go into one of the new OMAs.
// If the weights of the values don't split exactly evenly, then the implementation has the freedom to choose whether
//  the new left OMA or the new right OMA is larger.
//
// Performance:
//  Insertion and deletion should run with $O(\log |V|)$ time and $O(\log |V|)$ calls to the Heaviside function.
//  The memory required is O(|V|).
//
// The programming API:

typedef struct value *OMAVALUE; // A slight improvement over using void*.
typedef struct oma *OMA;

int toku_oma_create (OMA *omap);
// Effect: Create an empty OMA.  Stores it in *omap.
// Returns:
//   0        success
//   ENOMEM   out of memory (and doesn't modify *omap)
// Performance: constant time.

int toku_oma_create_from_sorted_array(OMA* omap, OMAVALUE *values, u_int32_t numvalues);
// Effect: Create a OMA containing values.  The number of values is in numvalues.
//  Stores the new OMA in *omap.
// Returns:
//   0        success
//   ENOMEM   out of memory (and doesn't modify *omap)
// Performance:  time=O(numvalues)

void toku_oma_destroy(OMA *omap);
// Effect:  Destroy an OMA, freeing all its memory.
//   Does not free the OMAVALUEs stored in the OMA.
//   Those values may be freed before or after calling toku_oma_destroy.
//   Also sets *omap=NULL.
// Rationale:  The usage is to do something like
//   toku_oma_destroy(&s->oma);
// and now s->oma will have a NULL pointer instead of a dangling freed pointer.
// Rationale: Returns no values since free() cannot fail.
// Performance:  time=O(toku_oma_size(*omap))

u_int32_t toku_oma_size(OMA V);
// Effect: return |V|.
// Performance:  time=O(1)

int toku_oma_iterate(OMA oma, int (*f)(OMAVALUE, u_int32_t, void*), void*v);
// Effect:  Iterate over the values of the oma, from left to right, calling f on each value.
//  The second argument passed to f is the index of the value.
//  The third argument passed to f is v.
//  The indices run from 0 (inclusive) to toku_oma_size(oma) (exclusive).
// Returns:
//  If f ever returns nonzero, then the iteration stops, and the value returned by f is returned by toku_oma_iterate.
//  If f always returns zero, then toku_oma_iterate returns 0.
// Requires:  Don't modify oma while running.  (E.g., f may not insert or delete values form oma.)
// Performance: time=O(i+\log N) where i is the number of times f is called, and N is the number of elements in oma.

int toku_oma_insert_at(OMA oma, OMAVALUE value, u_int32_t index);
// Effect: Insert value into the position at index, moving everything to the right up one slot.
// Returns:
//   0         success
//   ERANGE    if index>toku_oma_size(oma)
//   ENOMEM
// On error, oma is unchanged.
// Performance: time=O(\log N) amortized time.
// Rationale: Some future implementation may be O(\log N) worst-case time, but O(\log N) amortized is good enough for now.

int toku_oma_insert(OMA oma, OMAVALUE value, int(*h)(OMAVALUE, void*v), void *v, u_int32_t* index);
// Effect:  Insert value into the OMA.
//   If there is some i such that $h(V_i, v)=0$ then returns DB_KEYEXIST.
//   Otherwise, let i be the minimum value such that $h(V_i, v)>0$.  Then this has the same effect as
//    oma_insert_at(tree, vlaue, i);
// Requires:  The signum of h must be monotonically increasing.
// Returns:
//    0            success
//    DB_KEYEXIST  the key is present (h was equal to zero for some value)
//    ENOMEM      
// On nonzero return, oma is unchanged.
// Performance: time=O(\log N) amortized.

int toku_oma_delete_at(OMA oma, u_int32_t index);
// Effect: Delete the item in slot index.
// Returns
//     0            success
//     ERANGE       if index out of range
//     ENOMEM
// On error, oma is unchanged.
// Rationale: To delete an item, first find its index using toku_oma_find, then delete it.
// Performance: time=O(\log N) amortized.


int toku_oma_find_index (OMA V, u_int32_t i, VALUE *v);
// Effect: Set *v=V_i
// Returns 0 on success
//    ERANGE   if i out of range (and doesn't modify v)
// Performance: time=O(\log N)

int toku_oma_find(OMA V, int (*h)(VALUE, void*extra), void*extra, int direction, VALUE *value, u_int32_t *index);
// Effect:
//  If direction==0 then find the smallest i such that h(V_i,extra)==0. 
//  If direction>0 then find the smallest i such that h(V_i,extra)>0.
//  If direction<0 then find the largest i such that h(V_i,extra)<0.
//    If no such vlaue is found, then return DB_NOTFOUND,
//    otherwise return 0 and set *value=V_i and set *index=i.
// Performance: time=O(\log N)

int toku_oma_split_at(OMA oma, OMA *newoma, u_itn32_t index);
// Effect: Create a new OMA, storing it in *newoma.
//  The values to the right of index (starting at index) are moved to *newoma.
// Returns 0 on success,
//   ERANGE if index out of range
//   ENOMEM
// On nonzero return, oma and *newoma are unmodified.
// Performance: time=O(n)
// Rationale:  We don't need a split-evenly operation.  We need to split items so that their total sizes
//  are even, and other similar splitting criteria.  It's easy to split evenly by calling toku_oma_size(), and dividing by two.
 
int toku_oma_merge(OMA leftoma, OMA rightoma, OMA *newoma);
// Effect: Appends leftoma and rightoma to produce a new oma.
//  Sets *newoma to the new oma.
//  leftoma and rightoma are left unchanged.
// Returns 0 on success
//   ENOMEM on out of memory.
// On error, nothing is modified.
// Performance: time=O(n) is acceptable, but one can imagine implementations that are O(\log n) worst-case.

#endif  /* #ifndef OM_H */