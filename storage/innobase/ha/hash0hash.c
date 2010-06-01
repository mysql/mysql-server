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
@file ha/hash0hash.c
The simple hash table utility

Created 5/20/1997 Heikki Tuuri
*******************************************************/

#include "hash0hash.h"
#ifdef UNIV_NONINL
#include "hash0hash.ic"
#endif

#include "mem0mem.h"

#ifndef UNIV_HOTBACKUP

# ifdef UNIV_PFS_MUTEX
UNIV_INTERN mysql_pfs_key_t	hash_table_mutex_key;
# endif /* UNIV_PFS_MUTEX */

/************************************************************//**
Reserves the mutex for a fold value in a hash table. */
UNIV_INTERN
void
hash_mutex_enter(
/*=============*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold)	/*!< in: fold */
{
	mutex_enter(hash_get_mutex(table, fold));
}

/************************************************************//**
Releases the mutex for a fold value in a hash table. */
UNIV_INTERN
void
hash_mutex_exit(
/*============*/
	hash_table_t*	table,	/*!< in: hash table */
	ulint		fold)	/*!< in: fold */
{
	mutex_exit(hash_get_mutex(table, fold));
}

/************************************************************//**
Reserves all the mutexes of a hash table, in an ascending order. */
UNIV_INTERN
void
hash_mutex_enter_all(
/*=================*/
	hash_table_t*	table)	/*!< in: hash table */
{
	ulint	i;

	for (i = 0; i < table->n_mutexes; i++) {

		mutex_enter(table->mutexes + i);
	}
}

/************************************************************//**
Releases all the mutexes of a hash table. */
UNIV_INTERN
void
hash_mutex_exit_all(
/*================*/
	hash_table_t*	table)	/*!< in: hash table */
{
	ulint	i;

	for (i = 0; i < table->n_mutexes; i++) {

		mutex_exit(table->mutexes + i);
	}
}
#endif /* !UNIV_HOTBACKUP */

/*************************************************************//**
Creates a hash table with >= n array cells. The actual number of cells is
chosen to be a prime number slightly bigger than n.
@return	own: created table */
UNIV_INTERN
hash_table_t*
hash_create(
/*========*/
	ulint	n)	/*!< in: number of array cells */
{
	hash_cell_t*	array;
	ulint		prime;
	hash_table_t*	table;

	prime = ut_find_prime(n);

	table = mem_alloc(sizeof(hash_table_t));

	array = ut_malloc(sizeof(hash_cell_t) * prime);

	table->array = array;
	table->n_cells = prime;
#ifndef UNIV_HOTBACKUP
# if defined UNIV_AHI_DEBUG || defined UNIV_DEBUG
	table->adaptive = FALSE;
# endif /* UNIV_AHI_DEBUG || UNIV_DEBUG */
	table->n_mutexes = 0;
	table->mutexes = NULL;
	table->heaps = NULL;
#endif /* !UNIV_HOTBACKUP */
	table->heap = NULL;
	ut_d(table->magic_n = HASH_TABLE_MAGIC_N);

	/* Initialize the cell array */
	hash_table_clear(table);

	return(table);
}

/*************************************************************//**
Frees a hash table. */
UNIV_INTERN
void
hash_table_free(
/*============*/
	hash_table_t*	table)	/*!< in, own: hash table */
{
	ut_ad(table);
	ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
#ifndef UNIV_HOTBACKUP
	ut_a(table->mutexes == NULL);
#endif /* !UNIV_HOTBACKUP */

	ut_free(table->array);
	mem_free(table);
}

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
	ulint		n_mutexes)	/*!< in: number of mutexes, must be a
					power of 2 */
{
	ulint	i;

	ut_ad(table);
	ut_ad(table->magic_n == HASH_TABLE_MAGIC_N);
	ut_a(n_mutexes > 0);
	ut_a(ut_is_2pow(n_mutexes));

	table->mutexes = mem_alloc(n_mutexes * sizeof(mutex_t));

	for (i = 0; i < n_mutexes; i++) {
		mutex_create(hash_table_mutex_key,
			     table->mutexes + i, sync_level);
	}

	table->n_mutexes = n_mutexes;
}
#endif /* !UNIV_HOTBACKUP */
