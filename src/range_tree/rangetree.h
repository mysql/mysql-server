/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007-8 Tokutek Inc.  All rights reserved."

#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

/**
 * Range trees
 *
 * This is the header file for range trees. */

#include <brttypes.h>

/** Represents a range of data with an extra value.
  * Parameters are never modified on failure with the exception of
  * buf and buflen.
 */
typedef struct {
	void*   left;
  	void*   right;
	void*   data; 
} toku_range;

struct __toku_range_tree_internal {
    //Shared fields:
    int         (*end_cmp)(void*,void*);
    int         (*data_cmp)(void*,void*);
    BOOL        allow_overlaps;
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

/* These lines will remain. */
struct __toku_range_tree_internal;
typedef struct __toku_range_tree_internal toku_range_tree;

/**
 *  Creates a range tree.
 *  Parameters:
 *      ptree:          *ptree will point to the new range tree if successful.
 *      end_cmp:        User provided function to compare end points.
 *                      Return value conforms to cmp in qsort(3).
 *      data_cmp:       User provided function to compare data values.
 *                      Return value conforms to cmp in qsort(3).
 *      allow_overlaps: Whether ranges in this range tree are permitted to
 *                      overlap.
 *  Exit codes:
 *      0:              Success.
 *      EINVAL:         If any pointer argument is NULL.
 *  Other exit codes may be forwarded from underlying system calls.
 */
int toku_rt_create(toku_range_tree** ptree, 
                   int (*end_cmp)(void*,void*), int (*data_cmp)(void*,void*),
                   BOOL allow_overlaps);

/**
 *  Destroys and frees a range tree.
 *  Parameters:
 *      tree:           The range tree to free.
 *  Exit codes:
 *      0:              Success.
 *      EINVAL:         If tree is NULL.
 *  Other exit codes may be forwarded from underlying system calls.
 */
int toku_rt_close(toku_range_tree* tree);

/**
 *  Finds ranges in the range tree that overlap a query range.
 *  Parameters:
 *      tree:           The range tree to search in.
 *      k:              The maximum number of ranges to return.
 *                      The special value '0' is used to request ALL overlapping
 *                      ranges.
 *      query:          The range to query.
 *                      range.data must be NULL.
 *      buf:            A pointer to the buffer used to return ranges.
 *                      The buffer will be increased using realloc(3) if
 *                      necessary.
 *                      NOTE: buf must have been dynamically allocated i.e.
 *                      via malloc(3), calloc(3), etc.
 *      buflen:         A pointer to the lenfth of the buffer.  If the buffer
 *                      is increased, then buflen will be updated.
 *      numfound:       The number of ranges found.
 *                      This will necessarily be <= k.
 *                      If k != 0 && numfound == k, there may be additional
 *                      ranges that overlap the queried range but were skipped
 *                      to accomodate the request of k.
 *  Exit codes:
 *      0:              Success.
 *      EINVAL:         If any pointer argument is NULL.
 *                      If range.data != NULL.
 *                      If buflen == 0.
 *  Other exit codes may be forwarded from underlying system calls.
 *  It may be useful in the future to add an extra out parameter to specify
 *  whether more elements exist in the tree that overlap (in excess of the
 *  requested limit of k).
 */
int toku_rt_find(toku_range_tree* tree, toku_range* query, unsigned k,
                 toku_range** buf, unsigned* buflen, unsigned* numfound);

/**
 *  Inserts a range into the range tree.
 *  Parameters:
 *      tree:           The range tree to insert into.
 *      range:          The range to insert.
 *  Exit codes:
 *      0:              Success.
 *      EINVAL:         If any pointer argument is NULL.
 *      EDOM:           If the exact range (left, right, and data) already
 *                      exists in the tree.
 *                      If an overlapping range exists in the tree and
 *                      allow_overlaps == FALSE.
 *  Other exit codes may be forwarded from underlying system calls.
 */
int toku_rt_insert(toku_range_tree* tree, toku_range* range);

/**
 *  Deletes a range from the range tree.
 *  Parameters:
 *      tree:           The range tree to delete from.
 *      range:          The range to delete.
 *  Exit codes:
 *      0:              Success.
 *      EINVAL:         If any pointer argument is NULL.
 *      EDOM:           If the exact range (left, right, and data) did
 *                      not exist in the tree.
 *  Other exit codes may be forwarded from underlying system calls.
 */
int toku_rt_delete(toku_range_tree* tree, toku_range* range);

/**
 *  Finds the strict predecessor range of a point i.e. the rightmost range
 *  completely to the left of the query point according to end_cmp.
 *  This operation is only defined if allow_overlaps == FALSE.
 *  Parameters:
 *      tree:           The range tree to search in.
 *      point:          The point to query.  Must be a valid argument to
 *                      end_cmp.
 *      pred:           A buffer to return the predecessor.
 *      wasfound:       Whether a predecessor was found.
 *                      If no range is strictly to the left of the query point
 *                      this will be false.
 *  Exit codes:
 *      0:              Success.
 *      EINVAL:         If any pointer argument is NULL.
 *                      If tree allows overlaps.
 *  Other exit codes may be forwarded from underlying system calls.
 */
int toku_rt_predecessor(toku_range_tree* tree, void* point, toku_range* pred,
                        BOOL* wasfound);

/**
 *  Finds the strict successor range of a point i.e. the leftmost range
 *  completely to the right of the query point according to end_cmp.
 *  This operation is only defined if allow_overlaps == FALSE.
 *  Parameters:
 *      tree:           The range tree to search in.
 *      point:          The point to query.  Must be a valid argument to
 *                      end_cmp.
 *      succ:           A buffer to return the successor range.
 *      wasfound:       Whether a predecessor was found.
 *                      If no range is strictly to the left of the query point
 *                      this will be false.
 *  Exit codes:
 *      0:              Success.
 *      EINVAL:         If any pointer argument is NULL.
 *                      If tree allows overlaps.
 *  Other exit codes may be forwarded from underlying system calls.
 */
int toku_rt_successor(toku_range_tree* tree, void* point, toku_range* succ,
                      BOOL* wasfound);
