/******************************************************
The hash table with external chains

(c) 1994-1997 Innobase Oy

Created 8/18/1994 Heikki Tuuri
*******************************************************/

#ifndef ha0ha_h
#define ha0ha_h

#include "univ.i"

#include "hash0hash.h"
#include "page0types.h"

/*****************************************************************
Looks for an element in a hash table. */
UNIV_INLINE
void*
ha_search_and_get_data(
/*===================*/
				/* out: pointer to the data of the first hash
				table node in chain having the fold number,
				NULL if not found */
	hash_table_t*	table,	/* in: hash table */
	ulint		fold);	/* in: folded value of the searched data */
/*************************************************************
Looks for an element when we know the pointer to the data and updates
the pointer to data if found. */
UNIV_INLINE
void
ha_search_and_update_if_found(
/*==========================*/
	hash_table_t*	table,	/* in: hash table */
	ulint		fold,	/* in: folded value of the searched data */
	void*		data,	/* in: pointer to the data */
	void*		new_data);/* in: new pointer to the data */
/*****************************************************************
Creates a hash table with >= n array cells. The actual number of cells is
chosen to be a prime number slightly bigger than n. */

hash_table_t*
ha_create(
/*======*/
				/* out, own: created table */
	ibool	in_btr_search,	/* in: TRUE if the hash table is used in
				the btr_search module */
	ulint	n,		/* in: number of array cells */
	ulint	n_mutexes,	/* in: number of mutexes to protect the
				hash table: must be a power of 2 */
	ulint	mutex_level);	/* in: level of the mutexes in the latching
				order: this is used in the debug version */
/*****************************************************************
Inserts an entry into a hash table. If an entry with the same fold number
is found, its node is updated to point to the new data, and no new node
is inserted. */

ibool
ha_insert_for_fold(
/*===============*/
				/* out: TRUE if succeed, FALSE if no more
				memory could be allocated */
	hash_table_t*	table,	/* in: hash table */
	ulint		fold,	/* in: folded value of data; if a node with
				the same fold value already exists, it is
				updated to point to the same data, and no new
				node is created! */
	void*		data);	/* in: data, must not be NULL */
/*****************************************************************
Reserves the necessary hash table mutex and inserts an entry into the hash
table. */
UNIV_INLINE
ibool
ha_insert_for_fold_mutex(
/*=====================*/
				/* out: TRUE if succeed, FALSE if no more
				memory could be allocated */
	hash_table_t*	table,	/* in: hash table */
	ulint		fold,	/* in: folded value of data; if a node with
				the same fold value already exists, it is
				updated to point to the same data, and no new
				node is created! */
	void*		data);	/* in: data, must not be NULL */
/*****************************************************************
Deletes an entry from a hash table. */

void
ha_delete(
/*======*/
	hash_table_t*	table,	/* in: hash table */
	ulint		fold,	/* in: folded value of data */
	void*		data);	/* in: data, must not be NULL and must exist
				in the hash table */
/*************************************************************
Looks for an element when we know the pointer to the data and deletes
it from the hash table if found. */
UNIV_INLINE
ibool
ha_search_and_delete_if_found(
/*==========================*/
				/* out: TRUE if found */
	hash_table_t*	table,	/* in: hash table */
	ulint		fold,	/* in: folded value of the searched data */
	void*		data);	/* in: pointer to the data */
/*********************************************************************
Removes from the chain determined by fold all nodes whose data pointer
points to the page given. */

void
ha_remove_all_nodes_to_page(
/*========================*/
	hash_table_t*	table,	/* in: hash table */
	ulint		fold,	/* in: fold value */
	page_t*		page);	/* in: buffer page */
/*****************************************************************
Validates a hash table. */

ibool
ha_validate(
/*========*/
				/* out: TRUE if ok */
	hash_table_t*	table);	/* in: hash table */
/*****************************************************************
Prints info of a hash table. */

void
ha_print_info(
/*==========*/
	hash_table_t*	table);	/* in: hash table */


#ifndef UNIV_NONINL
#include "ha0ha.ic"
#endif

#endif 
