/*****************************************************************************

Copyright (c) 1997, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file include/hash0hash.h
The simple hash table utility

Created 5/20/1997 Heikki Tuuri
*******************************************************/

#ifndef hash0hash_h
#define hash0hash_h

#include "univ.i"
#include "mem0mem.h"
#ifndef UNIV_HOTBACKUP
# include "sync0sync.h"
#endif /* !UNIV_HOTBACKUP */

typedef struct hash_table_struct hash_table_t;
typedef struct hash_cell_struct hash_cell_t;

typedef void*	hash_node_t;

/* Fix Bug #13859: symbol collision between imap/mysql */
#define hash_create hash0_create

/*************************************************************//**
Creates a hash table with >= n array cells. The actual number
of cells is chosen to be a prime number slightly bigger than n.
@return	own: created table */
UNIV_INTERN
hash_table_t*
hash_create(
/*========*/
	ulint	n);	/*!< in: number of array cells */
#ifndef UNIV_HOTBACKUP
/*************************************************************//**
Creates a mutex array to protect a hash table. */
UNIV_INTERN
void
hash_create_mutexes_func(
/*=====================*/
	hash_table_t*	table,		/*!< in: hash table */
#ifdef UNIV_SYNC_DEBUG
	ulint		sync_level,	/*!< in: latching order level of the
					mutexes: used in the debug version */
#endif /* UNIV_SYNC_DEBUG */
	ulint		n_mutexes);	/*!< in: number of mutexes */
#ifdef UNIV_SYNC_DEBUG
# define hash_create_mutexes(t,n,level) hash_create_mutexes_func(t,level,n)
#else /* UNIV_SYNC_DEBUG */
# define hash_create_mutexes(t,n,level) hash_create_mutexes_func(t,n)
#endif /* UNIV_SYNC_DEBUG */
#endif /* !UNIV_HOTBACKUP */

/*************************************************************//**
Frees a hash table. */
UNIV_INTERN
void
hash_table_free(
/*============*/
	hash_table_t*	table);	/*!< in, own: hash table */
/**************************************************************//**
Calculates the hash value from a folded value.
@return	hashed value */
UNIV_INLINE
ulint
hash_calc_hash(
/*===========*/
	ulint		fold,	/*!< in: folded value */
	hash_table_t*	table);	/*!< in: hash table */
#ifndef UNIV_HOTBACKUP
/********************************************************************//**
Assert that the mutex for the table in a hash operation is owned. */
# define HASH_ASSERT_OWNED(TABLE, FOLD)					\
ut_ad(!(TABLE)->mutexes || mutex_own(hash_get_mutex(TABLE, FOLD)));
#else /* !UNIV_HOTBACKUP */
# define HASH_ASSERT_OWNED(TABLE, FOLD)
#endif /* !UNIV_HOTBACKUP */

/*******************************************************************//**
Inserts a struct to a hash table. */

#define HASH_INSERT(TYPE, NAME, TABLE, FOLD, DATA)\
do {\
	hash_cell_t*	cell3333;\
	TYPE*		struct3333;\
\
	HASH_ASSERT_OWNED(TABLE, FOLD)\
\
	(DATA)->NAME = NULL;\
\
	cell3333 = hash_get_nth_cell(TABLE, hash_calc_hash(FOLD, TABLE));\
\
	if (cell3333->node == NULL) {\
		cell3333->node = DATA;\
	} else {\
		struct3333 = (TYPE*) cell3333->node;\
\
		while (struct3333->NAME != NULL) {\
\
			struct3333 = (TYPE*) struct3333->NAME;\
		}\
\
		struct3333->NAME = DATA;\
	}\
} while (0)

#ifdef UNIV_HASH_DEBUG
# define HASH_ASSERT_VALID(DATA) ut_a((void*) (DATA) != (void*) -1)
# define HASH_INVALIDATE(DATA, NAME) DATA->NAME = (void*) -1
#else
# define HASH_ASSERT_VALID(DATA) do {} while (0)
# define HASH_INVALIDATE(DATA, NAME) do {} while (0)
#endif

/*******************************************************************//**
Deletes a struct from a hash table. */

#define HASH_DELETE(TYPE, NAME, TABLE, FOLD, DATA)\
do {\
	hash_cell_t*	cell3333;\
	TYPE*		struct3333;\
\
	HASH_ASSERT_OWNED(TABLE, FOLD)\
\
	cell3333 = hash_get_nth_cell(TABLE, hash_calc_hash(FOLD, TABLE));\
\
	if (cell3333->node == DATA) {\
		HASH_ASSERT_VALID(DATA->NAME);\
		cell3333->node = DATA->NAME;\
	} else {\
		struct3333 = (TYPE*) cell3333->node;\
\
		while (struct3333->NAME != DATA) {\
\
			struct3333 = (TYPE*) struct3333->NAME;\
			ut_a(struct3333);\
		}\
\
		struct3333->NAME = DATA->NAME;\
	}\
	HASH_INVALIDATE(DATA, NAME);\
} while (0)

/*******************************************************************//**
Gets the first struct in a hash chain, NULL if none. */

#define HASH_GET_FIRST(TABLE, HASH_VAL)\
	(hash_get_nth_cell(TABLE, HASH_VAL)->node)

/*******************************************************************//**
Gets the next struct in a hash chain, NULL if none. */

#define HASH_GET_NEXT(NAME, DATA)	((DATA)->NAME)

/********************************************************************//**
Looks for a struct in a hash table. */
#define HASH_SEARCH(NAME, TABLE, FOLD, TYPE, DATA, ASSERTION, TEST)\
{\
\
	HASH_ASSERT_OWNED(TABLE, FOLD)\
\
	(DATA) = (TYPE) HASH_GET_FIRST(TABLE, hash_calc_hash(FOLD, TABLE));\
	HASH_ASSERT_VALID(DATA);\
\
	while ((DATA) != NULL) {\
		ASSERTION;\
		if (TEST) {\
			break;\
		} else {\
			HASH_ASSERT_VALID(HASH_GET_NEXT(NAME, DATA));\
			(DATA) = (TYPE) HASH_GET_NEXT(NAME, DATA);\
		}\
	}\
}

/********************************************************************//**
Looks for an item in all hash buckets. */
#define HASH_SEARCH_ALL(NAME, TABLE, TYPE, DATA, ASSERTION, TEST)	\
do {									\
	ulint	i3333;							\
									\
	for (i3333 = (TABLE)->n_cells; i3333--; ) {			\
		(DATA) = (TYPE) HASH_GET_FIRST(TABLE, i3333);		\
									\
		while ((DATA) != NULL) {				\
			HASH_ASSERT_VALID(DATA);			\
			ASSERTION;					\
									\
			if (TEST) {					\
				break;					\
			}						\
									\
			(DATA) = (TYPE) HASH_GET_NEXT(NAME, DATA);	\
		}							\
									\
		if ((DATA) != NULL) {					\
			break;						\
		}							\
	}								\
} while (0)

/************************************************************//**
Gets the nth cell in a hash table.
@return	pointer to cell */
UNIV_INLINE
hash_cell_t*
hash_get_nth_cell(
/*==============*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		n);	/*!< in: cell index */

/*************************************************************//**
Clears a hash table so that all the cells become empty. */
UNIV_INLINE
void
hash_table_clear(
/*=============*/
	hash_table_t*	table);	/*!< in/out: hash table */

/*************************************************************//**
Returns the number of cells in a hash table.
@return	number of cells */
UNIV_INLINE
ulint
hash_get_n_cells(
/*=============*/
	hash_table_t*	table);	/*!< in: table */
/*******************************************************************//**
Deletes a struct which is stored in the heap of the hash table, and compacts
the heap. The fold value must be stored in the struct NODE in a field named
'fold'. */

#define HASH_DELETE_AND_COMPACT(TYPE, NAME, TABLE, NODE)\
do {\
	TYPE*		node111;\
	TYPE*		top_node111;\
	hash_cell_t*	cell111;\
	ulint		fold111;\
\
	fold111 = (NODE)->fold;\
\
	HASH_DELETE(TYPE, NAME, TABLE, fold111, NODE);\
\
	top_node111 = (TYPE*)mem_heap_get_top(\
				hash_get_heap(TABLE, fold111),\
							sizeof(TYPE));\
\
	/* If the node to remove is not the top node in the heap, compact the\
	heap of nodes by moving the top node in the place of NODE. */\
\
	if (NODE != top_node111) {\
\
		/* Copy the top node in place of NODE */\
\
		*(NODE) = *top_node111;\
\
		cell111 = hash_get_nth_cell(TABLE,\
				hash_calc_hash(top_node111->fold, TABLE));\
\
		/* Look for the pointer to the top node, to update it */\
\
		if (cell111->node == top_node111) {\
			/* The top node is the first in the chain */\
\
			cell111->node = NODE;\
		} else {\
			/* We have to look for the predecessor of the top\
			node */\
			node111 = cell111->node;\
\
			while (top_node111 != HASH_GET_NEXT(NAME, node111)) {\
\
				node111 = HASH_GET_NEXT(NAME, node111);\
			}\
\
			/* Now we have the predecessor node */\
\
			node111->NAME = NODE;\
		}\
	}\
\
	/* Free the space occupied by the top node */\
\
	mem_heap_free_top(hash_get_heap(TABLE, fold111), sizeof(TYPE));\
} while (0)

#ifndef UNIV_HOTBACKUP
/****************************************************************//**
Move all hash table entries from OLD_TABLE to NEW_TABLE. */

#define HASH_MIGRATE(OLD_TABLE, NEW_TABLE, NODE_TYPE, PTR_NAME, FOLD_FUNC) \
do {\
	ulint		i2222;\
	ulint		cell_count2222;\
\
	cell_count2222 = hash_get_n_cells(OLD_TABLE);\
\
	for (i2222 = 0; i2222 < cell_count2222; i2222++) {\
		NODE_TYPE*	node2222 = HASH_GET_FIRST((OLD_TABLE), i2222);\
\
		while (node2222) {\
			NODE_TYPE*	next2222 = node2222->PTR_NAME;\
			ulint		fold2222 = FOLD_FUNC(node2222);\
\
			HASH_INSERT(NODE_TYPE, PTR_NAME, (NEW_TABLE),\
				fold2222, node2222);\
\
			node2222 = next2222;\
		}\
	}\
} while (0)

/************************************************************//**
Gets the mutex index for a fold value in a hash table.
@return	mutex number */
UNIV_INLINE
ulint
hash_get_mutex_no(
/*==============*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold);	/*!< in: fold */
/************************************************************//**
Gets the nth heap in a hash table.
@return	mem heap */
UNIV_INLINE
mem_heap_t*
hash_get_nth_heap(
/*==============*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		i);	/*!< in: index of the heap */
/************************************************************//**
Gets the heap for a fold value in a hash table.
@return	mem heap */
UNIV_INLINE
mem_heap_t*
hash_get_heap(
/*==========*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold);	/*!< in: fold */
/************************************************************//**
Gets the nth mutex in a hash table.
@return	mutex */
UNIV_INLINE
mutex_t*
hash_get_nth_mutex(
/*===============*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		i);	/*!< in: index of the mutex */
/************************************************************//**
Gets the mutex for a fold value in a hash table.
@return	mutex */
UNIV_INLINE
mutex_t*
hash_get_mutex(
/*===========*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold);	/*!< in: fold */
/************************************************************//**
Reserves the mutex for a fold value in a hash table. */
UNIV_INTERN
void
hash_mutex_enter(
/*=============*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold);	/*!< in: fold */
/************************************************************//**
Releases the mutex for a fold value in a hash table. */
UNIV_INTERN
void
hash_mutex_exit(
/*============*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold);	/*!< in: fold */
/************************************************************//**
Reserves all the mutexes of a hash table, in an ascending order. */
UNIV_INTERN
void
hash_mutex_enter_all(
/*=================*/
	hash_table_t*	table);	/*!< in: hash table */
/************************************************************//**
Releases all the mutexes of a hash table. */
UNIV_INTERN
void
hash_mutex_exit_all(
/*================*/
	hash_table_t*	table);	/*!< in: hash table */
#else /* !UNIV_HOTBACKUP */
# define hash_get_heap(table, fold)	((table)->heap)
# define hash_mutex_enter(table, fold)	((void) 0)
# define hash_mutex_exit(table, fold)	((void) 0)
#endif /* !UNIV_HOTBACKUP */

struct hash_cell_struct{
	void*	node;	/*!< hash chain node, NULL if none */
};

/* The hash table structure */
struct hash_table_struct {
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
# ifndef UNIV_HOTBACKUP
	ibool		adaptive;/* TRUE if this is the hash table of the
				adaptive hash index */
# endif /* !UNIV_HOTBACKUP */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	ulint		n_cells;/* number of cells in the hash table */
	hash_cell_t*	array;	/*!< pointer to cell array */
#ifndef UNIV_HOTBACKUP
	ulint		n_mutexes;/* if mutexes != NULL, then the number of
				mutexes, must be a power of 2 */
	mutex_t*	mutexes;/* NULL, or an array of mutexes used to
				protect segments of the hash table */
	mem_heap_t**	heaps;	/*!< if this is non-NULL, hash chain nodes for
				external chaining can be allocated from these
				memory heaps; there are then n_mutexes many of
				these heaps */
#endif /* !UNIV_HOTBACKUP */
	mem_heap_t*	heap;
#ifdef UNIV_DEBUG
	ulint		magic_n;
# define HASH_TABLE_MAGIC_N	76561114
#endif /* UNIV_DEBUG */
};

#ifndef UNIV_NONINL
#include "hash0hash.ic"
#endif

#endif
