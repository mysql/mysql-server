/******************************************************
The simple hash table utility

(c) 1997 Innobase Oy

Created 5/20/1997 Heikki Tuuri
*******************************************************/

#ifndef hash0hash_h
#define hash0hash_h

#include "univ.i"
#include "mem0mem.h"
#include "sync0sync.h"

typedef struct hash_table_struct hash_table_t;
typedef struct hash_cell_struct hash_cell_t;

typedef void*	hash_node_t;

/*****************************************************************
Creates a hash table with >= n array cells. The actual number
of cells is chosen to be a prime number slightly bigger than n. */

hash_table_t*
hash_create(
/*========*/
			/* out, own: created table */
	ulint	n);	/* in: number of array cells */
/*****************************************************************
Creates a mutex array to protect a hash table. */

void
hash_create_mutexes(
/*================*/
	hash_table_t*	table,		/* in: hash table */
	ulint		n_mutexes,	/* in: number of mutexes */
	ulint		sync_level);	/* in: latching order level of the
					mutexes: used in the debug version */
/*****************************************************************
Frees a hash table. */

void
hash_table_free(
/*============*/
	hash_table_t*	table);	/* in, own: hash table */
/******************************************************************
Calculates the hash value from a folded value. */
UNIV_INLINE
ulint
hash_calc_hash(
/*===========*/
				/* out: hashed value */
	ulint		fold,	/* in: folded value */
	hash_table_t*	table);	/* in: hash table */
/***********************************************************************
Inserts a struct to a hash table. */

#define HASH_INSERT(TYPE, NAME, TABLE, FOLD, DATA)\
{\
	hash_cell_t*	cell3333;\
	TYPE*		struct3333;\
\
	ut_ad(!(TABLE)->mutexes || mutex_own(hash_get_mutex(TABLE, FOLD)));\
\
	(DATA)->NAME = NULL;\
\
	cell3333 = hash_get_nth_cell(TABLE, hash_calc_hash(FOLD, TABLE));\
\
	if (cell3333->node == NULL) {\
		cell3333->node = DATA;\
	} else {\
		struct3333 = cell3333->node;\
\
		while (struct3333->NAME != NULL) {\
\
			struct3333 = struct3333->NAME;\
		}\
\
		struct3333->NAME = DATA;\
	}\
}

/***********************************************************************
Deletes a struct from a hash table. */

#define HASH_DELETE(TYPE, NAME, TABLE, FOLD, DATA)\
{\
	hash_cell_t*	cell3333;\
	TYPE*		struct3333;\
\
	ut_ad(!(TABLE)->mutexes || mutex_own(hash_get_mutex(TABLE, FOLD)));\
\
	cell3333 = hash_get_nth_cell(TABLE, hash_calc_hash(FOLD, TABLE));\
\
	if (cell3333->node == DATA) {\
		cell3333->node = DATA->NAME;\
	} else {\
		struct3333 = cell3333->node;\
\
		while (struct3333->NAME != DATA) {\
\
			ut_ad(struct3333)\
			struct3333 = struct3333->NAME;\
		}\
\
		struct3333->NAME = DATA->NAME;\
	}\
}

/***********************************************************************
Gets the first struct in a hash chain, NULL if none. */

#define HASH_GET_FIRST(TABLE, HASH_VAL)\
	(hash_get_nth_cell(TABLE, HASH_VAL)->node)

/***********************************************************************
Gets the next struct in a hash chain, NULL if none. */

#define HASH_GET_NEXT(NAME, DATA)	((DATA)->NAME)

/************************************************************************
Looks for a struct in a hash table. */
#define HASH_SEARCH(NAME, TABLE, FOLD, DATA, TEST)\
{\
\
	ut_ad(!(TABLE)->mutexes || mutex_own(hash_get_mutex(TABLE, FOLD)));\
\
	(DATA) = HASH_GET_FIRST(TABLE, hash_calc_hash(FOLD, TABLE));\
\
	while ((DATA) != NULL) {\
		if (TEST) {\
			break;\
		} else {\
			(DATA) = HASH_GET_NEXT(NAME, DATA);\
		}\
	}\
}

/****************************************************************
Gets the nth cell in a hash table. */
UNIV_INLINE
hash_cell_t*
hash_get_nth_cell(
/*==============*/
				/* out: pointer to cell */
	hash_table_t* 	table,	/* in: hash table */
	ulint 		n);	/* in: cell index */
/*****************************************************************
Returns the number of cells in a hash table. */
UNIV_INLINE
ulint
hash_get_n_cells(
/*=============*/
				/* out: number of cells */
	hash_table_t*	table);	/* in: table */
/***********************************************************************
Deletes a struct which is stored in the heap of the hash table, and compacts
the heap. The fold value must be stored in the struct NODE in a field named
'fold'. */

#define HASH_DELETE_AND_COMPACT(TYPE, NAME, TABLE, NODE)\
{\
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
}

/***********************************************************************
Calculates the number of stored structs in a hash table. */

#define HASH_GET_N_NODES(TYPE, NAME, TABLE, N)\
{\
	hash_cell_t*	cell3333;\
	TYPE*		struct3333;\
	ulint		i3333;\
\
	(N) = 0;\
\
	for (i3333 = 0; i3333 < hash_get_n_cells(TABLE); i3333++) {\
\
		cell3333 = hash_get_nth_cell(TABLE, i3333);\
\
		struct3333 = cell3333->node;\
\
		while (struct3333) {\
\
			(N) = (N) + 1;\
\
			struct = HASH_GET_NEXT(NAME, struct3333);\
		}\
	}\
}

/****************************************************************
Gets the mutex index for a fold value in a hash table. */
UNIV_INLINE
ulint
hash_get_mutex_no(
/*==============*/
				/* out: mutex number */
	hash_table_t* 	table,	/* in: hash table */
	ulint 		fold);	/* in: fold */
/****************************************************************
Gets the nth heap in a hash table. */
UNIV_INLINE
mem_heap_t*
hash_get_nth_heap(
/*==============*/
				/* out: mem heap */
	hash_table_t* 	table,	/* in: hash table */
	ulint 		i);	/* in: index of the heap */
/****************************************************************
Gets the heap for a fold value in a hash table. */
UNIV_INLINE
mem_heap_t*
hash_get_heap(
/*==========*/
				/* out: mem heap */
	hash_table_t* 	table,	/* in: hash table */
	ulint 		fold);	/* in: fold */
/****************************************************************
Gets the nth mutex in a hash table. */
UNIV_INLINE
mutex_t*
hash_get_nth_mutex(
/*===============*/
				/* out: mutex */
	hash_table_t* 	table,	/* in: hash table */
	ulint 		i);	/* in: index of the mutex */
/****************************************************************
Gets the mutex for a fold value in a hash table. */
UNIV_INLINE
mutex_t*
hash_get_mutex(
/*===========*/
				/* out: mutex */
	hash_table_t* 	table,	/* in: hash table */
	ulint 		fold);	/* in: fold */
/****************************************************************
Reserves the mutex for a fold value in a hash table. */

void
hash_mutex_enter(
/*=============*/
	hash_table_t* 	table,	/* in: hash table */
	ulint 		fold);	/* in: fold */
/****************************************************************
Releases the mutex for a fold value in a hash table. */

void
hash_mutex_exit(
/*============*/
	hash_table_t* 	table,	/* in: hash table */
	ulint 		fold);	/* in: fold */
/****************************************************************
Reserves all the mutexes of a hash table, in an ascending order. */

void
hash_mutex_enter_all(
/*=================*/
	hash_table_t* 	table);	/* in: hash table */
/****************************************************************
Releases all the mutexes of a hash table. */

void
hash_mutex_exit_all(
/*================*/
	hash_table_t* 	table);	/* in: hash table */


struct hash_cell_struct{
	void*	node;	/* hash chain node, NULL if none */
};

/* The hash table structure */
struct hash_table_struct {
	ulint		n_cells;/* number of cells in the hash table */
	hash_cell_t*	array;	/* pointer to cell array */
	ulint		n_mutexes;/* if mutexes != NULL, then the number of
				mutexes, must be a power of 2 */
	mutex_t*	mutexes;/* NULL, or an array of mutexes used to
				protect segments of the hash table */
	mem_heap_t**	heaps;	/* if this is non-NULL, hash chain nodes for
				external chaining can be allocated from these
				memory heaps; there are then n_mutexes many of
				these heaps */
	mem_heap_t*	heap;
	ulint		magic_n;
};

#define HASH_TABLE_MAGIC_N	76561114

#ifndef UNIV_NONINL
#include "hash0hash.ic"
#endif

#endif
