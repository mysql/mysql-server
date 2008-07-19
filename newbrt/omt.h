#if !defined(OMT_H)
#define OMT_H

#ident "Copyright (c) 2008 Tokutek Inc.  All rights reserved."

// Order Maintenance Tree (OMT)
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
// OMTs also support cursors.   An OMTCURSOR is a  mutable
// An OMTCURSOR is a mutable object that, at any moment in time, is
// either associated with a single OMT or is not associated with any
// OMT.  Many different OMTCURSORs can be associated with a single OMT.
//
// We say that an OMTCURSOR is *invalid* if it is not currently
// associated with an OMT.
//
// Abstractly, an OMTCURSOR simply contains an integer offset of a
// particular OMTVALUE.   We call this abstract integer the *offset*.
// Note, however, that the implementation may use a more
// complex representation in order to obtain higher performance.
// (Note: A first implementation might use the integer.)
//
// Given a valid OMTCURSOR, one
//  * obtain the OMTVALUE at which the integer points in O(1) time,
//  * increment or decrement the abstract integer (usually quickly.)
//    The requirements are that the cursor is initialized to a
//    randomly chosen valid integer, then the integer can be
//    incremented in O(1) expected time.

// The OMTCURSOR may become invalidated under several conditions:
//  * Incrementing or decrementing the abstract integer out of its
//    valid range invalidates the OMTCURSOR.
//  * If the OMT is modified, it may invalidate the cursor.
//  * The user of the OMTCURSOR may explicitly invalidate the cursor.
//  * The OMT is destroyed (in which case the OMTCURSOR is
//    invalidated, but not destroyed.)


// Implementation Hints
//
// One way to implement the OMTCURSOR is with an integer.  The problem
// is that obtaining the value at which the integer
// points takes O(\log n) time, which is not fast enough to meet the
// specification.    However, this implementation is probably much
// faster than our current implementation because it is O(\log n)
// integer comparisons instead of O(\log n) key comparisons.  This
// simple implementation may be the right thing for a first cut.
//
// To actually achieve the performance requirements, here's a better
// implementation:   The OMTCURSOR contains a path from root to leaf.
// Fetching the current value is O(1) time since the leaf is
// immediately accessible.   Modifying the path to find the next or
// previous item has O(1) expected time at a randomly chosen valid
// point
//
// The path can be implemented as an array.  It probably makes sense
// for the array to by dynamically resized as needed.  Since the
// array's size is O(log n), it is not necessary to ever shrink the
// array.  Also, from the perspective of testing, it's probably best
// if the array is initialized to a short length (e.g., length 4) so
// that the doubling code is actually exercised.
//
// One way to implement invalidation is for each OMT to maintain a
// doubly linked list of OMTCURSORs.  When destroying an OMT or
// changing the OMT's shape, one can simply step through the list
// invalidating all the OMTCURSORs.
//
// The list of OMTCURSORs should use the list.h abstraction.  If it's
// not clear how to use it, Rich can explain it.



// Usage Hint:   The OMTCURSOR is designed to be used inside the
// BRTcursor.   A BRTcursor includes a pointer to an OMTCURSOR, which
// is created when the BRTcursor is created.
//
// The brt cursor implements its search by first finding a leaf node,
// containing an OMT.  The BRT then passes its OMTCURSOR into the lookup
// method (i.e., one of toku_ebdomt_fetch, toku_omt_find_zero,
// toku_omt_find).  The lookup method, if successful, sets the
// OMTCURSOR to refer to that element.
//
// As long as the OMTCURSOR remains valid, a BRTCURSOR next or prev
// operation can be implemented using next or prev on the OMTCURSOR.
//
// If the OMTCURSOR becomes invalidated, then the BRT must search
// again from the root of the tree.   The only error that an OMTCURSOR
// next operation  can raise is that it is invalid.
//
// If an element is inserted into the BRT, it may cause an OMTCURSOR
// to become invalid.  This is especially true if the element will end
// up in the OMT associated with the cursor.  A simple implementation
// is to invalidate all OMTCURSORS any time anything is inserted into
// into the BRT.  Since the BRT already contains a list of BRT cursors
// associated with it, it is straightforward to go through that list
// and invalidate all the cursors.
//
// When the BRT closes a cursor, it destroys the OMTCURSOR.



// The programming API:

//typedef struct value *OMTVALUE; // A slight improvement over using void*.
typedef struct omt *OMT;
typedef struct omt_cursor *OMTCURSOR;


int toku_omt_create (OMT *omtp);
// Effect: Create an empty OMT.  Stores it in *omtp.
// Requires: omtp != NULL
// Returns:
//   0        success
//   ENOMEM   out of memory (and doesn't modify *omtp)
// Performance: constant time.

int toku_omt_create_from_sorted_array(OMT *omtp, OMTVALUE *values, u_int32_t numvalues);
// Effect: Create a OMT containing values.  The number of values is in numvalues.
//  Stores the new OMT in *omtp.
// Requires: omtp != NULL
// Requires: values != NULL
// Requires: values is sorted
// Returns:
//   0        success
//   ENOMEM   out of memory (and doesn't modify *omtp)
// Performance:  time=O(numvalues)
// Rational:     Normally to insert N values takes O(N lg N) amortized time.
//               If the N values are known in advance, are sorted, and
//               the structure is empty, we can batch insert them much faster.

void toku_omt_destroy(OMT *omtp);
// Effect:  Destroy an OMT, freeing all its memory.
//   Does not free the OMTVALUEs stored in the OMT.
//   Those values may be freed before or after calling toku_omt_destroy.
//   Also sets *omtp=NULL.
// Requires: omtp != NULL
// Requires: *omtp != NULL
// Rationale:  The usage is to do something like
//   toku_omt_destroy(&s->omt);
// and now s->omt will have a NULL pointer instead of a dangling freed pointer.
// Rationale: Returns no values since free() cannot fail.
// Rationale: Does not free the OMTVALUEs to reduce complexity.
// Performance:  time=O(toku_omt_size(*omtp))

u_int32_t toku_omt_size(OMT V);
// Effect: return |V|.
// Requires: V != NULL
// Performance:  time=O(1)

int toku_omt_iterate_on_range(OMT omt, u_int32_t left, u_int32_t right, int (*f)(OMTVALUE, u_int32_t, void*), void*v);
// Effect:  Iterate over the values of the omt, from left to right, calling f on each value.
//  The second argument passed to f is the index of the value.
//  The third argument passed to f is v.
//  The indices run from 0 (inclusive) to toku_omt_size(omt) (exclusive).
//  We will iterate only over [left,right)
//
// Requires: omt != NULL
// left <= right
// Requires: f != NULL
// Returns:
//  If f ever returns nonzero, then the iteration stops, and the value returned by f is returned by toku_omt_iterate.
//  If f always returns zero, then toku_omt_iterate returns 0.
// Requires:  Don't modify omt while running.  (E.g., f may not insert or delete values form omt.)
// Performance: time=O(i+\log N) where i is the number of times f is called, and N is the number of elements in omt.
// Rational: Although the functional iterator requires defining another function (as opposed to C++ style iterator), it is much easier to read.

int toku_omt_iterate(OMT omt, int (*f)(OMTVALUE, u_int32_t, void*), void*v);
// Effect:  Iterate over the values of the omt, from left to right, calling f on each value.
//  The second argument passed to f is the index of the value.
//  The third argument passed to f is v.
//  The indices run from 0 (inclusive) to toku_omt_size(omt) (exclusive).
// Requires: omt != NULL
// Requires: f != NULL
// Returns:
//  If f ever returns nonzero, then the iteration stops, and the value returned by f is returned by toku_omt_iterate.
//  If f always returns zero, then toku_omt_iterate returns 0.
// Requires:  Don't modify omt while running.  (E.g., f may not insert or delete values form omt.)
// Performance: time=O(i+\log N) where i is the number of times f is called, and N is the number of elements in omt.
// Rational: Although the functional iterator requires defining another function (as opposed to C++ style iterator), it is much easier to read.

int toku_omt_insert_at(OMT omt, OMTVALUE value, u_int32_t index);
// Effect: Increases indexes of all items at slot >= index by 1.
//         Insert value into the position at index.
//
// Returns:
//   0         success
//   EINVAL    if index>toku_omt_size(omt)
//   ENOMEM
// On error, omt is unchanged.
// Performance: time=O(\log N) amortized time.
// Rationale: Some future implementation may be O(\log N) worst-case time, but O(\log N) amortized is good enough for now.

int toku_omt_set_at (OMT omt, OMTVALUE value, u_int32_t index);
// Effect:  Replaces the item at index with value.
// Returns:
//   0       success
//   EINVAL  if index>=toku_omt_size(omt)
// On error, omt i sunchanged.
// Performance: time=O(\log N)
// Rationale: The BRT needs to be able to replace a value with another copy of the same value (allocated in a different location)

int toku_omt_insert(OMT omt, OMTVALUE value, int(*h)(OMTVALUE, void*v), void *v, u_int32_t *index);
// Effect:  Insert value into the OMT.
//   If there is some i such that $h(V_i, v)=0$ then returns DB_KEYEXIST.
//   Otherwise, let i be the minimum value such that $h(V_i, v)>0$.
//      If no such i exists, then let i be |V|
//   Then this has the same effect as
//    omt_insert_at(tree, value, i);
//   If index!=NULL then i is stored in *index
// Requires:  The signum of h must be monotonically increasing.
// Returns:
//    0            success
//    DB_KEYEXIST  the key is present (h was equal to zero for some value)
//    ENOMEM      
// On nonzero return, omt is unchanged.
// On nonzero non-DB_KEYEXIST return, *index is unchanged.
// Performance: time=O(\log N) amortized.
// Rationale: Some future implementation may be O(\log N) worst-case time, but O(\log N) amortized is good enough for now.

int toku_omt_delete_at(OMT omt, u_int32_t index);
// Effect: Delete the item in slot index.
//         Decreases indexes of all items at slot >= index by 1.
// Returns
//     0            success
//     EINVAL       if index>=toku_omt_size(omt)
// On error, omt is unchanged.
// Rationale: To delete an item, first find its index using toku_omt_find, then delete it.
// Performance: time=O(\log N) amortized.


int toku_omt_fetch (OMT V, u_int32_t i, OMTVALUE *v, OMTCURSOR c);
// Effect: Set *v=V_i
//   If c!=NULL then set c's abstract offset to i.
// Requires: v   != NULL
// Returns
//    0             success
//    EINVAL        if index>=toku_omt_size(omt)
// On nonzero return, *v is unchanged, and c (if nonnull) is either
//   invalidated or unchanged.
// Performance: time=O(\log N)
// Implementation Notes: It is possible that c was previously valid and was
//   associated with a different OMT.   If c is changed by this
//   function, the function must remove c's association with the old
//   OMT, and associate it with the new OMT.

int toku_omt_find_zero(OMT V, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index, OMTCURSOR c);
// Effect:  Find the smallest i such that h(V_i, extra)>=0
//  If there is such an i and h(V_i,extra)==0 then set *index=i and return 0.
//  If there is such an i and h(V_i,extra)>0  then set *index=i and return DB_NOTFOUND.
//  If there is no such i then set *index=toku_omt_size(V) and return DB_NOTFOUND.
// Requires: index!=NULL

int toku_omt_find(OMT V, int (*h)(OMTVALUE, void*extra), void*extra, int direction, OMTVALUE *value, u_int32_t *index, OMTCURSOR c);
//   Effect:
//    If direction >0 then find the smallest i such that h(V_i,extra)>0.
//    If direction <0 then find the largest  i such that h(V_i,extra)<0.
//    (Direction may not be equal to zero.)
//    If value!=NULL then store V_i in *value
//    If index!=NULL then store i in *index.
//   Requires: The signum of h is monotically increasing.
//   Returns
//      0             success
//      DB_NOTFOUND   no such value is found.
//   On nonzero return, *value and *index are unchanged, and c (if nonnull) is either invalidated
//      or unchanged.
//   Performance: time=O(\log N)
//   Rationale:
//     Here's how to use the find function to find various things
//       Cases for find:
//        find first value:         ( h(v)=+1, direction=+1 )
//        find last value           ( h(v)=-1, direction=-1 )
//        find first X              ( h(v)=(v< x) ? -1 : 1    direction=+1 )
//        find last X               ( h(v)=(v<=x) ? -1 : 1    direction=-1 )
//        find X or successor to X  ( same as find first X. )
//
//   Rationale: To help understand heaviside functions and behavor of find:
//    There are 7 kinds of heaviside functions.
//    The signus of the h must be monotonically increasing.
//    Given a function of the following form, A is the element
//    returned for direction>0, B is the element returned
//    for direction<0, C is the element returned for
//    direction==0 (see find_zero) (with a return of 0), and D is the element
//    returned for direction==0 (see find_zero) with a return of DB_NOTFOUND.
//    If any of A, B, or C are not found, then asking for the
//    associated direction will return DB_NOTFOUND.
//    See find_zero for more information.
//    
//    Let the following represent the signus of the heaviside function.
//
//    -...-
//        A
//         D
//
//    +...+
//    B
//    D
//
//    0...0
//    C
//
//    -...-0...0
//        AC
//
//    0...0+...+
//    C    B
//
//    -...-+...+
//        AB
//         D
//
//    -...-0...0+...+
//        AC    B

int toku_omt_split_at(OMT omt, OMT *newomt, u_int32_t index);
// Effect: Create a new OMT, storing it in *newomt.
//  The values to the right of index (starting at index) are moved to *newomt.
// Requires: omt != NULL
// Requires: newomt != NULL
// Returns
//    0             success,
//    EINVAL        if index > toku_omt_size(omt)
//    ENOMEM
// On nonzero return, omt and *newomt are unmodified.
// Performance: time=O(n)
// Rationale:  We don't need a split-evenly operation.  We need to split items so that their total sizes
//  are even, and other similar splitting criteria.  It's easy to split evenly by calling toku_omt_size(), and dividing by two.
 
int toku_omt_merge(OMT leftomt, OMT rightomt, OMT *newomt);
// Effect: Appends leftomt and rightomt to produce a new omt.
//  Sets *newomt to the new omt.
//  On success, leftomt and rightomt destroyed,.
// Returns 0 on success
//   ENOMEM on out of memory.
// On error, nothing is modified.
// Performance: time=O(n) is acceptable, but one can imagine implementations that are O(\log n) worst-case.

void toku_omt_clear(OMT omt);
// Effect: Set the tree to be empty.
//  Note: Will not reallocate or resize any memory, since returning void precludes calling malloc.
// Performance: time=O(1)

unsigned long toku_omt_memory_size (OMT omt);
// Effect: Return the size (in bytes) of the omt, as it resides in main memory.  Don't include any of the OMTVALUES.

int toku_omt_cursor_create (OMTCURSOR *p);
// Effect: Create an OMTCURSOR.  Stores it in *p.  The OMTCURSOR is
// initially invalid.
// Requires: p != NULL
// Returns:
//   0        success
//   ENOMEM   out of memory (and doesn't modify *omtp)
// Performance: constant time.

void toku_omt_cursor_destroy (OMTCURSOR *p);
// Effect:  Invalidates *p (if it is valid) and frees any memory
// associated with *p.
//  Also sets *p=NULL.
// Rationale:  The usage is to do something like
//   toku_omt_cursor_destroy(&c);
// and now c will have a NULL pointer instead of a dangling freed pointer.
// Rationale: Returns no values since free() cannot fail.

int toku_omt_cursor_is_valid (OMTCURSOR c);
// Effect:  returns 0 iff c is invalid.
// Performance:  time=O(1)

int toku_omt_cursor_next (OMTCURSOR c, OMTVALUE *v);
// Effect: Increment c's offset, and find and store the value in v.
// Requires: v != NULL
// Returns
//   0 success
//   EINVAL if the offset goes out of range or c is invalid.
// On nonzero return, *v is unchanged and c is invalidated.
// Performance:  time=O(log N) worst case, expected time=O(1) for a randomly
//  chosen initial position.

int toku_omt_cursor_current_index(OMTCURSOR c, u_int32_t *index);
// Effect: Stores c's offset in *index.
// Requires: index != NULL
// Returns
//  0 success
//  EINVAL c is invalid
// On nonzero return, *index is unchanged and c is unchanged.
// Performance: time=O(1)

OMT toku_omt_cursor_get_omt(OMTCURSOR c);
// Effect: returns the associated omt or NULL if not associated.
// Performance: time=O(1)

int toku_omt_cursor_current (OMTCURSOR c, OMTVALUE *v);
// Effect: Store in v the value pointed by c's abstract offset
// Requires: v != NULL
// Returns
//  0 success
//  EINVAL if c is invalid
// On non-zero return, *v is unchanged
// Performance: O(1) time

int toku_omt_cursor_prev (OMTCURSOR c, OMTVALUE *v);
// Effect: Decrement c's offset, and find and store the value in v.
// Requires: v != NULL
// Returns
//   0 success
//   EINVAL if the offset goes out of range or c is invalid.
// On nonzero return, *v is unchanged and c is invalidated.
// Performance:  time=O(log N) worst case, expected time=O(1) for a randomly
//  chosen initial position.


void toku_omt_cursor_invalidate (OMTCURSOR c);
// Effect: Invalidate c.  (This does not mean that c is destroyed or
// that its memory is freed.)
// If c is valid, the invalidate callback function (if any) will be called
// before invalidating c.

void toku_omt_cursor_set_invalidate_callback(OMTCURSOR c, void (*f)(OMTCURSOR,void*), void* extra);
// Effect:
//  Saves function 'f' to be called whenever the cursor is invalidated.
//  'extra' is passed as an additional parameter to f.
// Requires:
//  The lifetime of the 'extra' parameter must continue at least till the cursor
//  is destroyed.

#endif  /* #ifndef OMT_H */

