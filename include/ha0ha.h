/*****************************************************************************

Copyright (c) 1994, 2009, Innobase Oy. All Rights Reserved.

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

/******************************************************
The hash table with external chains

Created 8/18/1994 Heikki Tuuri
*******************************************************/

#ifndef ha0ha_h
#define ha0ha_h

#include "univ.i"

#include "hash0hash.h"
#include "page0types.h"
#include "buf0types.h"

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
UNIV_INTERN
void
ha_search_and_update_if_found_func(
/*===============================*/
	hash_table_t*	table,	/* in: hash table */
	ulint		fold,	/* in: folded value of the searched data */
	void*		data,	/* in: pointer to the data */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	buf_block_t*	new_block,/* in: block containing new_data */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	void*		new_data);/* in: new pointer to the data */

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
# define ha_search_and_update_if_found(table,fold,data,new_block,new_data) \
	ha_search_and_update_if_found_func(table,fold,data,new_block,new_data)
#else /* UNIV_AHI_DEBUG || UNIV_DEBUG */
# define ha_search_and_update_if_found(table,fold,data,new_block,new_data) \
	ha_search_and_update_if_found_func(table,fold,data,new_data)
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
/*****************************************************************
Creates a hash table with >= n array cells. The actual number of cells is
chosen to be a prime number slightly bigger than n. */
UNIV_INTERN
hash_table_t*
ha_create_func(
/*===========*/
				/* out, own: created table */
	ulint	n,		/* in: number of array cells */
#ifdef UNIV_SYNC_DEBUG
	ulint	mutex_level,	/* in: level of the mutexes in the latching
				order: this is used in the debug version */
#endif /* UNIV_SYNC_DEBUG */
	ulint	n_mutexes);	/* in: number of mutexes to protect the
				hash table: must be a power of 2 */
#ifdef UNIV_SYNC_DEBUG
# define ha_create(n_c,n_m,level) ha_create_func(n_c,level,n_m)
#else /* UNIV_SYNC_DEBUG */
# define ha_create(n_c,n_m,level) ha_create_func(n_c,n_m)
#endif /* UNIV_SYNC_DEBUG */

/*****************************************************************
Empties a hash table and frees the memory heaps. */
UNIV_INTERN
void
ha_clear(
/*=====*/
	hash_table_t*	table);	/* in, own: hash table */

/*****************************************************************
Inserts an entry into a hash table. If an entry with the same fold number
is found, its node is updated to point to the new data, and no new node
is inserted. */
UNIV_INTERN
ibool
ha_insert_for_fold_func(
/*====================*/
				/* out: TRUE if succeed, FALSE if no more
				memory could be allocated */
	hash_table_t*	table,	/* in: hash table */
	ulint		fold,	/* in: folded value of data; if a node with
				the same fold value already exists, it is
				updated to point to the same data, and no new
				node is created! */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	buf_block_t*	block,	/* in: buffer block containing the data */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	void*		data);	/* in: data, must not be NULL */

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
# define ha_insert_for_fold(t,f,b,d) ha_insert_for_fold_func(t,f,b,d)
#else /* UNIV_AHI_DEBUG || UNIV_DEBUG */
# define ha_insert_for_fold(t,f,b,d) ha_insert_for_fold_func(t,f,d)
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */

/*****************************************************************
Deletes an entry from a hash table. */
UNIV_INTERN
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
UNIV_INTERN
void
ha_remove_all_nodes_to_page(
/*========================*/
	hash_table_t*	table,	/* in: hash table */
	ulint		fold,	/* in: fold value */
	const page_t*	page);	/* in: buffer page */
/*****************************************************************
Validates a given range of the cells in hash table. */
UNIV_INTERN
ibool
ha_validate(
/*========*/
					/* out: TRUE if ok */
	hash_table_t*	table,		/* in: hash table */
	ulint		start_index,	/* in: start index */
	ulint		end_index);	/* in: end index */
/*****************************************************************
Prints info of a hash table. */
UNIV_INTERN
void
ha_print_info(
/*==========*/
	FILE*		file,	/* in: file where to print */
	hash_table_t*	table);	/* in: hash table */

/* The hash table external chain node */

typedef struct ha_node_struct ha_node_t;
struct ha_node_struct {
	ha_node_t*	next;	/* next chain node or NULL if none */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	buf_block_t*	block;	/* buffer block containing the data, or NULL */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	void*		data;	/* pointer to the data */
	ulint		fold;	/* fold value for the data */
};

#ifndef UNIV_NONINL
#include "ha0ha.ic"
#endif

#endif
