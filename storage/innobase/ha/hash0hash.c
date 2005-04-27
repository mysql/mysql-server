/******************************************************
The simple hash table utility

(c) 1997 Innobase Oy

Created 5/20/1997 Heikki Tuuri
*******************************************************/

#include "hash0hash.h"
#ifdef UNIV_NONINL
#include "hash0hash.ic"
#endif

#include "mem0mem.h"

/****************************************************************
Reserves the mutex for a fold value in a hash table. */

void
hash_mutex_enter(
/*=============*/
	hash_table_t* 	table,	/* in: hash table */
	ulint 		fold)	/* in: fold */
{
	mutex_enter(hash_get_mutex(table, fold));
}

/****************************************************************
Releases the mutex for a fold value in a hash table. */

void
hash_mutex_exit(
/*============*/
	hash_table_t* 	table,	/* in: hash table */
	ulint 		fold)	/* in: fold */
{
	mutex_exit(hash_get_mutex(table, fold));
}

/****************************************************************
Reserves all the mutexes of a hash table, in an ascending order. */

void
hash_mutex_enter_all(
/*=================*/
	hash_table_t* 	table)	/* in: hash table */
{
	ulint	i;

	for (i = 0; i < table->n_mutexes; i++) {

		mutex_enter(table->mutexes + i);
	}
}

/****************************************************************
Releases all the mutexes of a hash table. */

void
hash_mutex_exit_all(
/*================*/
	hash_table_t* 	table)	/* in: hash table */
{
	ulint	i;

	for (i = 0; i < table->n_mutexes; i++) {

		mutex_exit(table->mutexes + i);
	}
}

/*****************************************************************
Creates a hash table with >= n array cells. The actual number of cells is
chosen to be a prime number slightly bigger than n. */

hash_table_t*
hash_create(
/*========*/
			/* out, own: created table */
	ulint	n)	/* in: number of array cells */
{
	hash_cell_t*	array;
	ulint		prime;
	hash_table_t*	table;
	ulint		i;
	hash_cell_t*	cell;
	
	prime = ut_find_prime(n);

	table = mem_alloc(sizeof(hash_table_t));

	array = ut_malloc(sizeof(hash_cell_t) * prime);
	
	table->adaptive = FALSE;
	table->array = array;
	table->n_cells = prime;
	table->n_mutexes = 0;
	table->mutexes = NULL;
	table->heaps = NULL;
	table->heap = NULL;
	table->magic_n = HASH_TABLE_MAGIC_N;
	
	/* Initialize the cell array */

	for (i = 0; i < prime; i++) {

		cell = hash_get_nth_cell(table, i);
		cell->node = NULL;
	}

	return(table);
}

/*****************************************************************
Frees a hash table. */

void
hash_table_free(
/*============*/
	hash_table_t*	table)	/* in, own: hash table */
{
	ut_a(table->mutexes == NULL);

	ut_free(table->array);
	mem_free(table);
}

/*****************************************************************
Creates a mutex array to protect a hash table. */

void
hash_create_mutexes(
/*================*/
	hash_table_t*	table,		/* in: hash table */
	ulint		n_mutexes,	/* in: number of mutexes, must be a
					power of 2 */
	ulint		sync_level)	/* in: latching order level of the
					mutexes: used in the debug version */
{
	ulint	i;

	ut_a(n_mutexes == ut_2_power_up(n_mutexes));

	table->mutexes = mem_alloc(n_mutexes * sizeof(mutex_t));

	for (i = 0; i < n_mutexes; i++) {
		mutex_create(table->mutexes + i);

		mutex_set_level(table->mutexes + i, sync_level);
	}

	table->n_mutexes = n_mutexes;
}
