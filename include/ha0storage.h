/******************************************************
Hash storage.
Provides a data structure that stores chunks of data in
its own storage, avoiding duplicates.

(c) 2007 Innobase Oy

Created September 22, 2007 Vasil Dimov
*******************************************************/

#ifndef ha0storage_h
#define ha0storage_h

#include "univ.i"

/* This value is used by default by ha_storage_create(). More memory
is allocated later when/if it is needed. */
#define HA_STORAGE_DEFAULT_HEAP_BYTES	1024

/* This value is used by default by ha_storage_create(). It is a
constant per ha_storage's lifetime. */
#define HA_STORAGE_DEFAULT_HASH_CELLS	4096

typedef struct ha_storage_struct	ha_storage_t;

/***********************************************************************
Creates a hash storage. If any of the parameters is 0, then a default
value is used. */
UNIV_INLINE
ha_storage_t*
ha_storage_create(
/*==============*/
					/* out, own: hash storage */
	ulint	initial_heap_bytes,	/* in: initial heap's size */
	ulint	initial_hash_cells);	/* in: initial number of cells
					in the hash table */

/***********************************************************************
Copies string into the storage and returns a pointer to the copy. If the
same string is already present, then pointer to it is returned.
Strings are considered to be equal if strcmp(str1, str2) == 0. */

#define ha_storage_put_str(storage, str)	\
	((const char*) ha_storage_put((storage), (str), strlen(str) + 1))

/***********************************************************************
Copies data into the storage and returns a pointer to the copy. If the
same data chunk is already present, then pointer to it is returned.
Data chunks are considered to be equal if len1 == len2 and
memcmp(data1, data2, len1) == 0. */

const void*
ha_storage_put(
/*===========*/
	ha_storage_t*	storage,	/* in/out: hash storage */
	const void*	data,		/* in: data to store */
	ulint		data_len);	/* in: data length */

/***********************************************************************
Empties a hash storage, freeing memory occupied by data chunks.
This invalidates any pointers previously returned by ha_storage_put().
The hash storage is not invalidated itself and can be used again. */
UNIV_INLINE
void
ha_storage_empty(
/*=============*/
	ha_storage_t**	storage);	/* in/out: hash storage */

/***********************************************************************
Frees a hash storage and everything it contains, it cannot be used after
this call.
This invalidates any pointers previously returned by ha_storage_put().
*/
UNIV_INLINE
void
ha_storage_free(
/*============*/
	ha_storage_t*	storage);	/* in/out: hash storage */

#ifndef UNIV_NONINL
#include "ha0storage.ic"
#endif

#endif /* ha0storage_h */
