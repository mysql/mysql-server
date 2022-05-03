/* Copyright (c) 2010, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef XDR_UTILS_H
#define XDR_UTILS_H

#include <assert.h>
#include "xcom/xcom_memory.h"

#ifdef __APPLE__
/* OSX missing xdr_sizeof() */
extern "C" u_long xdr_sizeof(xdrproc_t, void *);
#endif

/**
    Initialize an array
 */
#define def_init_xdr_array(name) \
  static inline void init_##name##_array(name##_array *x)
#define init_xdr_array(name)                                            \
  def_init_xdr_array(name) {                                            \
    x->name##_array_len = 2;                                            \
    x->name##_array_val =                                               \
        (name *)xcom_calloc((size_t)x->name##_array_len, sizeof(name)); \
  }

/**
    Free the contents of an array
 */
#define def_free_xdr_array(name) \
  static inline void free_##name##_array(name##_array *x)
#define free_xdr_array(name)   \
  def_free_xdr_array(name) {   \
    free(x->name##_array_val); \
    x->name##_array_val = 0;   \
    x->name##_array_len = 0;   \
  }

#define in_range(x, name, n) \
  (((int)n) >= 0 && ((int)n) < ((int)(x).name##_array_len))

/**
    Resize an array
 */
#define expand_xdr_array(name)                                                 \
  u_int old_length = x->name##_array_len;                                      \
  if (n + 1 > (x->name##_array_len)) {                                         \
    if (x->name##_array_len == 0) x->name##_array_len = 1;                     \
    do {                                                                       \
      x->name##_array_len *= 2;                                                \
    } while (n + 1 > (x->name##_array_len));                                   \
    x->name##_array_val = (name *)realloc(x->name##_array_val,                 \
                                          x->name##_array_len * sizeof(name)); \
    memset(&x->name##_array_val[old_length], 0,                                \
           (x->name##_array_len - old_length) * sizeof(name));                 \
  }

/**
    Define a set function for an array
 */
#define def_set_xdr_array(name) \
  static inline void set_##name(name##_array *x, name a, u_int n)
#define set_xdr_array(name)          \
  def_set_xdr_array(name) {          \
    expand_xdr_array(name);          \
    assert(n < x->name##_array_len); \
    x->name##_array_val[n] = a;      \
  }

/**
    Define a get function for an array
 */
#define def_get_xdr_array(name) \
  static inline name get_##name(name##_array *x, u_int n)
#define get_xdr_array(name)          \
  def_get_xdr_array(name) {          \
    expand_xdr_array(name);          \
    assert(n < x->name##_array_len); \
    return x->name##_array_val[n];   \
  }

/**
    Define a function to clone an array
 */
#define def_clone_xdr_array(name) \
  static inline name##_array clone_##name##_array(name##_array x)
#define clone_xdr_array(name)                                            \
  def_clone_xdr_array(name) {                                            \
    name##_array retval = x;                                             \
    u_int i;                                                             \
    retval.name##_array_len = x.name##_array_len;                        \
    IFDBG(D_XDR, FN; NDBG(retval.name##_array_len, u));                  \
    if (retval.name##_array_len > 0) {                                   \
      retval.name##_array_val =                                          \
          (name *)xcom_calloc((size_t)x.name##_array_len, sizeof(name)); \
      for (i = 0; i < retval.name##_array_len; i++) {                    \
        retval.name##_array_val[i] = x.name##_array_val[i];              \
        IFDBG(D_XDR, FN; STRLIT("clone_xdr_array"); NDBG(i, u));         \
      }                                                                  \
    } else {                                                             \
      retval.name##_array_val = 0;                                       \
    }                                                                    \
    return retval;                                                       \
  }

/**
    Declare all functions for an array
 */
#define d_xdr_funcs(name)   \
  def_init_xdr_array(name); \
  def_free_xdr_array(name); \
  def_set_xdr_array(name);  \
  def_get_xdr_array(name);  \
  def_clone_xdr_array(name);

/**
    Define all functions for an array
 */
#define define_xdr_funcs(name)                                  \
  init_xdr_array(name) free_xdr_array(name) set_xdr_array(name) \
      get_xdr_array(name) clone_xdr_array(name)

/**
    Macro to do insertion sort
 */
#define insert_sort(type, x, n)                         \
  {                                                     \
    int i, j;                                           \
    for (i = 1; i < n; i++) { /* x[0..i-1] is sorted */ \
      type tmp;                                         \
      j = i;                                            \
      tmp = x[j];                                       \
      while (j > 0 && insert_sort_gt(x[j - 1], tmp)) {  \
        x[j] = x[j - 1];                                \
        j--;                                            \
      }                                                 \
      x[j] = tmp;                                       \
    }                                                   \
  }

/**
    Macro to do binary search for first occurrence

    Invariant: x[l] < key and x[u] >= key and l < u
*/
#define bin_search_first_body(x, first, last, key, p) \
  int l = first - 1;                                  \
  int u = last + 1;                                   \
  int m = 0;                                          \
  while (l + 1 != u) {                                \
    m = (l + u) / 2;                                  \
    if (bin_search_lt((x)[m], (key))) {               \
      l = m;                                          \
    } else {                                          \
      u = m;                                          \
    }                                                 \
  }

/**
    Macro to do binary search for last occurrence.

    Invariant: x[l] <= key and x[u] > key and l < u
*/
#define bin_search_last_body(x, first, last, key, p) \
  int l = first - 1;                                 \
  int u = last + 1;                                  \
  int m = 0;                                         \
  while (l + 1 != u) {                               \
    m = (l + u) / 2;                                 \
    if (bin_search_gt((x)[m], (key))) {              \
      u = m;                                         \
    } else {                                         \
      l = m;                                         \
    }                                                \
  }

/**
   Find first element which matches key
*/
#define bin_search_first(x, first, last, key, p)             \
  {                                                          \
    bin_search_first_body(x, first, last, key, p);           \
    p = u;                                                   \
    if (p > last || (!bin_search_eq((x)[p], (key)))) p = -1; \
  }

/**
 Find first element which is greater than key
*/
#define bin_search_first_gt(x, first, last, key, p)          \
  {                                                          \
    bin_search_last_body(x, first, last, key, p);            \
    p = u;                                                   \
    if (p > last || (!bin_search_gt((x)[p], (key)))) p = -1; \
  }

/**
   Find last element which matches key
*/
#define bin_search_last(x, first, last, key, p)               \
  {                                                           \
    bin_search_last_body(x, first, last, key, p);             \
    p = l;                                                    \
    if (p < first || (!bin_search_eq((x)[p], (key)))) p = -1; \
  }

/**
    Find first element which is less than key
*/
#define bin_search_last_lt(x, first, last, key, p)            \
  {                                                           \
    bin_search_first_body(x, first, last, key, p);            \
    p = l;                                                    \
    if (p < first || (!bin_search_lt((x)[p], (key)))) p = -1; \
  }

#define diff_get(type, a, i) get_##type##_array(a, i)
#define diff_output(type, x) set_##type##_array(&retval, x, retval_i++)
#define diff_gt(x, y) insert_sort_gt(x, y)

/**
    Macro to compute diff of two arrays, which as a side effect will
    be sorted after the operation has completed.
 */
#define diff_xdr_array(type, x, y)                                          \
  type##_array diff_##type##_array(type##_array x, type##_array y) {        \
    int x_i = 0;                                                            \
    int y_i = 0;                                                            \
    type retval;                                                            \
    int retval_i = 0;                                                       \
    init_##type##_array(&retval);                                           \
    insert_sort(type, x.type##_val, x.type##_len);                          \
    insert_sort(type, y.type##_val, y.type##_len);                          \
    while (x_i < x.type##_len && y < y.type##_len) {                        \
      if (diff_eq(diff_get(type, x, x_i), diff_get(type, y, y_i))) {        \
        x_i++;                                                              \
        y_i++;                                                              \
      } else if (diff_lt(diff_get(type, x, x_i), diff_get(type, y, y_i))) { \
        diff_output(type, diff_get(type, x, x_i++));                        \
      } else {                                                              \
        diff_output(type, diff_get(type, y, y_i++));                        \
      }                                                                     \
    }                                                                       \
    while (x_i < x.type##_len) {                                            \
      diff_output(type, diff_get(type, x, x_i++));                          \
    }                                                                       \
    while (y_i < y.type##_len) {                                            \
      diff_output(type, diff_get(type, y, y_i++));                          \
    }                                                                       \
    retval.type##_len = retval_i;                                           \
    return retval;                                                          \
  }

/* Reverse elements n1..n2 */

#define x_reverse(type, x, in_n1, in_n2) \
  {                                      \
    int n1 = in_n1;                      \
    int n2 = in_n2;                      \
    while ((n1) < (n2)) {                \
      type tmp = (x)[n1];                \
      (x)[n1] = (x)[n2];                 \
      (x)[n2] = tmp;                     \
      (n1)++;                            \
      (n2)--;                            \
    }                                    \
  }

/* Move elements n1..n2 to after n3 */

#define x_blkmove(type, x, n1, n2, n3)        \
  {                                           \
    if ((n3) < (n1)-1) {                      \
      x_reverse(type, (x), (n3) + 1, (n1)-1); \
      x_reverse(type, (x), (n1), (n2));       \
      x_reverse(type, (x), (n3) + 1, (n2));   \
    } else if ((n3) > (n2)) {                 \
      x_reverse(type, (x), (n1), (n2));       \
      x_reverse(type, (x), (n2) + 1, (n3));   \
      x_reverse(type, (x), (n1), (n3));       \
    }                                         \
  }

#endif
