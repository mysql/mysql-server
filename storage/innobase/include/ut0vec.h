/*****************************************************************************

Copyright (c) 2006, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ut0vec.h
 A vector of pointers to data items

 Created 4/6/2006 Osku Salerma
 ************************************************************************/

#ifndef IB_VECTOR_H
#define IB_VECTOR_H

#include "mem0mem.h"
#include "univ.i"

struct ib_alloc_t;
struct ib_vector_t;

typedef void *(*ib_mem_alloc_t)(
    /* out: Pointer to allocated memory */
    ib_alloc_t *allocator, /* in: Pointer to allocator instance */
    ulint size);           /* in: Number of bytes to allocate */

typedef void (*ib_mem_free_t)(
    ib_alloc_t *allocator, /* in: Pointer to allocator instance */
    void *ptr);            /* in: Memory to free */

typedef void *(*ib_mem_resize_t)(
    /* out: Pointer to resized memory */
    ib_alloc_t *allocator, /* in: Pointer to allocator */
    void *ptr,             /* in: Memory to resize */
    ulint old_size,        /* in: Old memory size in bytes */
    ulint new_size);       /* in: New size in bytes */

typedef int (*ib_compare_t)(const void *, const void *);

/* An automatically resizing vector datatype with the following properties:

 -All memory allocation is done through an allocator, which is  responsible for
freeing it when done with the vector.
*/

/* This is useful shorthand for elements of type void* */
#define ib_vector_getp(v, n) (*(void **)ib_vector_get(v, n))
#define ib_vector_getp_const(v, n) (*(void **)ib_vector_get_const(v, n))

#define ib_vector_allocator(v) (v->allocator)

/********************************************************************
Create a new vector with the given initial size. */
ib_vector_t *ib_vector_create(
    /* out: vector */
    ib_alloc_t *alloc,               /* in: Allocator */
                                     /* in: size of the data item */
    ulint sizeof_value, ulint size); /* in: initial size */

/********************************************************************
Destroy the vector. Make sure the vector owns the allocator, e.g.,
the heap in the the heap allocator. */
static inline void ib_vector_free(ib_vector_t *vec); /* in/out: vector */

/********************************************************************
Push a new element to the vector, increasing its size if necessary,
if elem is not NULL then elem is copied to the vector.*/
static inline void *ib_vector_push(
    /* out: pointer the "new" element */
    ib_vector_t *vec,  /* in/out: vector */
    const void *elem); /* in: data element */

/********************************************************************
Pop the last element from the vector.*/
static inline void *ib_vector_pop(
    /* out: pointer to the "new" element */
    ib_vector_t *vec); /* in/out: vector */

/** Remove an element to the vector
@param[in]      vec     vector
@param[in]      elem    value to remove
@return pointer to the "removed" element */
static inline void *ib_vector_remove(ib_vector_t *vec, const void *elem);

/********************************************************************
Get the number of elements in the vector. */
static inline ulint ib_vector_size(
    /* out: number of elements in vector */
    const ib_vector_t *vec); /* in: vector */

/********************************************************************
Increase the size of the vector. */
void ib_vector_resize(
    /* out: number of elements in vector */
    ib_vector_t *vec); /* in/out: vector */

/********************************************************************
Test whether a vector is empty or not.
@return true if empty */
static inline bool ib_vector_is_empty(
    const ib_vector_t *vec); /*!< in: vector */

/** Get the n'th element.
@param[in]      vec     vector
@param[in]      n       element index to get
@return n'th element */
static inline void *ib_vector_get(ib_vector_t *vec, ulint n);

/********************************************************************
Const version of the get n'th element.
@return n'th element */
static inline const void *ib_vector_get_const(
    const ib_vector_t *vec, /* in: vector */
    ulint n);               /* in: element index to get */
/** Get last element. The vector must not be empty.
 @return last element */
static inline void *ib_vector_get_last(ib_vector_t *vec); /*!< in: vector */

/** Set the n'th element.
@param[in]      vec     vector
@param[in]      n       element index to set
@param[in]      elem    data element */
static inline void ib_vector_set(ib_vector_t *vec, ulint n, void *elem);

/********************************************************************
Reset the vector size to 0 elements. */
static inline void ib_vector_reset(ib_vector_t *vec); /* in/out: vector */

/********************************************************************
Get the last element of the vector. */
static inline void *ib_vector_last(
    /* out: pointer to last element */
    ib_vector_t *vec); /* in/out: vector */

/********************************************************************
Get the last element of the vector. */
static inline const void *ib_vector_last_const(
    /* out: pointer to last element */
    const ib_vector_t *vec); /* in: vector */

/********************************************************************
Sort the vector elements. */
static inline void ib_vector_sort(
    ib_vector_t *vec,      /* in/out: vector */
    ib_compare_t compare); /* in: the comparator to use for sort */

/********************************************************************
The default ib_vector_t heap free. Does nothing. */
static inline void ib_heap_free(ib_alloc_t *allocator, /* in: allocator */
                                void *ptr);            /* in: size in bytes */

/********************************************************************
The default ib_vector_t heap malloc. Uses mem_heap_alloc(). */
static inline void *ib_heap_malloc(
    /* out: pointer to allocated memory */
    ib_alloc_t *allocator, /* in: allocator */
    ulint size);           /* in: size in bytes */

/********************************************************************
The default ib_vector_t heap resize. Since we can't resize the heap
we have to copy the elements from the old ptr to the new ptr.
Uses mem_heap_alloc(). */
static inline void *ib_heap_resize(
    /* out: pointer to reallocated
    memory */
    ib_alloc_t *allocator, /* in: allocator */
    void *old_ptr,         /* in: pointer to memory */
    ulint old_size,        /* in: old size in bytes */
    ulint new_size);       /* in: new size in bytes */

/********************************************************************
Create a heap allocator that uses the passed in heap. */
static inline ib_alloc_t *ib_heap_allocator_create(
    /* out: heap allocator instance */
    mem_heap_t *heap); /* in: heap to use */

/********************************************************************
Free a heap allocator. */
static inline void ib_heap_allocator_free(
    ib_alloc_t *ib_ut_alloc); /* in: alloc instance to free */

/* Allocator used by ib_vector_t. */
struct ib_alloc_t {
  ib_mem_alloc_t mem_malloc;  /* For allocating memory */
  ib_mem_free_t mem_release;  /* For freeing memory */
  ib_mem_resize_t mem_resize; /* For resizing memory */
  void *arg;                  /* Currently if not NULL then it
                              points to the heap instance */
};

/* See comment at beginning of file. */
struct ib_vector_t {
  ib_alloc_t *allocator; /* Allocator, because one size
                         doesn't fit all */
  void *data;            /* data elements */
  ulint used;            /* number of elements currently used */
  ulint total;           /* number of elements allocated */
                         /* Size of a data item */
  ulint sizeof_value;
};

#include "ut0vec.ic"

#endif /* IB_VECTOR_H */
