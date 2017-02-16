/*****************************************************************************

Copyright (c) 1997, 2009, Innobase Oy. All Rights Reserved.

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

/********************************************************************//**
@file mem/mem0pool.c
The lowest-level memory management

Created 5/12/1997 Heikki Tuuri
*************************************************************************/

#include "mem0pool.h"
#ifdef UNIV_NONINL
#include "mem0pool.ic"
#endif

#include "srv0srv.h"
#include "sync0sync.h"
#include "ut0mem.h"
#include "ut0lst.h"
#include "ut0byte.h"
#include "mem0mem.h"
#include "srv0start.h"

/* We would like to use also the buffer frames to allocate memory. This
would be desirable, because then the memory consumption of the database
would be fixed, and we might even lock the buffer pool to the main memory.
The problem here is that the buffer management routines can themselves call
memory allocation, while the buffer pool mutex is reserved.

The main components of the memory consumption are:

1. buffer pool,
2. parsed and optimized SQL statements,
3. data dictionary cache,
4. log buffer,
5. locks for each transaction,
6. hash table for the adaptive index,
7. state and buffers for each SQL query currently being executed,
8. session for each user, and
9. stack for each OS thread.

Items 1 and 2 are managed by an LRU algorithm. Items 5 and 6 can potentially
consume very much memory. Items 7 and 8 should consume quite little memory,
and the OS should take care of item 9, which too should consume little memory.

A solution to the memory management:

1. the buffer pool size is set separately;
2. log buffer size is set separately;
3. the common pool size for all the other entries, except 8, is set separately.

Problems: we may waste memory if the common pool is set too big. Another
problem is the locks, which may take very much space in big transactions.
Then the shared pool size should be set very big. We can allow locks to take
space from the buffer pool, but the SQL optimizer is then unaware of the
usable size of the buffer pool. We could also combine the objects in the
common pool and the buffers in the buffer pool into a single LRU list and
manage it uniformly, but this approach does not take into account the parsing
and other costs unique to SQL statements.

The locks for a transaction can be seen as a part of the state of the
transaction. Hence, they should be stored in the common pool. We still
have the problem of a very big update transaction, for example, which
will set very many x-locks on rows, and the locks will consume a lot
of memory, say, half of the buffer pool size.

Another problem is what to do if we are not able to malloc a requested
block of memory from the common pool. Then we can request memory from
the operating system. If it does not help, a system error results.

Because 5 and 6 may potentially consume very much memory, we let them grow
into the buffer pool. We may let the locks of a transaction take frames
from the buffer pool, when the corresponding memory heap block has grown to
the size of a buffer frame. Similarly for the hash node cells of the locks,
and for the adaptive index. Thus, for each individual transaction, its locks
can occupy at most about the size of the buffer frame of memory in the common
pool, and after that its locks will grow into the buffer pool. */

/** Mask used to extract the free bit from area->size */
#define MEM_AREA_FREE	1

/** The smallest memory area total size */
#define MEM_AREA_MIN_SIZE	(2 * MEM_AREA_EXTRA_SIZE)


/** Data structure for a memory pool. The space is allocated using the buddy
algorithm, where free list i contains areas of size 2 to power i. */
struct mem_pool_struct{
	byte*		buf;		/*!< memory pool */
	ulint		size;		/*!< memory common pool size */
	ulint		reserved;	/*!< amount of currently allocated
					memory */
	mutex_t		mutex;		/*!< mutex protecting this struct */
	UT_LIST_BASE_NODE_T(mem_area_t)
			free_list[64];	/*!< lists of free memory areas: an
					area is put to the list whose number
					is the 2-logarithm of the area size */
};

/** The common memory pool */
UNIV_INTERN mem_pool_t*	mem_comm_pool	= NULL;

#ifdef UNIV_PFS_MUTEX
/* Key to register mutex in mem_pool_struct with performance schema */
UNIV_INTERN mysql_pfs_key_t	mem_pool_mutex_key;
#endif /* UNIV_PFS_MUTEX */

/* We use this counter to check that the mem pool mutex does not leak;
this is to track a strange assertion failure reported at
mysql@lists.mysql.com */

UNIV_INTERN ulint	mem_n_threads_inside		= 0;

/********************************************************************//**
Reserves the mem pool mutex if we are not in server shutdown. Use
this function only in memory free functions, since only memory
free functions are used during server shutdown. */
UNIV_INLINE
void
mem_pool_mutex_enter(
/*=================*/
	mem_pool_t*	pool)		/*!< in: memory pool */
{
	if (srv_shutdown_state < SRV_SHUTDOWN_EXIT_THREADS) {
		mutex_enter(&(pool->mutex));
	}
}

/********************************************************************//**
Releases the mem pool mutex if we are not in server shutdown. As
its corresponding mem_pool_mutex_enter() function, use it only
in memory free functions */
UNIV_INLINE
void
mem_pool_mutex_exit(
/*================*/
	mem_pool_t*	pool)		/*!< in: memory pool */
{
	if (srv_shutdown_state < SRV_SHUTDOWN_EXIT_THREADS) {
		mutex_exit(&(pool->mutex));
	}
}

/********************************************************************//**
Returns memory area size.
@return	size */
UNIV_INLINE
ulint
mem_area_get_size(
/*==============*/
	mem_area_t*	area)	/*!< in: area */
{
	return(area->size_and_free & ~MEM_AREA_FREE);
}

/********************************************************************//**
Sets memory area size. */
UNIV_INLINE
void
mem_area_set_size(
/*==============*/
	mem_area_t*	area,	/*!< in: area */
	ulint		size)	/*!< in: size */
{
	area->size_and_free = (area->size_and_free & MEM_AREA_FREE)
		| size;
}

/********************************************************************//**
Returns memory area free bit.
@return	TRUE if free */
UNIV_INLINE
ibool
mem_area_get_free(
/*==============*/
	mem_area_t*	area)	/*!< in: area */
{
#if TRUE != MEM_AREA_FREE
# error "TRUE != MEM_AREA_FREE"
#endif
	return(area->size_and_free & MEM_AREA_FREE);
}

/********************************************************************//**
Sets memory area free bit. */
UNIV_INLINE
void
mem_area_set_free(
/*==============*/
	mem_area_t*	area,	/*!< in: area */
	ibool		free)	/*!< in: free bit value */
{
#if TRUE != MEM_AREA_FREE
# error "TRUE != MEM_AREA_FREE"
#endif
	area->size_and_free = (area->size_and_free & ~MEM_AREA_FREE)
		| free;
}

/********************************************************************//**
Creates a memory pool.
@return	memory pool */
UNIV_INTERN
mem_pool_t*
mem_pool_create(
/*============*/
	ulint	size)	/*!< in: pool size in bytes */
{
	mem_pool_t*	pool;
	mem_area_t*	area;
	ulint		i;
	ulint		used;

	pool = ut_malloc(sizeof(mem_pool_t));

	pool->buf = ut_malloc_low(size, TRUE);
	pool->size = size;

	mutex_create(mem_pool_mutex_key, &pool->mutex, SYNC_MEM_POOL);

	/* Initialize the free lists */

	for (i = 0; i < 64; i++) {

		UT_LIST_INIT(pool->free_list[i]);
	}

	used = 0;

	while (size - used >= MEM_AREA_MIN_SIZE) {

		i = ut_2_log(size - used);

		if (ut_2_exp(i) > size - used) {

			/* ut_2_log rounds upward */

			i--;
		}

		area = (mem_area_t*)(pool->buf + used);

		mem_area_set_size(area, ut_2_exp(i));
		mem_area_set_free(area, TRUE);
		UNIV_MEM_FREE(MEM_AREA_EXTRA_SIZE + (byte*) area,
			      ut_2_exp(i) - MEM_AREA_EXTRA_SIZE);

		UT_LIST_ADD_FIRST(free_list, pool->free_list[i], area);

		used = used + ut_2_exp(i);
	}

	ut_ad(size >= used);

	pool->reserved = 0;

	return(pool);
}

/********************************************************************//**
Frees a memory pool. */
UNIV_INTERN
void
mem_pool_free(
/*==========*/
	mem_pool_t*	pool)	/*!< in, own: memory pool */
{
	ut_free(pool->buf);
	ut_free(pool);
}

/********************************************************************//**
Fills the specified free list.
@return	TRUE if we were able to insert a block to the free list */
static
ibool
mem_pool_fill_free_list(
/*====================*/
	ulint		i,	/*!< in: free list index */
	mem_pool_t*	pool)	/*!< in: memory pool */
{
	mem_area_t*	area;
	mem_area_t*	area2;
	ibool		ret;

	ut_ad(mutex_own(&(pool->mutex)));

	if (UNIV_UNLIKELY(i >= 63)) {
		/* We come here when we have run out of space in the
		memory pool: */

		return(FALSE);
	}

	area = UT_LIST_GET_FIRST(pool->free_list[i + 1]);

	if (area == NULL) {
		if (UT_LIST_GET_LEN(pool->free_list[i + 1]) > 0) {
			ut_print_timestamp(stderr);

			fprintf(stderr,
				"  InnoDB: Error: mem pool free list %lu"
				" length is %lu\n"
				"InnoDB: though the list is empty!\n",
				(ulong) i + 1,
				(ulong)
				UT_LIST_GET_LEN(pool->free_list[i + 1]));
		}

		ret = mem_pool_fill_free_list(i + 1, pool);

		if (ret == FALSE) {

			return(FALSE);
		}

		area = UT_LIST_GET_FIRST(pool->free_list[i + 1]);
	}

	if (UNIV_UNLIKELY(UT_LIST_GET_LEN(pool->free_list[i + 1]) == 0)) {
		mem_analyze_corruption(area);

		ut_error;
	}

	UT_LIST_REMOVE(free_list, pool->free_list[i + 1], area);

	area2 = (mem_area_t*)(((byte*)area) + ut_2_exp(i));
	UNIV_MEM_ALLOC(area2, MEM_AREA_EXTRA_SIZE);

	mem_area_set_size(area2, ut_2_exp(i));
	mem_area_set_free(area2, TRUE);

	UT_LIST_ADD_FIRST(free_list, pool->free_list[i], area2);

	mem_area_set_size(area, ut_2_exp(i));

	UT_LIST_ADD_FIRST(free_list, pool->free_list[i], area);

	return(TRUE);
}

/********************************************************************//**
Allocates memory from a pool. NOTE: This low-level function should only be
used in mem0mem.*!
@return	own: allocated memory buffer */
UNIV_INTERN
void*
mem_area_alloc(
/*===========*/
	ulint*		psize,	/*!< in: requested size in bytes; for optimum
				space usage, the size should be a power of 2
				minus MEM_AREA_EXTRA_SIZE;
				out: allocated size in bytes (greater than
				or equal to the requested size) */
	mem_pool_t*	pool)	/*!< in: memory pool */
{
	mem_area_t*	area;
	ulint		size;
	ulint		n;
	ibool		ret;

	/* If we are using os allocator just make a simple call
	to malloc */
	if (UNIV_LIKELY(srv_use_sys_malloc)) {
		return(malloc(*psize));
	}

	size = *psize;
	n = ut_2_log(ut_max(size + MEM_AREA_EXTRA_SIZE, MEM_AREA_MIN_SIZE));

	mutex_enter(&(pool->mutex));
	mem_n_threads_inside++;

	ut_a(mem_n_threads_inside == 1);

	area = UT_LIST_GET_FIRST(pool->free_list[n]);

	if (area == NULL) {
		ret = mem_pool_fill_free_list(n, pool);

		if (ret == FALSE) {
			/* Out of memory in memory pool: we try to allocate
			from the operating system with the regular malloc: */

			mem_n_threads_inside--;
			mutex_exit(&(pool->mutex));

			return(ut_malloc(size));
		}

		area = UT_LIST_GET_FIRST(pool->free_list[n]);
	}

	if (!mem_area_get_free(area)) {
		fprintf(stderr,
			"InnoDB: Error: Removing element from mem pool"
			" free list %lu though the\n"
			"InnoDB: element is not marked free!\n",
			(ulong) n);

		mem_analyze_corruption(area);

		/* Try to analyze a strange assertion failure reported at
		mysql@lists.mysql.com where the free bit IS 1 in the
		hex dump above */

		if (mem_area_get_free(area)) {
			fprintf(stderr,
				"InnoDB: Probably a race condition"
				" because now the area is marked free!\n");
		}

		ut_error;
	}

	if (UT_LIST_GET_LEN(pool->free_list[n]) == 0) {
		fprintf(stderr,
			"InnoDB: Error: Removing element from mem pool"
			" free list %lu\n"
			"InnoDB: though the list length is 0!\n",
			(ulong) n);
		mem_analyze_corruption(area);

		ut_error;
	}

	ut_ad(mem_area_get_size(area) == ut_2_exp(n));

	mem_area_set_free(area, FALSE);

	UT_LIST_REMOVE(free_list, pool->free_list[n], area);

	pool->reserved += mem_area_get_size(area);

	mem_n_threads_inside--;
	mutex_exit(&(pool->mutex));

	ut_ad(mem_pool_validate(pool));

	*psize = ut_2_exp(n) - MEM_AREA_EXTRA_SIZE;
	UNIV_MEM_ALLOC(MEM_AREA_EXTRA_SIZE + (byte*)area, *psize);

	return((void*)(MEM_AREA_EXTRA_SIZE + ((byte*)area)));
}

/********************************************************************//**
Gets the buddy of an area, if it exists in pool.
@return	the buddy, NULL if no buddy in pool */
UNIV_INLINE
mem_area_t*
mem_area_get_buddy(
/*===============*/
	mem_area_t*	area,	/*!< in: memory area */
	ulint		size,	/*!< in: memory area size */
	mem_pool_t*	pool)	/*!< in: memory pool */
{
	mem_area_t*	buddy;

	ut_ad(size != 0);

	if (((((byte*)area) - pool->buf) % (2 * size)) == 0) {

		/* The buddy is in a higher address */

		buddy = (mem_area_t*)(((byte*)area) + size);

		if ((((byte*)buddy) - pool->buf) + size > pool->size) {

			/* The buddy is not wholly contained in the pool:
			there is no buddy */

			buddy = NULL;
		}
	} else {
		/* The buddy is in a lower address; NOTE that area cannot
		be at the pool lower end, because then we would end up to
		the upper branch in this if-clause: the remainder would be
		0 */

		buddy = (mem_area_t*)(((byte*)area) - size);
	}

	return(buddy);
}

/********************************************************************//**
Frees memory to a pool. */
UNIV_INTERN
void
mem_area_free(
/*==========*/
	void*		ptr,	/*!< in, own: pointer to allocated memory
				buffer */
	mem_pool_t*	pool)	/*!< in: memory pool */
{
	mem_area_t*	area;
	mem_area_t*	buddy;
	void*		new_ptr;
	ulint		size;
	ulint		n;

	if (UNIV_LIKELY(srv_use_sys_malloc)) {
		free(ptr);

		return;
	}

	/* It may be that the area was really allocated from the OS with
	regular malloc: check if ptr points within our memory pool */

	if ((byte*)ptr < pool->buf || (byte*)ptr >= pool->buf + pool->size) {
		ut_free(ptr);

		return;
	}

	area = (mem_area_t*) (((byte*)ptr) - MEM_AREA_EXTRA_SIZE);

	if (mem_area_get_free(area)) {
		fprintf(stderr,
			"InnoDB: Error: Freeing element to mem pool"
			" free list though the\n"
			"InnoDB: element is marked free!\n");

		mem_analyze_corruption(area);
		ut_error;
	}

	size = mem_area_get_size(area);
	UNIV_MEM_FREE(ptr, size - MEM_AREA_EXTRA_SIZE);

	if (size == 0) {
		fprintf(stderr,
			"InnoDB: Error: Mem area size is 0. Possibly a"
			" memory overrun of the\n"
			"InnoDB: previous allocated area!\n");

		mem_analyze_corruption(area);
		ut_error;
	}

#ifdef UNIV_LIGHT_MEM_DEBUG
	if (((byte*)area) + size < pool->buf + pool->size) {

		ulint	next_size;

		next_size = mem_area_get_size(
			(mem_area_t*)(((byte*)area) + size));
		if (UNIV_UNLIKELY(!next_size || !ut_is_2pow(next_size))) {
			fprintf(stderr,
				"InnoDB: Error: Memory area size %lu,"
				" next area size %lu not a power of 2!\n"
				"InnoDB: Possibly a memory overrun of"
				" the buffer being freed here.\n",
				(ulong) size, (ulong) next_size);
			mem_analyze_corruption(area);

			ut_error;
		}
	}
#endif
	buddy = mem_area_get_buddy(area, size, pool);

	n = ut_2_log(size);

	mem_pool_mutex_enter(pool);
	mem_n_threads_inside++;

	ut_a(mem_n_threads_inside == 1);

	if (buddy && mem_area_get_free(buddy)
	    && (size == mem_area_get_size(buddy))) {

		/* The buddy is in a free list */

		if ((byte*)buddy < (byte*)area) {
			new_ptr = ((byte*)buddy) + MEM_AREA_EXTRA_SIZE;

			mem_area_set_size(buddy, 2 * size);
			mem_area_set_free(buddy, FALSE);
		} else {
			new_ptr = ptr;

			mem_area_set_size(area, 2 * size);
		}

		/* Remove the buddy from its free list and merge it to area */

		UT_LIST_REMOVE(free_list, pool->free_list[n], buddy);

		pool->reserved += ut_2_exp(n);

		mem_n_threads_inside--;
		mem_pool_mutex_exit(pool);

		mem_area_free(new_ptr, pool);

		return;
	} else {
		UT_LIST_ADD_FIRST(free_list, pool->free_list[n], area);

		mem_area_set_free(area, TRUE);

		ut_ad(pool->reserved >= size);

		pool->reserved -= size;
	}

	mem_n_threads_inside--;
	mem_pool_mutex_exit(pool);

	ut_ad(mem_pool_validate(pool));
}

/********************************************************************//**
Validates a memory pool.
@return	TRUE if ok */
UNIV_INTERN
ibool
mem_pool_validate(
/*==============*/
	mem_pool_t*	pool)	/*!< in: memory pool */
{
	mem_area_t*	area;
	mem_area_t*	buddy;
	ulint		free;
	ulint		i;

	mem_pool_mutex_enter(pool);

	free = 0;

	for (i = 0; i < 64; i++) {

		UT_LIST_VALIDATE(free_list, mem_area_t, pool->free_list[i],
				 (void) 0);

		area = UT_LIST_GET_FIRST(pool->free_list[i]);

		while (area != NULL) {
			ut_a(mem_area_get_free(area));
			ut_a(mem_area_get_size(area) == ut_2_exp(i));

			buddy = mem_area_get_buddy(area, ut_2_exp(i), pool);

			ut_a(!buddy || !mem_area_get_free(buddy)
			     || (ut_2_exp(i) != mem_area_get_size(buddy)));

			area = UT_LIST_GET_NEXT(free_list, area);

			free += ut_2_exp(i);
		}
	}

	ut_a(free + pool->reserved == pool->size);

	mem_pool_mutex_exit(pool);

	return(TRUE);
}

/********************************************************************//**
Prints info of a memory pool. */
UNIV_INTERN
void
mem_pool_print_info(
/*================*/
	FILE*		outfile,/*!< in: output file to write to */
	mem_pool_t*	pool)	/*!< in: memory pool */
{
	ulint		i;

	mem_pool_validate(pool);

	fprintf(outfile, "INFO OF A MEMORY POOL\n");

	mutex_enter(&(pool->mutex));

	for (i = 0; i < 64; i++) {
		if (UT_LIST_GET_LEN(pool->free_list[i]) > 0) {

			fprintf(outfile,
				"Free list length %lu for"
				" blocks of size %lu\n",
				(ulong) UT_LIST_GET_LEN(pool->free_list[i]),
				(ulong) ut_2_exp(i));
		}
	}

	fprintf(outfile, "Pool size %lu, reserved %lu.\n", (ulong) pool->size,
		(ulong) pool->reserved);
	mutex_exit(&(pool->mutex));
}

/********************************************************************//**
Returns the amount of reserved memory.
@return	reserved memory in bytes */
UNIV_INTERN
ulint
mem_pool_get_reserved(
/*==================*/
	mem_pool_t*	pool)	/*!< in: memory pool */
{
	ulint	reserved;

	mutex_enter(&(pool->mutex));

	reserved = pool->reserved;

	mutex_exit(&(pool->mutex));

	return(reserved);
}
