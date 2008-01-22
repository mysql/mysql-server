/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
   \file  rangetree.h
   \brief Range trees: header and comments
  
   Range trees are an ordered data structure to hold intervals.
   You can learn about range trees at 
   http://en.wikipedia.org/wiki/Interval_tree
   or by consulting the textbooks, e.g., 
   Thomas H. Cormen, Charles E. Leiserson, Ronald L. Rivest, and 
   Clifford Stein. Introduction to Algorithms, Second Edition. 
   MIT Press and McGraw-Hill, 2001. ISBN 0-262-03293-7
*/

#include <brttypes.h>

/** \brief Range with value
    Represents a range of data with an associated value.
    Parameters are never modified on failure with the exception of
    buf and buflen.
 */
typedef struct {
	void*   left;  /**< Left end-point */
  	void*   right; /**< Right end-point */
	void*   data;  /**< Data associated with the range */
} toku_range;

/** \brief Internal range representation 
    Internal representation of a range tree. Some fields depend on the
    implementation of range trees, and some others are shared. */
struct __toku_range_tree_internal {
    //Shared fields:
    /** A comparison function, as in bsearch(3), to compare the end-points of 
        a range */
    int         (*end_cmp)(void*,void*);  
    /** A comparison function, as in bsearch(3), to compare the data associated
        with a range */
    int         (*data_cmp)(void*,void*);
    /** Whether this tree allows ranges to overlap */
    BOOL        allow_overlaps;
   /** The number of ranges in the range tree */
    unsigned    numelements;
#if defined(TOKU_LINEAR_RANGE_TREE)
    #if defined(TOKU_LOG_RANGE_TREE)
        #error Choose just one range tree type.
    #endif
    //Linear version only fields:
    toku_range* ranges;
    unsigned    ranges_len;
#elif defined(TOKU_LOG_RANGE_TREE)
    #error Not defined yet.
    //Log version only fields:
#else
    #error Using an undefined RANGE TREE TYPE.
#endif
};

/** Export the internal representation to a sensible name */
/*  These lines will remain. */
typedef struct __toku_range_tree_internal toku_range_tree;

/**
    Creates a range tree.

    \param ptree          Points to the new range tree if create is successful 
    \param end_cmp        User provided function to compare end points.
                          Return value conforms to cmp in qsort(3). 
    \param data_cmp       User provided function to compare data values.
                          Return value conforms to cmp in qsort(3). 
    \param allow_overlaps Whether ranges in this range tree are permitted 
                          to overlap. 

    \return
    - 0:              Success.
    - EINVAL:         If any pointer argument is NULL.
    - Other exit codes may be forwarded from underlying system calls.  */
int toku_rt_create(toku_range_tree** ptree, int (*end_cmp)(void*,void*), 
                   int (*data_cmp)(void*,void*), BOOL allow_overlaps); 

/**
    Destroys and frees a range tree.
    \return
    - 0:              Success.
    - EINVAL:         If tree is NULL.
    - Other exit codes may be forwarded from underlying system calls.
 */
int toku_rt_close(toku_range_tree* tree  /**< The range tree to free */);

/**
   Finds ranges in the range tree that overlap a query range.

   \param tree     The range tree to search in. 
   \param query    The range to query. range.data must be NULL. 
   \param k        The maximum number of ranges to return. 
                   The special value '0' is used to request ALL overlapping 
                   ranges. 
   \param buf      A pointer to the buffer used to return ranges.
                   The buffer will be increased using realloc(3) if necessary.
                   NOTE: buf must have been dynamically allocated i.e. via 
                   malloc(3), calloc(3), etc.
                   The buffer will not be modified unless it is too small.
   \param buflen   A pointer to the lenfth of the buffer.  
                   If the buffer is increased, then buflen will be updated. 
   \param numfound The number of ranges found. This will necessarily be <= k.
                   If k != 0 && numfound == k, there may be additional
                   ranges that overlap the queried range but were skipped
                   to accomodate the request of k. 

   \return
   - 0:              Success.
   - EINVAL:         If any pointer argument is NULL. If range.data != NULL.
                     If buflen == 0.
   - Other exit codes may be forwarded from underlying system calls.

    Growth direction: It may be useful in the future to add an extra out 
    parameter to specify whether more elements exist in the tree that overlap 
    (in excess of the requested limit of k).
 */
int toku_rt_find(toku_range_tree* tree,toku_range* query, unsigned k,
                 toku_range** buf, unsigned* buflen, unsigned* numfound);
 

/**
    Inserts a range into the range tree.

    \param tree            The range tree to insert into.
    \param range           The range to insert.

    \return
    - 0:              Success.
    - EINVAL:         If any pointer argument is NULL.
    - EDOM:           If the exact range (left, right, and data) already
                      exists in the tree.
                      If an overlapping range exists in the tree and
                      allow_overlaps == FALSE.
    - Other exit codes may be forwarded from underlying system calls.
 */
int toku_rt_insert(toku_range_tree* tree, toku_range* range);

/**
    Deletes a range from the range tree.

    \param tree            The range tree to delete from.
    \param range           The range to delete.

    \return
    - 0:              Success.
    - EINVAL:         If any pointer argument is NULL.
    - EDOM:           If the exact range (left, right, and data) did
                      not exist in the tree.
    - Other exit codes may be forwarded from underlying system calls.
 */
int toku_rt_delete(toku_range_tree* tree, toku_range* range);

/**
    Finds the strict predecessor range of a point i.e. the rightmost range
    completely to the left of the query point according to end_cmp.
    This operation is only defined if allow_overlaps == FALSE.

    \param tree            The range tree to search in.
    \param point           The point to query.  Must be a valid argument to
                           end_cmp.
    \param pred            A buffer to return the predecessor.
    \param wasfound        Whether a predecessor was found.
                           If no range is strictly to the left of the query 
                           point this will be false.

    \return
    - 0:              Success.
    - EINVAL:         If any pointer argument is NULL.
                      If tree allows overlaps.
    - Other exit codes may be forwarded from underlying system calls.
 */
int toku_rt_predecessor(toku_range_tree* tree, void* point, toku_range* pred,
                        BOOL* wasfound);

/**
    Finds the strict successor range of a point i.e. the leftmost range
    completely to the right of the query point according to end_cmp.
    This operation is only defined if allow_overlaps == FALSE.

    \param tree         The range tree to search in.
    \param point        The point to query.  Must be a valid argument to
                        end_cmp.
    \param succ         A buffer to return the successor range.
    \param wasfound     Whether a predecessor was found.
                        If no range is strictly to the left of the query point
                        this will be false.

    \return
    - 0:              Success.
    - EINVAL:         If any pointer argument is NULL.
                      If tree allows overlaps.
    - Other exit codes may be forwarded from underlying system calls.
 */
int toku_rt_successor(toku_range_tree* tree, void* point, toku_range* succ,
                      BOOL* wasfound);
