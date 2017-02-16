/*****************************************************************************

Copyright (c) 1994, 2011, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

/**************************************************//**
@file include/ha0ha.h
The hash table with external chains

Created 8/18/1994 Heikki Tuuri
*******************************************************/

#ifndef ha0ha_h
#define ha0ha_h

#include "univ.i"

#include "hash0hash.h"
#include "page0types.h"
#include "buf0types.h"
#include "rem0types.h"

/*************************************************************//**
Looks for an element in a hash table.
@return pointer to the data of the first hash table node in chain
having the fold number, NULL if not found */
UNIV_INLINE
rec_t*
ha_search_and_get_data(
/*===================*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold);	/*!< in: folded value of the searched data */
/*********************************************************//**
Looks for an element when we know the pointer to the data and updates
the pointer to data if found. */
UNIV_INTERN
void
ha_search_and_update_if_found_func(
/*===============================*/
	hash_table_t*	table,	/*!< in/out: hash table */
	ulint		fold,	/*!< in: folded value of the searched data */
	rec_t*		data,	/*!< in: pointer to the data */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	buf_block_t*	new_block,/*!< in: block containing new_data */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	rec_t*		new_data);/*!< in: new pointer to the data */

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/** Looks for an element when we know the pointer to the data and
updates the pointer to data if found.
@param table		in/out: hash table
@param fold		in: folded value of the searched data
@param data		in: pointer to the data
@param new_block	in: block containing new_data
@param new_data		in: new pointer to the data */
# define ha_search_and_update_if_found(table,fold,data,new_block,new_data) \
	ha_search_and_update_if_found_func(table,fold,data,new_block,new_data)
#else /* UNIV_AHI_DEBUG || UNIV_DEBUG */
/** Looks for an element when we know the pointer to the data and
updates the pointer to data if found.
@param table		in/out: hash table
@param fold		in: folded value of the searched data
@param data		in: pointer to the data
@param new_block	ignored: block containing new_data
@param new_data		in: new pointer to the data */
# define ha_search_and_update_if_found(table,fold,data,new_block,new_data) \
	ha_search_and_update_if_found_func(table,fold,data,new_data)
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
/*************************************************************//**
Creates a hash table with at least n array cells.  The actual number
of cells is chosen to be a prime number slightly bigger than n.
@return	own: created table */
UNIV_INTERN
hash_table_t*
ha_create_func(
/*===========*/
	ulint	n,		/*!< in: number of array cells */
#ifdef UNIV_SYNC_DEBUG
	ulint	mutex_level,	/*!< in: level of the mutexes in the latching
				order: this is used in the debug version */
#endif /* UNIV_SYNC_DEBUG */
	ulint	n_mutexes);	/*!< in: number of mutexes to protect the
				hash table: must be a power of 2, or 0 */
#ifdef UNIV_SYNC_DEBUG
/** Creates a hash table.
@return		own: created table
@param n_c	in: number of array cells.  The actual number of cells is
chosen to be a slightly bigger prime number.
@param level	in: level of the mutexes in the latching order
@param n_m	in: number of mutexes to protect the hash table;
		must be a power of 2, or 0 */
# define ha_create(n_c,n_m,level) ha_create_func(n_c,level,n_m)
#else /* UNIV_SYNC_DEBUG */
/** Creates a hash table.
@return		own: created table
@param n_c	in: number of array cells.  The actual number of cells is
chosen to be a slightly bigger prime number.
@param level	in: level of the mutexes in the latching order
@param n_m	in: number of mutexes to protect the hash table;
		must be a power of 2, or 0 */
# define ha_create(n_c,n_m,level) ha_create_func(n_c,n_m)
#endif /* UNIV_SYNC_DEBUG */

/*************************************************************//**
Inserts an entry into a hash table. If an entry with the same fold number
is found, its node is updated to point to the new data, and no new node
is inserted.
@return	TRUE if succeed, FALSE if no more memory could be allocated */
UNIV_INTERN
ibool
ha_insert_for_fold_func(
/*====================*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold,	/*!< in: folded value of data; if a node with
				the same fold value already exists, it is
				updated to point to the same data, and no new
				node is created! */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	buf_block_t*	block,	/*!< in: buffer block containing the data */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	rec_t*	data);	/*!< in: data, must not be NULL */

#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/**
Inserts an entry into a hash table. If an entry with the same fold number
is found, its node is updated to point to the new data, and no new node
is inserted.
@return	TRUE if succeed, FALSE if no more memory could be allocated
@param t	in: hash table
@param f	in: folded value of data
@param b	in: buffer block containing the data
@param d	in: data, must not be NULL */
# define ha_insert_for_fold(t,f,b,d) ha_insert_for_fold_func(t,f,b,d)
#else /* UNIV_AHI_DEBUG || UNIV_DEBUG */
/**
Inserts an entry into a hash table. If an entry with the same fold number
is found, its node is updated to point to the new data, and no new node
is inserted.
@return	TRUE if succeed, FALSE if no more memory could be allocated
@param t	in: hash table
@param f	in: folded value of data
@param b	ignored: buffer block containing the data
@param d	in: data, must not be NULL */
# define ha_insert_for_fold(t,f,b,d) ha_insert_for_fold_func(t,f,d)
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */

/*********************************************************//**
Looks for an element when we know the pointer to the data and deletes
it from the hash table if found.
@return	TRUE if found */
UNIV_INLINE
ibool
ha_search_and_delete_if_found(
/*==========================*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold,	/*!< in: folded value of the searched data */
	const rec_t*	data);	/*!< in: pointer to the data */
#ifndef UNIV_HOTBACKUP
/*****************************************************************//**
Removes from the chain determined by fold all nodes whose data pointer
points to the page given. */
UNIV_INTERN
void
ha_remove_all_nodes_to_page(
/*========================*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold,	/*!< in: fold value */
	const page_t*	page);	/*!< in: buffer page */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
/*************************************************************//**
Validates a given range of the cells in hash table.
@return	TRUE if ok */
UNIV_INTERN
ibool
ha_validate(
/*========*/
	hash_table_t*	table,		/*!< in: hash table */
	ulint		start_index,	/*!< in: start index */
	ulint		end_index);	/*!< in: end index */
#endif /* defined UNIV_AHI_DEBUG || defined UNIV_DEBUG */
/*************************************************************//**
Prints info of a hash table. */
UNIV_INTERN
void
ha_print_info(
/*==========*/
	FILE*		file,	/*!< in: file where to print */
	hash_table_t*	table);	/*!< in: hash table */
#endif /* !UNIV_HOTBACKUP */

/** The hash table external chain node */
typedef struct ha_node_struct ha_node_t;

/** The hash table external chain node */
struct ha_node_struct {
	ha_node_t*	next;	/*!< next chain node or NULL if none */
#if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	buf_block_t*	block;	/*!< buffer block containing the data, or NULL */
#endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
        rec_t*		data;	/*!< pointer to the data */
	ulint		fold;	/*!< fold value for the data */
};

#ifndef UNIV_HOTBACKUP
/** Assert that the current thread is holding the mutex protecting a
hash bucket corresponding to a fold value.
@param table	in: hash table
@param fold	in: fold value */
# define ASSERT_HASH_MUTEX_OWN(table, fold)				\
	ut_ad(!(table)->mutexes || mutex_own(hash_get_mutex(table, fold)))
#else /* !UNIV_HOTBACKUP */
/** Assert that the current thread is holding the mutex protecting a
hash bucket corresponding to a fold value.
@param table	in: hash table
@param fold	in: fold value */
# define ASSERT_HASH_MUTEX_OWN(table, fold) ((void) 0)
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_NONINL
#include "ha0ha.ic"
#endif

#endif
