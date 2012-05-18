/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef SORT_H
#define SORT_H
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// apes qsort_r which is not available in centos 5's version of libc
// is parallelized with cilk, therefore probably faster than qsort_r on large arrays
// TODO: switch to qsort_r for small arrays (at the bottom of the recursion)
//       this requires figuring out what to do about libc
//
// a: array of elements
// n: number of elements
// width: size of each element in bytes
// extra: extra data for comparison function (passed in as first parameter)
// cmp: comparison function, compatible with qsort_r
//
// Returns 0 on success.
int
mergesort_r(void *a, int n, int width,
            void *extra, int (*cmp)(void *, const void *, const void *));


#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
